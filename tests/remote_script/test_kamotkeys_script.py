# SPDX-License-Identifier: GPL-3.0-only
"""Logic tests for remote_script/KamoteKeys against a mock Live API.
Run: python3 tests/remote_script/test_kamotkeys_script.py
This checks the script's behaviour, not Ableton itself."""
import os, sys, types, unittest

# ------------------------------------------------------------ mock Live
# Models the _Framework pieces the script uses: ControlSurface,
# component_guard and ButtonElement value listeners. Messages are delivered
# only to registered controls, as Live does.
import contextlib
fw = types.ModuleType('_Framework')
cs = types.ModuleType('_Framework.ControlSurface')
be = types.ModuleType('_Framework.ButtonElement')
ice = types.ModuleType('_Framework.InputControlElement')
ice.MIDI_NOTE_TYPE, ice.MIDI_CC_TYPE = 0, 1
registry = []
guard = [False]

class ButtonElement(object):
    def __init__(self, is_momentary, msg_type, channel, identifier):
        assert guard[0], 'controls must be created inside component_guard()'
        self.msg_type, self.channel, self.identifier = msg_type, channel, identifier
        self.listeners = []
        registry.append(self)
    def add_value_listener(self, callback, identify_sender=False):
        self.listeners.append((callback, identify_sender))
    def message_identifier(self): return self.identifier
    def receive(self, value):
        for callback, identify in self.listeners:
            callback(value, self) if identify else callback(value)
be.ButtonElement = ButtonElement

class ControlSurface(object):
    def __init__(self, c_instance): self._c_instance = c_instance; self.sent = []
    @contextlib.contextmanager
    def component_guard(self):
        guard[0] = True
        try: yield
        finally: guard[0] = False
    def song(self): return self._c_instance.song_obj
    def _send_midi(self, m): self.sent.append(tuple(m))
    def log_message(self, *a): pass
    def show_message(self, text): self.shown = getattr(self, 'shown', []) + [text]
    def update_display(self): pass
    def refresh_state(self): pass
    def disconnect(self): pass
    def schedule_message(self, delay, callback): scheduled.append(callback)
scheduled = []
def run_scheduled():
    while scheduled: scheduled.pop(0)()
cs.ControlSurface = ControlSurface
# Live 12 note API used by the step sequencer.
live = types.ModuleType('Live')
live.Clip = types.SimpleNamespace()
class MidiNoteSpecification(object):
    def __init__(self, pitch, start_time, duration, velocity=100, mute=False):
        self.pitch, self.start_time, self.duration, self.velocity, self.mute = pitch, start_time, duration, velocity, mute
live.Clip.MidiNoteSpecification = MidiNoteSpecification
sys.modules['Live'] = live
for name, mod in (('_Framework', fw), ('_Framework.ControlSurface', cs),
                  ('_Framework.ButtonElement', be), ('_Framework.InputControlElement', ice)):
    sys.modules[name] = mod

def deliver(msg):
    """Route a 3-byte message to the matching registered control (Live's job)."""
    status, d1, d2 = msg
    kind, channel = status & 0xF0, status & 0x0F
    msg_type = 0 if kind in (0x90, 0x80) else 1 if kind == 0xB0 else None
    value = 0 if kind == 0x80 else d2
    hits = [b for b in registry if b.msg_type == msg_type and b.channel == channel and b.identifier == d1]
    for b in hits: b.receive(value)
    return len(hits)

class Clip(object):
    def __init__(self, slot): self.slot = slot; self.is_playing = False; self.is_triggered = False; self.is_recording = False
    def fire(self):
        if self.is_recording: self.is_recording = False; self.is_playing = True; self.slot.track.log.append('finish')
        else: self.is_triggered = True; self.slot.track.log.append('fire')

class Slot(object):
    def __init__(self, track): self.track = track; self.clip = None; self.is_triggered = False; self.will_record_on_start = False
    @property
    def has_clip(self): return self.clip is not None
    def fire(self):
        if self.clip: self.clip.fire()
        else: self.is_triggered = True; self.will_record_on_start = True; self.track.log.append('record')
    def stop(self): self.track.log.append('stop')
    def delete_clip(self): self.clip = None; self.track.log.append('delete')

