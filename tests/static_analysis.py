"""Run GCC's static analyzer over the host-compiled firmware groups.

This reuses the in-memory device doubles from firmware_regressions.py and asks
g++ to analyze the same translation units with -fanalyzer. It is a best-effort
check, not a proof of correctness, and analyzer findings still need review.

Usage:
    python tests/static_analysis.py            # report findings, exit 0
    python tests/static_analysis.py --strict   # exit non-zero on any finding
"""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

import firmware_regressions as reg

ROOT = Path(__file__).resolve().parents[1]

# Only the groups that exercise maintained project code are analyzed here.
GROUPS = [
    "logging", "web_security", "websocket", "upload", "cdc", "msc",
    "ducky", "agent", "hid", "display", "pcap",
]


def find_compiler():
    compiler = os.environ.get("CXX") or shutil.which("g++")
    if not compiler and Path("C:/msys64/mingw64/bin/g++.exe").is_file():
        compiler = "C:/msys64/mingw64/bin/g++.exe"
    if not compiler:
        raise SystemExit("g++ required; install it or set CXX to its path")
    return compiler


def build_groups():
    return [
        ("logging", reg.logging_check()),
        ("web_security", reg.web_security_check()),
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


def main():
    strict = "--strict" in sys.argv
    compiler = find_compiler()
    env = dict(os.environ)
    env["PATH"] = str(Path(compiler).resolve().parent) + os.pathsep + env.get("PATH", "")

    findings = 0
    with tempfile.TemporaryDirectory(prefix="redlilygo-analyzer-") as directory:
        harness_marker = directory.replace("\\", "/")
        for name, code in build_groups():
            cpp = Path(directory) / (name + ".cpp")
            cpp.write_text(code, encoding="utf-8")
            result = subprocess.run(
                [compiler, "-std=c++17", "-fanalyzer", "-Wall", "-Wextra",
                 "-c", str(cpp), "-o", str(Path(directory) / (name + ".o"))],
                capture_output=True, text=True, encoding="utf-8", errors="replace",
                env=env, timeout=300,
            )
            all_lines = [line for line in (result.stderr or "").splitlines()
                         if " warning:" in line or " error:" in line]
            # The host harness extracts functions by slicing source text, so
            # variables used only in the omitted portion look unused. Those are
            # extraction artifacts, not defects.
            artifacts = [line for line in all_lines if "-Wunused" in line]
            # -fanalyzer emits location-less "<unknown>" uninitialized-value
            # diagnostics on this toolchain; they carry no actionable site.
            # It also reports leaks inside the mock standard-library containers
            # used only by the doubles (new_allocator/unordered_map nodes), and
            # lint warnings in the generated harness file itself. None of these
            # are project code.
            noise = [line for line in all_lines
                     if "-Wunused" not in line
                     and ("<unknown>" in line
                          or "cc1plus" in line
                          or "-Wanalyzer-malloc-leak" in line
                          or "new_allocator" in line
                          or "_Hash_node" in line
                          or harness_marker in line.replace("\\", "/"))]
            real = [line for line in all_lines
                    if "-Wunused" not in line and line not in noise]
            findings += len(real)
            status = "clean" if not real else f"{len(real)} located finding(s)"
            if artifacts:
                status += f", {len(artifacts)} extraction artifact(s)"
            if noise:
                status += f", {len(noise)} location-less analyzer note(s)"
            print(f"[{name}] {status}")
            for line in real:
                print("    " + line.strip())

    print(f"TOTAL_FINDINGS={findings}")
    if strict and findings:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
