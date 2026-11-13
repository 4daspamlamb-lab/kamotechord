// KamoteKeys integration test against the SMK37 OPEN browser engine.
// Build simulator.wasm with the web toolchain (see tests/README.md), then:
//   node tests/integration/sim_test.mjs path/to/simulator.wasm [outdir]
// It drives physical events into the real engine port (DX7/808 synths,
// project storage, MIDI router) and checks MIDI output and audio.
import fs from 'node:fs';
import { WASI } from 'node:wasi';
const [wasmPath, outDir = '.'] = process.argv.slice(2);
const wasi = new WASI({ version: 'preview1', args: [], env: {} });
const { instance } = await WebAssembly.instantiate(fs.readFileSync(wasmPath), { wasi_snapshot_preview1: wasi.wasiImport });
wasi.initialize(instance);
const c = instance.exports;
const mem = () => new Uint8Array(c.memory.buffer);
let checks = 0, failures = 0;
const check = (ok, msg) => { ++checks; if (!ok) { ++failures; console.log('FAIL ' + msg); } };
const KEY = 1, PAD = 3, ENC = 4, FADER = 7, BTN = 0;
const B = { KNOB: 0, FADERBANK: 1, LEFT: 2, RIGHT: 3, OCTD: 4, OCTU: 5, ARP: 6, REPEAT: 7, SCALE: 8, CHORD: 9, GLOBE: 10, BT: 11,
            PATCH: 12, PARA: 13, FX: 14, SEQ: 15, PLAY: 16, REC: 17 };
let midi = [];
let energy = 0;
function render(blocks) {
  for (let i = 0; i < blocks; ++i) {
    const p = c.sim_render(), a = new Float32Array(c.memory.buffer, p, 256);
    for (let k = 0; k < 256; ++k) energy += a[k] * a[k];
    for (let r; (r = c.sim_midi_output());) {
      const n = r & 255, port = r >> 8, d = mem().slice(c.sim_file(), c.sim_file() + n);
      midi.push({ port, d: Array.from(d) });
    }
  }
}
const ms = (t) => render(Math.ceil(t / (128000 / 44100)));
const ev = (k, id, v) => { c.sim_smk37(k, id, v); render(1); };
const tap = (b) => { ev(BTN, b, 1); ev(BTN, b, 0); };
const out = (port, status, d1) => midi.filter(m => m.port === port && m.d[0] === status && (d1 === undefined || m.d[1] === d1));

c.sim_boot(1);
c.sim_midi_ports(6, 0);
c.sim_midi_capture(1);
ms(400);
for (const [i, v] of [[0, 0], [1, 0], [2, 0], [3, 127]]) ev(FADER, i, v);
ms(1600);
check(!((c.sim_arp(0) >> 8) & 1) && !((c.sim_arp(7) >> 8) & 1), 'engine ARP disabled on tracks 1 and 8 (bit 8)');

// Keyboard: note out on USB (AUTO), local DX7 audio.
midi = []; energy = 0;
ev(KEY, 12, 100); ms(300);
check(out(1, 0x90, 36).length === 1 && out(2, 0x90).length === 0, 'key (octave -2: second C = 36) -> USB note on, no BLE duplicate');
check(energy > 0.01, 'internal DX7 sounds (energy ' + energy.toFixed(3) + ')');
ev(KEY, 12, 0); ms(50);
check(out(1, 0x80, 36).length === 1, 'key note off');

// Ableton CCs on channel 15.
midi = [];
tap(B.PLAY); tap(B.GLOBE); tap(B.FADERBANK); tap(B.LEFT); tap(B.RIGHT); tap(B.KNOB); tap(B.KNOB);
const cc = out(1, 0xbe).map(m => m.d[1] + ':' + m.d[2]).join(' ');
check(cc === '20:127 20:0 21:127 21:0 22:127 22:0 24:127 24:0 27:127 27:0 25:127 25:0 25:127 25:0', 'Ableton CCs (KNOB BANK momentary): ' + cc);

