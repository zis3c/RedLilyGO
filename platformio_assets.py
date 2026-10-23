"""Embed compressed dashboard and VNC assets as firmware headers."""

import gzip
import os
import re
import subprocess

staticFiles = {}
headerFiles = []

Import("env")

# Make project headers resolvable even when PlatformIO compiles from staged build paths.
env.Append(
    CPPPATH=[
        env.subst("$PROJECT_DIR"),
        env.subst("$PROJECT_SRC_DIR"),
    ]
)

def compress_to_c_array(srcFile, dstFile, varName, dataConvertFunc=None, staticKey=None, gzip_data=True):
    try:
        with open(srcFile, 'rb') as f:
            data = f.read()

        if dataConvertFunc:
            data = dataConvertFunc(data)

        # mtime=0 keeps the output deterministic so identical inputs produce
        # byte-identical headers instead of triggering pointless rebuilds.
        compressed_data = gzip.compress(data, mtime=0) if gzip_data else data

        c_array_string = ', '.join(str(byte) for byte in compressed_data)

        # Create the output directory before writing the generated header.
        os.makedirs(os.path.dirname(dstFile), exist_ok=True)

        header = f"#pragma once\n\n#ifndef NO_WEB\n\nconst uint8_t PROGMEM {varName}[{len(compressed_data)}] = {{ {c_array_string} }};\n\n#endif"
        try:
            with open(dstFile, 'r', encoding='utf-8') as f:
                previous = f.read()
        except FileNotFoundError:
            previous = None
        if previous != header:
            with open(dstFile, 'w', encoding='utf-8') as f:
                f.write(header)
            print(f"Compressed data written to {dstFile}")

        headerFiles.append(dstFile)
        staticFiles[staticKey or srcFile.replace("ui/", "/")] = (varName, gzip_data)
    except FileNotFoundError as e:
        print("Error: Source file not found.")
        print (e)
        raise

def read_ui_source(relative_path):
    with open(relative_path, 'rb') as source_file:
        return source_file.read()

compress_to_c_array("ui/content/assets/css/tailwind.min.css", "src/html/tailwindMinCss.h", "tailwindMinCssGz")
import build_ui
build_ui.build()


compress_to_c_array(
    "ui/content/assets/css/styles.min.css",
    "src/html/stylesMinCss.h",
    "stylesMinCssGz",
    lambda data: data + b"\n" + read_ui_source("ui/content/assets/css/redlilygo.css"),
)

# Bundle Font Awesome so icons work offline and in browsers that block CDNs.
font_awesome_assets = [
    ("ui/content/assets/vendor/fontawesome/css/all.min.css", "src/html/fontAwesomeCss.h", "fontAwesomeCssGz"),
    ("ui/content/assets/vendor/fontawesome/webfonts/fa-brands-400.woff2", "src/html/fontAwesomeBrandsWoff2.h", "fontAwesomeBrandsWoff2Gz"),
    ("ui/content/assets/vendor/fontawesome/webfonts/fa-regular-400.woff2", "src/html/fontAwesomeRegularWoff2.h", "fontAwesomeRegularWoff2Gz"),
    ("ui/content/assets/vendor/fontawesome/webfonts/fa-solid-900.woff2", "src/html/fontAwesomeSolidWoff2.h", "fontAwesomeSolidWoff2Gz"),
]
for source, header, variable in font_awesome_assets:
    if source.endswith("all.min.css"):
        with open(source, 'rb') as font_css:
            css = font_css.read().replace(b"url(../webfonts/", b"url(/assets/vendor/fontawesome/webfonts/")
            # The firmware embeds WOFF2 only. Remove unavailable TTF fallbacks.
            css = re.sub(rb',url\([^)]*\.ttf\)[^,;)]*\)', b'', css)
            # No UI class uses Font Awesome v4 compatibility glyphs.
            css = re.sub(rb'@font-face\{[^}]*fa-v4compatibility[^}]*\}', b'', css)
        temp_source = source + ".absolute"
        with open(temp_source, 'wb') as font_css:
            font_css.write(css)
        compress_to_c_array(temp_source, header, variable, staticKey=source.replace("ui/", "/"))
        os.remove(temp_source)
    else:
        compress_to_c_array(source, header, variable, gzip_data=False)

