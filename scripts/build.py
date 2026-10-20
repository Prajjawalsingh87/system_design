#!/usr/bin/env python3
"""Compile all examples without flattening paths or executing exercise code."""
import os
from pathlib import Path
import re
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
sources = sorted(p for p in root.rglob("*.cpp") if not set(p.relative_to(root).parts) & {".git", "build", "node_modules"})
failed = 0
for source in sources:
    target = root / "build" / source.relative_to(root).with_suffix("")
    target.parent.mkdir(parents=True, exist_ok=True)
    has_main = bool(re.search(r"\b(?:int|signed|int32_t)\s+main\s*\(", source.read_text()))
    command = [os.environ.get("CXX", "g++"), "-std=c++17", "-O2", "-Wall", str(source)]
    command += ["-o", str(target)] if has_main else ["-c", "-o", str(target) + ".o"]
    result = subprocess.run(command, check=False)
    print(("OK " if result.returncode == 0 else "FAILED ") + str(source.relative_to(root)), flush=True)
    failed += result.returncode != 0
print(f"{len(sources) - failed}/{len(sources)} files compiled")
sys.exit(bool(failed))
