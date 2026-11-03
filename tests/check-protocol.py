#!/usr/bin/env python3
"""Real compiler/current host checks; no fake results or public export interface."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
ROOT=Path(__file__).resolve().parents[1]
HOST=Path(sys.argv[1]).resolve()
env=dict(os.environ)
env.pop('LMCODE_INVOCATION_FILE',None)
def frame(result,code):
 assert result.returncode==code,(code,result.returncode,result.stdout,result.stderr)
 text=result.stdout.decode()
 assert text.count('LMCODE_RETURN_BEGIN\n')==text.count('LMCODE_RETURN_END\n')==1,text
 assert text.endswith('LMCODE_RETURN_END\n'),text
 value=json.loads(text.split('LMCODE_RETURN_BEGIN\n')[1].splitlines()[0])
 assert value['ok']==(code==0),value
 if code:assert 'result' not in value,value
 return value
with tempfile.TemporaryDirectory(prefix='lmplot protocol 水 ') as directory:
 work=Path(directory).resolve()
 def compile(name,body):
  source=work/(name+'.lms');output=work/(name+'.dslb')
  source.write_text('program = _lmcode_ schema='+json.dumps(str(ROOT/'schema/lmplot.dsl.json'))+';\n'+body+'\nend _lmcode_\n')
  result=subprocess.run(['node',str(ROOT/'scripts/compile-test.mjs'),str(source),str(output)],env=env,capture_output=True)
  assert result.returncode==0,(result.stdout,result.stderr)
  assert output.read_bytes()[:6]==b'DSLB\x00\x06'
  return output
 def run(files,dry=False,change=None,argv=None):
  value={'version':1,'mode':'builder' if len(files)>1 else 'single','dry':dry,'parts':[{'index':i,'file':str(f)} for i,f in enumerate(files)]}
  if change:value=change(value)
  manifest=work/'manifest.json';manifest.write_text(json.dumps(value))
  args=argv if argv is not None else [a for f in files for a in ('--lmcode',str(f))]
  return subprocess.run([str(HOST),*args],env=dict(env,LMCODE_INVOCATION_FILE=str(manifest)),capture_output=True,cwd=work)
 for args in [['--lmcode-schema'],['--lmcode-schema','--json']]:
  r=subprocess.run([str(HOST),*args],env=dict(env,LMCODE_INVOCATION_FILE='bad'),capture_output=True)
  assert r.returncode==0 and r.stdout==(ROOT/'schema/lmplot.dsl.json').read_bytes() and not r.stderr,r
 r=subprocess.run([str(HOST),'--lmcode-signatures'],env=env,capture_output=True)
 assert r.returncode==0 and b'Plot.png' in r.stdout and not r.stderr,r
 first=compile('first','$return = 11;');second=compile('second','$return = 22;')
 assert frame(run([first]),0)=={'ok':True,'result':11}
 assert frame(run([first,second]),0)=={'ok':True,'result':[11,22],'parts':2}
 assert frame(run([first,second],argv=['--lmcode',str(second),'--lmcode',str(first)]),0)=={'ok':True,'result':[11,22],'parts':2}
 write=compile('write','plot.png("must-not-exist.png", 64, 64); plot.cmd("unset key; unset tics; set margins 0,0,0,0; plot sin(x)"); plot.close(); $return = plot.num("2+3");')
 for flag in ['--first','--last','--dry-run','--cli-schema','--dsl','--dsl-schema','--dsl-signatures']:
  for args in [[flag,str(write)],['--lmcode',str(first),flag,str(write)],['gnuplot',flag,str(write)]]:
   assert frame(run([write],argv=args),2)['phase']=='cli'
   assert not (work/'must-not-exist.png').exists()
 print('PASS discovery and retired selectors refused in all host/native routes')
 for mode in ['single','builder']:
  files=[write] if mode=='single' else [first,write]
  for corrupt in [lambda x:{k:v for k,v in x.items() if k!='dry'},lambda x:dict(x,dry='yes'),lambda x:dict(x,version=2),lambda x:dict(x,extra=True)]:
   assert frame(run(files,change=corrupt),2)['phase']=='cli'
   assert not (work/'must-not-exist.png').exists()
 print('PASS strict manifest/dry validation before effects, single/builder identity')
 assert frame(run([write],dry=True),0)=={'ok':True,'result':0,'dry':True}
 assert not (work/'must-not-exist.png').exists()
 # Typed string and float dummies; no interpreter initialized for invalid gnuplot text.
 dummy=compile('dummy','plot.cmd("this would fail live"); $return = plot.str("unknown");')
 assert frame(run([dummy],dry=True),0)=={'ok':True,'result':'','dry':True}
 assert frame(run([write]),0)['result']==5
 assert (work/'must-not-exist.png').read_bytes()[:8]==b'\x89PNG\r\n\x1a\n'
 (work/'must-not-exist.png').unlink()
 failure=compile('failure','plot.cmd("this is not valid gnuplot");')
 actual=frame(run([first,failure,write]),3)
 assert actual['part']==1 and actual['phase']=='run',actual
 assert not (work/'must-not-exist.png').exists()
 missing=work/'missing.dslb';assert frame(run([missing]),4)['phase']=='app'
 junk=work/'bad.dslb';junk.write_bytes(b'invalid');assert frame(run([junk]),3)['phase']=='load'
 print('PASS dry dummies, live render, first failure/stop, framed exits 0/2/3/4')
 # More than the old arbitrary 30,000 pixel width, with a small actual allocation.
 large=compile('wide','plot.png("wide.png", 30001, 64); plot.cmd("set margins 0,0,0,0; unset border; unset tics; unset key; plot x"); plot.close();')
 assert frame(run([large]),0)['result'] is None
 assert int.from_bytes((work/'wide.png').read_bytes()[16:20],'big')==30001
 print('PASS renderer width above prior implicit cutoff')
