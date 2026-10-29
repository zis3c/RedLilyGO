"""Measure host-regression line coverage with GCC gcov.

Compiles each host regression group with --coverage, runs it, and reports the
combined line coverage of the extracted production code. This measures the
firmware logic exercised by tests/firmware_regressions.py, not the whole tree or
any board runtime.

Usage:
    python tests/coverage.py            # print a coverage summary
    python tests/coverage.py --min 40  # exit non-zero below the floor
"""
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

import firmware_regressions as reg


def find_compiler():
    compiler = os.environ.get("CXX") or shutil.which("g++")
    if not compiler and Path("C:/msys64/mingw64/bin/g++.exe").is_file():
        compiler = "C:/msys64/mingw64/bin/g++.exe"
    if not compiler:
        raise SystemExit("g++ required; install it or set CXX to its path")
    return compiler


def groups():
    return [
        ("logging", reg.logging_check()),
        ("web_security", reg.web_security_check()),
        ("web_mutation_gate", reg.web_mutation_gate_check()),
        ("websocket", reg.websocket_check()),
        ("upload", reg.upload_check()),
        ("cdc", reg.cdc_check()),
        ("msc", reg.msc_check()),
        ("ducky", reg.ducky_check()),
        ("agent", reg.agent_check()),
        ("hid", reg.hid_check()),
        ("display", reg.display_check()),
        ("pcap", reg.pcap_check()),
    ]


def gcov_lines(gcov_text):
    """Return (executed_lines, executable_lines) from a .gcov file body."""
    executed = total = 0
    for entry in gcov_text.splitlines():
        if ":" not in entry:
            continue
        raw = entry.split(":", 1)[0].strip().rstrip("*")
        if raw in ("-", "#", "###", "====="):
            continue
        if entry.split(":", 1)[0].strip() == "-":
            continue
        if raw == "#####":
            total += 1
        elif raw.isdigit():
            total += 1
            if int(raw) > 0:
                executed += 1
    return executed, total


def collect(compiler, directory):
    env = dict(os.environ)
    env["PATH"] = str(Path(compiler).resolve().parent) + os.pathsep + env.get("PATH", "")
    covered = {}
    for gcda in Path(directory).glob("*.gcda"):
        result = subprocess.run(
            ["gcov", "-b", "-c", "-o", str(gcda.parent), str(gcda)],
            capture_output=True, text=True, encoding="utf-8", errors="replace",
            cwd=directory, env=env, timeout=120,
        )
        # Fall back to the summary if .gcov files are not produced.
        for match in re.finditer(r"File '([^']+)'\nLines executed:([\d.]+)% of (\d+)", result.stdout):
            pass
    for gcov_file in Path(directory).glob("*.gcov"):
        source = gcov_file.name[:-5]
        if source in ("stdout", "stderr"):
            continue
        executed, total = gcov_lines(gcov_file.read_text(errors="replace"))
        if total:
            prev_e, prev_t = covered.get(source, (0, 0))
            covered[source] = (prev_e + executed, prev_t + total)
    return covered


def main():
    minimum = None
    if "--min" in sys.argv:
        minimum = float(sys.argv[sys.argv.index("--min") + 1])
    compiler = find_compiler()
    env = dict(os.environ)
    env["PATH"] = str(Path(compiler).resolve().parent) + os.pathsep + env.get("PATH", "")

    totals = [0, 0]
    with tempfile.TemporaryDirectory(prefix="redlilygo-coverage-") as directory:
        for name, code in groups():
            cpp = Path(directory) / (name + ".cpp")
            binary = Path(directory) / (name + ".exe")
            cpp.write_text(code, encoding="utf-8")
            subprocess.run([compiler, "-std=c++17", "-pthread", "--coverage",
                            str(cpp), "-o", str(binary)],
                           check=True, env=env, timeout=120)
            subprocess.run([str(binary)], check=True, env=env, timeout=30)
            covered = collect(compiler, directory)
            lines = sum(e for e, _ in covered.values())
            total = sum(t for _, t in covered.values())
            pct = (lines * 100.0 / total) if total else 0.0
            print(f"[{name}] {pct:5.1f}% ({lines}/{total} lines)")
            totals[0] += lines
            totals[1] += total

    overall = (totals[0] * 100.0 / totals[1]) if totals[1] else 0.0
    print(f"TOTAL_COVERAGE={overall:.1f}% ({totals[0]}/{totals[1]} lines)")
    if minimum is not None and overall < minimum:
        raise SystemExit(f"coverage {overall:.1f}% below floor {minimum:.1f}%")


if __name__ == "__main__":
    main()
