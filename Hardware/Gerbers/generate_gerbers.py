#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
TP-CAN-2I Gerber generator  (DEPRECATED ENTRYPOINT - now defers to the real one)
================================================================================
The original version of this script emitted *placeholder* copper layers (valid
RS-274X headers with no pads/traces/pours).  That is no longer acceptable: the
real, copper-complete generator lives in `generate_pcb.py` and produces actual
land patterns, 4-layer pours with the galvanic-isolation split, vias, routing,
solder mask, paste and drill.

This file is kept only so the historical command keeps working: it simply runs
`generate_pcb.py`.  Run either:

    python generate_pcb.py        # preferred
    python generate_gerbers.py    # same result, via this shim

(c) 2026 Jack Leighton - Tester Present Specialist Automotive Solutions.
"""
import runpy
from pathlib import Path

if __name__ == "__main__":
    target = Path(__file__).with_name("generate_pcb.py")
    print("[generate_gerbers.py] deprecated -> running %s" % target.name)
    runpy.run_path(str(target), run_name="__main__")
