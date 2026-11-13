// KamoteKeys (FM-1) integration test against the FM1 OPEN browser engine.
// Build the simulator wasm with the web toolchain (tools/Build-Web.ps1, or
// the same clang command on Linux), then:
//   node tests/integration/fm1_sim_test.mjs path/to/simulator.wasm [outdir]
// It presses FM-1 keys and buttons through the real fm1_controls path and
// checks MIDI output, audio and the key/button LEDs.
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
// FM-1 buttons (fm1_controls.buttons bit order) and encoders. Functions as
// on the SMK-37 at the same position: FX=ARP, SEL=SUSTAIN, ENV=PAD SCALE,
// LFO=CLIP, EDIT=STOP, GLO=BT, ARP=DRUM, SEQ=KEY SCALE, PLAY, REC=RECORD.
const B = { OCTD: 0, OCTU: 1, FX: 2, SEL: 3, ENV: 4, LFO: 5, EDIT: 6, GLO: 7, HOME: 8, SAVE: 9, ARP: 10, SEQ: 11, PLAY: 12, REC: 13 };
const E = { SELECT: 0, ALGORITHM: 1, K1: 2, K2: 3, K3: 4, K4: 5, PRESETS: 6 };
const WHITE = [0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26];   // white keys 01..16
let midi = [], energy = 0;
function render(blocks) {
  for (let i = 0; i < blocks; ++i) {
    const p = c.sim_render(), a = new Float32Array(c.memory.buffer, p, 2 * c.sim_block_frames());
    for (let k = 0; k < a.length; ++k) energy += a[k] * a[k];
    for (let r; (r = c.sim_midi_output());) {
      const n = r & 255, port = r >> 8, d = mem().slice(c.sim_file(), c.sim_file() + n);
      midi.push({ port, d: Array.from(d) });
    }
  }
}
const ms = (t) => render(Math.ceil(t * 44.1 / c.sim_block_frames()) + 1);
const key = (k, down) => { c.sim_control(1, k, down ? 1 : 0); ms(12); };
const btn = (b, down) => { c.sim_control(0, b, down ? 1 : 0); ms(12); };
const tap = (b) => { btn(b, 1); btn(b, 0); };
const knob = (e, d) => { c.sim_control(2, e, d); ms(12); };
const out = (port, status, d1) => midi.filter(m => m.port === port && m.d[0] === status && (d1 === undefined || m.d[1] === d1));
const leds = () => mem().slice(c.sim_state() + 168, c.sim_state() + 168 + 41);
const keyLed = (k) => leds()[14 + k];
const USB = 1;
function shot(name) {
  const px = new Uint16Array(c.memory.buffer, c.sim_pixels(), 240 * 240);
  const rgb = Buffer.alloc(240 * 240 * 3);
  for (let i = 0; i < px.length; ++i) {
    const v = px[i]; rgb[i * 3] = (v >> 11) * 255 / 31; rgb[i * 3 + 1] = ((v >> 5) & 63) * 255 / 63; rgb[i * 3 + 2] = (v & 31) * 255 / 31;
  }
  fs.writeFileSync(`${outDir}/${name}.ppm`, Buffer.concat([Buffer.from('P6\n240 240\n255\n'), rgb]));
}

c.sim_boot(1);
c.sim_midi_ports(6, 0);
c.sim_midi_capture(1);
ms(300); shot('fm1_00_boot');
ms(1700);
check(!((c.sim_arp(0) >> 8) & 1) && !((c.sim_arp(7) >> 8) & 1), 'engine ARP disabled on tracks 1 and 8');
shot('fm1_01_main');

// Playing zone (keys 9..26): key 19 (C) = middle C at the fixed velocity 100.
midi = []; energy = 0;
key(19, 1); ms(300);
check(out(USB, 0x90, 60).length === 1 && out(USB, 0x90, 60)[0].d[2] === 100, 'key 19 -> C4 (60) velocity 100 on USB');
check(energy > 0.01, 'internal DX7 sounds (energy ' + energy.toFixed(3) + ')');
check(keyLed(19) === 1, 'held playing key lights its LED');
key(19, 0); ms(50);
check(out(USB, 0x80, 60).length === 1 && keyLed(19) === 0, 'note off, LED off');
midi = []; key(9, 1); key(9, 0);
check(out(USB, 0x90, 50).length === 1, 'key 9 (D) is the lowest playing key');

