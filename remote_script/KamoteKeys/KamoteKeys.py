# SPDX-License-Identifier: GPL-3.0-only
"""KamoteKeys Remote Script for Ableton Live.

Protocol (must match core/kamotkeys/kk_config.h in the firmware):

  device -> Live   ch 16  Note 36..51        Clip Mode pads 1..16 (press / release)
                   ch 16  Note 52..67        pads 1..16 with OVERDUB held (1st black key)
                   ch 16  Note 68..83        pads 1..16 with DELETE held (2nd black key)
                   ch 16  Note 84..99        pads 1..16 with SELECT held (3rd black key):
                                             select the slot, arm its track (only it)
                   ch 15  CC 28 / 29 / 30    scene down / scene up / launch selected scene
                   ch 16  CC 127 = 127       "send the full state" (sync request)
                   ch 15  CC 20/21/22/24/25/27  Start, Stop, Capture MIDI, Undo,
                                             Session Record, Redo (127 press, 0 release)
                   ch 15  CC 23 / 26         Play/Pause, Metronome on/off
                   ch 14                     Step Mode (drum step sequencer): see
                                             StepSequencer.py
  Live -> device   ch 16  CC 80..95 = state  clip state of pads 1..16
                   ch 16  CC 126 = 1         protocol hello (script connected)
                   ch 16  CC 102 / 103       Live tempo x 10, MSB then LSB (ARP follows Live)
                   ch 16  CC 104             first scene shown on the grid (0 based)
                   ch 16  CC 105             pad of Live's highlighted slot (0..15), 127 = none

Clip states: 0 empty, 1 stopped, 2 playing, 3 queued, 4 recording,
5 queued to record, 6 empty slot on an armed track.

Pads 1-8 are the first shown scene of tracks 1-8, pads 9-16 the next one;
scene up / down scroll the grid (and Live's selected scene) one scene.
Only these messages are claimed by the script; keys and pads on channels
1 and 10 still reach the tracks normally.
"""
from __future__ import absolute_import, print_function, unicode_literals

import time

from _Framework.ControlSurface import ControlSurface
from _Framework.ButtonElement import ButtonElement
from _Framework.InputControlElement import MIDI_NOTE_TYPE, MIDI_CC_TYPE

from .CopyPaste import CopyPaste
from .StepSequencer import StepSequencer, STEP_CHANNEL, PAD_NOTE as STEP_PAD_NOTE, PADS as STEP_PADS, \
    FUNCTION_NOTES as STEP_FUNCTION_NOTES, MODE_CC as STEP_MODE_CC, SELECT_CHANNEL as STEP_SELECT_CHANNEL

PROTOCOL = 1
CLIP_CHANNEL = 15            # MIDI channel 16 (zero based)
TRANSPORT_CHANNEL = 14       # MIDI channel 15
NOTE_BASE = 36
OVERDUB_BASE = NOTE_BASE + 16
DELETE_BASE = NOTE_BASE + 32
SELECT_BASE = NOTE_BASE + 48
SCENE_CC = 104
COPY_NOTE, PASTE_NOTE = 100, 101           # 0.65: Clip Mode COPY / PASTE keys
COPY_KIND_CC, COPY_A_CC, COPY_B_CC = 106, 107, 108   # clipboard shown on the keyboard's screen
COPY_CLIP = 1
ARRANGE_NOTE = 102                     # 0.66: Scene Arrange on / off
TRACKS_CC, ARRANGE_CC = 109, 110       # first track shown, Scene Arrange on (to the keyboard)
CC_TRACKS_LEFT, CC_TRACKS_RIGHT = 31, 32
CC_CLIP_UP, CC_CLIP_DOWN = 33, 34       # 0.75: move Live's selected clip a scene up / down
CC_MASTER, CC_FINISH = 35, 36           # 0.77: Fader 4 = master volume; SEQ PLAY = finish clip, next track
CC_TEMPO = 37                           # 0.79: K1 = change Live's tempo by value - 64 BPM
CC_TRACK_VOLUME, CC_TRACK_MACRO = 38, 39  # 0.80: Fader 2 / 3 = selected track volume / macro 1 (else Send A)
CC_SOLO, CC_CLIP_PLAY, CC_CLIP_DELETE, CC_MUTE, CC_ARM, CC_QUANTIZE = 44, 45, 47, 48, 49, 50   # 0.86 top keys (46 unused since 0.88)
CC_ARM_ADD = 51                          # 0.90: PATCH + top G: arm without disarming the others
CC_CLIP_DUPLICATE, CC_LOOP_DOUBLE, CC_REC_LENGTH = 52, 53, 54   # 0.91: SHIFT + A / B; fixed length (bars, 0 = off)
CC_MACRO = tuple(range(55, 63))          # 0.93: E1-E8 = macros 1-8 of the selected track, relative
CC_CLIP_PADS = 63                        # 0.93: Clip Mode pad mode 0 normal, 1 play, 2 stop, 3 mute
SCENE_PADS = 4                           # 0.99: CC 63 = 4: Scene Mode (pads 1-8 = scenes)
MACRO_CC, MACRO_VALUE_CC = 118, 119      # 0.93: reply: macro index, value 0..126 (127 = no device)
STEP_EDIT_CC = 20                        # 0.91: ch 14 CC 20 / 21 / 22 = held steps' velocity / length / repeats
STEP_AUTO_CC, STEP_AUTO_STATE_CC = 30, 38   # 0.93: ch 14 CC 30-37 = held steps' macro automation; CC 38 = AUTO on / off
CC_FX = (40, 41, 42, 43)                 # 0.82: K4-K7 relative: filter, delay, reverb, reverb decay
STOP_NOTE = 103                         # 0.77: Clip Mode STOP key (G#)
ARR_ON, ARR_OFF, ARR_NONE = 7, 8, 9
ARRANGE_KEY = 'kamotekeys_arrange'     # per-scene track lists, saved in the Live set
STEP_TIMEOUT = 6.0                     # s without a Step Mode heartbeat: leave it
HIGHLIGHT_CC = 105
STATE_CC = 80
HELLO_CC = 126
TEMPO_MSB_CC, TEMPO_LSB_CC = 102, 103
SYNC_CC = 127
TRACKS = 8
SCENES = 2
CC_START, CC_STOP, CC_CAPTURE, CC_UNDO, CC_RECORD, CC_REDO = 20, 21, 22, 24, 25, 27
CC_PLAY_PAUSE, CC_METRONOME = 23, 26
CC_SCENE_DOWN, CC_SCENE_UP, CC_SCENE_LAUNCH = 28, 29, 30
TRANSPORT_CCS = (CC_START, CC_STOP, CC_CAPTURE, CC_UNDO, CC_RECORD, CC_REDO, CC_PLAY_PAUSE, CC_METRONOME,
                 CC_SCENE_DOWN, CC_SCENE_UP, CC_SCENE_LAUNCH, CC_TRACKS_LEFT, CC_TRACKS_RIGHT,
                 CC_CLIP_UP, CC_CLIP_DOWN, CC_MASTER, CC_FINISH, CC_TEMPO, CC_TRACK_VOLUME, CC_TRACK_MACRO,
                 CC_SOLO, CC_CLIP_PLAY, CC_CLIP_DELETE, CC_MUTE, CC_ARM, CC_QUANTIZE, CC_ARM_ADD,
                 CC_CLIP_DUPLICATE, CC_LOOP_DOUBLE, CC_REC_LENGTH, CC_CLIP_PADS) + CC_FX + CC_MACRO

EMPTY, STOPPED, PLAYING, QUEUED, RECORDING, QUEUED_RECORD, EMPTY_ARMED = range(7)
NAME_CC, CHAR_CC, SELTRACK_CC = 111, 112, 113   # 0.81: track names / selected track for the screen
NAME_LEN = 12
FX_CC = 114                              # 0.82: reply 114-117 = new position 0..126, 127 = no device
FX_NONE = 127
FULL_REFRESH_TICKS = 50      # update_display runs ~10 Hz: full resend every ~5 s


