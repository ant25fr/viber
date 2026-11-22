#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Print the editor's HD icon table: for each icon id of the firmware's enum (gen_aa_icons.py: the legacy names of
assets/icons.json, then EXTRA) the Fukiai codepoint it is drawn from, 0 for none. web/editor.html keeps it as
SCR_ICONS (the Screen tab's HD rendering draws the icon again from the font at the display's resolution).
  tools/gen_hd_icons.py > table; paste it between the SCR_ICONS markers"""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gen_aa_icons as g  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
cps = g.glyph_table(ROOT / "web" / "fukiai.ttf")
legacy = json.loads((ROOT / "assets" / "icons.json").read_text())["names"]
names = list(legacy) + list(g.EXTRA)
glyph_of = {n: g.resolve(x, cps) for n, x in {**g.LEGACY, **g.EXTRA}.items()}
mended = set(g.FIT_TO)                         # (mended in the firmware's copy: left as the unit's pixels)
out = [0 if (n not in glyph_of or glyph_of[n] not in cps or glyph_of[n] in mended or n in g.NOT_DRAWN) else cps[glyph_of[n]]
       for n in names]
print("[" + ",".join(str(v) for v in out) + "]")
