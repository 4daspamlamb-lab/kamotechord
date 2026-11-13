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
cs.ControlSurface = ControlSurface
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
        notes = sorted(b.identifier for b in registry if b.msg_type == 0)
        self.assertEqual(notes, list(range(36, 100)))     # launch, overdub, delete, select
        self.assertTrue(all(b.channel in (14, 15) for b in registry))
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

if __name__ == '__main__':
    unittest.main(verbosity=1)