// Drum mode on channel 10 reaches the drum engine, never the DX7.
tap(B.FX); midi = []; energy = 0;
ev(PAD, 0, 120); ms(200); ev(PAD, 0, 0); ms(50);
check(out(1, 0x99, 36).length === 1 && out(1, 0x89, 36).length === 1, 'drum pad 1 -> note 36 ch10');
check(energy > 0.001, 'drum engine sounds (energy ' + energy.toFixed(4) + ')');
tap(B.FX);

// Key Scale Mode + chord pads + ARP with the real engine timing.
ev(FADER, 1, 10 * 128 / 23 + 2 | 0); ev(FADER, 2, 9 * 128 / 12 + 4 | 0);
tap(B.SEQ); ev(PAD, 1, 100); ev(PAD, 8, 100);
midi = []; ev(KEY, 12, 100); ms(20);
check(out(1, 0x90).map(m => m.d[1]).join(',') === '45,48,52,55', 'Am7 from second C in A minor pentatonic (octave -2)');
ms(150);
const lcd = new Uint16Array(c.memory.buffer, c.sim_pixels(), 240 * 240).slice();
ev(KEY, 12, 0); ev(PAD, 1, 0); ev(PAD, 8, 0);
tap(B.ARP); midi = [];
ev(KEY, 12, 100); ev(KEY, 16, 100); ms(1000); ev(KEY, 12, 0); ev(KEY, 16, 0); ms(300);
const arpOn = out(1, 0x90).length, arpOff = out(1, 0x80).length;
check(arpOn >= 3 && arpOn <= 5 && arpOn === arpOff, `ARP 1/8 @120 in 1 s: ${arpOn} on / ${arpOff} off`);
tap(B.ARP); tap(B.SEQ);


// Clip Mode (0.31): pads -> ch16 notes, feedback via the real MIDI input path.
const hostIn = (bytes) => { mem().set(bytes, c.sim_file()); return c.sim_midi_from(1, bytes.length); };
tap(B.CHORD); ms(4000);                            // let earlier synth tails die away
energy = 0; ms(150); const baseline = energy;      // silence over the same window
midi = []; energy = 0;
ev(PAD, 4, 100); ms(100); ev(PAD, 4, 0); ms(50);
check(out(1, 0x9f, 40).length === 1 && out(1, 0x8f, 40).length === 1, 'clip pad 5 -> ch16 note 40 on/off');
check(energy <= baseline * 1.5 + 1e-6, `clip launches make no internal sound (${energy.toExponential(2)} vs ${baseline.toExponential(2)})`);
check(hostIn([0xbf, 126, 1]) === 0 && hostIn([0xbf, 84, 2]) === 0, 'host feedback accepted');
ms(300);
const lcdClip = new Uint16Array(c.memory.buffer, c.sim_pixels(), 240 * 240).slice();
fs.writeFileSync(outDir + '/integration_clip.rgb565', Buffer.from(lcdClip.buffer));
tap(B.CHORD);
// Settings persist across a simulated power cycle (deferred flash write).
for (let i = 0; i < 6; ++i) ev(ENC, 0, 1);       // BPM 120 -> 126 (E1)
tap(B.ARP); tap(B.OCTU);
ms(5000);                                          // > 1.5 s idle: deferred save
c.sim_boot(0); c.sim_midi_ports(6, 0); c.sim_midi_capture(1); ms(400);  // boot resets capture
for (const [i, v] of [[0, 0], [1, 0], [2, 0], [3, 127]]) ev(FADER, i, v);
ms(1600);
const st = c.sim_state(), tempo = mem()[st + 8 + 10] | mem()[st + 8 + 11] << 8;
check(tempo === 126, 'BPM restored after restart (engine tempo ' + tempo + ')');
midi = []; ev(KEY, 0, 100); ms(600); ev(KEY, 0, 0); ms(300);
check(out(1, 0x90, 24).length >= 1, 'octave back to -2 after restart (lowest C = 24)');
check(out(1, 0x90).length >= 2, 'ARP still enabled after restart');

// LCD screenshot (raw RGB565) for the report.
fs.writeFileSync(outDir + '/integration_lcd.rgb565', Buffer.from(lcd.buffer));
console.log(`${checks} integration checks, ${failures} failures`);
process.exit(failures ? 1 : 0);
