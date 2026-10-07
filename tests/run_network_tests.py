"""Network acceptance checks; --fault-inject builds an isolated fault-injection binary."""
import argparse
import concurrent.futures
import http.client
from pathlib import Path
import signal
import socket
import subprocess
import tempfile
import threading
import time

ROOT = Path(__file__).resolve().parents[1]


def request(port, method="GET", path="/", body=None):
    connection = http.client.HTTPConnection("127.0.0.1", port, timeout=10)
    try:
        connection.request(method, path, body=body)
        response = connection.getresponse()
        data = response.read()
        assert int(response.getheader("Content-Length")) == len(data)
        assert response.getheader("Connection") == "close"
        return response.status, data
    finally:
        connection.close()


def raw_response(port, payload, delay=0):
    with socket.create_connection(("127.0.0.1", port), timeout=10) as client:
        client.sendall(payload)
        if delay:
            time.sleep(delay)
        client.settimeout(10)
        response = http.client.HTTPResponse(client)
        response.begin()
        body = response.read()
        assert int(response.getheader("Content-Length")) == len(body)
        assert response.getheader("Connection") == "close"
        return response.status, body


def wait_ready(server, port):
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        assert server.poll() is None, "Server exited at startup"
        try:
            assert request(port)[0] == 200
            return
        except OSError:
            time.sleep(0.05)
    raise AssertionError("Server did not start")


def children(server):
    path = Path(f"/proc/{server.pid}/task/{server.pid}/children")
    return path.read_text().split()


def stop(server):
    if server.poll() is None:
        server.send_signal(signal.SIGINT)
        try:
            assert server.wait(timeout=3) == 0
        except subprocess.TimeoutExpired:
            server.kill()
            server.wait()
            raise


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--fault-inject", action="store_true")
    options = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="webserv-network-") as directory:
        temp = Path(directory)
        executable = ROOT / "webserv"
        if options.fault_inject:
            network_object = temp / "NetworkManager.o"
            flags = ["-Wall", "-Wextra", "-Werror", "-std=c++98", "-Iinclude"]
            subprocess.run(["c++", *flags, "-DWEBSERV_FAULT_INJECT", "-c",
                            "src/NetworkManager.cpp", "-o", str(network_object)], cwd=ROOT, check=True)
            objects = sorted((ROOT / "out").rglob("*.o"))
            objects = [obj for obj in objects if obj.name != "NetworkManager.o"]
            executable = temp / "webserv-fault"
            subprocess.run(["c++", *flags, *map(str, objects), str(network_object),
                            "-o", str(executable)], cwd=ROOT, check=True)
            with socket.socket() as probe:
                probe.bind(("127.0.0.1", 0))
                port = probe.getsockname()[1]
            config = temp / "fault.conf"
            config.write_text(f"server {{ host 127.0.0.1; listen {port}; root ./www; "
                              "location / { allowed_methods GET; index index.html; } }")
            ports = [port]
        else:
            config = ROOT / "configs/multiple-servers.conf"
            ports = [8080, 8081, 9090]
        with (temp / "server.log").open("wb") as log:
            server = subprocess.Popen([str(executable), str(config)], cwd=ROOT, stdout=log, stderr=log)
            try:
                for port in ports:
                    wait_ready(server, port)
                port = ports[0]
                baseline = len(list(Path(f"/proc/{server.pid}/fd").iterdir()))
                if not options.fault_inject:
                    barrier = threading.Barrier(300)

                    def concurrent_get(_):
                        barrier.wait(timeout=10)
                        return request(port)[0]

                    with concurrent.futures.ThreadPoolExecutor(max_workers=300) as pool:
                        assert list(pool.map(concurrent_get, range(300))) == [200] * 300
                    assert request(port)[0] == 200
                    fuzz = [
                        (b"GET / HTTP/1.1\r\n\r\n", 400),
                        (b"GET /" + b"x" * 9000 + b" HTTP/1.1\r\nHost: localhost\r\n\r\n", 431),
                        (b"GET / HTTP/9.9\r\nHost: localhost\r\n\r\n", 400),
                        (b"GET /../../etc/passwd HTTP/1.1\r\nHost: localhost\r\n\r\n", 400),
                        (b"GET /%00 HTTP/1.1\r\nHost: localhost\r\n\r\n", 400),
                        (b"POST /upload/test HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\nZ\r\nx\r\n0\r\n\r\n", 400),
                        (b"BREW / HTTP/1.1\r\nHost: localhost\r\n\r\n", 501),
                        (b"POST /upload/test HTTP/1.1\r\nHost: localhost\r\nContent-Length: 2097152\r\n\r\n", 413),
                    ]
                    for payload, expected in fuzz:
                        assert raw_response(port, payload)[0] == expected, expected
                    assert request(port, path="/old-page")[0] == 301
                    name = f"network-test-{server.pid}.txt"
                    try:
                        assert request(port, "POST", "/upload/" + name, b"upload test")[0] == 201
                        assert request(port, path="/upload/" + name) == (200, b"upload test")
                        assert request(port, "DELETE", "/upload/" + name)[0] == 204
                        assert request(port, path="/upload/" + name)[0] == 404
                    finally:
                        (ROOT / "www/uploads" / name).unlink(missing_ok=True)
                    # An abruptly disconnected incomplete POST must be removed.
                    disconnected = socket.create_connection(("127.0.0.1", port))
                    disconnected.sendall(b"POST /cgi-bin/hello.py HTTP/1.1\r\nHost: localhost\r\n"
                                          b"Content-Length: 900000\r\n\r\n")
                    disconnected.close()
                    time.sleep(0.2)
                status, body = raw_response(port, b"GET / HTTP/1.1\r\nHost: localhost\r\n", delay=40)
                assert status == 408
                if options.fault_inject:
                    assert body == b"", "Fault path must use the fixed empty-body fallback"
                assert request(port)[0] == 200
                time.sleep(0.2)
                assert not children(server), "CGI child leaked"
                assert len(list(Path(f"/proc/{server.pid}/fd").iterdir())) == baseline, "Descriptor leaked"
                stop(server)
                mode = "fault-injection fallback" if options.fault_inject else "multiport, stress, fuzz, upload/DELETE"
                print(f"Passed {mode}, 40-second timeout, descriptor check, and SIGINT shutdown")
            finally:
                stop(server)
        if not options.fault_inject:
            with (temp / "default.log").open("wb") as log:
                server = subprocess.Popen([str(executable)], cwd=ROOT, stdout=log, stderr=log)
                try:
                    wait_ready(server, 8080)
                finally:
                    stop(server)
            print("Default configuration path passed")


if __name__ == "__main__":
    main()
