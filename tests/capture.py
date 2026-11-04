"""Test fixtures from the real compiler: lmscript --dry-run hands the private program file to a recording command."""
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SCHEMA = ROOT / 'schema/lmplot.dsl.json'


def capture(host, work, name, body):
    """Return the program file for BODY. A dry invocation: the real host loads it and runs no callback."""
    source, output = work / (name + '.lms'), work / (name + '.bin')
    lmscript = os.environ['LMSCRIPT_BIN']
    command = [sys.executable, str(ROOT / 'tests/record-program.py'), str(host), str(output), '--lmcode', '$DSLFILE']
    source.write_text('program = _lmcode_ schema=' + json.dumps(str(SCHEMA).replace('\\', '/')) + ';\n' + body + '\nend _lmcode_\n'
                      'r = run_lmcode(program, {command: ' + json.dumps(command, ensure_ascii=False) + '});\nrequire(r.ok, json_stringify(r));\n',
                      encoding='utf-8')
    env = {k: v for k, v in os.environ.items() if k != 'LMCODE_INVOCATION_FILE'}
    # npm's .bin entry is a sh script on Windows, which Python cannot start directly, and a bare `bash` there can be the
    # WSL launcher. Use the .cmd shim npm writes beside it.
    runner = ['cmd.exe', '/c', lmscript + '.cmd'] if os.name == 'nt' and Path(lmscript + '.cmd').exists() else [lmscript]
    result = subprocess.run([*runner, '--dry-run', str(source)], env=env, capture_output=True, cwd=work)
    assert result.returncode == 0, (name, result.stdout, result.stderr)
    assert output.read_bytes()[:6] == b'DSLB\x00\x06', name
    return output