class Track(object):
    def __init__(self): self._live_ptr = id(self); self.name = 'Track'; self.arm = False; self.can_be_armed = True; self.log = []; self.clip_slots = [Slot(self) for _ in range(4)]

class Scene(object):
    def __init__(self, song): self.song = song
    def fire(self): self.song.calls.append('scene %d' % self.song.scenes.index(self))

class View(object):
    selected_track = None; selected_scene = None; highlighted_clip_slot = None

class TrackProxy(object):
    """Like Live: every access returns a new object for the same track."""
    def __init__(self, track): object.__setattr__(self, '_t', track)
    def __getattr__(self, name): return getattr(self._t, name)
    def __setattr__(self, name, value): setattr(self._t, name, value)

class Song(object):
    @property
    def visible_tracks(self): return [TrackProxy(t) for t in self._tracks]
    def __init__(self, n):
        self._tracks = [Track() for _ in range(n)]; self.calls = []; self.session_record = False; self.can_undo = True; self.can_redo = False
        self.scenes = [Scene(self) for _ in range(4)]; self.view = View(); self.view.selected_scene = self.scenes[0]
    is_playing = False; metronome = False; tempo = 120.0
    def continue_playing(self): self.calls.append('continue'); self.is_playing = True
    def start_playing(self): self.calls.append('start')
    def stop_playing(self): self.calls.append('stop')
    def capture_midi(self): self.calls.append('capture')
    def undo(self): self.calls.append('undo')
    def redo(self): self.calls.append('redo')

class CInstance(object):
    def __init__(self, song): self.song_obj = song
    def handle(self): return 1

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'remote_script'))
from KamoteKeys.KamoteKeys import KamoteKeys, STATE_CC, HELLO_CC  # noqa: E402

