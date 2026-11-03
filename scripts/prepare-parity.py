#!/usr/bin/env python3
"""Unmodified historical host/SDK for a same-platform operation oracle; never shipped."""
from pathlib import Path
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]
COMMIT='70a609f143777e8e5f2e49c19b56a18204e9cc27'
if subprocess.run(['git','cat-file','-e',COMMIT+'^{commit}'],cwd=ROOT,capture_output=True).returncode:
 subprocess.run(['git','fetch','--depth','1','origin',COMMIT],cwd=ROOT,check=True)
output=Path(sys.argv[1]).resolve()
files=subprocess.check_output(['git','ls-tree','-r','--name-only',COMMIT,'--','cpp/main.cpp','cpp/dslvm.hpp','cpp/stdlib'],cwd=ROOT,text=True).splitlines()
assert 'cpp/main.cpp' in files and 'cpp/dslvm.hpp' in files
for file in files:
 target=output/file;target.parent.mkdir(parents=True,exist_ok=True)
 data=subprocess.check_output(['git','show',COMMIT+':'+file],cwd=ROOT)
 if not target.exists() or target.read_bytes()!=data:target.write_bytes(data)
print('PASS unmodified pre-adoption host/SDK oracle extracted at '+COMMIT)
