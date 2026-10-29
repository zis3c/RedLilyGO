"""Check required UI inputs and stable generated headers using temporary files."""
import ast
import contextlib
import gzip
import importlib.util
import io
import os
from pathlib import Path
import tempfile
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("build_ui", ROOT / "build_ui.py")
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)
tree = ast.parse((ROOT / "platformio_assets.py").read_text())
function = next(node for node in tree.body if isinstance(node, ast.FunctionDef) and node.name == "compress_to_c_array")
namespace = {"gzip": gzip, "os": os, "staticFiles": {}, "headerFiles": []}
exec(compile(ast.Module(body=[function], type_ignores=[]), "platformio_assets.py", "exec"), namespace)


def main():
    with tempfile.TemporaryDirectory() as folder, contextlib.redirect_stdout(io.StringIO()):
        source = Path(folder) / "source.css"
        source.write_text("body { color: red; }")
        destination = Path(folder) / "generated/header.h"
        generate = namespace["compress_to_c_array"]
        generate(str(source), str(destination), "fixture", staticKey="/fixture")
        first = destination.read_bytes()
        original_time = destination.stat().st_mtime_ns
        generate(str(source), str(destination), "fixture", staticKey="/fixture")
        assert destination.read_bytes() == first
        assert destination.stat().st_mtime_ns == original_time, "unchanged headers must not be rewritten"
        assert namespace["staticFiles"]["/fixture"] == ("fixture", True)
        source.write_text("body { color: blue; }")
        generate(str(source), str(destination), "fixture")
        assert destination.read_bytes() != first

        bundle = Path(folder) / "bundle.js"
        builder.write_output(str(bundle), "complete bundle\n")
        timestamp = bundle.stat().st_mtime_ns
        builder.write_output(str(bundle), "complete bundle\n")
        assert bundle.stat().st_mtime_ns == timestamp
        with patch.object(builder.os, "replace", side_effect=OSError("test publish failure")):
            try:
                builder.write_output(str(bundle), "new bundle")
            except OSError:
                pass
            else:
                raise AssertionError("publication failures must propagate")
        assert bundle.read_text() == "complete bundle\n", "failed publish must preserve previous complete bundle"
        assert {p.name for p in Path(folder).iterdir()} == {"source.css", "generated", "bundle.js"}, "temporary file leaked"

        missing = str(Path(folder) / "missing")
        for action in (
            lambda: generate(missing, str(destination), "fixture"),
            lambda: builder.build_css(),
            lambda: builder.build_js(),
            lambda: builder.build_html(),
        ):
            with patch.object(builder, "CSS_SOURCE_FILES", [missing]), patch.object(builder, "JS_SOURCE_FILES", [missing]), patch.object(builder, "SRC_DIR", missing):
                try:
                    action()
                except FileNotFoundError:
                    pass
                else:
                    raise AssertionError("missing required source must fail the build")
    print("PASS stable asset headers, changed input regeneration and missing UI source rejection")


if __name__ == "__main__":
    main()