class T(unittest.TestCase):
    def setUp(self):
        del registry[:]
        self.song = Song(6)                       # fewer tracks than pads on purpose
        self.s = KamoteKeys(CInstance(self.song))
    def press(self, pad, ch=15): deliver((0x90 | ch, 36 + pad, 127)); deliver((0x80 | ch, 36 + pad, 0))
    def state(self, pad): return [m[2] for m in self.s.sent if m[0] == 0xBF and m[1] == STATE_CC + pad][-1]

    def test_hello_and_full_state_on_load(self):
        self.assertIn((0xBF, HELLO_CC, 1), self.s.sent)
        self.assertEqual(len([m for m in self.s.sent if m[0] == 0xBF and STATE_CC <= m[1] < STATE_CC + 16]), 16)

    def test_controls_claim_only_protocol_messages(self):
        notes = sorted(b.identifier for b in registry if b.msg_type == 0 and b.channel == 15)
        self.assertEqual(notes, list(range(36, 100)))     # launch, overdub, delete, select
        steps = sorted(b.identifier for b in registry if b.msg_type == 0 and b.channel == 13)
        self.assertEqual(steps, list(range(36, 52)) + list(range(60, 86)))   # step pads, step functions
        self.assertEqual(len([b for b in registry if b.channel == 12]), 128)   # key-zone pitches (ch 13)
        self.assertTrue(all(b.channel in (12, 13, 14, 15) for b in registry))
        self.assertEqual(deliver((0x90, 60, 100)), 0)             # channel 1 notes pass to tracks

    def test_launch_stop_switch(self):
        t = self.song._tracks[2]; t.clip_slots[0].clip = Clip(t.clip_slots[0]); t.clip_slots[1].clip = Clip(t.clip_slots[1])
        self.press(2); self.assertEqual(t.log, ['fire'])
        self.s.update_display(); self.assertEqual(self.state(2), 3)   # queued
        t.clip_slots[0].clip.is_triggered = False; t.clip_slots[0].clip.is_playing = True
        self.s.update_display(); self.assertEqual(self.state(2), 2)   # playing
        self.press(2); self.assertEqual(t.log[-1], 'stop')             # same pad: stop
        self.press(10); self.assertEqual(t.log[-1], 'fire')            # clip 2 on the same track: switch

    def test_status_bar_messages(self):
        self.assertIn('KamoteKeys Remote Script loaded', self.s.shown)
        t = self.song._tracks[2]; t.clip_slots[0].clip = Clip(t.clip_slots[0])
        self.press(2); self.assertTrue(self.s.shown[-1].endswith('launch'), self.s.shown[-1])
        self.press(0); self.assertIn('not armed', self.s.shown[-1])
        self.press(7); self.assertIn('does not exist', self.s.shown[-1])

    def test_record_empty_armed_only(self):
        t = self.song._tracks[0]
        self.press(0); self.assertEqual(t.log, [])                     # unarmed empty: nothing
        t.arm = True; self.s.update_display(); self.assertEqual(self.state(0), 6)
        self.press(0); self.assertEqual(t.log, ['record'])
        self.s.update_display(); self.assertEqual(self.state(0), 5)    # queued to record
        slot = t.clip_slots[0]; slot.clip = Clip(slot); slot.clip.is_recording = True; slot.is_triggered = False
        self.s.update_display(); self.assertEqual(self.state(0), 4)    # recording
        self.press(0); self.assertEqual(t.log[-1], 'finish')           # press again: finish recording

    def test_missing_tracks_are_empty_and_safe(self):
        self.press(7); self.press(15)
        self.s.update_display(); self.assertEqual(self.state(7), 0)

    def test_one_action_per_press(self):
        t = self.song._tracks[1]; t.clip_slots[0].clip = Clip(t.clip_slots[0])
        deliver((0x9F, 37, 127)); deliver((0x8F, 37, 0))
        self.assertEqual(t.log, ['fire'])

    def test_transport_and_session_record(self):
        for cc in (20, 21, 22, 24, 27): deliver((0xBE, cc, 127)); deliver((0xBE, cc, 0))
        self.assertEqual(self.song.calls, ['start', 'stop', 'capture', 'undo'])   # redo not possible
        deliver((0xBE, 25, 127)); deliver((0xBE, 25, 0)); self.assertTrue(self.song.session_record)
        deliver((0xBE, 25, 127)); deliver((0xBE, 25, 0)); self.assertFalse(self.song.session_record)

    def tap(self, note): deliver((0x9F, note, 127)); deliver((0x8F, note, 0))

    def test_overdub_clip(self):
        t = self.song._tracks[1]; slot = t.clip_slots[0]; slot.clip = Clip(slot)
        self.tap(52 + 1)                                   # stopped clip: launch + overdub
        self.assertEqual(t.log, ['fire']); self.assertTrue(self.song.session_record)
        slot.clip.is_triggered = False; slot.clip.is_playing = True
        self.tap(52 + 1); self.assertFalse(self.song.session_record)   # again: overdub off
        self.assertIn('overdub off', self.s.shown[-1])

    def test_plain_press_ends_overdub(self):
        t = self.song._tracks[1]; slot = t.clip_slots[0]; slot.clip = Clip(slot)
        self.tap(52 + 1); slot.clip.is_triggered = False; slot.clip.is_playing = True
        self.assertTrue(self.song.session_record)
        self.press(1)                                     # plain launch note
        self.assertFalse(self.song.session_record); self.assertEqual(t.log, ['fire'])   # not stopped
        self.assertIn('overdub off', self.s.shown[-1])
        self.press(1); self.assertEqual(t.log[-1], 'stop')   # next plain press stops as before

    def test_overdub_empty_slot_arms_and_records(self):
        t = self.song._tracks[3]
        self.tap(52 + 3); self.assertTrue(t.arm); self.assertEqual(t.log, ['record'])

    def test_delete_clip(self):
        t = self.song._tracks[2]; slot = t.clip_slots[1]; slot.clip = Clip(slot)
        self.tap(68 + 8 + 2); self.assertEqual(t.log, ['delete']); self.assertIsNone(slot.clip)
        self.tap(68 + 8 + 2); self.assertEqual(t.log, ['delete'])       # nothing left: no action
        self.assertIn('no clip', self.s.shown[-1])

    def test_play_pause_and_metronome(self):
        deliver((0xBE, 23, 127)); deliver((0xBE, 23, 0))
        self.assertEqual(self.song.calls[-1], 'continue')
        self.song.is_playing = True
        deliver((0xBE, 23, 127)); deliver((0xBE, 23, 0))
        self.assertEqual(self.song.calls[-1], 'stop')
        deliver((0xBE, 26, 127)); deliver((0xBE, 26, 0)); self.assertTrue(self.song.metronome)
        deliver((0xBE, 26, 127)); deliver((0xBE, 26, 0)); self.assertFalse(self.song.metronome)

    def test_select_arms_only_that_track(self):
        a, b = self.song._tracks[0], self.song._tracks[2]; a.arm = True
        self.tap(84 + 8 + 2)                               # select: track 3, scene 2
        v = self.song.view
        self.assertIs(v.selected_track._t, b); self.assertIs(v.selected_scene, self.song.scenes[1])
        self.assertIs(v.highlighted_clip_slot, b.clip_slots[1]); self.assertTrue(b.arm); self.assertFalse(a.arm)
        self.assertIn('armed', self.s.shown[-1])

    def test_scene_scroll_and_launch(self):
        t = self.song._tracks[0]; t.clip_slots[2].clip = Clip(t.clip_slots[2])
        self.s.sent.clear(); deliver((0xBE, 28, 127)); deliver((0xBE, 28, 0))   # scene down
        self.assertIn((0xBF, 104, 1), self.s.sent); self.assertIs(self.song.view.selected_scene, self.song.scenes[1])
        self.press(8)                                       # pad 9 = track 1, scene 3 now
        self.assertEqual(t.log, ['fire'])
        deliver((0xBE, 30, 127)); deliver((0xBE, 30, 0)); self.assertEqual(self.song.calls[-1], 'scene 1')
        for _ in range(5): deliver((0xBE, 29, 127)); deliver((0xBE, 29, 0))      # up, clamped at scene 1
        self.assertIs(self.song.view.selected_scene, self.song.scenes[0]); self.assertEqual(self.s._scene, 0)

    def test_highlight_follows_selection(self):
        self.s.update_display(); self.s.sent.clear()
        self.tap(84 + 8 + 2); self.s.update_display()      # select track 3, scene 2 -> pad 11
        self.assertIn((0xBF, 105, 10), self.s.sent)
        self.s.sent.clear(); deliver((0xBE, 28, 127)); deliver((0xBE, 28, 0))   # scene down: grid 2-3
        self.assertIn((0xBF, 105, 2), self.s.sent)          # scene 2 is now the top row
        self.s.sent.clear(); self.song.view.selected_track = None; self.s.update_display()
        self.assertIn((0xBF, 105, 127), self.s.sent)        # nothing highlighted

    def test_tempo_follows_live(self):
        self.assertIn((0xBF, 102, 1200 >> 7), self.s.sent); self.assertIn((0xBF, 103, 1200 & 127), self.s.sent)
        self.s.sent.clear(); self.song.tempo = 98.5; self.s.update_display()
        self.assertEqual(self.s.sent, [(0xBF, 102, 985 >> 7), (0xBF, 103, 985 & 127)])
        self.s.sent.clear(); self.s.update_display(); self.assertEqual(self.s.sent, [])   # only changes

    def test_sync_request_resends_everything(self):
        self.s.sent.clear(); deliver((0xBF, 127, 127))
        self.assertIn((0xBF, HELLO_CC, 1), self.s.sent); self.assertEqual(len(self.s.sent), 21)   # hello, scene, tempo x2, highlight, 16 states

    def test_only_changes_are_sent(self):
        self.s.sent.clear(); self.s.update_display(); self.assertEqual(self.s.sent, [])

    def test_mouse_changes_are_reflected(self):
        t = self.song._tracks[4]; t.clip_slots[1].clip = Clip(t.clip_slots[1]); t.clip_slots[1].clip.is_playing = True
        self.s.update_display(); self.assertEqual(self.state(12), 2)

