#!/usr/bin/env python3
"""Assemble the browser demo book: one fixed WebAssembly host plus precompiled lmcode programs.

  build-site.py --wasm build-wasm --pre PRECOMPILED --renders RENDERS_PNG --out OUT

OUT is self-contained static content (the lmctl.com /tools/lmplot/ demo files):
  demo.html                          the gallery (with boot.js, demo.css, favicon.svg beside it)
  browser/demo-<name>.html           one runnable page per example
  browser/lmplot.js, lmplot.wasm     the host (system layer): gnuplot + lmcode VM + Plot functions
  browser/worker.js, lmplot-runner.js, boot.js, demo.css, favicon.svg
  browser/examples.json              example list and the data files every run mounts
  browser/examples/<name>.bin        bytecode (application layer), loaded at run time
  browser/data/<file>.bin            the examples' data files, mounted at examples/data/<file>
  browser/thumbs/<name>.png          gallery thumbnails (rendered by the wasm host, PNG mode)
"""
import argparse
import hashlib
import html
import json
import re
import shutil
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]

TITLES = {
    'kepler_rv_fit': 'Exoplanet radial-velocity fit',
    'blackbody_planck': 'Planck blackbody spectra',
    'potential_vortex_surface': 'Double-well free-energy surface',
    'urban_heat_island': 'Urban heat island field',
    'phased_array_polar': 'Phased-array beam steering',
    'quant_trading_dashboard': 'Trading regime dashboard',
    'enzyme_michaelis_menten': 'Michaelis–Menten kinetics',
    'damped_driven_resonance': 'Driven damped resonance',
    'quadrature_convergence': 'Quadrature convergence',
    'option_greeks_dashboard': 'Black–Scholes Greeks',
    'roc_auc': 'ROC curve and AUC',
    'pr_average_precision': 'Precision–recall and AP',
    'confusion_matrix': 'Confusion matrix',
    'poisson_contours': 'Poisson equation contours',
    'cavity_quiver': 'Lid-driven cavity flow',
    'symlog_outlier': 'Symlog axis with an outlier',
    'residual_scatter': 'Regression residuals',
    'simple': 'Simple functions',
    'fillcrvs': 'Filled curves',
    'fillbetween': 'Fill between curves',
    'transparent': 'Transparent fills',
    'errorbars': 'Error bars',
    'candlesticks': 'Candlesticks',
    'boxplot': 'Box plots',
    'histograms': 'Histograms',
    'steps': 'Step plots',
    'param': 'Parametric plots',
    'polar': 'Polar plots',
    'surface1': '3D surfaces',
    'hidden': 'Hidden-line removal',
    'contours': 'Contours',
    'pm3d': 'pm3d colour surfaces',
    'heatmaps': 'Heat maps',
    'vector': 'Vector field',
    'image': 'Binary RGB image',
    'layout': 'Multiplot layouts',
    'spiderplot': 'Spider plots',
    'parallel': 'Parallel axes',
    'dashtypes': 'Dash types',
    'lines_arrows': 'Lines, points and arrows',
    'enhanced_utf8': 'Enhanced text and UTF-8',
    'rotate_labels': 'Text boxes and rotated labels',
    'nonlinear2': 'Nonlinear axes',
    'stats': 'Data, stats and labels',
    'turntable': 'Turntable frames',
}
SECTIONS = [
    ('Showcase', 'Showcase examples'),
    ('Model evaluation', 'Model evaluation and field overlays'),
    ('gnuplot gallery', 'Classic gnuplot gallery examples'),
]
TEX = [(r'\lambda', 'λ'), (r'\sigma', 'σ'), (r'\omega', 'ω'), (r'\theta', 'θ'), (r'\Delta', 'Δ'), (r'\nabla', '∇'),
       (r'\phi', 'φ'), (r'\psi', 'ψ'), (r'\sum', '∑'), (r'\int', '∫'), (r'\pm', '±'), (r'\to', '→'), (r'^\circ', '°'),
       (r'\sin', 'sin'), (r'\cos', 'cos'), (r'\,', ' '), (r'\chi', 'χ'), (r'\nu', 'ν'), (r'\pi', 'π'), (r'\cdot', '·')]


