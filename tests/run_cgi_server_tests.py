"""CGI integration checks. Run on POSIX after the networking hook is connected."""
import concurrent.futures
import http.client
import os
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]


def main():
    if os.name != "posix":
        raise SystemExit("A POSIX server executable is required")
    executable = Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / "webserv").resolve()
    if not executable.is_file():
        raise SystemExit("Build webserv first with make")
    with tempfile.TemporaryDirectory(prefix="webserv-cgi-server-") as temporary:
        root = Path(temporary)
        shutil.copytree(ROOT / "www" / "cgi-bin", root / "cgi")
        (root / "uploads" / "nested").mkdir(parents=True)
        (root / "index.html").write_text("server alive")
        (root / "cgi" / "hang.py").write_text("import time\ntime.sleep(60)\n")
        (root / "cgi" / "crash.py").write_text("raise RuntimeError('CGI test crash')\n")
        (root / "cgi" / "large.py").write_text(
            "import sys\nsys.stdout.write('Content-Type: text/plain\\n\\n' + 'x' * 32768)\n")
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
                # Parser must decode chunked input before handing it to CGI stdin.
                status, _, page = request("POST", "/cgi/hello.py", b"5\r\nhello\r\n0\r\n\r\n",
                                          {"Transfer-Encoding": "chunked"})
                assert status == 200 and b"hello" in page
                status, _, page = request("GET", "/cgi/large.py")
                assert status == 200 and page == b"x" * 32768
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
                print("Live CGI and nested POST checks passed")
            finally:
                server.terminate()
                try:
                    server.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    server.kill()
                    server.wait(timeout=3)


if __name__ == "__main__":
    main()
