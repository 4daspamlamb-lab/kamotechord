// Renders the SMK-37 diagrams (docs/diagrams/*.png) from these HTML sources.
// Run from this folder with Playwright installed:  node render.js
const {chromium} = require('playwright');
const path = require('path');
const jobs = [
  ['controls.html', 'SMK37-Controls.png', 'body', 1632],
  ['matrix.html', 'SMK37-Modes-Table.png', '.wrap', 1780],
  ['playmodes.html', 'SMK37-Play-Modes.png', '.wrap', 1780],
  ['clipmode.html', 'SMK37-Clip-Mode.png', '.wrap', 1632],
  ['stepmode.html', 'SMK37-Step-Mode.png', '.wrap', 1632],
  ['livekeys.html', 'SMK37-Live-Keys.png', '.wrap', 1632],
];
(async () => {
  const browser = await chromium.launch();
  for (const [src, out, sel, width] of jobs) {
    const page = await browser.newPage({viewport: {width, height: 900}, deviceScaleFactor: 2});
    await page.goto('file://' + path.join(__dirname, src));
    const height = await page.evaluate(s => document.querySelector(s).scrollHeight, sel);
    await page.setViewportSize({width, height});
    await page.screenshot({path: path.join(__dirname, '..', out), fullPage: true});
    await page.close();
  }
  await browser.close();
})();