# --------------------------------------------------------- step sequencer
class Listened(object):
    """add_X_listener / remove_X_listener / X_has_listener, as in Live."""
    def __init__(self): object.__setattr__(self, '_cbs', {})
    def __getattr__(self, name):
        if name.startswith('add_') and name.endswith('_listener'):
            return lambda cb, n=name[4:-9]: self._cbs.setdefault(n, []).append(cb)
        if name.startswith('remove_') and name.endswith('_listener'):
            return lambda cb, n=name[7:-9]: self._cbs.get(n, []).remove(cb)
        if name.endswith('_has_listener'):
            return lambda cb, n=name[:-13]: cb in self._cbs.get(n, [])
        raise AttributeError(name)
    def fire(self, name):
        for cb in list(self._cbs.get(name, [])): cb()

class Note(object):
    _ids = [0]
    def __init__(self, spec):
        Note._ids[0] += 1; self.note_id = Note._ids[0]
        self.pitch, self.start_time, self.duration, self.velocity, self.mute = spec.pitch, spec.start_time, spec.duration, spec.velocity, spec.mute
    def copy(self):
        n = Note.__new__(Note); n.__dict__.update(self.__dict__); return n

class MClip(Listened):
    def __init__(self, length=4.0):
        Listened.__init__(self)
        self.is_midi_clip = True; self.name = ''; self.loop_start = 0.0; self.loop_end = length; self.end_marker = length
        self.is_playing = False; self.playing_position = 0.0; self.notes = []
        self.is_recording = False; self.is_triggered = False
    def get_notes_extended(self, from_pitch, pitch_span, from_time, time_span):
        return tuple(n.copy() for n in self.notes if from_pitch <= n.pitch < from_pitch + pitch_span
                     and from_time <= n.start_time < from_time + time_span)
    def add_new_notes(self, specs):
        self.notes += [Note(x) for x in specs]; self.fire('notes')
    def remove_notes_extended(self, from_pitch, pitch_span, from_time, time_span):
        self.notes = [n for n in self.notes if not (from_pitch <= n.pitch < from_pitch + pitch_span
                                                     and from_time <= n.start_time < from_time + time_span)]
        self.fire('notes')
    def apply_note_modifications(self, notes):
        by_id = dict((n.note_id, n) for n in notes)
        for n in self.notes:
            if n.note_id in by_id: n.__dict__.update(by_id[n.note_id].__dict__)
        self.fire('notes')
    def duplicate_loop(self):
        length = self.loop_end - self.loop_start
        self.notes += [Note(MidiNoteSpecification(n.pitch, n.start_time + length, n.duration, n.velocity)) for n in self.notes]
        self.loop_end += length; self.end_marker = self.loop_end; self.fire('notes')

