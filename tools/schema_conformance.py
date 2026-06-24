#!/usr/bin/env python3
# Copyright (c) 2026 Cisco Systems, Inc.
# SPDX-License-Identifier: GPL-2.0-only

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    schema_root = root / "schemas" / "razorback"
    tool = schema_root / "tools" / "conformance.py"
    subprocess.run([sys.executable, str(tool), "--root", str(schema_root)], check=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