class KamoteKeys(ControlSurface):

    def __init__(self, c_instance):
        ControlSurface.__init__(self, c_instance)
        self._sent = [None] * (TRACKS * SCENES)
        self._tempo_sent = None
        self._scene = 0                  # first Live scene shown on the 8 x 2 grid
        self._track = 0                  # first Live track shown (0.66: D / E move it)
        self._arrange = False            # Scene Arrange view (0.66)
        self._arrangement = None         # {scene index: set(track indexes)}, loaded lazily
        self._step_seen = 0.0            # last Step Mode heartbeat
        self._highlight_sent = None
        self._names_sent = {}            # 0.81: slot -> name last sent
        self._seltrack_sent = None
        self._rec_bars = 0
        self._clip_pads = 0              # 0.93: 0 normal (play / stop), 1 play, 2 stop, 3 mute               # 0.91: fixed record length in bars (from the keyboard)
        self._clip_notes_sent = set()     # 0.89: selected clip's sounding notes, as sent
        self._note_cache = (None, 0, [])  # (clip, tick, [(pitch, start, end)])
        self._ticks = 0
        self._ticks_total = 0
        self._received = False
        self._cp = CopyPaste()           # Clip Mode copy / paste (0.65)
        self._stop_held = self._stop_used = False   # Clip Mode STOP key (0.77)
        self._clip_copy = None           # (track index, scene index) of the copied clip
        self._rec_finishing = False      # 0.99: Session Record pressed again: waiting for the bar line
        self._rec_quant = None           # launch quantization to put back afterwards
        self._board_sent = None
        self._step = StepSequencer(self)
        # Standard _Framework controls (as in Ableton's own controller
        # scripts): Live registers them and delivers their messages here.
        with self.component_guard():
            self._pads = []
            for base in (NOTE_BASE, OVERDUB_BASE, DELETE_BASE, SELECT_BASE):
                for pad in range(TRACKS * SCENES):
                    button = ButtonElement(True, MIDI_NOTE_TYPE, CLIP_CHANNEL, base + pad)
                    button.add_value_listener(self._pad_value, identify_sender=True)
                    self._pads.append(button)
            self._copy_keys = []
            for note in (COPY_NOTE, PASTE_NOTE, ARRANGE_NOTE, STOP_NOTE):
                button = ButtonElement(True, MIDI_NOTE_TYPE, CLIP_CHANNEL, note)
                button.add_value_listener(self._copy_key_value, identify_sender=True)
                self._copy_keys.append(button)
            self._sync = ButtonElement(True, MIDI_CC_TYPE, CLIP_CHANNEL, SYNC_CC)
            self._sync.add_value_listener(self._sync_value)
            # Step Mode (channel 14): step pads, function keys, mode on/off.
            self._step_buttons = []
            for note in [STEP_PAD_NOTE + i for i in range(STEP_PADS)] + list(STEP_FUNCTION_NOTES):
                button = ButtonElement(True, MIDI_NOTE_TYPE, STEP_CHANNEL, note)
                button.add_value_listener(self._step_value, identify_sender=True)
                self._step_buttons.append(button)
            # Key-zone keys (channel 13): the pitch to sequence (0.62).
            self._step_select = []
            for note in range(128):
                button = ButtonElement(True, MIDI_NOTE_TYPE, STEP_SELECT_CHANNEL, note)
                button.add_value_listener(self._step_select_value, identify_sender=True)
                self._step_select.append(button)
            self._step_edit = []                 # 0.91: hold a step + E1 / E2 / E3
            for cc in list(range(STEP_EDIT_CC, STEP_EDIT_CC + 3)) + list(range(STEP_AUTO_CC, STEP_AUTO_CC + 8)) + [STEP_AUTO_STATE_CC]:
                button = ButtonElement(True, MIDI_CC_TYPE, STEP_CHANNEL, cc)
                button.add_value_listener(self._step_edit_value, identify_sender=True)
                self._step_edit.append(button)
            self._step_mode = ButtonElement(True, MIDI_CC_TYPE, STEP_CHANNEL, STEP_MODE_CC)
            self._step_mode.add_value_listener(self._step_mode_value)
            self._transport_buttons = []
            for cc in TRANSPORT_CCS:
                button = ButtonElement(True, MIDI_CC_TYPE, TRANSPORT_CHANNEL, cc)
                button.add_value_listener(self._transport_value, identify_sender=True)
                self._transport_buttons.append(button)
        self.log_message('KamoteKeys Remote Script (protocol %d) loaded' % PROTOCOL)
        self._status('KamoteKeys Remote Script loaded')
        self._send_all()

    # ------------------------------------------------------------- input
    def _first_message(self):
        if not self._received:
            self._received = True
            self.log_message('KamoteKeys: first message received from the keyboard')

    def _pad_value(self, value, sender):
        self._first_message()
        if not value:                    # note on: one action per physical press
            return
        note = sender.message_identifier()
        if self._arrange:
            self._arrange_toggle((note - NOTE_BASE) % (TRACKS * SCENES))   # Scene Arrange: track on / off
            return
        if self._stop_held:
            self._stop_used = True
            self._stop_pad((note - NOTE_BASE) % (TRACKS * SCENES))         # STOP + clip
            return
        if self._cp.target():
            self._clip_target((note - NOTE_BASE) % (TRACKS * SCENES))   # COPY / PASTE armed: this clip
            return
        if note >= SELECT_BASE:
            self._select(note - SELECT_BASE)
        elif note >= DELETE_BASE:
            self._delete(note - DELETE_BASE)
        elif note >= OVERDUB_BASE:
            self._overdub(note - OVERDUB_BASE)
        else:
            self._pad_press(note - NOTE_BASE)

    def _pad_press(self, pad):
        """0.93 Clip Mode pad modes (from the keyboard, CC 63): 0 normal =
        play / stop the clip and select it (an empty slot: select + arm);
        1 play = launch every clip pressed; 2 stop = stop; 3 mute = the
        clip's track on / off."""
        mode = self._clip_pads
        if mode == SCENE_PADS:
            if pad < TRACKS:
                self._launch_scene_at(self._scene + pad)   # 0.99 Scene Mode: pads 1-8 = scenes
        elif mode == 1:
            self._launch(pad, only_launch=True)
        elif mode == 2:
            self._stop_pad(pad)
        elif mode == 3:
            track, slot = self._slot(pad)
            if track is not None:
                try:
                    track.mute = not track.mute
                    self._status('KamoteKeys: %s %s' % (track.name, 'muted' if track.mute else 'unmuted'))
                except Exception as error:
                    self._status('KamoteKeys: mute: %s' % error)
        else:
            track, slot = self._slot(pad)
            if slot is not None and slot.has_clip:
                self._launch(pad)
                self._highlight_slot(pad)
            else:
                self._select(pad)

    def _highlight_slot(self, pad):
        track, slot = self._slot(pad)
        try:
            if slot is not None:
                self.song().view.selected_track = track
                self.song().view.highlighted_clip_slot = slot
        except Exception:
            pass

    def _copy_key_value(self, value, sender):
        self._first_message()
        if sender.message_identifier() == STOP_NOTE:
            if value:
                self._stop_held, self._stop_used = True, False
            else:
                self._stop_held = False
                if not self._stop_used:
                    self._stop_selected()        # a tap: stop the selected clip
            return
        if sender.message_identifier() == ARRANGE_NOTE:
            if value:
                self._arrange = not self._arrange
                self._status('KamoteKeys: Scene Arrange %s' % ('on: tap pads to choose the tracks of each scene' if self._arrange else 'off'))
                self._send_all()
            return
        if sender.message_identifier() == COPY_NOTE:
            self._cp.copy_key(bool(value))
        else:
            self._cp.paste_key(bool(value))
        self._refresh_clipboard()

    def _clip_target(self, pad):
        """Copy this clip (replacing the clipboard) or paste the copied clip here."""
        action = self._cp.target()
        track, slot = self._slot(pad)
        if slot is None:
            self._status('KamoteKeys: no slot there')
            return
        if action == 'copy':
            if not slot.has_clip:
                self._status('KamoteKeys: empty slot, nothing to copy')
                return
            self._clip_copy = (self._track + pad % TRACKS, self._scene + pad // TRACKS)
            self._status('KamoteKeys: copied %s scene %d' % (track.name, self._clip_copy[1] + 1))
        else:
            if self._clip_copy is None:
                self._status('KamoteKeys: nothing copied yet')
            else:
                try:
                    source = list(list(self.song().visible_tracks)[self._clip_copy[0]].clip_slots)[self._clip_copy[1]]
                    if (self._clip_copy[0], self._clip_copy[1]) != (self._track + pad % TRACKS, self._scene + pad // TRACKS):
                        source.duplicate_clip_to(slot)
                        self._status('KamoteKeys: pasted into %s scene %d' % (track.name, self._scene + pad // TRACKS + 1))
                except Exception as error:
                    self._status('KamoteKeys: cannot paste there (%s)' % error)
        self._cp.done(action)
        self._refresh_clipboard()

    def _send_clipboard(self, kind, a, b):
        board = (kind, min(127, a), min(127, b))
        if board != self._board_sent:
            self._board_sent = board
            for cc, value in zip((COPY_KIND_CC, COPY_A_CC, COPY_B_CC), board):
                self._send_midi((0xB0 | CLIP_CHANNEL, cc, value))

    def _refresh_clipboard(self):
        if self._step.active:
            self._step._send_clipboard()
        elif self._clip_copy is not None:
            self._send_clipboard(COPY_CLIP | self._cp.flags(), self._clip_copy[0] + 1, self._clip_copy[1] + 1)
        else:
            self._send_clipboard(self._cp.flags(), 0, 0)

    def _step_value(self, value, sender):
        self._first_message()
        note = sender.message_identifier()
        if note < STEP_PAD_NOTE + STEP_PADS:
            self._step.pad(note - STEP_PAD_NOTE, bool(value), value)
        else:
            self._step.function(note, bool(value))

    def _step_edit_value(self, value, sender):
        self._first_message()
        cc = sender.message_identifier()
        if cc == STEP_AUTO_STATE_CC:
            self._step.auto = bool(value)
        elif cc >= STEP_AUTO_CC:
            index = cc - STEP_AUTO_CC
            try:
                param = self._macro_param(index)
            except Exception:
                param = None
            self._step.automate(index, value - 64, param)
        else:
            self._step.knob(cc - STEP_EDIT_CC, value - 64)

    def _step_select_value(self, value, sender):
        self._first_message()
        self._step.key_zone(sender.message_identifier(), bool(value))

    def _step_mode_value(self, value):
        self._first_message()
        # 127 = start; leaving: 0 = and play the clip, 64 = quietly (PATCH held).
        if value == 127:
            self._step_seen = time.time()   # also the 2 s heartbeat (0.66)
        self._step.set_active(value == 127)   # 0.67: leaving no longer plays the clip (PARA does)
        self._cp.reset()
        self._refresh_clipboard()

    def _sync_value(self, value):
        self._first_message()
        if value:
            self.log_message('KamoteKeys: sync request from the keyboard')
            self._send_all()

    def _transport_value(self, value, sender):
        self._first_message()
        cc = sender.message_identifier()
        if cc == CC_MASTER:
            self._master(value)          # a fader: every value counts
        elif cc == CC_TEMPO:
            self._nudge_tempo(value - 64)
        elif cc == CC_TRACK_VOLUME:
            self._track_volume(value)
        elif cc == CC_TRACK_MACRO:
            self._track_macro(value)
        elif cc in CC_FX:
            self._fx(CC_FX.index(cc), value - 64)
        elif cc in CC_MACRO:
            self._macro(CC_MACRO.index(cc), value - 64)
        elif cc == CC_CLIP_PADS:
            changed = (value == SCENE_PADS) != (self._clip_pads == SCENE_PADS)
            self._clip_pads = value              # 0.93: what a clip pad press does; 0.99: 4 = Scene Mode
            if changed:
                self._names_sent = {}
                self._send_all()                 # scene states and names <-> clip states and track names
        elif cc == CC_REC_LENGTH:
            self._rec_bars = value               # sent just before Session Record (0.91)
        elif value:                      # act on press; release is ignored
            self._transport(cc)

    # -------------------------------------------------------------- clips
    def _slot(self, pad):
        tracks = list(self.song().visible_tracks)
        index, scene = self._track + pad % TRACKS, self._scene + pad // TRACKS
        if index >= len(tracks):
            return None, None
        track = tracks[index]
        slots = list(track.clip_slots)
        if scene >= len(slots):
            return track, None
        return track, slots[scene]

    def _launch(self, pad, only_launch=False):
        track, slot = self._slot(pad)
        what = 'no such slot' if slot is None else 'clip' if slot.has_clip else 'empty slot'
        self.log_message('KamoteKeys: pad %d -> track %d clip %d (%s)' % (
            pad + 1, pad % TRACKS + 1, pad // TRACKS + 1, what))
        if slot is None:
            self._status('KamoteKeys pad %d: track %d does not exist' % (pad + 1, pad % TRACKS + 1))
            return
        if slot.has_clip:
            clip = slot.clip
            if clip.is_recording:
                clip.fire()              # finish recording (Live keeps playing it)
                action = 'finish recording'
            elif self.song().session_record and (clip.is_playing or clip.is_triggered):
                # Overdubbing this clip: a plain press ends the overdub and the
                # clip keeps playing (no need to hold the OVERDUB key).
                self.song().session_record = False
                action = 'overdub off'
            elif (clip.is_playing or clip.is_triggered) and not only_launch:
                slot.stop()              # same pad again: stop (quantized)
                action = 'stop'
            else:
                slot.fire()              # launch; switches clips on the track
                action = 'launch'
        elif self._armed(track):
            slot.fire()                  # empty slot on an armed track: record
            action = 'record new clip'
        else:
            action = 'empty slot, track not armed: nothing to do'
        # Unarmed empty slot: nothing, so a stray press never stops a track.
        self._status('KamoteKeys pad %d: %s %d / clip %d - %s' % (
            pad + 1, track.name, pad % TRACKS + 1, pad // TRACKS + 1, action))

    def _overdub(self, pad):
        """OVERDUB key + pad: record into that clip (Session Record overdub),
        or record a new clip in an empty slot (arming the track)."""
        track, slot = self._slot(pad)
        if slot is None:
            self._status('KamoteKeys pad %d: track %d does not exist' % (pad + 1, pad % TRACKS + 1))
            return
        song = self.song()
        if slot.has_clip:
            clip = slot.clip
            if clip.is_recording or (song.session_record and (clip.is_playing or clip.is_triggered)):
                song.session_record = False      # same gesture again: stop overdubbing
                action = 'overdub off'
            else:
                if not (clip.is_playing or clip.is_triggered):
                    slot.fire()
                song.session_record = True       # overdub the playing clip
                action = 'overdub'
        elif getattr(track, 'can_be_armed', False):
            if not track.arm:
                track.arm = True
            slot.fire()                          # empty slot on an armed track: record
            action = 'record new clip'
        else:
            action = 'track cannot record'
        self._status('KamoteKeys pad %d: %s %d / clip %d - %s' % (
            pad + 1, track.name, pad % TRACKS + 1, pad // TRACKS + 1, action))

    def _select(self, pad):
        """SELECT key + pad: select that slot (Live's selected track, scene and
        highlighted slot) and arm its track, disarming the others, so the next
        Session Record (KNOB BANK / FM-1 REC) records exactly there."""
        track, slot = self._slot(pad)
        if slot is None:
            self._status('KamoteKeys pad %d: no such slot' % (pad + 1))
            return
        song = self.song()
        view = song.view
        try:
            view.selected_track = track
            view.selected_scene = list(song.scenes)[self._scene + pad // TRACKS]
            view.highlighted_clip_slot = slot
        except Exception:
            pass
        # Compare by position: Live hands out a new Python object for a track
        # on every access, so identity ('is') never matches.
        armed = False
        for index, other in enumerate(list(song.visible_tracks)):
            if not getattr(other, 'can_be_armed', False):
                continue
            mine = index == self._track + pad % TRACKS
            if bool(other.arm) != mine:
                other.arm = mine
            armed = armed or mine
        self._status('KamoteKeys: selected %s %d / scene %d%s' % (
            track.name, pad % TRACKS + 1, self._scene + pad // TRACKS + 1,
            ' (armed: press Session Record to record)' if armed else ' (track cannot record)'))

    def _scroll(self, delta):
        scenes = list(self.song().scenes)
        last = max(0, len(scenes) - 1)
        new = max(0, min(last, self._scene + delta))
        if new != self._scene:
            self._scene = new
            self._send_all()
        try:
            self.song().view.selected_scene = scenes[self._scene]
        except Exception:
            pass
        # Keep Live's selected track inside the 8-track view. (0.84: no session
        # box any more: Live scrolled its window to the box, then back, on
        # every move. Live now only scrolls to keep the selected track in sight.)
        self._keep_selected_in_view()
        self._status('KamoteKeys: scenes %d-%d' % (self._scene + 1, self._scene + 2))

    def _launch_scene_at(self, index):
        """Scene Mode (0.99): select and launch scene `index` (it starts at the
        launch quantization: the scene that plays next)."""
        song = self.song()
        scenes = list(song.scenes)
        if not 0 <= index < len(scenes):
            self._status('KamoteKeys: no scene %d' % (index + 1))
            return
        try:
            song.view.selected_scene = scenes[index]
        except Exception:
            pass
        self._launch_scene()

    def _scene_state(self, index):
        """Scene Mode pad colour: queued, playing (all its clips play), stopped (has clips) or empty."""
        song = self.song()
        scenes = list(song.scenes)
        if not 0 <= index < len(scenes):
            return EMPTY
        if getattr(scenes[index], 'is_triggered', False):
            return QUEUED
        clips = playing = 0
        for track in list(song.visible_tracks):
            slots = list(getattr(track, 'clip_slots', []) or [])
            if index >= len(slots):
                continue
            slot = slots[index]
            if slot.has_clip:
                clips += 1
                clip = slot.clip
                if clip.is_triggered:
                    return QUEUED
                playing += bool(clip.is_playing)
        if not clips:
            return EMPTY
        return PLAYING if playing == clips else STOPPED

    def _launch_scene(self):
        song = self.song()
        scene = song.view.selected_scene
        scenes = list(song.scenes)
        index = next((i for i, x in enumerate(scenes) if self._same(x, scene)), None)
        if index is not None and index in self._arranged():
            self._launch_arranged(index)      # 0.66: the tracks chosen in Scene Arrange
            return
        try:
            scene.fire()
        except Exception:
            pass
        self._status('KamoteKeys: launch scene %s' % ('?' if index is None else index + 1))

    # --------------------------------------------- 0.77 transport helpers
    def _nudge_tempo(self, delta):
        """K1 on the keyboard (0.79): Live's tempo in whole BPM steps."""
        if not delta:
            return
        song = self.song()
        try:
            song.tempo = max(20.0, min(999.0, round(song.tempo) + delta))
            self._send_tempo()
        except Exception as error:
            self.log_message('KamoteKeys: tempo: %s' % error)

    def _track_volume(self, value):
        """Fader 2: the selected track's volume (top = 0 dB, like Fader 4)."""
        try:
            self.song().view.selected_track.mixer_device.volume.value = 0.85 * value / 127.0
        except Exception as error:
            self.log_message('KamoteKeys: track volume: %s' % error)

    def _track_macro(self, value):
        """Fader 3: macro 1 of the selected track's first device (a rack), else Send A."""
        try:
            track = self.song().view.selected_track
            target = None
            devices = list(getattr(track, 'devices', []) or [])
            if devices:
                params = list(devices[0].parameters)
                if len(params) > 1 and getattr(devices[0], 'can_have_chains', False):
                    target = params[1]                      # parameters[0] is Device On
            if target is None:
                sends = list(track.mixer_device.sends)
                target = sends[0] if sends else None
            if target is not None:
                lo, hi = target.min, target.max
                target.value = lo + (hi - lo) * value / 127.0
        except Exception as error:
            self.log_message('KamoteKeys: macro: %s' % error)

    # ------------------------------------------- track effects (0.82)
    @staticmethod
    def _devices(track):
        """The track's devices, including those one rack level down."""
        found = []
        for device in list(getattr(track, 'devices', []) or []):
            found.append(device)
            if getattr(device, 'can_have_chains', False):
                for chain in list(getattr(device, 'chains', []) or []):
                    found.extend(list(getattr(chain, 'devices', []) or []))
        return found

    @staticmethod
    def _param(device, *names):
        for p in list(device.parameters):
            if p.name in names:
                return p
        for p in list(device.parameters):
            if any(n.lower() in p.name.lower() for n in names):
                return p
        return None

    @staticmethod
    def _labels(device):
        """Internal, display and shown names, lower case without spaces."""
        return [str(getattr(device, a, '') or '').lower().replace(' ', '').replace('-', '')
                for a in ('class_name', 'class_display_name', 'name')]

    def _is(self, device, kind):
        labels = self._labels(device)
        if kind == 'filter':
            return any('autofilter' in l for l in labels)
        if kind == 'delay':
            return any(('delay' in l or 'echo' in l) and 'autofilter' not in l for l in labels)
        return any('reverb' in l for l in labels) or labels[0] == 'hybrid'

    def _find(self, track, kind):
        # 0.85: match the internal, display or shown name (Live versions name
        # devices differently, e.g. the newer Auto Filter).
        return next((d for d in self._devices(track) if self._is(d, kind)), None)

    def _report_missing(self, track, kind):
        try:
            seen = ', '.join('%s (%s)' % (getattr(d, 'name', '?'), getattr(d, 'class_name', '?')) for d in self._devices(track))
            text = 'KamoteKeys: no %s on %s. Devices: %s' % (kind, getattr(track, 'name', '?'), seen or 'none')
        except Exception as error:
            text = 'KamoteKeys: no %s (%s)' % (kind, error)
        self.log_message(text)
        self._status(text)

    @staticmethod
    def _nudge(param, steps, per_step):
        lo, hi = param.min, param.max
        param.value = max(lo, min(hi, param.value + (hi - lo) * per_step * steps))
        return (param.value - lo) / float(hi - lo) if hi > lo else 0.0

    def _filter(self, device, steps):
        """One-knob DJ filter: left = low-pass closing, centre = open, right = high-pass."""
        kind = self._param(device, 'Filter Type', 'Type')
        freq = self._param(device, 'Frequency', 'Freq', 'Cutoff')
        if freq is None:
            return None
        if kind is None or not list(getattr(kind, 'value_items', []) or []):
            return self._nudge(freq, steps, 0.02)              # no type switch: frequency only
        items = [str(i).lower() for i in list(getattr(kind, 'value_items', []) or [])]
        low = next((i for i, n in enumerate(items) if 'low' in n), 0)
        high = next((i for i, n in enumerate(items) if 'high' in n), 1)
        span = float(freq.max - freq.min) or 1.0
        f = (freq.value - freq.min) / span
        if int(kind.value) == high:
            pos = max(0.0, min(1.0, (f - 0.12) / 0.76))
        else:
            pos = -max(0.0, min(1.0, (1.0 - f) / 0.88))
        pos = max(-1.0, min(1.0, pos + steps * 0.03))
        if abs(pos) < 0.015:
            pos = 0.0
        if pos > 0:
            kind.value = high
            freq.value = freq.min + span * (0.12 + 0.76 * pos)
        else:
            kind.value = low
            freq.value = freq.min + span * (1.0 + 0.88 * pos)
        return (pos + 1.0) / 2.0

    def _fx(self, which, steps):
        """K4-K7 on the selected track: 0 filter, 1 delay amount, 2 reverb amount, 3 reverb decay."""
        position = None
        kind = ('filter', 'delay', 'reverb', 'reverb')[which]
        try:
            track = self.song().view.selected_track
            device = self._find(track, kind)
            if device is None:
                self._report_missing(track, kind)
            elif which == 0:
                position = self._filter(device, steps)
            else:
                param = self._param(device, 'Decay Time', 'Decay') if which == 3 else self._param(device, 'Dry/Wet', 'Dry Wet', 'Mix')
                position = self._nudge(param, steps, 0.02) if param is not None else None
            if device is not None and position is None:
                names = ', '.join(p.name for p in list(device.parameters))
                self._status('KamoteKeys: %s found, but not its control. Parameters: %s' % (kind, names[:300]))
                self.log_message('KamoteKeys: %s parameters: %s' % (kind, names))
        except Exception as error:
            self.log_message('KamoteKeys: effect %d: %s' % (which, error))
            self._status('KamoteKeys: effect error: %s' % error)
        value = FX_NONE if position is None else max(0, min(126, int(round(position * 126))))
        self._send_midi((0xB0 | CLIP_CHANNEL, FX_CC + which, value))

    # -------------------------------------------- macro page (0.93)
    def _macro_param(self, index):
        """Macro index+1 of the selected track's first rack, else the
        index+1-th control of its first device (parameter 0 is Device On)."""
        track = self.song().view.selected_track
        devices = list(getattr(track, 'devices', []) or [])
        device = next((d for d in devices if getattr(d, 'can_have_chains', False)), None)
        if device is None and devices:
            device = devices[0]
        if device is None:
            return None
        params = list(device.parameters)
        return params[index + 1] if index + 1 < len(params) else None

    def _macro(self, index, steps):
        param = None
        try:
            param = self._macro_param(index)
            if param is not None and steps:
                if getattr(param, 'is_quantized', False):
                    param.value = max(param.min, min(param.max, param.value + (1 if steps > 0 else -1)))
                else:
                    self._nudge(param, steps, 0.01)
        except Exception as error:
            self.log_message('KamoteKeys: macro %d: %s' % (index + 1, error))
            param = None
        name = self._ascii(getattr(param, 'name', '')) if param is not None else ''
        self._send_midi((0xB0 | CLIP_CHANNEL, NAME_CC, 9))
        for c in name:
            self._send_midi((0xB0 | CLIP_CHANNEL, CHAR_CC, ord(c)))
        self._send_midi((0xB0 | CLIP_CHANNEL, MACRO_CC, index))
        if param is None:
            value = FX_NONE
        else:
            span = float(param.max - param.min) or 1.0
            value = max(0, min(126, int(round((param.value - param.min) / span * 126))))
        self._send_midi((0xB0 | CLIP_CHANNEL, MACRO_VALUE_CC, value))

    def _master(self, value):
        """Fader 4: Live's master volume (top = 0 dB)."""
        try:
            self.song().master_track.mixer_device.volume.value = 0.85 * value / 127.0
        except Exception as error:
            self.log_message('KamoteKeys: master volume: %s' % error)

    def _stop_pad(self, pad):
        track, slot = self._slot(pad)
        if slot is None:
            return
        try:
            slot.stop()
            self._status('KamoteKeys: stop %s' % track.name)
        except Exception as error:
            self._status('KamoteKeys: cannot stop (%s)' % error)

    def _stop_selected(self):
        try:
            slot = self.song().view.highlighted_clip_slot
            if slot is not None:
                slot.stop()
                self._status('KamoteKeys: stop the selected clip')
        except Exception as error:
            self._status('KamoteKeys: cannot stop (%s)' % error)

    def _finish_and_next(self):
        """SEQ PLAY: finish the clip recording on the selected track (it keeps
        playing), then select and arm the next track at the same scene."""
        song = self.song()
        view = song.view
        tracks = list(song.visible_tracks)
        t = next((i for i, x in enumerate(tracks) if self._same(x, view.selected_track)), None)
        scenes = list(song.scenes)
        s = next((i for i, x in enumerate(scenes) if self._same(x, view.selected_scene)), 0)
        if t is None:
            return
        try:
            for slot in list(tracks[t].clip_slots):
                if slot.has_clip and (slot.clip.is_recording or (slot.clip.is_triggered and getattr(slot, 'will_record_on_start', False))):
                    slot.fire()                       # ends the recording; the clip plays on
            if t + 1 >= len(tracks):
                self._status('KamoteKeys: last track')
                return
            nxt = tracks[t + 1]
            for i, other in enumerate(tracks):
                if getattr(other, 'can_be_armed', False):
                    mine = i == t + 1
                    if bool(other.arm) != mine:
                        other.arm = mine
            view.selected_track = nxt
            slots = list(nxt.clip_slots)
            if s < len(slots):
                view.highlighted_clip_slot = slots[s]
            if not (self._track <= t + 1 < self._track + TRACKS):
                self._track = max(0, t + 1 - TRACKS + 1)
                self._send_all()
            self._status('KamoteKeys: finished, now %s (armed)' % nxt.name)
        except Exception as error:
            self._status('KamoteKeys: finish / next track: %s' % error)

    # ------------------------------------- top-octave Live keys (0.86)
    def _stop_everything(self):
        """PARA (0.99): stop every clip and the transport."""
        song = self.song()
        try:
            song.stop_all_clips(False)            # at once, not at the next bar
        except TypeError:
            song.stop_all_clips()
        except Exception as error:
            self.log_message('KamoteKeys: stop all clips: %s' % error)
        song.stop_playing()
        self._status('KamoteKeys: stop (all clips)')

    def _toggle_track(self, cc):
        song = self.song()
        track = song.view.selected_track
        what = {CC_SOLO: 'solo', CC_MUTE: 'mute', CC_ARM: 'arm'}[cc]
        try:
            if what == 'arm' and not getattr(track, 'can_be_armed', False):
                self._status('KamoteKeys: %s cannot be armed' % track.name)
                return
            if what == 'solo' and not track.solo:
                # 0.99: exclusive solo: this track only, every other solo off.
                for other in list(song.tracks) + list(getattr(song, 'return_tracks', []) or []):
                    if getattr(other, 'solo', False) and not self._same(other, track):
                        other.solo = False
            setattr(track, what, not getattr(track, what))
            self._status('KamoteKeys: %s %s %s' % (track.name, what, 'on' if getattr(track, what) else 'off'))
        except Exception as error:
            self._status('KamoteKeys: %s: %s' % (what, error))

    def _arm_only_selected(self):
        """Top G (0.90): arm the selected track and disarm every other one;
        pressed on the armed track it disarms it."""
        song = self.song()
        track = song.view.selected_track
        try:
            if not getattr(track, 'can_be_armed', False):
                self._status('KamoteKeys: %s cannot be armed' % track.name)
                return
            arm = not track.arm
            for other in list(song.tracks):
                if getattr(other, 'can_be_armed', False) and other.arm and not self._same(other, track):
                    other.arm = False
            track.arm = arm
            self._status('KamoteKeys: %s %s' % (track.name, 'armed (only this one)' if arm else 'disarmed'))
        except Exception as error:
            self._status('KamoteKeys: arm: %s' % error)

    def _recording_slots(self):
        slots = []
        for track in list(self.song().tracks):
            for slot in list(getattr(track, 'clip_slots', []) or []):
                try:
                    if slot.has_clip and slot.clip.is_recording:
                        slots.append(slot)
                except Exception:
                    pass
        return slots

    def _finish_recording(self):
        """Session Record pressed again with new clips recording: end them on
        the next whole bar (launch quantization at least 1 bar for that)."""
        song = self.song()
        slots = self._recording_slots()
        if not slots:
            return False
        try:
            q = int(getattr(song, 'clip_trigger_quantization', 4))
            if q == 0 or q > 4:                         # none or under a bar: use 1 bar
                self._rec_quant = q
                song.clip_trigger_quantization = 4
            for slot in slots:
                slot.fire()                              # ends the recording at the bar line, then loops
            self._rec_finishing = True
            self._status('KamoteKeys: recording to the end of the bar')
        except Exception as error:
            self.log_message('KamoteKeys: finish recording: %s' % error)
            song.session_record = False
        return True

    def _poll_recording(self):
        """After _finish_recording: once nothing records, Session Record off and the quantization back."""
        if not self._rec_finishing or self._recording_slots():
            return
        self._rec_finishing = False
        song = self.song()
        try:
            song.session_record = False
            if self._rec_quant is not None:
                song.clip_trigger_quantization = self._rec_quant
        except Exception as error:
            self.log_message('KamoteKeys: after recording: %s' % error)
        self._rec_quant = None
        self._status('KamoteKeys: recorded (whole bars), playing')

    def _fixed_record(self):
        """Record exactly self._rec_bars bars into the selected empty slot of an
        armed track; False = not applicable (use normal Session Record)."""
        song = self.song()
        if not self._rec_bars or song.session_record:
            return False
        slot = self._selected_slot()
        try:
            track = slot.canonical_parent if slot is not None else None
            if slot is None or slot.has_clip or not getattr(track, 'arm', False):
                return False
            beats = self._rec_bars * 4.0 * song.signature_numerator / float(song.signature_denominator)
            slot.fire(record_length=beats)
            self._status('KamoteKeys: recording %d bar%s' % (self._rec_bars, '' if self._rec_bars == 1 else 's'))
            return True
        except Exception as error:
            self.log_message('KamoteKeys: fixed-length record: %s' % error)
            return False

    def _duplicate_selected(self):
        """SHIFT + A (0.91): copy the selected clip into the next empty slot below and select it."""
        song = self.song()
        slot = self._selected_slot()
        try:
            if slot is None or not slot.has_clip:
                return
            slots = list(slot.canonical_parent.clip_slots)
            i = next((k for k, x in enumerate(slots) if self._same(x, slot)), None)
            target = next((x for x in slots[i + 1:] if not x.has_clip), None) if i is not None else None
            if target is None:
                self._status('KamoteKeys: no empty slot below')
                return
            slot.duplicate_clip_to(target)
            song.view.highlighted_clip_slot = target
            self._status('KamoteKeys: clip duplicated')
        except Exception as error:
            self._status('KamoteKeys: duplicate: %s' % error)

    def _double_selected(self):
        """SHIFT + B (0.91): the selected clip's loop twice as long, notes copied."""
        slot = self._selected_slot()
        try:
            if slot is not None and slot.has_clip:
                slot.clip.duplicate_loop()
                self._status('KamoteKeys: loop doubled')
        except Exception as error:
            self._status('KamoteKeys: double loop: %s' % error)

    def _selected_slot(self):
        try:
            return self.song().view.highlighted_clip_slot
        except Exception:
            return None

    def _play_selected(self):
        """Launch the selected clip, or stop it when it plays (an empty slot on an armed track records)."""
        slot = self._selected_slot()
        if slot is None:
            return
        try:
            if slot.has_clip and (slot.clip.is_playing or slot.clip.is_triggered):
                slot.stop()
            elif slot.has_clip or getattr(slot.canonical_parent, 'arm', False):
                slot.fire()
        except Exception as error:
            self._status('KamoteKeys: play: %s' % error)

    def _delete_selected(self):
        slot = self._selected_slot()
        try:
            if slot is not None and slot.has_clip:
                slot.delete_clip()
                self._status('KamoteKeys: clip deleted')
        except Exception as error:
            self._status('KamoteKeys: delete: %s' % error)

    def _quantize_selected(self):
        """Quantize the selected clip to Live's record quantization, else 1/16."""
        slot = self._selected_slot()
        try:
            if slot is None or not slot.has_clip:
                return
            grid = int(getattr(self.song(), 'midi_recording_quantization', 0) or 0) or 5   # 5 = 1/16
            slot.clip.quantize(grid, 1.0)
            self._status('KamoteKeys: clip quantized')
        except Exception as error:
            self._status('KamoteKeys: quantize: %s' % error)

    # ------------------------------------------------- move a clip (0.75)
    def _move_clip(self, delta):
        """Move Live's selected clip one scene up / down (into an empty slot;
        an occupied slot is swapped with it). The moved clip stays selected."""
        song = self.song()
        view = song.view
        tracks = list(song.visible_tracks)
        t = next((i for i, x in enumerate(tracks) if self._same(x, view.selected_track)), None)
        scenes = list(song.scenes)
        s = next((i for i, x in enumerate(scenes) if self._same(x, view.selected_scene)), None)
        if t is None or s is None:
            self._status('KamoteKeys: select a clip to move')
            return
        slots = list(tracks[t].clip_slots)
        target = s + delta
        if not (0 <= target < len(slots)) or not slots[s].has_clip:
            self._status('KamoteKeys: no clip to move there')
            return
        source, dest = slots[s], slots[target]
        try:
            if dest.has_clip:
                # Swap through the source: keep a copy of the target in a new scene, then put it back.
                song.create_scene(-1)
                spare = list(tracks[t].clip_slots)[-1]
                dest.duplicate_clip_to(spare)
                source.duplicate_clip_to(dest)
                spare.duplicate_clip_to(source)
                spare.delete_clip()
                song.delete_scene(len(list(song.scenes)) - 1)
            else:
                source.duplicate_clip_to(dest)
                source.delete_clip()
            view.selected_scene = list(song.scenes)[target]
            view.highlighted_clip_slot = dest
            if not (self._scene <= target < self._scene + SCENES):
                self._scene = max(0, min(len(scenes) - SCENES, target if delta > 0 else target - 1))
                self._send_all()
            self._status('KamoteKeys: moved %s to scene %d' % (tracks[t].name, target + 1))
        except Exception as error:
            self._status('KamoteKeys: cannot move the clip (%s)' % error)

    # ------------------------------------------------- tracks view (0.66)
    def _move_tracks(self, delta):
        # 0.81: like Move: A / B select the previous / next track; the
        # 8-track view follows it but never shows past the last track (Live's
        # session box cannot either, which made the view jump back before).
        song = self.song()
        tracks = list(song.visible_tracks)
        if not tracks:
            return
        current = next((i for i, x in enumerate(tracks) if self._same(x, song.view.selected_track)), None)
        target = max(0, min(len(tracks) - 1, (self._track if current is None else current) + delta))
        try:
            song.view.selected_track = tracks[target]
        except Exception:
            pass
        # The view moves only when the selection leaves it: going left the
        # selected track becomes the first of the 8, going right the last.
        last = max(0, len(tracks) - TRACKS)
        view = self._track
        if target < view:
            view = target
        elif target >= view + TRACKS:
            view = target - TRACKS + 1
        view = max(0, min(last, view))
        if view != self._track:
            self._track = view
            self._send_all()
        else:
            self._send_highlight()
            self._send_names()
        self._status('KamoteKeys: track %d (view %d-%d)' % (target + 1, self._track + 1, self._track + TRACKS))

    def _keep_selected_in_view(self):
        try:
            song = self.song()
            tracks = list(song.visible_tracks)
            current = next((i for i, x in enumerate(tracks) if self._same(x, song.view.selected_track)), None)
        except Exception:
            return
        if current is None:
            return
        view = self._track
        if current < view:
            view = current
        elif current >= view + TRACKS:
            view = current - TRACKS + 1
        view = max(0, min(max(0, len(tracks) - TRACKS), view))
        if view != self._track:
            self._track = view
            self._send_all()

    # ------------------------------------------------ Scene Arrange (0.66)
    # One clip per track; each scene stores which tracks play. Launching the
    # scene from the keyboard (top F#) launches those tracks' clips and stops
    # the others, so every scene plays the same clips: edit once, heard everywhere.
    def _arranged(self):
        if self._arrangement is None:
            self._arrangement = {}
            try:
                text = self.song().get_data(ARRANGE_KEY, '') or ''
                for part in str(text).split(';'):
                    if ':' in part:
                        scene, tracks = part.split(':', 1)
                        self._arrangement[int(scene)] = set(int(t) for t in tracks.split(',') if t != '')
            except Exception as error:
                self.log_message('KamoteKeys: cannot read the arrangement: %s' % error)
        return self._arrangement

    def _save_arrangement(self):
        text = ';'.join('%d:%s' % (s, ','.join(str(t) for t in sorted(ts))) for s, ts in sorted(self._arranged().items()))
        try:
            self.song().set_data(ARRANGE_KEY, text)
        except Exception as error:
            self.log_message('KamoteKeys: cannot save the arrangement: %s' % error)

    def _source_slot(self, track, scene):
        """The clip a track plays in a scene: its own slot, else the nearest clip above, then below."""
        slots = list(track.clip_slots)
        order = [scene] + list(range(scene - 1, -1, -1)) + list(range(scene + 1, len(slots)))
        for s in order:
            if 0 <= s < len(slots) and slots[s].has_clip:
                return slots[s]
        return None

    def _scene_tracks(self, scene):
        """Tracks playing in a scene: the stored choice, else the tracks with a clip in that scene."""
        stored = self._arranged().get(scene)
        if stored is not None:
            return stored
        tracks = list(self.song().visible_tracks)
        return set(i for i, t in enumerate(tracks) if scene < len(list(t.clip_slots)) and list(t.clip_slots)[scene].has_clip)

    def _arrange_state(self, pad):
        tracks = list(self.song().visible_tracks)
        index, scene = self._track + pad % TRACKS, self._scene + pad // TRACKS
        if index >= len(tracks) or scene >= len(list(self.song().scenes)) or self._source_slot(tracks[index], scene) is None:
            return ARR_NONE
        return ARR_ON if index in self._scene_tracks(scene) else ARR_OFF

    def _arrange_toggle(self, pad):
        tracks = list(self.song().visible_tracks)
        index, scene = self._track + pad % TRACKS, self._scene + pad // TRACKS
        if index >= len(tracks) or self._source_slot(tracks[index], scene) is None:
            self._status('KamoteKeys: that track has no clip')
            return
        chosen = set(self._scene_tracks(scene))
        chosen.symmetric_difference_update([index])
        self._arranged()[scene] = chosen
        self._save_arrangement()
        self._status('KamoteKeys: scene %d: %s %s' % (scene + 1, tracks[index].name, 'on' if index in chosen else 'off'))
        self._send_state(pad, force=True)

    def _launch_arranged(self, scene):
        song = self.song()
        chosen = self._scene_tracks(scene)
        for index, track in enumerate(list(song.visible_tracks)):
            try:
                source = self._source_slot(track, scene) if index in chosen else None
                if source is not None:
                    source.fire()
                elif hasattr(track, 'stop_all_clips'):
                    track.stop_all_clips()
            except Exception as error:
                self.log_message('KamoteKeys: arrange launch: %s' % error)
        self._status('KamoteKeys: launch scene %d (arranged: %d tracks)' % (scene + 1, len(chosen)))

    def _delete(self, pad):
        """DELETE key + pad: delete that clip."""
        track, slot = self._slot(pad)
        if slot is None or not slot.has_clip:
            self._status('KamoteKeys pad %d: no clip to delete' % (pad + 1))
            return
        slot.delete_clip()
        self._status('KamoteKeys pad %d: %s %d / clip %d - deleted' % (
            pad + 1, track.name, pad % TRACKS + 1, pad // TRACKS + 1))

    # Lets Live preselect the keyboard's port when the script is chosen. The
    # SMK37 OPEN / KamoteKeys firmware names its USB MIDI port "SMK37 Open".
    def suggest_input_port(self):
        return 'SMK37 Open'

    def suggest_output_port(self):
        return 'SMK37 Open'

    def _status(self, text):
        """Live's status bar (bottom of the window)."""
        try:
            self.show_message(text)
        except Exception:
            pass

    @staticmethod
    def _armed(track):
        try:
            return bool(track.can_be_armed and track.arm)
        except Exception:
            return False

    def _state(self, pad):
        if self._arrange:
            return self._arrange_state(pad)
        if self._clip_pads == SCENE_PADS:
            return self._scene_state(self._scene + pad) if pad < TRACKS else EMPTY
        track, slot = self._slot(pad)
        if slot is None:
            return EMPTY
        if slot.has_clip:
            clip = slot.clip
            if clip.is_recording:
                return RECORDING
            if clip.is_triggered:
                return QUEUED_RECORD if getattr(slot, 'will_record_on_start', False) else QUEUED
            if clip.is_playing:
                return PLAYING
            return STOPPED
        if slot.is_triggered and getattr(slot, 'will_record_on_start', False):
            return QUEUED_RECORD
        return EMPTY_ARMED if self._armed(track) else EMPTY

    def _send_state(self, pad, force=False):
        state = self._state(pad)
        if force or state != self._sent[pad]:
            self._sent[pad] = state
            self._send_midi((0xB0 | CLIP_CHANNEL, STATE_CC + pad, state))

    def _send_tempo(self, force=False):
        """Live's tempo to the keyboard (tenths of BPM): its ARP follows Live."""
        try:
            tempo = int(round(self.song().tempo * 10))
        except Exception:
            return
        tempo = max(200, min(9990, tempo))
        if force or tempo != self._tempo_sent:
            self._tempo_sent = tempo
            self._send_midi((0xB0 | CLIP_CHANNEL, TEMPO_MSB_CC, tempo >> 7))
            self._send_midi((0xB0 | CLIP_CHANNEL, TEMPO_LSB_CC, tempo & 127))

    @staticmethod
    def _same(a, b):
        """Live hands out a new Python object for a track or scene on every
        access, so compare the underlying Live objects, not identity."""
        if a is None or b is None:
            return False
        return getattr(a, '_live_ptr', a) == getattr(b, '_live_ptr', b)

    def _highlight_pad(self):
        """Pad (0..15) showing Live's highlighted slot, or 127."""
        if self._clip_pads == SCENE_PADS:     # 0.99: the selected scene
            try:
                scenes = list(self.song().scenes)
                s = next((i for i, x in enumerate(scenes) if self._same(x, self.song().view.selected_scene)), None)
            except Exception:
                return 127
            return s - self._scene if s is not None and 0 <= s - self._scene < TRACKS else 127
        try:
            song = self.song()
            view = song.view
            tracks = list(song.visible_tracks)[self._track:self._track + TRACKS]
            scenes = list(song.scenes)
            t = next((i for i, x in enumerate(tracks) if self._same(x, view.selected_track)), None)
            s = next((i for i, x in enumerate(scenes) if self._same(x, view.selected_scene)), None)
        except Exception:
            return 127
        if t is None or s is None or not (self._scene <= s < self._scene + SCENES):
            return 127
        return (s - self._scene) * TRACKS + t

    def _send_highlight(self, force=False):
        pad = self._highlight_pad()
        if force or pad != self._highlight_sent:
            self._highlight_sent = pad
            self._send_midi((0xB0 | CLIP_CHANNEL, HIGHLIGHT_CC, pad))

    # ---------------------------------------------- track names (0.81)
    @staticmethod
    def _ascii(name):
        text = ''.join(c if 32 <= ord(c) < 127 else '?' for c in str(name or ''))
        return text[:NAME_LEN]

    def _send_names(self, force=False):
        """Names of the 8 tracks shown (slots 0-7) and of Live's selected track (slot 8)."""
        try:
            song = self.song()
            tracks = list(song.visible_tracks)
            shown = tracks[self._track:self._track + TRACKS]
            selected = song.view.selected_track
            names = [self._ascii(t.name) for t in shown] + [''] * (TRACKS - len(shown))
            if self._clip_pads == SCENE_PADS:  # 0.99 Scene Mode: the 8 scenes' names
                scenes = list(song.scenes)[self._scene:self._scene + TRACKS]
                names = [self._ascii(getattr(x, 'name', '')) for x in scenes] + [''] * (TRACKS - len(scenes))
            names.append(self._ascii(getattr(selected, 'name', '')))
            number = next((i + 1 for i, x in enumerate(tracks) if self._same(x, selected)), 0)
        except Exception:
            return
        for slot, name in enumerate(names):
            if force or self._names_sent.get(slot) != name:
                self._names_sent[slot] = name
                self._send_midi((0xB0 | CLIP_CHANNEL, NAME_CC, slot))
                for c in name:
                    self._send_midi((0xB0 | CLIP_CHANNEL, CHAR_CC, ord(c)))
        number = min(number, 127)
        if force or number != self._seltrack_sent:
            self._seltrack_sent = number
            self._send_midi((0xB0 | CLIP_CHANNEL, SELTRACK_CC, number))

    # --------------------------- selected clip's notes for Pad Scale (0.89)
    def _playing_clip(self):
        track = self.song().view.selected_track
        for slot in list(getattr(track, 'clip_slots', []) or []):
            if slot.has_clip and slot.clip.is_playing and getattr(slot.clip, 'is_midi_clip', True):
                return slot.clip
        return None

    def _clip_notes(self, clip):
        """[(pitch, start, end)] of the clip, re-read about once a second."""
        cached, tick, notes = self._note_cache
        if self._same(cached, clip) and self._ticks_total - tick < 10:
            return notes
        span = max(float(getattr(clip, 'length', 0) or 0), float(getattr(clip, 'loop_end', 0) or 0)) + 1.0
        notes = []
        if hasattr(clip, 'get_notes_extended'):
            for n in clip.get_notes_extended(0, 128, 0.0, span):
                if not getattr(n, 'mute', False):
                    notes.append((n.pitch, n.start_time, n.start_time + n.duration))
        else:
            for pitch, start, duration, _velocity, mute in clip.get_notes(0.0, 0, span, 128):
                if not mute:
                    notes.append((pitch, start, start + duration))
        self._note_cache = (clip, self._ticks_total, notes)
        return notes

    def _send_clip_notes(self):
        active = set()
        try:
            clip = self._playing_clip()
            if clip is not None:
                pos = float(clip.playing_position)
                active = set(p for p, a, b in self._clip_notes(clip) if a <= pos < b)
        except Exception as error:
            self.log_message('KamoteKeys: clip notes: %s' % error)
        for pitch in sorted(self._clip_notes_sent - active):
            self._send_midi((0x80 | CLIP_CHANNEL, pitch, 0))
        for pitch in sorted(active - self._clip_notes_sent):
            self._send_midi((0x90 | CLIP_CHANNEL, pitch, 100))
        self._clip_notes_sent = active

    def _send_all(self):
        self._send_midi((0xB0 | CLIP_CHANNEL, HELLO_CC, PROTOCOL))
        self._send_midi((0xB0 | CLIP_CHANNEL, SCENE_CC, min(self._scene, 127)))
        self._send_midi((0xB0 | CLIP_CHANNEL, TRACKS_CC, min(self._track, 127)))
        self._send_midi((0xB0 | CLIP_CHANNEL, ARRANGE_CC, int(self._arrange)))
        self._send_tempo(force=True)
        self._send_highlight(force=True)
        self._send_names(force=True)
        self._clip_notes_sent = set()     # the keyboard may have restarted: re-send what sounds
        for pad in range(TRACKS * SCENES):
            self._send_state(pad, force=True)
        self._step.send_all()
        self._board_sent = None
        self._refresh_clipboard()

    def update_display(self):
        ControlSurface.update_display(self)
        # Polling catches every change: mouse, other controllers, follow
        # actions, project loads and track insertion or reordering.
        self._ticks += 1
        if self._ticks >= FULL_REFRESH_TICKS:
            self._ticks = 0
            self._send_all()             # also re-greets a reconnected KamoteKeys
            return
        for pad in range(TRACKS * SCENES):
            self._send_state(pad)
        self._send_tempo()
        self._send_highlight()
        self._send_names()
        self._ticks_total += 1
        self._send_clip_notes()
        self._poll_recording()
        if self._step.active and time.time() - self._step_seen > STEP_TIMEOUT:
            self.log_message('KamoteKeys: no Step Mode heartbeat, leaving Step Mode')
            self._step.set_active(False)     # keyboard gone or switched port: never stay stuck in Step Mode
        self._step.update()              # playhead and changes only

    def refresh_state(self):
        ControlSurface.refresh_state(self)
        self._send_all()

    # ---------------------------------------------------------- transport
    def _transport(self, cc):
        song = self.song()
        if cc == CC_START:
            song.start_playing()
        elif cc == CC_STOP:
            song.stop_playing()
        elif cc == CC_CAPTURE:
            if hasattr(song, 'capture_midi') and getattr(song, 'can_capture_midi', True):
                song.capture_midi()
        elif cc == CC_UNDO:
            if song.can_undo:
                song.undo()
        elif cc == CC_REDO:
            if song.can_redo:
                song.redo()
        elif cc == CC_PLAY_PAUSE:
            # 0.77: like the spacebar; 0.99: a global stop: every clip stops
            # too (Stop All Clips), so the next play starts from nothing.
            if song.is_playing:
                self._stop_everything()
            else:
                song.start_playing()
                self._status('KamoteKeys: play')
        elif cc == CC_FINISH:
            self._finish_and_next()
        elif cc in (CC_SOLO, CC_MUTE, CC_ARM_ADD):
            self._toggle_track(CC_ARM if cc == CC_ARM_ADD else cc)
        elif cc == CC_ARM:
            self._arm_only_selected()
        elif cc == CC_CLIP_PLAY:
            self._play_selected()
        elif cc == CC_CLIP_DELETE:
            self._delete_selected()
        elif cc == CC_QUANTIZE:
            self._quantize_selected()
        elif cc == CC_METRONOME:
            song.metronome = not song.metronome
            self._status('KamoteKeys: metronome %s' % ('on' if song.metronome else 'off'))
        elif cc == CC_SCENE_DOWN:
            self._scroll(1)
        elif cc == CC_SCENE_UP:
            self._scroll(-1)
        elif cc == CC_SCENE_LAUNCH:
            self._launch_scene()
        elif cc in (CC_CLIP_UP, CC_CLIP_DOWN):
            self._move_clip(-1 if cc == CC_CLIP_UP else 1)
        elif cc in (CC_TRACKS_LEFT, CC_TRACKS_RIGHT):
            self._move_tracks(-1 if cc == CC_TRACKS_LEFT else 1)
        elif cc == CC_RECORD:
            # Session Record: records new clips in armed tracks at the selected
            # scene, or overdubs the playing clips; a second press stops it.
            # 0.91: with a fixed length, an empty selected slot on an armed
            # track records exactly that many bars and then loops by itself.
            # 0.99: pressed again while a new clip records: it records on to
            # the end of the bar (whole bars) and then loops; Session Record
            # turns off once it has finished, so nothing gets overdubbed.
            if self._finish_recording():
                pass
            elif not self._fixed_record():
                song.session_record = not song.session_record
        elif cc == CC_CLIP_DUPLICATE:
            self._duplicate_selected()
        elif cc == CC_LOOP_DOUBLE:
            self._double_selected()

    def disconnect(self):
        self._step.disconnect()
        for pad in range(TRACKS * SCENES):
            self._send_midi((0xB0 | CLIP_CHANNEL, STATE_CC + pad, EMPTY))
        self._send_midi((0xB0 | CLIP_CHANNEL, HELLO_CC, 0))
        ControlSurface.disconnect(self)
