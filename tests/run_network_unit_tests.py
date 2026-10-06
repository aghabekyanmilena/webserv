"""Run Linux C++98 regression checks with test-only allocation failure wrappers."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="webserv-network-unit-") as directory:
    executable = Path(directory) / "network-unit-tests"
    objects = sorted((ROOT / "out/src").glob("*.o"))
    objects = [obj for obj in objects if obj.name != "Cgi.o"]
    assert objects, "Build webserv first with make"
    subprocess.run(["c++", "-Wall", "-Wextra", "-Werror", "-std=c++98", "-Iinclude",
                    "tests/network_unit_tests.cpp", *map(str, objects),
                    "-Wl,--wrap=_Znwm", "-Wl,--wrap=_Znam", "-o", str(executable)],
                   cwd=ROOT, check=True)
    subprocess.run([str(executable)], cwd=ROOT, check=True, timeout=5)