class MSlot(Listened):
    def __init__(self): Listened.__init__(self); self.clip = None; self.is_triggered = False; self.will_record_on_start = False
    @property
    def has_clip(self): return self.clip is not None
    def create_clip(self, length): self.clip = MClip(length); self.fire('has_clip')

class DrumPad(object):
    def __init__(self, note): self.note = note; self.chains = []; self.name = 'Pad %d' % note

class DrumRack(object):
    can_have_drum_pads = True
    def __init__(self):
        self.drum_pads = [DrumPad(n) for n in range(128)]
        for n in (36, 38, 42, 46): self.drum_pads[n].chains = ['chain']
        self.drum_pads[36].name, self.drum_pads[38].name = 'Kick', 'Snare'
        self.visible_drum_pads = self.drum_pads[36:52]
        self.view = types.SimpleNamespace(selected_drum_pad=self.drum_pads[36])
        self.chains = []

class MTrack(Listened):
    def __init__(self, rack=True):
        Listened.__init__(self); self.name = 'Drums'; self.has_midi_input = True; self.arm = False; self.can_be_armed = True
        object.__setattr__(self, 'data', {})
        self.devices = [DrumRack()] if rack else []; self.clip_slots = [MSlot() for _ in range(4)]

def _get_data(self, key, default): return self.data.get(key, default)
def _set_data(self, key, value): self.data[key] = value
MTrack.get_data = _get_data; MTrack.set_data = _set_data

class StepSong(object):
    signature_numerator = 4; signature_denominator = 4
    def __init__(self, rack=True):
        self.track = MTrack(rack)
        self.view = Listened()
        object.__setattr__(self.view, 'selected_track', self.track)
        object.__setattr__(self.view, 'highlighted_clip_slot', self.track.clip_slots[0])
        self.visible_tracks = [self.track]; self.scenes = []

