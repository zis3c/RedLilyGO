#!/usr/bin/env python3
"""
RedLilyGO UI Build System
Assembles modular HTML, CSS, and JS from ui/src/ into ui/content/
"""
import os
import re
import tempfile

REPO_ROOT = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.join(REPO_ROOT, "ui", "src")
CONTENT_DIR = os.path.join(REPO_ROOT, "ui", "content")

# Keep CSS bundles in cascade order so feature rules can build on shared styles.
CSS_SOURCE_FILES = [
    os.path.join(SRC_DIR, "core", "base.css"),
    os.path.join(SRC_DIR, "core", "layout.css"),
    os.path.join(SRC_DIR, "core", "components.css"),
    os.path.join(SRC_DIR, "features", "dashboard", "dashboard.css"),
    os.path.join(SRC_DIR, "features", "tools", "tools.css"),
    os.path.join(SRC_DIR, "features", "files", "files.css"),
    os.path.join(SRC_DIR, "features", "logs", "logs.css"),
]

# Load shared behavior before feature modules that depend on it.
JS_SOURCE_FILES = [
    os.path.join(SRC_DIR, "core", "navigation.js"),
    os.path.join(SRC_DIR, "core", "shortcuts.js"),
    os.path.join(SRC_DIR, "core", "dropdowns.js"),
    os.path.join(SRC_DIR, "core", "modal.js"),
    os.path.join(SRC_DIR, "core", "toast-actions.js"),
    os.path.join(SRC_DIR, "features", "dashboard", "telemetry.js"),
    os.path.join(SRC_DIR, "features", "dashboard", "capabilities.js"),
    os.path.join(SRC_DIR, "features", "dashboard", "dashboard.js"),

    os.path.join(SRC_DIR, "features", "tools", "autocomplete.js"),
    os.path.join(SRC_DIR, "features", "tools", "marauder.js"),
    os.path.join(SRC_DIR, "features", "tools", "payloads.js"),
    os.path.join(SRC_DIR, "features", "tools", "agent-status.js"),
    os.path.join(SRC_DIR, "features", "files", "editor-upload.js"),
    os.path.join(SRC_DIR, "features", "files", "drag-drop.js"),
    os.path.join(SRC_DIR, "features", "logs", "filter.js"),
    os.path.join(SRC_DIR, "features", "logs", "saved.js"),
    os.path.join(SRC_DIR, "features", "logs", "runtime-controls.js"),
    os.path.join(SRC_DIR, "features", "settings", "toggles.js"),
    os.path.join(SRC_DIR, "features", "settings", "channel-hop.js"),
    os.path.join(SRC_DIR, "features", "settings", "setup.js"),
    os.path.join(SRC_DIR, "features", "settings", "settings-actions.js"),
    os.path.join(SRC_DIR, "features", "about", "report.js"),
]

def resolve_includes(content, base_dir, depth=0):
    """Recursively resolves <!-- @include: rel/path.html --> tags."""
    if depth > 10:
        raise RecursionError("Max include depth exceeded in template")

    pattern = re.compile(r'<!--\s*@include:\s*([^\s]+)\s*-->')
    
    def replacer(match):
        rel_path = match.group(1).strip()
        full_path = os.path.normpath(os.path.join(base_dir, rel_path))
        if not os.path.exists(full_path):
            raise FileNotFoundError(f"Template include not found: {full_path} (from {rel_path})")
        with open(full_path, "r", encoding="utf-8") as inc_file:
            inc_content = inc_file.read()
        return resolve_includes(inc_content, os.path.dirname(full_path), depth + 1)

    return pattern.sub(replacer, content)

def write_output(dest_path, content):
    """Publish complete bundles atomically; leave unchanged outputs untouched."""
    content = content.replace("\r\n", "\n")
    if os.path.exists(dest_path):
        with open(dest_path, "r", encoding="utf-8") as current:
            if current.read() == content:
                return
    os.makedirs(os.path.dirname(dest_path), exist_ok=True)
    temp_path = None
    try:
        with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", newline="\n",
                                         dir=os.path.dirname(dest_path), delete=False) as output:
            temp_path = output.name
            output.write(content)
        os.replace(temp_path, dest_path)
    finally:
        if temp_path and os.path.exists(temp_path):
            os.remove(temp_path)

def build_html():
    template_path = os.path.join(SRC_DIR, "template.html")
    with open(template_path, "r", encoding="utf-8") as f:
        template = f.read()
    output_html = resolve_includes(template, SRC_DIR)
    dest_path = os.path.join(CONTENT_DIR, "index.html")
    write_output(dest_path, output_html)
    return True

def build_css():
    parts = []
    for css_file in CSS_SOURCE_FILES:
        with open(css_file, "r", encoding="utf-8") as f:
            parts.append(f"/* === {os.path.relpath(css_file, SRC_DIR)} === */\n" + f.read().strip())
    if not parts:
        return False
    bundle_css = "\n\n".join(parts) + "\n"
    dest_path = os.path.join(CONTENT_DIR, "assets", "css", "redlilygo.css")
    write_output(dest_path, bundle_css)
    return True

def build_js():
    parts = []
    for js_file in JS_SOURCE_FILES:
        with open(js_file, "r", encoding="utf-8") as f:
            parts.append(f"// === {os.path.relpath(js_file, SRC_DIR)} ===\n" + f.read().strip())
    if not parts:
        return False
    bundle_js = "\n\n".join(parts) + "\n"
    dest_path = os.path.join(CONTENT_DIR, "assets", "js", "ui.js")
    write_output(dest_path, bundle_js)
    return True

def build():
    h = build_html()
    c = build_css()
    j = build_js()
    return h and c and j

if __name__ == "__main__":
    print("[RedLilyGO UI Build] Assembling modular UI assets...")
    h = build_html()
    c = build_css()
    j = build_js()
    print(f"  HTML: {'[OK]' if h else '[SKIPPED]'}")
    print(f"  CSS:  {'[OK]' if c else '[SKIPPED]'}")
    print(f"  JS:   {'[OK]' if j else '[SKIPPED]'}")
    print("[RedLilyGO UI Build] Complete.")