// Compact chord keys 0..8 (white and black): MAJ MIN DIM SUS4 7 MAJ7 6 9 LOCK.
midi = [];
key(0, 1); check(midi.length === 0, 'chord key alone sends nothing');
key(19, 1);
check(out(USB, 0x90).map(m => m.d[1]).join(',') === '60,64,67', 'MAJ (F) + C -> C E G: ' + out(USB, 0x90).map(m => m.d[1]));
check(keyLed(0) === 1, 'held chord key lit');
key(19, 0); key(0, 0);
check(out(USB, 0x80).length === 3, 'chord released');
midi = [];
key(1, 1); key(4, 1); key(19, 1);
check(out(USB, 0x90).map(m => m.d[1]).join(',') === '60,63,67,70', 'MIN (F#, black) + 7 (A) -> Cm7: ' + out(USB, 0x90).map(m => m.d[1]));
key(19, 0); key(4, 0); key(1, 0);
midi = [];
key(8, 1); key(8, 0); key(0, 1); key(0, 0);       // LOCK (C#), then MAJ
key(19, 1);
check(out(USB, 0x90).length === 3, 'LOCK + MAJ latched -> triad without holding');
check(keyLed(8) === 1 && keyLed(0) === 1, 'LOCK and MAJ LEDs stay lit');
key(19, 0); key(8, 1); key(8, 0); ms(20);
check(keyLed(8) === 0, 'LOCK off');

// Ableton buttons: PLAY start, EDIT stop, REC record, HOME+REC capture,
// HOME+OCT- undo, HOME+OCT+ redo (no octave change, no settings page).
midi = []; const octave = () => { midi = []; key(19, 1); key(19, 0); return out(USB, 0x90)[0].d[1]; };
const before = octave(); midi = [];
tap(B.PLAY); tap(B.EDIT); tap(B.REC);
btn(B.HOME, 1); tap(B.REC); tap(B.OCTD); tap(B.OCTU); btn(B.HOME, 0); ms(30);
const cc = out(USB, 0xbe).map(m => m.d[1] + ':' + m.d[2]).join(' ');
check(cc === '20:127 20:0 21:127 21:0 25:127 25:0 22:127 22:0 24:127 24:0 27:127 27:0', 'PLAY, EDIT=STOP, REC, HOME+REC/OCT-/OCT+: ' + cc);
check(leds()[B.HOME] === 0 && octave() === before, 'HOME combos open no page and leave the octave alone');

// SAVE (where the SMK-37 has PARA): tap = Play/Pause; HOME + SAVE = metronome;
// SAVE held while turning a knob sends nothing.
midi = []; tap(B.SAVE); btn(B.HOME, 1); tap(B.SAVE); btn(B.HOME, 0); ms(30);
btn(B.SAVE, 1); knob(E.K4, 1); btn(B.SAVE, 0); ms(30);
const pp = out(USB, 0xbe).map(m => m.d[1] + ':' + m.d[2]).join(' ');
check(pp === '23:127 23:0 26:127 26:0', 'SAVE tap = Play/Pause, HOME+SAVE = metronome, SAVE+knob = nothing: ' + pp);
check(leds()[B.HOME] === 0, 'HOME + SAVE opens no page');

// Sustain (SEL, where the SMK-37 has SUSTAIN, 0.54): notes are held until
// the next key; keys pressed together are held together; no CC 64.
midi = [];
tap(B.SEL); check(leds()[B.SEL] === 1 && out(USB, 0xb0, 64).length === 0, 'SEL -> sustain on, LED on, no CC 64');
key(19, 1); key(23, 1); key(19, 0); key(23, 0); ms(50);
check(out(USB, 0x90).length === 2 && out(USB, 0x80).length === 0, 'released notes keep sounding');
key(21, 1); key(21, 0); ms(20);
check(out(USB, 0x80, 60).length === 1 && out(USB, 0x80, 64).length === 1 && out(USB, 0x90, 62).length === 1, 'next key ends them');
tap(B.SEL); ms(20);
check(leds()[B.SEL] === 0 && out(USB, 0x80, 62).length === 1, 'SEL again -> sustain off, held note released');