# Keep chart dependency available when the device has no internet.
for source, header, variable in [
    ("ui/content/assets/vendor/chartjs/chart.umd.min.js", "src/html/chartUmdMinJs.h", "chartUmdMinJsGz"),
]:
    compress_to_c_array(source, header, variable)

compress_to_c_array(
    "ui/content/assets/js/ui.js",
    "src/html/uiJs.h",
    "uiJsGz",
    staticKey="/content/assets/js/ui.js",
)
compress_to_c_array(
    "ui/content/index.html",
    "src/html/indexHtml.h", "indexHtmlGz",
    lambda data: data.decode('utf-8')
        .replace("<link rel=\"stylesheet\" href=\"https://fonts.googleapis.com/css?family=Nunito:200,200i,300,300i,400,400i,600,600i,700,700i,800,800i,900,900i&amp;display=swap\">", "")
        .replace('<link rel="stylesheet" href="assets/css/redlilygo.css">', "")
        .replace('<link rel="stylesheet" href="/assets/css/redlilygo.css">', "")
    .encode('utf-8')
)
compress_to_c_array(
    "ui/content/404.html",
    "src/html/404Html.h", "page404HtmlGz",
    lambda data: data.decode('utf-8')
        .replace('<link rel="stylesheet" href="/assets/css/redlilygo.css">', "")
        .encode('utf-8')
)

# Bundle the noVNC client files used by the firmware's remote desktop page.
directory_path = "ui/vnc"

usedHeaderNames = set()

def unique_header_name(filename):
    # Collapsing dots/dashes onto the same sanitized name ("ui.js" vs
    # "ui-js.js") would silently overwrite one generated header with another,
    # so disambiguate any collisions.
    base_name = filename.replace(".", "").replace("-", "_")
    candidate = base_name
    suffix = 1
    while candidate in usedHeaderNames:
        suffix += 1
        candidate = f"{base_name}_{suffix}"
    usedHeaderNames.add(candidate)
    return candidate

for root, dirs, files in os.walk(directory_path):
    for filename in files:
        if filename == "Orbitron700.ttf":
            continue
        file_path = os.path.join(root, filename).replace("\\", "/")

        file_name = unique_header_name(filename)

        compress_to_c_array(file_path, f"src/html/vnc/{file_name}.h", f"noVNC{file_name}Gz")

cpp_vector_code = "#ifndef NO_WEB\n\n#include <string>\n#include <unordered_map>\n#include <cstdint>\n#include <pgmspace.h>\n\n";
for header in headerFiles:
    cpp_vector_code += '#include "'+header.replace("src/html/", "")+'"\n'

cpp_vector_code += "\nstruct StaticHtmlFile { const uint8_t *data; size_t size; bool gzip; };\nstruct StaticHtmlFileEntry { const char *path; StaticHtmlFile file; };\nextern const StaticHtmlFileEntry staticHtmlFilesLookup[] = {\n"
lastFile = list(staticFiles.keys())[-1]
for key, (value, gzip_data) in staticFiles.items():
    path = key.replace("/content/", "/");
    cpp_vector_code += f"    {{\"{path}\",{{ {value},sizeof({value}), {'true' if gzip_data else 'false'} }} }}"
    if key != lastFile:
        cpp_vector_code += ",\n"
    else:
        cpp_vector_code += "\n"
cpp_vector_code += "};\nextern const size_t staticHtmlFilesLookupCount = sizeof(staticHtmlFilesLookup) / sizeof(staticHtmlFilesLookup[0]);\n\n#endif"

with open("src/html/htmlFiles.cpp", "w") as file:
    file.write(cpp_vector_code)

# Embed the current revision so firmware can report its build source.
try:
    commit_hash = subprocess.check_output(
        ['git', 'rev-parse', 'HEAD'],
        stderr=subprocess.DEVNULL
    ).strip().decode('utf-8')
except (subprocess.CalledProcessError, FileNotFoundError):
    commit_hash = "nogit"

header_content = f'#pragma once\nconst char GIT_COMMIT_HASH[] = "{commit_hash}";\n'

with open('src/version.h', 'w') as header_file:
    header_file.write(header_content)
