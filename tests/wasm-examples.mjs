// node tests/wasm-examples.mjs BUILD_WASM_DIR PRECOMPILED_DIR OUT_DIR
// Runs every precompiled example on the WebAssembly host in both output modes and checks that it
// succeeds and writes the same image file names the native host wrote (PNG mode: identical bytes).
import fs from 'node:fs';
import path from 'node:path';
import {createRequire} from 'node:module';


const [buildDir, preDir, outDir] = process.argv.slice(2);
const require = createRequire(import.meta.url);
const {runProgram} = require('../web/lmplot-runner.js');
const createLmplot = require(path.resolve(buildDir, 'lmplot.js'));
const wasmModule = await WebAssembly.compile(fs.readFileSync(path.join(buildDir, 'lmplot.wasm')));
const root = path.resolve(path.dirname(new URL(import.meta.url).pathname), '..');
const dataDir = path.join(root, 'examples/data');
const files = fs.readdirSync(dataDir).map(name => ({path: 'examples/data/' + name, bytes: fs.readFileSync(path.join(dataDir, name))}));
let failures = 0, identical = 0, differing = 0;
const diffs = [];
const names = fs.readdirSync(preDir).filter(f => f.endsWith('.dslb')).map(f => f.slice(0, -5)).sort();
for (const name of names) {
  const program = fs.readFileSync(path.join(preDir, name + '.dslb'));
  const refDir = path.join(preDir, 'ref', name);
  const refs = fs.existsSync(refDir) ? fs.readdirSync(refDir) : [];
  for (const output of ['svg', 'png']) {
    const t0 = Date.now();
    let note = '';
    const r = await runProgram(createLmplot, wasmModule, {program, files, output});
    const ms = Date.now() - t0;
    const dir = path.join(outDir, output, name);
    fs.mkdirSync(dir, {recursive: true});
    for (const img of r.images) fs.writeFileSync(path.join(dir, path.basename(img.path)), img.bytes);
    const got = r.images.map(i => path.basename(i.path));
    // Images the lmscript wrapper copies after run_lmcode (e.g. `cp images/x-2.png images/x.png`) are native-only.
    const expected = refs.filter(f => !(refs.length > 1 && f === name + '.png' && fs.existsSync(path.join(refDir, name + '-1.png'))))
      .map(f => output === 'svg' ? f.replace(/\.png$/, '.svg') : f);
    const missing = expected.filter(f => !got.includes(f));
    const extra = got.filter(f => !expected.includes(f));
    let ok = r.code === 0 && r.frame && r.frame.ok === true && r.images.length > 0 && !missing.length && !extra.length;
    if (output === 'svg' && !r.images.every(i => i.mime === 'image/svg+xml' && Buffer.from(i.bytes).subarray(0, 200).toString().includes('<svg'))) { ok = false; note += ' (not svg)'; }
    if (output === 'png') {
      for (const img of r.images) {
        const ref = path.join(refDir, path.basename(img.path));
        if (!fs.existsSync(ref)) continue;
        if (Buffer.compare(Buffer.from(img.bytes), fs.readFileSync(ref)) === 0) identical++; else { differing++; diffs.push(name + '/' + path.basename(img.path)); }
      }
    }
    if (missing.length || extra.length) note += ` (missing: ${missing.join(' ')}; extra: ${extra.join(' ')})`;
    if (!ok) { failures++; note += ` code=${r.code} frame=${JSON.stringify(r.frame)} stderr=${r.stderr.trim().slice(0, 400)}`; }
    console.log(`${ok ? 'ok  ' : 'FAIL'} ${output} ${name}: ${r.images.length} image(s) ${ms} ms${note}`);
  }
}
console.log(`png images byte-identical to native: ${identical}, differing: ${differing} ${diffs.join(' ')}`);
if (failures) { console.log(`${failures} failure(s)`); process.exit(1); }
console.log(`all ${names.length} examples ran on the WebAssembly host in svg and png modes`);
