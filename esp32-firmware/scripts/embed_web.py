from pathlib import Path

try:
    Import("env")  # type: ignore[name-defined]  # noqa: F821 — PlatformIO
    PROJECT_DIR = Path(env["PROJECT_DIR"])  # type: ignore[name-defined]  # noqa: F821
except Exception:
    PROJECT_DIR = Path(__file__).resolve().parent.parent

DATA_DIR = PROJECT_DIR / "data"
OUT_CPP = PROJECT_DIR / "src" / "web" / "web_assets.cpp"
OUT_H = PROJECT_DIR / "src" / "web" / "web_assets.h"

ASSETS = [
    ("kIndexHtml", "/index.html", "index.html", "text/html"),
    ("kAppJs", "/app.js", "app.js", "application/javascript"),
    ("kStyleCss", "/style.css", "style.css", "text/css"),
]


def raw_delimiter(content: str) -> str:
    index = 0
    while True:
        tag = f"webembed{index}"
        if f"){tag}" not in content:
            return tag
        index += 1


def build_assets() -> None:
    cpp_parts = [
        '#include "web_assets.h"\n',
        "#include <pgmspace.h>\n",
        "#include <cstring>\n\n",
        "namespace {\n\n",
        "struct AssetEntry {\n",
        "    const char* path;\n",
        "    const char* contentType;\n",
        "    const char* data;\n",
        "};\n\n",
    ]

    table_rows = []
    for symbol, uri, filename, content_type in ASSETS:
        source = (DATA_DIR / filename).read_text(encoding="utf-8")
        tag = raw_delimiter(source)
        cpp_parts.append(f'const char {symbol}[] PROGMEM = R"{tag}({source}){tag}";\n\n')
        table_rows.append(f'    {{"{uri}", "{content_type}", {symbol}}},')

    cpp_parts.append("constexpr AssetEntry kAssets[] = {\n")
    cpp_parts.extend(f"{row}\n" for row in table_rows)
    cpp_parts.append("};\n\n")
    cpp_parts.append("}  // namespace\n\n")
    cpp_parts.append("bool webAssetFind(const char* path, WebAsset& out) {\n")
    cpp_parts.append("    for (const auto& asset : kAssets) {\n")
    cpp_parts.append("        if (strcmp(path, asset.path) == 0) {\n")
    cpp_parts.append("            out.contentType = asset.contentType;\n")
    cpp_parts.append("            out.data = asset.data;\n")
    cpp_parts.append("            out.length = strlen_P(asset.data);\n")
    cpp_parts.append("            return true;\n")
    cpp_parts.append("        }\n")
    cpp_parts.append("    }\n")
    cpp_parts.append("    return false;\n")
    cpp_parts.append("}\n")

    OUT_H.write_text(
        """#pragma once

#include <Arduino.h>

struct WebAsset {
    const char* contentType = nullptr;
    const char* data = nullptr;
    size_t length = 0;
};

bool webAssetFind(const char* path, WebAsset& out);
""",
        encoding="utf-8",
    )
    OUT_CPP.write_text("".join(cpp_parts), encoding="utf-8")
    print(f"[embed_web] {len(ASSETS)} fichiers embarques dans le firmware")


build_assets()
