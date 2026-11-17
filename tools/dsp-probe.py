#!/usr/bin/env python3
"""Compile a diagnostic renderer against an existing build, optionally its baseline archive."""
from pathlib import Path
import os, shlex, subprocess, sys
root=Path(__file__).resolve().parent.parent
os.chdir(root)
flags=(root/'build/CMakeFiles/OrbitSmoke.dir/flags.make').read_text().splitlines()
args=[]
for key in ('CXX_DEFINES','CXX_INCLUDES','CXX_FLAGS'):
    args += shlex.split(next(line.split(' = ',1)[1] for line in flags if line.startswith(key+' = ')))
env=dict(os.environ,TMPDIR=str(root/'.local-cache'))
subprocess.run(['/usr/bin/c++',*args,'-c','Tests/DspProbe.cpp','-o','build/DspProbe.o'],env=env,check=True)
link=shlex.split((root/'build/CMakeFiles/OrbitSmoke.dir/link.txt').read_text())
link[link.index('CMakeFiles/OrbitSmoke.dir/Tests/Smoke.cpp.o')]='DspProbe.o'
link[link.index('-o')+1]='DspProbe'
if len(sys.argv)>2 and sys.argv[2]=='baseline':
    link[link.index('Vantage_artefacts/Release/libVantage_SharedCode.a')]='dsp-baseline/libVantage_SharedCode.a'
subprocess.run(link,cwd=root/'build',env=env,check=True)
subprocess.run([str(root/'build/DspProbe'),str(root/sys.argv[1])],cwd=root,env=env,check=True)
