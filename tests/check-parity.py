"""Same compiler inputs on original and new host: numeric/data/script and render facts."""
import hashlib,json,os,re,subprocess,sys,tempfile
from pathlib import Path
sys.dont_write_bytecode=True
sys.path.insert(0,str(Path(__file__).resolve().parent))
from capture import capture
ROOT=Path(__file__).resolve().parents[1]
HOST=Path(sys.argv[1]).resolve()
CASES={
 'num':'plot.echo(str(plot.num("sqrt(81)+3")));',
 'str':'plot.echo(plot.str('+json.dumps("'hello'")+'));',
 'data':'plot.data("d", "1 2\\n2 4\\n3 6"); plot.cmd("set terminal unknown; stats $d using 1:2 nooutput"); plot.echo(str(plot.num("STATS_slope")));',
 'script':'plot.script("a=10\\nb=32"); plot.echo(str(plot.num("a+b")));',
 'png':'plot.png("artifact.png", 96, 64); plot.cmd("unset key; unset tics; set margins 0,0,0,0; plot sin(x)"); plot.close();',
 'svg':'plot.svg("artifact.svg", 96, 64); plot.cmd("unset key; unset tics; set margins 0,0,0,0; plot sin(x)"); plot.close();',
}
def collect(host):
 with tempfile.TemporaryDirectory(prefix='plot parity ') as directory:
  work=Path(directory).resolve();results={}
  for name,body in CASES.items():
   output=capture(HOST,work,name,body)
   r=subprocess.run([str(host),'--lmcode',str(output)],cwd=work,capture_output=True,text=True)
   assert r.returncode==0,(name,r.stdout,r.stderr)
   trace=r.stdout.split('LMCODE_RETURN_BEGIN\n')[0]
   trace='\n'.join(line for line in trace.splitlines() if not line.startswith('{"ok":'))
   item={'stdout':trace,'stderr':r.stderr}
   for ext in ['png','svg']:
    p=work/('artifact.'+ext)
    if p.exists():
     data=p.read_bytes()
     if ext=='svg':data=re.sub(rb'\(lmplot [0-9.]+\)',b'(lmplot VERSION)',data.replace(b'\r\n',b'\n'))
     item[ext]=hashlib.sha256(data).hexdigest();p.unlink()
   results[name]=item
  return results
results=collect(HOST)
baseline=ROOT/'tests/parity.json'
if '--record-before' in sys.argv:
 baseline.write_text(json.dumps({'source':'70a609f143777e8e5f2e49c19b56a18204e9cc27','values':results},indent=2)+'\n')
elif '--oracle' in sys.argv:
 oracle=Path(sys.argv[sys.argv.index('--oracle')+1]).resolve()
 expected=collect(oracle)
 assert results==expected,(results,expected)
 print('PASS original/current native host results match on this OS (all six cases)')
else:
 assert results==json.loads(baseline.read_text())['values'],(results,json.loads(baseline.read_text()))
print('PASS six original/current operation records including exact PNG/normalized SVG bytes')