// Knobs: K1 = tempo; with SAVE held K1-K3 = velocity, scale, root.
btn(B.SAVE, 1); knob(E.K1, -20); btn(B.SAVE, 0);
midi = []; key(19, 1); key(19, 0);
check(out(USB, 0x90, 60).length === 1 && out(USB, 0x90, 60)[0].d[2] === 80, 'SAVE + K1 -20 -> velocity 80');
btn(B.SAVE, 1); knob(E.K1, 20); btn(B.SAVE, 0);
tap(B.OCTU); midi = []; key(19, 1); key(19, 0);
check(out(USB, 0x90, 72).length === 1, 'OCT + -> C5');
tap(B.OCTD);

// Key Scale (SEQ): A minor pentatonic via SAVE + K2 (scale) / K3 (root).
btn(B.SAVE, 1); knob(E.K2, 10); knob(E.K3, 9); btn(B.SAVE, 0);
tap(B.SEQ); check(leds()[B.SEQ] === 1, 'SEQ LED on in Key Scale');
midi = []; key(19, 1); key(19, 0);
check(out(USB, 0x90, 69).length === 1, 'Key Scale: C key -> A (scale root) ' + out(USB, 0x90).map(m => m.d[1]));
midi = []; key(20, 1); key(20, 0);
check(midi.length === 0, 'Key Scale: black key silent');
tap(B.SEQ);
btn(B.SAVE, 1); knob(E.K2, -10); knob(E.K3, -9); btn(B.SAVE, 0);

// Drum Mode (ARP button, where the SMK-37 has FX): white keys 01..16 are pads.
tap(B.ARP); check(leds()[B.ARP] === 1, 'ARP-button LED on in Drum Mode');
midi = []; energy = 0;
key(WHITE[0], 1); ms(150); key(WHITE[0], 0);
key(WHITE[1], 1); key(WHITE[1], 0);
check(out(USB, 0x99, 36).length === 1 && out(USB, 0x99, 38).length === 1 && out(USB, 0x90).length === 0, 'white 01 -> 36, white 02 -> 38 on channel 10');
check(energy > 0.0005, 'drum engine sounds (' + energy.toFixed(4) + ')');
midi = []; key(1, 1); key(1, 0);
check(midi.length === 0, 'black key silent in Drum Mode');
key(WHITE[2], 1); ms(30); shot('fm1_02_drum'); key(WHITE[2], 0);
midi = []; key(WHITE[0], 1); tap(B.ARP); key(WHITE[0], 0); ms(30);
check(out(USB, 0x89, 36).length === 1, 'mode change releases the held drum pad');

// Pad Scale (ENV, 0.52): chord keys stay; keys 9..26 (white and black) play
// consecutive scale notes: key 9 = C3, key 10 = D3, ... key 16 = C4.
tap(B.ENV); check(leds()[B.ENV] === 1, 'ENV LED on in Pad Scale');
midi = []; key(9, 1); key(9, 0); key(10, 1); key(10, 0); key(11, 1); key(11, 0);
check(out(USB, 0x90).map(m => m.d[1]).join(',') === '48,50,52', 'Pad Scale keys 9, 10 (black), 11 -> C3 D3 E3: ' + out(USB, 0x90).map(m => m.d[1]));
ms(200);
check(keyLed(9) === 1 && keyLed(16) === 1 && keyLed(10) === 0, 'root keys (C3, C4) lit at rest');
midi = []; key(1, 1); key(9, 1);
check(out(USB, 0x90).map(m => m.d[1]).join(',') === '48,51,55', 'MIN chord key + key 9 -> C minor in Pad Scale: ' + out(USB, 0x90).map(m => m.d[1]));
check(keyLed(1) === 1, 'chord key lit while held');
ms(30); shot('fm1_03_pad_scale');
key(9, 0); key(1, 0);
check(out(USB, 0x80).length === 3, 'chord released');
tap(B.OCTU); midi = []; key(9, 1); key(9, 0); check(out(USB, 0x90, 60).length === 1, 'OCT + moves Pad Scale up (C4)'); tap(B.OCTD);
midi = []; key(12, 1); tap(B.ENV); key(12, 0); ms(30);
check(out(USB, 0x80, 53).length === 1, 'leaving Pad Scale releases the held key (F3)');

