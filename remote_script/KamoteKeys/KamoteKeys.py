# SPDX-License-Identifier: GPL-3.0-only
"""KamoteKeys Remote Script for Ableton Live.

Protocol (must match core/kamotkeys/kk_config.h in the firmware):

  device -> Live   ch 16  Note 36..51        Clip Mode pads 1..16 (press / release)
                   ch 16  Note 52..67        pads 1..16 with OVERDUB held (1st black key)
                   ch 16  Note 68..83        pads 1..16 with DELETE held (2nd black key)
                   ch 16  CC 127 = 127       "send the full state" (sync request)
                   ch 15  CC 20/21/22/24/25/27  Start, Stop, Capture MIDI, Undo,
                                             Session Record, Redo (127 press, 0 release)
                   ch 15  CC 23 / 26         Play/Pause, Metronome on/off
  Live -> device   ch 16  CC 80..95 = state  clip state of pads 1..16
                   ch 16  CC 126 = 1         protocol hello (script connected)

Clip states: 0 empty, 1 stopped, 2 playing, 3 queued, 4 recording,
5 queued to record, 6 empty slot on an armed track.

Pads 1-8 are Clip 1 (first scene) of tracks 1-8, pads 9-16 are Clip 2.
Only these messages are claimed by the script; keys and pads on channels
1 and 10 still reach the tracks normally.
"""
from __future__ import absolute_import, print_function, unicode_literals

from _Framework.ControlSurface import ControlSurface
from _Framework.ButtonElement import ButtonElement
from _Framework.InputControlElement import MIDI_NOTE_TYPE, MIDI_CC_TYPE

PROTOCOL = 1
CLIP_CHANNEL = 15            # MIDI channel 16 (zero based)
TRANSPORT_CHANNEL = 14       # MIDI channel 15
NOTE_BASE = 36
OVERDUB_BASE = NOTE_BASE + 16
DELETE_BASE = NOTE_BASE + 32
STATE_CC = 80
HELLO_CC = 126
SYNC_CC = 127
TRACKS = 8
SCENES = 2
CC_START, CC_STOP, CC_CAPTURE, CC_UNDO, CC_RECORD, CC_REDO = 20, 21, 22, 24, 25, 27
CC_PLAY_PAUSE, CC_METRONOME = 23, 26
TRANSPORT_CCS = (CC_START, CC_STOP, CC_CAPTURE, CC_UNDO, CC_RECORD, CC_REDO, CC_PLAY_PAUSE, CC_METRONOME)

EMPTY, STOPPED, PLAYING, QUEUED, RECORDING, QUEUED_RECORD, EMPTY_ARMED = range(7)
FULL_REFRESH_TICKS = 50      # update_display runs ~10 Hz: full resend every ~5 s


class KamoteKeys(ControlSurface):

    def __init__(self, c_instance):
        ControlSurface.__init__(self, c_instance)
        self._sent = [None] * (TRACKS * SCENES)
        self._ticks = 0
        self._received = False
        # Standard _Framework controls (as in Ableton's own controller
        # scripts): Live registers them and delivers their messages here.
        with self.component_guard():
            self._pads = []
            for base in (NOTE_BASE, OVERDUB_BASE, DELETE_BASE):
                for pad in range(TRACKS * SCENES):
                    button = ButtonElement(True, MIDI_NOTE_TYPE, CLIP_CHANNEL, base + pad)
                    button.add_value_listener(self._pad_value, identify_sender=True)
                    self._pads.append(button)
            self._sync = ButtonElement(True, MIDI_CC_TYPE, CLIP_CHANNEL, SYNC_CC)
            self._sync.add_value_listener(self._sync_value)
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
        if note >= DELETE_BASE:
            self._delete(note - DELETE_BASE)
        elif note >= OVERDUB_BASE:
            self._overdub(note - OVERDUB_BASE)
        else:
            self._launch(note - NOTE_BASE)

    def _sync_value(self, value):
        self._first_message()
        if value:
            self.log_message('KamoteKeys: sync request from the keyboard')
            self._send_all()

    def _transport_value(self, value, sender):
        self._first_message()
        if value:                        # act on press; release is ignored
            self._transport(sender.message_identifier())

    # -------------------------------------------------------------- clips
    def _slot(self, pad):
        tracks = list(self.song().visible_tracks)
        index, scene = pad % TRACKS, pad // TRACKS
        if index >= len(tracks):
            return None, None
        track = tracks[index]
        slots = list(track.clip_slots)
        if scene >= len(slots):
            return track, None
        return track, slots[scene]

    def _launch(self, pad):
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
            elif clip.is_playing or clip.is_triggered:
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

    def _send_all(self):
        self._send_midi((0xB0 | CLIP_CHANNEL, HELLO_CC, PROTOCOL))
        for pad in range(TRACKS * SCENES):
            self._send_state(pad, force=True)

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
            # Pause keeps the position: stop when playing, continue when stopped.
            if song.is_playing:
                song.stop_playing()
            else:
                song.continue_playing()
            self._status('KamoteKeys: %s' % ('pause' if not song.is_playing else 'play'))
        elif cc == CC_METRONOME:
            song.metronome = not song.metronome
            self._status('KamoteKeys: metronome %s' % ('on' if song.metronome else 'off'))
        elif cc == CC_RECORD:
            # Session Record: records new clips in armed tracks at the selected
            # scene, or overdubs the playing clips; a second press stops it.
            song.session_record = not song.session_record

    def disconnect(self):
        for pad in range(TRACKS * SCENES):
            self._send_midi((0xB0 | CLIP_CHANNEL, STATE_CC + pad, EMPTY))
        self._send_midi((0xB0 | CLIP_CHANNEL, HELLO_CC, 0))
        ControlSurface.disconnect(self)
