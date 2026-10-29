#!/usr/bin/env python3
"""Check generated subset/inventory against every fixed scoreboard string."""
import importlib.util
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("generate_score_font", ROOT / "tools/generate_score_font.py")
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)


class FontTest(unittest.TestCase):
    def test_required_glyphs_and_generated_inventory(self):
        expected = set(generator.codepoints())
        for size in (14, 16):
            font = (ROOT / f"assets/fonts/score_font_{size}.c").read_text()
            actual = {int(cp, 16) for cp in re.findall(r'/\* U\+([0-9A-F]+) ', font)}
            self.assertEqual(expected, actual)
            self.assertIn(".bitmap_format = 0", font)
            self.assertIn(".bpp = 4", font)
            self.assertNotIn("CMAP_FORMAT0_FULL", font)
        inventory = (ROOT / "assets/fonts/score_font_inventory.c").read_text()
        self.assertEqual(expected, {int(cp, 16) for cp in re.findall(r'0x([0-9a-f]+)', inventory)})
        self.assertNotIn(0x9F98, actual)  # Arbitrary names are deliberately unsupported.
        self.assertIn(".bitmap_format = 0", font)
        self.assertIn(".bpp = 4", font)
        self.assertNotIn("CMAP_FORMAT0_FULL", font)

    def test_every_chinese_ui_literal_is_in_the_shared_inventory(self):
        for path in (ROOT / "main/score").glob("*.c"):
            self.assertIsNone(re.search(r'"[^"\n]*[\u4e00-\u9fff][^"\n]*"', path.read_text()), path)


if __name__ == "__main__":
    unittest.main()