class StepTest(unittest.TestCase):
    def setUp(self):
        del registry[:]; del scheduled[:]
        self.song = StepSong()
        self.s = KamoteKeys(CInstance(self.song))
        self.s.sent.clear()
    def mode(self, on): deliver((0xBD, 127, 127 if on else 0))
    def pad(self, i, down=True): deliver(((0x9D if down else 0x8D), 36 + i, 127 if down else 0))
    def tap(self, i): self.pad(i); self.pad(i, False)
    def fn(self, note, down=True): deliver(((0x9D if down else 0x8D), note, 127 if down else 0))
    def press(self, note): self.fn(note); self.fn(note, False)
    def codes(self):
        out = {}
        for st, cc, v in self.s.sent:
            if st == 0xBD and 80 <= cc < 96: out[cc - 80] = v
        return out
    def info(self, cc): return [v for st, c, v in self.s.sent if st == 0xBD and c == cc][-1]
    @property
    def clip(self): return self.song.track.clip_slots[0].clip

    def test_entering_creates_a_named_one_bar_clip(self):
        self.mode(True)
        self.assertIsNotNone(self.clip); self.assertEqual(self.clip.loop_end, 4.0); self.assertEqual(self.clip.name, 'Drums')
        c = self.codes(); self.assertEqual(len(c), 16)
        self.assertEqual(c[0], 1); self.assertEqual(c[4], 1)             # empty steps (0.62: no beat marker)
        self.assertEqual(self.info(100), 0); self.assertEqual(self.info(97), 1)

    def test_steps_write_real_notes(self):
        self.mode(True); self.tap(0); self.tap(4)
        notes = sorted((n.pitch, n.start_time, n.duration) for n in self.clip.notes)
        self.assertEqual(notes, [(36, 0.0, 0.25), (36, 1.0, 0.25)])
        self.s.update_display()
        self.assertTrue(3 <= self.codes()[0] <= 8)                       # note colour (sent on the press)
        self.tap(0); self.assertEqual([n.start_time for n in self.clip.notes], [1.0])   # off again

    def test_mouse_edits_show_on_pads(self):
        self.mode(True)
        self.clip.add_new_notes((MidiNoteSpecification(36, 0.5, 0.25, 127),))
        self.s.sent.clear(); self.s.update_display()
        self.assertEqual(self.codes().get(2), 8)                          # loudest = brightest

    def test_existing_clip_is_read(self):
        slot = self.song.track.clip_slots[0]; slot.clip = MClip(8.0)
        slot.clip.notes = [Note(MidiNoteSpecification(36, 5.0, 0.25, 64))]
        self.mode(True)
        self.assertEqual(self.info(97), 2)                                # 2 bars = 2 pages
        self.press(62); self.s.update_display()                           # page next
        self.assertEqual(self.info(96), 1); self.assertTrue(3 <= self.codes()[4] <= 8)

    def test_drum_view_selects_the_active_drum(self):
        self.mode(True); self.fn(60)
        c = self.codes()
        self.assertEqual(c[8], 22); self.assertEqual(c[10], 21); self.assertEqual(c[9], 20)   # active, loaded, empty
        self.tap(10); self.fn(60, False)                                  # pad 11 = note 38 (snare)
        self.assertIs(self.song.track.devices[0].view.selected_drum_pad, self.song.track.devices[0].drum_pads[38])
        self.tap(3); self.assertEqual([n.pitch for n in self.clip.notes], [38])

    def drum(self, i): deliver((0x9C, 36 + i, 127)); deliver((0x8C, 36 + i, 0))   # key-zone key = ch 13 pitch
    def pitches(self): return sorted((n.pitch, n.start_time) for n in self.clip.notes)

    def test_drum_keys_clear_and_auto_arm(self):
        """Move-style: drum keys select the sound (kick = drum key 0 too),
        CLEAR + drum key deletes that sound, a CLEAR tap clears the page,
        the track is armed while Step Mode is on."""
        self.mode(True)
        self.assertTrue(self.song.track.arm); self.assertEqual(self.info(103), 64); self.assertEqual(self.info(104), 0)
        self.drum(2); self.s.update_display(); self.assertEqual(self.info(102), 38)     # snare
        self.tap(0); self.tap(4)
        self.drum(0); self.s.update_display(); self.assertEqual(self.info(102), 36)     # kick, then a pad
        self.tap(2)
        self.assertEqual(self.pitches(), [(36, 0.5), (38, 0.0), (38, 1.0)])
        self.fn(68); self.drum(2); self.fn(68, False)                     # CLEAR + snare key
        self.assertEqual(self.pitches(), [(36, 0.5)]); self.assertEqual(self.info(102), 36)
        self.press(68); self.assertEqual(self.clip.notes, [])              # CLEAR tap = clear page
        self.mode(False); self.assertFalse(self.song.track.arm)           # auto-arm released

    def test_copy_paste(self):
        self.mode(True); self.tap(0); self.drum(2); self.tap(4)            # kick step 1, snare step 5
        self.press(67)                                                     # COPY tap = whole page
        self.fn(63); self.pad(1); self.pad(1, False); self.fn(63, False)   # 2 bars (pattern repeated)
        self.press(62); self.press(77)                                     # page 2, PASTE
        self.assertEqual(self.pitches(), [(36, 0.0), (36, 4.0), (38, 1.0), (38, 5.0)])
        self.fn(67); self.drum(2); self.fn(67, False)                      # COPY + snare key = snare lane
        self.fn(77); self.drum(6); self.fn(77, False)                      # PASTE + key 6 (42) = onto the hat
        self.assertIn((42, 5.0), self.pitches())
        self.assertEqual(len(self.clip.notes), 5)

    def test_length_bars_double_resolution_and_follow(self):
        self.mode(True)
        self.tap(0); self.tap(4)                                          # kick on beats 1 and 2
        self.fn(63); self.s.update_display()
        c = self.codes(); self.assertEqual((c[0], c[1], c[2]), (19, 18, 18))   # LENGTH held: bar view (shown, past end)
        self.pad(1); self.pad(1, False); self.fn(63, False)               # LENGTH + pad 2 = 2 bars
        self.assertEqual(self.clip.loop_end, 8.0)
        self.assertEqual(sorted(n.start_time for n in self.clip.notes), [0.0, 1.0, 4.0, 5.0])   # pattern repeated
        self.fn(63); self.pad(0); self.pad(0, False); self.fn(63, False)  # back to 1 bar
        self.assertEqual(self.clip.loop_end, 4.0)
        self.press(64)                                                    # DOUBLE
        self.assertEqual(self.clip.loop_end, 8.0); self.s.update_display(); self.assertEqual(self.info(106), 2)   # 2 bars
        self.press(65); self.s.update_display(); self.assertEqual(self.info(98), 1)          # 1/8
        self.press(65); self.press(65); self.press(65); self.press(65); self.assertEqual(self.info(98), 0)
        self.press(66); self.clip.is_playing = True; self.clip.playing_position = 4.5
        self.s.sent.clear(); self.s.update_display()
        self.assertEqual(self.info(96), 1); self.assertEqual(self.codes().get(2), 10)        # followed, playhead
        self.assertEqual(self.info(105), 2)                                                   # playing bar 2

    def test_chord_pick_note_length_and_bar_toggle(self):
        self.mode(True)
        for n in (48, 51, 55): deliver((0x9C, n, 127))                     # a locked Cm chord: all on ...
        for n in (48, 51, 55): deliver((0x8C, n, 0))                       # ... then all off
        self.s.update_display(); self.assertEqual(self.info(102), 48)
        self.fn(85); self.s.update_display(); self.assertEqual(self.codes()[0], 25)   # note length view
        self.pad(3); self.pad(3, False); self.fn(85, False)               # 4/16 = a quarter bar
        self.tap(0)
        self.assertEqual(sorted((n.pitch, n.duration) for n in self.clip.notes), [(48, 1.0), (51, 1.0), (55, 1.0)])
        self.tap(0); self.assertEqual(self.clip.notes, [])                  # the whole chord comes off
        self.press(78); self.s.update_display(); self.assertEqual(self.codes()[0], 19)   # BARS tap: bar view stays
        self.tap(0); self.s.update_display(); self.assertIn(self.codes()[0], (1, 13, 14, 15))   # bar picked: pattern again

    def test_bars_key_ghosts_repeats_and_key_prefs(self):
        self.mode(True)
        self.fn(63); self.pad(3); self.pad(3, False); self.fn(63, False)  # 4 bars
        self.fn(78); self.pad(2); self.pad(2, False); self.fn(78, False)  # BARS + pad 3 = show bar 3
        self.s.update_display(); self.assertEqual(self.info(96), 2)
        self.press(62); self.press(61); self.press(61); self.press(61)    # page back to 1
        self.drum(2); self.tap(0); self.drum(6); self.tap(0); self.drum(10); self.tap(0)   # snare, hat, open hat on step 1
        self.drum(0); self.s.update_display()
        self.assertEqual(self.codes()[0], 15)                            # kick view: 3 other sounds = strongest tint
        self.drum(6); self.s.update_display(); self.assertEqual(self.codes()[0] in (3, 4, 5, 6, 7, 8), True)
        self.fn(84); self.pad(4); self.pad(4, False); self.fn(84, False)  # repeat 3 + step 5
        hits = sorted(n.start_time for n in self.clip.notes if n.pitch == 42 and 1.0 <= n.start_time < 1.25)
        self.assertEqual(len(hits), 3); self.s.update_display(); self.assertEqual(self.codes()[4], 16)
        self.press(80); self.s.update_display(); self.assertEqual(self.info(103), 65)     # OCT + (remembered)
        self.press(81); self.s.update_display(); self.assertEqual(self.info(104), 1)      # SEQ: scale keys
        self.assertEqual(self.song.track.data['kamotekeys_step'], '1,1')
        self.mode(False); self.mode(True); self.s.update_display()
        self.assertEqual((self.info(103), self.info(104)), (65, 1))       # same track: same keys

    def test_playhead_only_on_shown_page_and_light(self):
        self.mode(True); self.clip.is_playing = True; self.clip.playing_position = 1.0
        self.s.sent.clear(); self.s.update_display(); self.assertEqual(self.codes(), {4: 10})   # only the change
        self.s.sent.clear(); self.s.update_display(); self.assertEqual(self.s.sent, [])           # nothing new

    def test_velocity_tie_copy_clear(self):
        self.mode(True); self.tap(0)
        self.pad(0); self.press(69); self.pad(0, False)                   # hold step + velocity 1
        self.assertEqual([n.velocity for n in self.clip.notes], [32])
        self.pad(4); self.pad(7); self.pad(7, False); self.pad(4, False)  # tie steps 5-8
        tie = [n for n in self.clip.notes if n.start_time == 1.0][0]; self.assertEqual(tie.duration, 1.0)
        self.s.update_display(); self.assertEqual(self.codes().get(5), 9)   # inside the long note
        self.fn(67); self.press(62); self.fn(67, False)                   # COPY + page next
        self.assertEqual(self.clip.loop_end, 8.0); self.assertEqual(sorted(n.start_time for n in self.clip.notes), [0.0, 1.0, 4.0, 5.0])
        self.press(68); self.assertEqual(sorted(n.start_time for n in self.clip.notes), [0.0, 1.0])   # CLEAR page 2

    def test_no_drum_rack_and_deleted_clip(self):
        self.song = StepSong(rack=False); del registry[:]
        self.s = KamoteKeys(CInstance(self.song)); self.s.sent.clear()
        self.mode(True); self.assertEqual(self.info(100), 1)
        self.tap(0); self.assertEqual([n.pitch for n in self.clip.notes], [36])   # still sequences C1
        slot = self.song.track.clip_slots[0]; slot.clip = None; slot.fire('has_clip')
        self.assertEqual(len(scheduled), 1)                               # rebind deferred, not in the listener
        run_scheduled(); self.assertIsNotNone(slot.clip)                   # a new clip
        self.s.update_display()

    def test_leaving_stops_listening(self):
        self.mode(True); clip = self.clip; self.mode(False)
        self.assertFalse(clip._cbs.get('notes')); self.s.sent.clear(); self.s.update_display()
        self.assertFalse([m for m in self.s.sent if m[0] == 0xBD])

if __name__ == '__main__':
    unittest.main(verbosity=1)