def plain(text):
    """README cell -> plain text: TeX math and backticks removed."""
    def math(m):
        s = m.group(1)
        for a, b in TEX:
            s = s.replace(a, b)
        s = s.replace('^2', '²').replace('^3', '³')
        s = re.sub(r'_\{?(\w+)\}?', r'\1', s)
        s = re.sub(r'\^\{?([\w.]+)\}?', r'^\1', s)
        return s.replace('{', '').replace('}', '').replace('\\', '')
    text = re.sub(r'\$([^$]*)\$', math, text)
    return text.replace('`', '')


def readme_rows():
    """{name: (section key, description)} from examples/README.md tables."""
    rows, section = {}, None
    for line in (ROOT / 'examples/README.md').read_text(encoding='utf-8').splitlines():
        if line.startswith('## '):
            section = next((key for key, head in SECTIONS if line[3:].startswith(head)), None)
            continue
        m = re.match(r'\| `lmctl https://\S+/examples/([a-z0-9_]+)\.lms` \| ([^|]*) \| (.*) \|$', line)
        if m and section:
            name, kind, desc = m.groups()
            if section == 'gnuplot gallery':
                desc = desc + (f' (gnuplot {kind.strip()})' if kind.strip().endswith('.dem') else '')
            rows[name] = (section, plain(f'{kind.strip()}: {desc}' if section != 'gnuplot gallery' else desc))
    return rows


def e(text):
    return html.escape(text, quote=True)


def header(gallery):
    return f'''    <header class="site-header">
        <a class="brand" href="{gallery}" aria-label="lmplot in the browser">lm<span>plot</span></a>
        <nav class="project-nav" aria-label="Project links">
            <a href="/tools/lmplot/">Quick reference</a>
            <a href="{gallery}">All examples</a>
            <a href="https://github.com/lmctl-ai/lmplot">Source on GitHub</a>
        </nav>
    </header>'''


def footer(version):
    return f'''    <footer>
        <p><strong>lmplot {e(version)}</strong> · gnuplot and the lmcode VM compiled to WebAssembly · everything runs in this tab</p>
        <p>lmplot is MIT licensed; gnuplot is distributed under its own license (see the notices).</p>
    </footer>
    <noscript>This page needs JavaScript and WebAssembly. The plots are drawn by gnuplot running in your browser.</noscript>'''


def head(title, description):
    return f'''<!doctype html>
<html lang="en">
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta name="description" content="{e(description)}">
    <title>{e(title)}</title>
    <link rel="icon" href="favicon.svg" type="image/svg+xml">
    <link rel="stylesheet" href="demo.css">
</head>'''


def book_nav(examples, current):
    parts = ['        <nav class="book-nav" aria-label="Examples">']
    for key, _ in SECTIONS:
        items = [x for x in examples if x['section'] == key]
        parts.append(f'            <p>{e(key)}</p>\n            <ol>')
        for x in items:
            cur = ' aria-current="page"' if x['name'] == current else ''
            parts.append(f'                <li><a href="demo-{x["name"]}.html"{cur}>{e(x["title"])}</a></li>')
        parts.append('            </ol>')
    parts.append('        </nav>')
    return '\n'.join(parts)


