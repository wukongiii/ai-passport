#!/usr/bin/env python3
"""Generate the seven-character startup wordmark with lv_font_conv 1.5.3."""
import argparse
import hashlib
from pathlib import Path
import re
import subprocess
import sys

sys.dont_write_bytecode = True
from generate_score_font import ROOT, SOURCE_SHA256


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--font", type=Path, required=True)
    parser.add_argument("--converter", type=Path, required=True)
    args = parser.parse_args()
    if hashlib.sha256(args.font.read_bytes()).hexdigest() != SOURCE_SHA256:
        parser.error("source font SHA-256 mismatch")
    title = re.search(r'#define APP_SPLASH_TITLE "([^"]+)"',
                      (ROOT / "main/app/app_splash.h").read_text()).group(1)
    destination = ROOT / "assets/fonts/startup_font_24.c"
    subprocess.run([str(args.converter.resolve()), "--font", str(args.font.resolve()),
                    "--symbols", title, "--size", "24", "--bpp", "4", "--format", "lvgl",
                    "--no-compress", "--no-kerning", "--lv-font-name", "startup_font_24",
                    "--lv-include", "lvgl.h", "-o", str(destination)], check=True)
    text = re.sub(r"Opts:.*", "Opts: tools/generate_startup_font.py (lv_font_conv 1.5.3)",
                  destination.read_text())
    destination.write_text(text.rstrip() + "\n")


if __name__ == "__main__":
    main()
