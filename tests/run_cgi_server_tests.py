"""CGI integration checks. Run on POSIX after the networking hook is connected."""
import concurrent.futures
import argparse
import http.client
import json
import os
from pathlib import Path
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", nargs="?", default=str(ROOT / "webserv"))
    parser.add_argument("--review-symlinks", action="store_true")
    options = parser.parse_args()
    if os.name != "posix":
        raise SystemExit("A POSIX server executable is required")
    executable = Path(options.executable).resolve()
    if not executable.is_file():
        raise SystemExit("Build webserv first with make")
    with tempfile.TemporaryDirectory(prefix="webserv-cgi-server-") as temporary, \
            tempfile.TemporaryDirectory(prefix="webserv-outside-") as outside_directory:
        root = Path(temporary)
        shutil.copytree(ROOT / "www" / "cgi-bin", root / "cgi")
        (root / "uploads" / "nested").mkdir(parents=True)
        (root / "index.html").write_text("server alive")
        (root / "cgi" / "hang.py").write_text("import time\ntime.sleep(60)\n")
        (root / "cgi" / "crash.py").write_text("raise RuntimeError('CGI test crash')\n")
        (root / "cgi" / "large.py").write_text(
            "import sys\nsys.stdout.write('Content-Type: text/plain\\n\\n' + 'x' * 32768)\n")
        (root / "cgi" / "stream.py").write_text(
            "import sys\nsys.stdout.buffer.write(b'Content-Type: application/octet-stream\\n\\n')\n"
            "sys.stdout.buffer.flush()\nwhile True:\n"
            "    data = sys.stdin.buffer.read(8192)\n"
            "    if not data: break\n"
            "    sys.stdout.buffer.write(data)\n"
            "    sys.stdout.buffer.flush()\n")
        (root / "cgi" / "status.py").write_text(
            "import sys\nsys.stdout.write('Status: 201 Created\\nContent-Type: text/plain\\nContent-Length: 2\\n\\nok')\n")
        (root / "cgi" / "empty-error.py").write_text(
            "import sys\nsys.stdout.write('Status: 400 Bad Request\\nContent-Type: text/plain\\nContent-Length: 0\\n\\n')\n")
        (root / "cgi" / "invalid.py").write_text(
            "import sys\nsys.stdout.write('Content-Type: text/plain\\nContent-Length: 9\\n\\nx')\n")
        (root / "cgi" / "meta.py").write_text(
            "import json, os, sys\nbody = sys.stdin.buffer.read()\n"
            "sys.stdout.write('Content-Type: application/json\\n\\n' + json.dumps({"
            "'query': os.environ['QUERY_STRING'], 'length': os.environ['CONTENT_LENGTH'], "
            "'remote': os.environ['REMOTE_ADDR'], 'port': os.environ['SERVER_PORT'], "
            "'protocol': os.environ['SERVER_PROTOCOL'], 'body': list(body)}))\n")
        (root / "%2e.txt").write_text("literal percent")
        with socket.socket() as probe:
            probe.bind(("127.0.0.1", 0))
            port = probe.getsockname()[1]
        config = root / "server.conf"
        config.write_text(f"""server {{
    listen {port};
    host 127.0.0.1;
    root {root};
    client_max_body_size 1000000;
    location / {{ allowed_methods GET; index index.html; }}
    location /cgi {{
        allowed_methods GET POST;
        root {root / 'cgi'};
        cgi_extension .py;
        cgi_path {sys.executable};
    }}
    location /upload {{
        allowed_methods POST;
        root {root / 'uploads'};
        upload_path {root / 'uploads'};
    }}
}}
""")

        def request(method, path, body=None, headers=None):
            connection = http.client.HTTPConnection("127.0.0.1", port, timeout=12)
            try:
                connection.request(method, path, body=body, headers=headers or {})
                response = connection.getresponse()
                return response.status, dict(response.getheaders()), response.read()
            finally:
                connection.close()

        with (root / "server.log").open("wb") as log:
            server = subprocess.Popen([str(executable), str(config)], cwd=ROOT,
                                      stdout=log, stderr=log)
            try:
                deadline = time.monotonic() + 5
                while True:
                    if server.poll() is not None:
                        raise AssertionError("Server exited before accepting requests")
                    try:
                        status, _, _ = request("GET", "/")
                        assert status == 200
                        break
                    except OSError:
                        if time.monotonic() >= deadline:
                            raise
                        time.sleep(0.05)
                for method, body in (("GET", None), ("POST", b"hello CGI")):
                    status, headers, page = request(method, "/cgi/hello.py?name=Arina%20%26%20team", body)
                    assert status == 200, (method, status)
                    assert b"Hello, Arina &amp; team!" in page
                    assert b"CGI relative path works" in page
                    assert int(headers["Content-Length"]) == len(page)
                    if body:
                        assert body in page
                curl = subprocess.run(["curl", "--silent", "--show-error", "--fail", "--include",
                    "--data-binary", "curl CGI", f"http://127.0.0.1:{port}/cgi/hello.py?name=curl"],
                    capture_output=True, check=True, timeout=5)
                curl_headers, curl_page = curl.stdout.split(b"\r\n\r\n", 1)
                assert curl_headers.startswith(b"HTTP/1.1 200") and b"curl CGI" in curl_page
                # Parser must decode chunked input before handing it to CGI stdin.
                status, _, page = request("POST", "/cgi/hello.py", b"5\r\nhello\r\n0\r\n\r\n",
                                          {"Transfer-Encoding": "chunked"})
                assert status == 200 and b"hello" in page
                status, _, page = request("GET", "/cgi/large.py")
                assert status == 200 and page == b"x" * 32768
                # Exceed socket buffers and exchange binary input/output at
                # the same time; neither direction may block the event loop.
                streamed = b"binary\x00" * 130000
                status, headers, page = request("POST", "/cgi/stream.py", streamed)
                assert status == 200 and page == streamed
                assert int(headers["Content-Length"]) == len(streamed)
                status, headers, page = request("GET", "/cgi/status.py")
                assert status == 201 and page == b"ok" and headers["Content-Length"] == "2"
                status, headers, page = request("GET", "/cgi/empty-error.py")
                assert status == 400 and page == b"" and headers["Content-Length"] == "0"
                assert request("GET", "/cgi/invalid.py")[0] == 500
                status, _, page = request("POST", "/cgi/meta.py?name=a%26b", b"3\r\na\x00b\r\n0\r\n\r\n",
                                          {"Transfer-Encoding": "chunked"})
                metadata = json.loads(page)
                assert status == 200 and metadata == {
                    "query": "name=a%26b", "length": "3", "remote": "127.0.0.1",
                    "port": str(port), "protocol": "HTTP/1.1", "body": [97, 0, 98]}
                assert request("GET", "/%252e.txt")[2] == b"literal percent"
                if options.review_symlinks:
                    outside = Path(outside_directory)
                    (outside / "sentinel.txt").write_text("disposable outside sentinel")
                    (outside / "sentinel.py").write_text("print('Content-Type: text/plain\\n\\noutside CGI')\n")
                    (root / "escape").symlink_to(outside, target_is_directory=True)
                    (root / "cgi" / "escape").symlink_to(outside, target_is_directory=True)
                    (root / "uploads" / "escape").symlink_to(outside, target_is_directory=True)
                    outcomes = {
                        "outside GET": request("GET", "/escape/sentinel.txt")[0],
                        "outside CGI": request("GET", "/cgi/escape/sentinel.py")[0],
                        "outside upload": request("POST", "/upload/escape/new.txt", b"test")[0],
                    }
                    # This fixture's upload route allows POST only; DELETE is
                    # reviewed from code, and normal DELETE has its own suite.
                    print("Symlink containment checks:", outcomes)
                    assert all(status == 403 for status in outcomes.values()), outcomes
                    assert not (outside / "new.txt").exists()
                    assert (outside / "sentinel.txt").read_text() == "disposable outside sentinel"
                status, _, _ = request("POST", "/upload/nested/file.txt", b"nested upload")
                assert status == 201
                assert (root / "uploads" / "nested" / "file.txt").read_bytes() == b"nested upload"
                assert request("POST", "/upload/missing/file.txt", b"x")[0] == 404
                assert request("GET", "/cgi/crash.py")[0] == 500
                with concurrent.futures.ThreadPoolExecutor(max_workers=1) as pool:
                    hanging = pool.submit(request, "GET", "/cgi/hang.py")
                    time.sleep(0.3)
                    start = time.monotonic()
                    assert request("GET", "/")[0] == 200
                    assert time.monotonic() - start < 2, "Hanging CGI blocked static requests"
                    assert hanging.result(timeout=12)[0] == 504
                assert request("GET", "/")[0] == 200
                child_list = Path(f"/proc/{server.pid}/task/{server.pid}/children")
                baseline = len(list(Path(f"/proc/{server.pid}/fd").iterdir()))
                client = socket.create_connection(("127.0.0.1", port))
                client.sendall(b"GET /cgi/hang.py HTTP/1.1\r\nHost: localhost\r\n\r\n")
                deadline = time.monotonic() + 3
                while not child_list.read_text().strip():
                    assert time.monotonic() < deadline, "CGI job never started"
                    time.sleep(0.05)
                client.close()
                while child_list.read_text().strip() or len(list(Path(f"/proc/{server.pid}/fd").iterdir())) != baseline:
                    assert time.monotonic() < deadline, "Disconnect leaked child or descriptors"
                    time.sleep(0.05)
                assert request("GET", "/")[0] == 200
                client = socket.create_connection(("127.0.0.1", port))
                client.sendall(b"GET /cgi/hang.py HTTP/1.1\r\nHost: localhost\r\n\r\n")
                deadline = time.monotonic() + 3
                while not child_list.read_text().strip():
                    assert time.monotonic() < deadline, "CGI job never started"
                    time.sleep(0.05)
                child_pids = child_list.read_text().split()
                server.send_signal(signal.SIGINT)
                assert server.wait(timeout=3) == 0
                client.close()
                assert all(not Path(f"/proc/{pid}").exists() for pid in child_pids), "SIGINT leaked a CGI child"
                print("Live CGI, metadata, nested POST, disconnect cleanup, and active-CGI SIGINT checks passed")
            finally:
                server.terminate()
                try:
                    server.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    server.kill()
                    server.wait(timeout=3)


if __name__ == "__main__":
    main()