def example_page(x, examples, version):
    i = examples.index(x)
    prev = examples[i - 1] if i > 0 else None
    nxt = examples[i + 1] if i + 1 < len(examples) else None
    pager = (f'<a href="demo-{prev["name"]}.html">← {e(prev["title"])}</a>' if prev else '<a href="../demo.html">← All examples</a>') + \
        (f'<a href="demo-{nxt["name"]}.html">{e(nxt["title"])} →</a>' if nxt else '<a href="../demo.html">All examples →</a>')
    return f'''{head(f'{x["title"]} · lmplot in the browser', x["description"])}
<body data-example="{e(x["name"])}">
{header('../demo.html')}
    <main class="book-layout">
{book_nav(examples, x["name"])}
        <article class="chapter">
            <header class="chapter-heading">
                <p class="eyebrow">{e(x["section"])} · examples/{e(x["name"])}.lms</p>
                <h1>{e(x["title"])}</h1>
                <p class="lede">{e(x["description"])}</p>
            </header>
            <section class="demo-shell" aria-label="Live output">
                <div class="controls">
                    <button id="run" type="button">Run in browser</button>
                    <label class="mode">Output
                        <select id="output-mode">
                            <option value="svg" selected>SVG (vector)</option>
                            <option value="png">PNG (lmplot rasterizer)</option>
                        </select>
                    </label>
                    <p id="status" class="status" role="status" aria-live="polite">Loading the WebAssembly host…</p>
                </div>
                <div id="images" class="images" aria-live="polite"></div>
                <pre id="echo" class="echo" hidden></pre>
            </section>
            <section class="chapter-notes">
                <div>
                    <h2>What runs</h2>
                    <p>The page fetches <a href="examples/{e(x["name"])}.bin">examples/{e(x["name"])}.bin</a>, {x["bytes"]:,} bytes of lmcode bytecode
                    (sha256 <code>{x["sha256"][:12]}…</code>), and hands it to the same <code>lmplot.wasm</code> every example uses.
                    The bytecode calls the host's <code>Plot</code> functions (<code>plot.cmd</code>, <code>plot.data</code>, <code>plot.num</code>,
                    <code>plot.png</code> …); gnuplot draws into an in-memory file system and the page shows the files the program wrote.</p>
                    <p>In SVG mode <code>plot.png("x.png", w, h)</code> writes <code>x.svg</code> with gnuplot's svg terminal; PNG mode uses lmplot's
                    own rasterizer and fonts, the same bytes as the native binary.</p>
                    <p>The program was compiled ahead of time from the source on the right with <code>lmscript</code>; the lmscript lines around the
                    <code>_lmcode_</code> block (data generation, <code>${{...}}</code> values, <code>run_lmcode</code>) ran at compile time.</p>
                </div>
                <pre><code>{e(x["source"])}</code></pre>
            </section>
            <nav class="pager" aria-label="Example navigation">{pager}</nav>
        </article>
    </main>
{footer(version)}
    <script src="boot.js"></script>
</body>
</html>
'''


