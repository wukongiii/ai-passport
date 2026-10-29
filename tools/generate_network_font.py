#!/usr/bin/env python3
"""Generate the 14 px network-name font from the existing pinned Noto CJK source."""
import argparse
import hashlib
from pathlib import Path
import subprocess
import re
import sys
sys.dont_write_bytecode = True
from generate_score_font import SOURCE_SHA256
p=argparse.ArgumentParser()
p.add_argument('--font',type=Path,required=True)
p.add_argument('--converter',type=Path,required=True)
a=p.parse_args()
if hashlib.sha256(a.font.read_bytes()).hexdigest()!=SOURCE_SHA256:p.error('source font SHA-256 mismatch')
destination=Path(__file__).resolve().parents[1]/'assets/fonts/network_font_14.c'
subprocess.run([str(a.converter.resolve()),'--font',str(a.font.resolve()),'--range','0x20-0x7e,0x3000-0x303f,0x4e00-0x9fff,0xff01-0xff60','--size','14','--bpp','2','--format','lvgl','--no-compress','--no-kerning','--lv-font-name','network_font_14','--lv-include','lvgl.h','-o',str(destination)],check=True)
s=re.sub(r'Opts:.*','Opts: tools/generate_network_font.py (lv_font_conv 1.5.3)',destination.read_text())
destination.write_text(s.rstrip()+'\n')
print('Generated network-name CJK font',destination.stat().st_size,'source bytes')