// Clip Mode (LFO): white keys launch clips; F# held = OVERDUB, G# held = DELETE.
tap(B.LFO); check(leds()[B.LFO] === 1, 'LFO LED on in Clip Mode'); midi = [];
key(WHITE[3], 1); key(WHITE[3], 0);
check(out(USB, 0x9f, 39).length === 1, 'Clip Mode white 04 -> note 39 channel 16');
midi = []; key(1, 1); ms(30); shot('fm1_07_overdub'); key(WHITE[3], 1); key(WHITE[3], 0); key(1, 0);
check(out(USB, 0x9f, 55).length === 1 && out(USB, 0x9f, 39).length === 0 && out(USB, 0x90).length === 0, 'F# + white 04 -> overdub note 55, F# plays nothing');
midi = []; key(3, 1); key(WHITE[9], 1); key(WHITE[9], 0); key(3, 0);
check(out(USB, 0x9f, 68 + 9).length === 1, 'G# + white 10 -> delete note 77');
midi = []; key(5, 1); key(WHITE[4], 1); key(WHITE[4], 0); key(5, 0);
check(out(USB, 0x9f, 84 + 4).length === 1 && out(USB, 0x90).length === 0, 'A# + white 05 -> select note 88');
midi = []; key(25, 1); key(25, 0); key(22, 1); key(22, 0); key(20, 1); key(20, 0);
check(out(USB, 0xbe).map(m => m.d[1] + ':' + m.d[2]).join(' ') === '28:127 28:0 29:127 29:0 30:127 30:0', 'top F# / D# / C# -> scene down / up / launch');
const hostIn = (bytes) => { mem().set(bytes, c.sim_file()); return c.sim_midi_from(1, bytes.length); };
hostIn([0xbf, 126, 1]); hostIn([0xbf, 80 + 3, 2]); ms(100);
check(keyLed(WHITE[3]) === 1, 'playing clip lights its key');
hostIn([0xbf, 80 + 3, 1]); ms(100);
check(keyLed(WHITE[3]) === 0, 'stopped clip unlit');
ms(30); shot('fm1_04_clip');
tap(B.LFO);

// HOME: settings page, and back; GLO: Bluetooth menu.
tap(B.HOME); ms(50); check(leds()[B.HOME] === 1, 'HOME tap opens settings (LED on)'); shot('fm1_05_settings');
tap(B.HOME); ms(50); check(leds()[B.HOME] === 0, 'HOME again: back');
midi = []; tap(B.GLO); ms(50);
check(leds()[B.GLO] === 1 && out(USB, 0xbe).length === 0, 'GLO -> Bluetooth menu'); shot('fm1_06_bluetooth');
tap(B.GLO); ms(50); check(leds()[B.GLO] === 0, 'GLO again closes the menu');

// ARP (FX button, where the SMK-37 has ARP): steps follow the BPM.
tap(B.FX); check(leds()[B.FX] === 1, 'FX LED on with ARP'); midi = [];
key(19, 1); key(23, 1); ms(1000); key(19, 0); key(23, 0); ms(300);
const on = out(USB, 0x90).length, off = out(USB, 0x80).length;
check(on >= 3 && on <= 5 && on === off, `ARP 1/8 @120 in 1 s: ${on} on / ${off} off`);
tap(B.FX);
// Random play across modes: nothing stuck.
for (let i = 0; i < 200; ++i) {
  const k = (i * 7) % 27; key(k, 1); if (i % 3 === 0) tap([B.ARP, B.ENV, B.SEQ, B.LFO][i % 4]); key(k, 0);
}
c.sim_release(); ms(300);
const ons = midi.filter(m => (m.d[0] & 0xf0) === 0x90 && m.d[2]).length, offs = midi.filter(m => (m.d[0] & 0xf0) === 0x80 || ((m.d[0] & 0xf0) === 0x90 && !m.d[2])).length;
check(ons === offs, `random play balanced: ${ons} on / ${offs} off`);

midi.filter(m => (m.d[0] & 0xf0) === 0x80 || ((m.d[0] & 0xf0) === 0x90 && !m.d[2])).length;
check(ons === offs, `random play balanced: ${ons} on / ${offs} off`);

console.log(`${checks} FM-1 integration checks, ${failures} failures`);
process.exit(failures ? 1 : 0);
