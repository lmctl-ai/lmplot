import json,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1]
s=json.loads((root/'schema/lmplot.dsl.json').read_text())
actual=set(subprocess.check_output([sys.argv[1],'--lmcode-signatures'],text=True).splitlines())
expected={f'context {n}: {t}' for n,t in s['context'].items()}
for kind,types in [('method',s['types'])]:
 for t,definition in types.items():
  for name,overloads in definition.get('methods',{}).items():
   for o in overloads:expected.add(f"method {t}.{name}({', '.join(o['params'])}) -> {o['returns']}")
assert expected<=actual,expected-actual
print('PASS authored host schema binds in registry')
