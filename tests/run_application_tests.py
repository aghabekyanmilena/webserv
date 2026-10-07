"""Run demo checks locally and C++ application/CGI checks on a POSIX toolchain."""
import ast
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
script = ROOT / "www" / "cgi-bin" / "hello.py"
ast.parse(script.read_text(encoding="utf-8"))
for method, body in (("GET", b""), ("POST", b"hello CGI")):
    env = os.environ.copy()
    env.update(REQUEST_METHOD=method, QUERY_STRING="name=Arina%20%26%20team",
               CONTENT_LENGTH=str(len(body)))
    result = subprocess.run([sys.executable, str(script)], cwd=script.parent,
                            env=env, input=body, capture_output=True, check=True, timeout=5)
    headers, page = result.stdout.split(b"\r\n\r\n", 1)
    assert b"Content-Type: text/html" in headers
    assert b"Hello, Arina &amp; team!" in page
    assert b"CGI relative path works" in page
    if body:
        assert body in page
print("CGI demonstration GET/POST checks passed")

compiler = shutil.which("c++") or shutil.which("g++") or shutil.which("clang++")
if os.name != "posix" or compiler is None:
    print("C++ handler and CGI process tests SKIPPED: a POSIX C++98 toolchain is required")
    sys.exit(0)

with tempfile.TemporaryDirectory(prefix="webserv-application-") as temporary:
    temp = Path(temporary)
    fixtures = temp / "www"
    shutil.copytree(script.parent, fixtures / "cgi-bin")
    executable = temp / "application-tests"
    command = [compiler, "-Wall", "-Wextra", "-Werror", "-std=c++98",
               "-I", str(ROOT / "include"), str(ROOT / "tests" / "application_tests.cpp"),
               *[str(ROOT / "src" / (name + ".cpp")) for name in ("RequestHandler", "RequestResources", "RequestRouting", "RequestUtils", "RootedPath", "RequestCgi")],
               str(ROOT / "src" / "CgiProcess.cpp"), str(ROOT / "src" / "CgiRequest.cpp"),
               str(ROOT / "src" / "CgiContext.cpp"), str(ROOT / "src" / "MultipartUpload.cpp"),
               str(ROOT / "src" / "Location.cpp"),
               str(ROOT / "src" / "ServerConfig.cpp"), "-o", str(executable)]
    subprocess.run(command, check=True)
    subprocess.run([str(executable), str(fixtures), sys.executable], check=True, timeout=20)