def gallery_page(examples, version, wasm_bytes):
    methods = len(json.loads((ROOT / 'schema/lmplot.dsl.json').read_text())['types']['Plot']['methods'])
    cards = []
    for key, _ in SECTIONS:
        items = [x for x in examples if x['section'] == key]
        cards.append(f'            <h2>{e(key)}</h2>\n            <ul class="gallery">')
        for x in items:
            cards.append(f'''                <li data-example="{e(x["name"])}">
                    <a href="browser/demo-{x["name"]}.html">
                        <img src="browser/thumbs/{x["name"]}.png" width="{x["thumb"][0]}" height="{x["thumb"][1]}" alt="{e(x["title"])}" loading="lazy">
                        <strong>{e(x["title"])}</strong>
                        <span>{e(x["description"])}</span>
                    </a>
                    <p class="card-status"></p>
                </li>''')
        cards.append('            </ul>')
    cards = '\n'.join(cards)
    return f'''{head('lmplot in the browser: gnuplot on WebAssembly, driven by lmcode', 'All lmplot examples running in the browser: gnuplot and the lmcode VM compiled to WebAssembly, each plot a precompiled lmcode program loaded at run time.')}
<body data-page="gallery" data-base="browser/">
{header('demo.html')}
    <main class="cover">
        <section class="cover-intro">
            <div>
                <p class="eyebrow">lmplot {e(version)} · WebAssembly</p>
                <h1>gnuplot in your <span>browser</span>, driven by lmcode.</h1>
            </div>
            <p class="lede">One fixed WebAssembly host, <code>lmplot.wasm</code> ({wasm_bytes / 1e6:.1f} MB), holds gnuplot, the lmcode virtual machine and the
            <code>Plot</code> functions. Each example below is application code: a few kilobytes of lmcode bytecode the page loads at run time and the
            host executes. New plots never need a new wasm build.</p>
        </section>
        <section class="demo-shell layers" aria-label="How it fits together">
            <div class="layer">
                <p class="eyebrow">System layer · C/C++ → wasm</p>
                <p>gnuplot (pinned, patched), lmplot's PNG rasterizer and fonts, the lmcode-cpp VM, and the {methods} host functions of
                <code>schema/lmplot.dsl.json</code>. Built once with Emscripten; runs in a Worker; a fresh instance per run.</p>
            </div>
            <div class="layer">
                <p class="eyebrow">Application layer · lmcode</p>
                <p><code>plot.data</code>, <code>plot.cmd</code>, <code>plot.num</code>, <code>plot.png</code>… Configuration, data, computation and
                drawing commands live in the program. Loaded as bytecode (<code>browser/examples/&lt;name&gt;.bin</code>) or embedded as an asset.</p>
            </div>
            <div class="controls">
                <button id="run-all" type="button">Run all {len(examples)} examples here</button>
                <p id="status" class="status" role="status" aria-live="polite">Thumbnails were rendered by the same wasm host. Open an example to run it live,
                or run every example now and time it.</p>
            </div>
        </section>
{cards}
    </main>
{footer(version)}
    <script src="boot.js"></script>
</body>
</html>
'''


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--wasm', required=True, type=Path)
    ap.add_argument('--pre', required=True, type=Path)
    ap.add_argument('--renders', required=True, type=Path, help='PNG renders from tests/wasm-examples.mjs (OUT/png)')
    ap.add_argument('--out', required=True, type=Path)
    args = ap.parse_args()
    version = re.search(r'project\(lmplot VERSION ([0-9.]+)', (ROOT / 'CMakeLists.txt').read_text()).group(1)
    top = args.out
    if top.exists():
        shutil.rmtree(top)
    out = top / 'browser'
    for sub in ('examples', 'data', 'thumbs'):
        (out / sub).mkdir(parents=True)
    for f in ('lmplot.js', 'lmplot.wasm'):
        shutil.copyfile(args.wasm / f, out / f)
    for f in ('worker.js', 'lmplot-runner.js', 'boot.js', 'demo.css', 'favicon.svg'):
        shutil.copyfile(ROOT / 'web' / f, out / f)
    for f in ('boot.js', 'demo.css', 'favicon.svg'):
        shutil.copyfile(ROOT / 'web' / f, top / f)
    rows = readme_rows()
    examples = []
    for key, _ in SECTIONS:
        for name, (section, desc) in rows.items():
            if section != key:
                continue
            program = args.pre / f'{name}.dslb'
            if not program.exists():
                raise SystemExit(f'no precompiled program for {name}')
            data = program.read_bytes()
            shutil.copyfile(program, out / 'examples' / f'{name}.bin')
            renders = sorted((args.renders / name).glob('*.png'))
            main_render = next((r for r in renders if r.name == f'{name}.png'), renders[0])
            img = Image.open(main_render).convert('RGB')
            img.thumbnail((480, 360), Image.LANCZOS)
            # lmctl.com /tools/ caps a PNG at 30000 bytes: fewer colours, then smaller, until it fits.
            thumb = out / 'thumbs' / f'{name}.png'
            for colors, scale in ((128, 1), (64, 1), (32, 1), (32, .85), (24, .75), (16, .7)):
                small = img if scale == 1 else img.resize((round(img.width * scale), round(img.height * scale)), Image.LANCZOS)
                small.quantize(colors=colors, method=Image.Quantize.MEDIANCUT).save(thumb, optimize=True)
                if thumb.stat().st_size < 29000:
                    break
            else:
                raise SystemExit(f'thumbnail for {name} stays above 29000 bytes')
            img = small
            examples.append(dict(name=name, section=key, title=TITLES[name], description=desc,
                                 source=(ROOT / 'examples' / f'{name}.lms').read_text(encoding='utf-8'),
                                 bytes=len(data), sha256=hashlib.sha256(data).hexdigest(), thumb=img.size))
    missing = set(TITLES) - {x['name'] for x in examples}
    if missing:
        raise SystemExit(f'examples missing from the README tables: {sorted(missing)}')
    data_files = []
    for f in sorted((ROOT / 'examples/data').iterdir()):
        shutil.copyfile(f, out / 'data' / (f.name + '.bin'))
        data_files.append({'path': f'examples/data/{f.name}', 'url': f'data/{f.name}.bin'})
    manifest = {'version': version, 'data': data_files,
                'examples': [{k: x[k] for k in ('name', 'section', 'title', 'bytes', 'sha256')} | {'program': f'examples/{x["name"]}.bin'}
                             for x in examples]}
    (out / 'examples.json').write_text(json.dumps(manifest, indent=1, ensure_ascii=False) + '\n', encoding='utf-8')
    wasm_bytes = (out / 'lmplot.wasm').stat().st_size
    (top / 'demo.html').write_text(gallery_page(examples, version, wasm_bytes), encoding='utf-8')
    for x in examples:
        (out / f'demo-{x["name"]}.html').write_text(example_page(x, examples, version), encoding='utf-8')
    print(f'wrote {top}: {len(examples)} example pages, {len(data_files)} data files, lmplot.wasm {wasm_bytes} bytes')


if __name__ == '__main__':
    main()
