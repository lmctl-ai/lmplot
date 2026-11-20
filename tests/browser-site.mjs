// node tests/browser-site.mjs URL [--all]: open the demo book in headless Chrome and check it runs.
// Loads demo-simple.html and demo-pm3d.html (SVG, then PNG), and with --all clicks "Run all" on demo.html.
import {createRequire} from 'node:module';
const require = createRequire(import.meta.url);
const {chromium} = require(process.env.PLAYWRIGHT || 'playwright');
const [base, flag] = process.argv.slice(2);
const browser = await chromium.launch({executablePath: process.env.CHROME || '/usr/bin/google-chrome', headless: true});
const page = await browser.newPage();
const errors = [];
page.on('pageerror', e => errors.push(String(e)));
page.on('console', m => { if (m.type() === 'error') errors.push(m.text()); });
let failed = 0;
const check = (ok, what) => { console.log((ok ? 'ok   ' : 'FAIL ') + what); if (!ok) failed++; };
for (const name of ['simple', 'pm3d']) {
  await page.goto(`${base}/browser/demo-${name}.html`);
  await page.waitForFunction(() => !document.getElementById('run').disabled && /Ran|Failed/.test(document.getElementById('status').textContent), null, {timeout: 60000});
  let status = await page.textContent('#status');
  let n = await page.locator('#images img').count();
  check(/^Ran/.test(status) && n > 0, `${name} svg: ${n} image(s): ${status}`);
  const svgOk = await page.$$eval('#images img', imgs => imgs.length > 0 && imgs.every(i => i.src.startsWith('data:image/svg+xml') && i.naturalWidth > 0));
  check(svgOk, `${name} svg images decoded`);
  await page.selectOption('#output-mode', 'png');
  await page.waitForFunction(() => !document.getElementById('run').disabled && /Ran .*PNG|Failed/.test(document.getElementById('status').textContent), null, {timeout: 60000});
  status = await page.textContent('#status');
  const pngOk = await page.$$eval('#images img', imgs => imgs.length > 0 && imgs.every(i => i.src.startsWith('data:image/png') && i.naturalWidth > 0));
  check(pngOk, `${name} png: ${status}`);
  await page.screenshot({path: `${process.env.SHOTS || '.'}/browser-${name}.png`, fullPage: false});
}
if (flag === '--all') {
  await page.goto(`${base}/demo.html`);
  await page.click('#run-all');
  await page.waitForFunction(() => /examples ran in this tab/.test(document.getElementById('status').textContent), null, {timeout: 300000});
  const status = await page.textContent('#status');
  const bad = await page.$$eval('.gallery li.failed', l => l.map(x => x.dataset.example + ': ' + x.querySelector('.card-status').textContent));
  check(bad.length === 0 && /^45 of 45/.test(status), `gallery run-all: ${status} ${bad.join('; ')}`);
  await page.screenshot({path: `${process.env.SHOTS || '.'}/browser-gallery.png`, fullPage: false});
}
check(errors.length === 0, `no page errors ${errors.slice(0, 3).join(' | ')}`);
await browser.close();
process.exit(failed ? 1 : 0);
