# SPDX-License-Identifier: GPL-3.0-only
"""KamoteKeys drum step sequencer (Step Mode) for Ableton Live 12.

The keyboard enters Step Mode when CHORD is held (FM-1: LFO). It then sends,
on MIDI channel 14 (must match core/kamotkeys/kk_config.h):

  Note 36..51   step pads 1..16 (press / release)
  Note 60..76   function keys, see F_* below (press / release)
  CC 127        127 = Step Mode started (bind and send everything), 0 = ended

and this module answers on channel 14:

  CC 80..95     colour code per pad (C_* below; the firmware draws the colours)
  CC 96..102    page, page count, resolution, follow, status, drum view,
                active drum note

Every step is written straight into the selected session clip with the
Live 12 note API (add_new_notes / get_notes_extended / remove_notes_extended
/ apply_note_modifications), so the result is an ordinary MIDI clip. An empty
selected slot gets a new 1-bar clip named after the track. Notes edited with
the mouse show up on the pads (notes listener).

Layout (16 pads, so pages): the 16 pads are 16 steps of the current page.
Hold DRUM and the pads become 16 Drum Rack pads (bottom row = lowest notes,
like Push): tap one to make it the active sound; page keys change the bank.
"""
from __future__ import absolute_import, print_function, unicode_literals

import math

import Live

STEP_CHANNEL = 13                      # MIDI channel 14
PAD_NOTE = 36
PADS = 16
(F_DRUM, F_PAGE_PREV, F_PAGE_NEXT, F_LENGTH, F_DOUBLE, F_RESOLUTION, F_FOLLOW, F_COPY, F_CLEAR,
 F_VEL_1, F_VEL_2, F_VEL_3, F_VEL_4, F_VEL_5, F_VEL_6, F_VEL_DOWN, F_VEL_UP, F_PASTE) = range(60, 78)
FUNCTION_NOTES = tuple(range(60, 78))
DRUM_NOTE = 80                         # device -> host: notes 80..95 = drum key 0..15 of the bank (0.61)
CODE_CC, PAGE_CC, PAGES_CC, RES_CC, FOLLOW_CC, STATUS_CC, DRUMVIEW_CC, ACTIVE_CC = 80, 96, 97, 98, 99, 100, 101, 102
BANK_CC = 103                          # host -> device: first note of the drum bank
MODE_CC = 127

# Pad colour codes (KK_STEPC_* in the firmware).
C_OUTSIDE, C_EMPTY, C_EMPTY_BEAT, C_NOTE_1 = 0, 1, 2, 3
C_TIE, C_PLAY_EMPTY, C_PLAY_NOTE, C_HELD = 9, 10, 11, 12
C_DRUM_EMPTY, C_DRUM_LOADED, C_DRUM_ACTIVE, C_DRUM_ACTIVE_EMPTY = 20, 21, 22, 23
# Status codes (CC 100).
S_OK, S_NO_DRUM_RACK, S_NO_MIDI_TRACK, S_NO_CLIP = 0, 1, 2, 3

# Step resolutions in beats; the same resolution is used for every page.
RESOLUTIONS = ((0.25, '1/16'), (0.5, '1/8'), (0.125, '1/32'), (1.0 / 6.0, '1/16T'), (1.0 / 3.0, '1/8T'))
VELOCITIES = (30, 50, 70, 90, 110, 127)
EPS = 1e-4
MAX_LENGTH = 4.0 * 64                  # beats: 64 bars of 4/4 is plenty


def _velocity_code(velocity):
    return C_NOTE_1 + min(5, int(velocity) * 6 // 128)


class StepSequencer(object):
    """Owned by the KamoteKeys control surface; all calls come from its MIDI
    handlers (allowed to change Live) or from Live listeners (which only mark
    state dirty or schedule work for later with schedule_message)."""

    def __init__(self, surface):
        self._surface = surface
        self._active = False
        self._listeners = []
        self._track = self._slot = self._clip = self._rack = None
        self._status = S_NO_MIDI_TRACK
        self._pitch = 36               # active drum (note)
        self._bank = 36                # first note of the Drum Rack bank shown
        self._page = 0
        self._res = 0
        self._follow = False
        self._velocity = 100           # velocity for new steps
        self._held = []                # step pads held, in press order: [step index on page]
        self._consumed = set()         # held steps already used by a tie / velocity change
        self._fn = set()               # function keys held
        self._notes = []               # cached notes of the clip (all pitches)
        self._dirty = True
        self._bind_pending = False
        self._sent = [None] * PADS
        self._info_sent = {}
        self._armed = None             # track this mode armed (disarmed again on exit)
        self._used = set()             # COPY / PASTE / CLEAR held and already used with another key
        self._clipboard = None         # (lane pitch or None, [(pitch, offset, duration, velocity, mute)])

    # ------------------------------------------------------------ helpers
    def _log(self, text):
        self._surface.log_message('KamoteKeys step: ' + text)

    def _say(self, text):
        self._surface._status('KamoteKeys step: ' + text)
        self._log(text)

    def _song(self):
        return self._surface.song()

    def _send(self, cc, value):
        self._surface._send_midi((0xB0 | STEP_CHANNEL, cc, max(0, min(127, int(value)))))

    def _defer(self, callback):
        """Run later, outside the notification that asked for it (Live forbids
        changes from inside listeners)."""
        self._surface.schedule_message(1, callback)

    def _listen(self, subject, name, callback):
        try:
            if not getattr(subject, name + '_has_listener')(callback):
                getattr(subject, 'add_' + name + '_listener')(callback)
            self._listeners.append((subject, name, callback))
        except Exception as error:
            self._log('cannot listen to %s: %s' % (name, error))

    def _unlisten_all(self):
        for subject, name, callback in self._listeners:
            try:
                if getattr(subject, name + '_has_listener')(callback):
                    getattr(subject, 'remove_' + name + '_listener')(callback)
            except Exception:
                pass                   # the object was deleted: nothing to remove
        self._listeners = []

    @property
    def active(self):
        return self._active

    # ------------------------------------------------------- mode / binding
    def set_active(self, on):
        if on and not self._active:
            self._active = True
            self._log('Step Mode started')
            self._bind(create=True)
            self.send_all()
        elif not on and self._active:
            self._active = False
            self._disarm()
            self._unbind()
            self._held, self._fn, self._consumed, self._used = [], set(), set(), set()
            self._log('Step Mode ended')

    def _unbind(self):
        self._unlisten_all()
        self._track = self._slot = self._clip = self._rack = None

    def _schedule_bind(self):
        if self._active and not self._bind_pending:
            self._bind_pending = True
            self._defer(self._deferred_bind)

    def _deferred_bind(self):
        self._bind_pending = False
        if self._active:
            self._bind(create=True)
            self.send_all()

    def _bind(self, create):
        """Attach to the selected track, its highlighted slot and its Drum
        Rack. create: make a 1-bar clip in an empty slot (only ever called from
        a MIDI handler or a deferred task, never from a listener)."""
        self._unbind()
        self._dirty = True
        song = self._song()
        self._listen(song.view, 'selected_track', self._on_selection)
        self._listen(song.view, 'selected_scene', self._on_selection)
        try:
            track = song.view.selected_track
            if track is None or not track.has_midi_input:
                self._status = S_NO_MIDI_TRACK
                self._log('selected track is not a MIDI track')
                return
            self._track = track
            self._arm(track)
            self._listen(track, 'devices', self._on_devices)
            self._rack = self._find_drum_rack(track)
            self._choose_drum()
            slot = song.view.highlighted_clip_slot
            if slot is None:
                self._status = S_NO_MIDI_TRACK
                self._log('no highlighted clip slot on %s' % track.name)
                return
            self._slot = slot
            self._listen(slot, 'has_clip', self._on_has_clip)
            if not slot.has_clip and create:
                self._create_clip()
            self._attach_clip()
        except Exception as error:
            self._log('bind failed: %s' % error)
            self._unbind()
            self._status = S_NO_MIDI_TRACK

    @staticmethod
    def _same(a, b):
        return a is not None and b is not None and getattr(a, '_live_ptr', id(a)) == getattr(b, '_live_ptr', id(b))

    def _arm(self, track):
        """Like Move/Push: the sequenced track is armed so the drum keys and
        the other keys are heard; the previous auto-armed track is released."""
        if self._same(self._armed, track):
            return
        self._disarm()
        try:
            if track.can_be_armed and not track.arm:
                track.arm = True
                self._armed = track
        except Exception as error:
            self._log('cannot arm %s: %s' % (getattr(track, 'name', 'track'), error))

    def _disarm(self):
        if self._armed is not None:
            try:
                self._armed.arm = False
            except Exception:
                pass
            self._armed = None

    def _attach_clip(self):
        self._clip = None
        if self._slot is None or not self._slot.has_clip:
            self._status = S_NO_CLIP
            return
        clip = self._slot.clip
        if not clip.is_midi_clip:
            self._status = S_NO_MIDI_TRACK
            return
        self._clip = clip
        for name in ('notes', 'loop_start', 'loop_end'):
            self._listen(clip, name, self._on_clip_changed)
        self._status = S_OK if self._rack is not None else S_NO_DRUM_RACK
        self._page = min(self._page, self._pages() - 1)
        self._log('bound to %s / %s (%.2f beats, %s)' % (
            self._track.name, clip.name, self._length(), 'Drum Rack' if self._rack else 'no Drum Rack'))

    def _create_clip(self):
        song = self._song()
        try:
            bar = 4.0 * song.signature_numerator / song.signature_denominator
        except Exception:
            bar = 4.0
        try:
            self._slot.create_clip(bar)
            self._slot.clip.name = self._track.name
            self._say('new 1-bar clip "%s"' % self._track.name)
        except Exception as error:
            self._log('cannot create a clip: %s' % error)

    @staticmethod
    def _find_drum_rack(track):
        """First Drum Rack on the track, also inside other racks."""
        def search(devices):
            for device in devices:
                if getattr(device, 'can_have_drum_pads', False):
                    return device
                for chain in getattr(device, 'chains', ()) or ():
                    found = search(chain.devices)
                    if found is not None:
                        return found
            return None
        try:
            return search(list(track.devices))
        except Exception:
            return None

    def _choose_drum(self):
        """Keep the active drum if it exists; else Live's selected Drum Rack
        pad; else the first pad with a sound."""
        if self._rack is None:
            return
        try:
            pads = self._rack.drum_pads
            if not len(pads[self._pitch].chains):
                selected = self._rack.view.selected_drum_pad
                if selected is not None and len(selected.chains):
                    self._pitch = selected.note
                else:
                    loaded = [p.note for p in self._rack.visible_drum_pads if len(p.chains)]
                    if loaded:
                        self._pitch = loaded[0]
        except Exception as error:
            self._log('cannot read the Drum Rack: %s' % error)
        self._bank = max(0, min(112, 36 + ((self._pitch - 36) // 16) * 16))   # banks of 16 from C1

    # ------------------------------------------------------------ listeners
    def _on_selection(self):
        self._schedule_bind()          # the selection changed: rebind later

    def _on_devices(self):
        self._schedule_bind()          # Drum Rack added or removed

    def _on_has_clip(self):
        self._schedule_bind()          # clip deleted or created with the mouse

    def _on_clip_changed(self):
        self._dirty = True             # redraw on the next display tick

    # -------------------------------------------------------- clip geometry
    def _step(self):
        return RESOLUTIONS[self._res][0]

    def _span(self):
        return self._step() * PADS

    def _start(self):
        return self._clip.loop_start if self._clip is not None else 0.0

    def _length(self):
        return (self._clip.loop_end - self._clip.loop_start) if self._clip is not None else 0.0

    def _pages(self):
        return max(1, int(math.ceil(self._length() / self._span() - EPS)))

    def _step_time(self, index):
        return self._start() + (self._page * PADS + index) * self._step()

    def _read_notes(self):
        self._notes = []
        if self._clip is None:
            return
        try:
            self._notes = list(self._clip.get_notes_extended(0, 128, self._clip.loop_start, self._length() + EPS))
        except Exception as error:
            self._log('cannot read notes: %s' % error)
            self._clip = None
            self._schedule_bind()      # the clip may have been deleted

    def _notes_at(self, time, pitch=None):
        pitch = self._pitch if pitch is None else pitch
        step = self._step()
        return [n for n in self._notes if n.pitch == pitch and time - EPS <= n.start_time < time + step - EPS]

    def _covered(self, time):
        """A longer note of the active drum is still sounding at this step."""
        return any(n.pitch == self._pitch and n.start_time < time - EPS and n.start_time + n.duration > time + EPS
                   for n in self._notes)

    # ---------------------------------------------------------- LED codes
    def _codes(self):
        if self._fn and F_DRUM in self._fn:
            return self._drum_codes()
        codes = []
        playhead = self._playhead_step()
        beat = max(1, int(round(1.0 / self._step())))
        end = self._start() + self._length()
        for i in range(PADS):
            t = self._step_time(i)
            notes = self._notes_at(t) if self._clip is not None else []
            if self._clip is None or t >= end - EPS:
                code = C_OUTSIDE
            elif i in self._held:
                code = C_HELD
            elif i == playhead:
                code = C_PLAY_NOTE if notes else C_PLAY_EMPTY
            elif notes:
                code = _velocity_code(max(n.velocity for n in notes))
            elif self._covered(t):
                code = C_TIE
            else:
                code = C_EMPTY_BEAT if (self._page * PADS + i) % beat == 0 else C_EMPTY
            codes.append(code)
        return codes

    def _drum_pad_note(self, pad):
        # Push-like: the bottom row (pads 9-16) holds the lower 8 notes.
        return self._bank + (pad - 8 if pad >= 8 else pad + 8)

    def _drum_codes(self):
        codes = []
        for pad in range(PADS):
            note = self._drum_pad_note(pad)
            loaded = False
            if self._rack is not None:
                try:
                    loaded = len(self._rack.drum_pads[note].chains) > 0
                except Exception:
                    loaded = False
            if note == self._pitch:
                codes.append(C_DRUM_ACTIVE if loaded or self._rack is None else C_DRUM_ACTIVE_EMPTY)
            else:
                codes.append(C_DRUM_LOADED if loaded else C_DRUM_EMPTY)
        return codes

    def _playhead_step(self):
        """Index of the playing step on the shown page, or None."""
        clip = self._clip
        try:
            if clip is None or not clip.is_playing:
                return None
            index = int((clip.playing_position - clip.loop_start) / self._step() + EPS)
        except Exception:
            return None
        page, step = divmod(index, PADS)
        if self._follow and page != self._page and 0 <= page < self._pages():
            self._page = page          # auto-follow turns the page
        return step if page == self._page else None

    def send_all(self):
        self._sent = [None] * PADS
        self._info_sent = {}
        self.update(force=True)

    def update(self, force=False):
        """Called from update_display (~10 Hz): send only what changed."""
        if not self._active:
            return
        if self._dirty or force:
            self._dirty = False
            self._read_notes()
            if self._clip is not None:
                self._page = min(self._page, self._pages() - 1 + (F_LENGTH in self._fn))
        for pad, code in enumerate(self._codes()):
            if force or code != self._sent[pad]:
                self._sent[pad] = code
                self._send(CODE_CC + pad, code)
        info = {PAGE_CC: self._page, PAGES_CC: self._pages(), RES_CC: self._res, FOLLOW_CC: int(self._follow),
                STATUS_CC: self._status, DRUMVIEW_CC: int(F_DRUM in self._fn), ACTIVE_CC: self._pitch,
                BANK_CC: self._bank}
        for cc, value in info.items():
            if force or self._info_sent.get(cc) != value:
                self._info_sent[cc] = value
                self._send(cc, value)

    # ------------------------------------------------------------- input
    def pad(self, index, pressed):
        if not self._active:
            return
        if F_DRUM in self._fn:
            if pressed:
                self._select_drum(self._drum_pad_note(index))
            return
        if pressed:
            if F_LENGTH in self._fn:
                self._set_length(self._step_time(index) + self._step() - self._start())
                return
            if self._held and self._held[0] != index:
                first = self._held[0]
                self._tie(min(first, index), max(first, index))   # hold a step, press another: one long note
                self._consumed.add(self._held[0])
                self._consumed.add(index)
            self._held.append(index)
        elif index in self._held:
            self._held.remove(index)
            if index in self._consumed:
                self._consumed.discard(index)
            else:
                self._toggle(index)
        self.update()

    def function(self, note, pressed):
        if not self._active:
            return
        if not pressed:
            self._fn.discard(note)
            if note not in self._used:          # plain taps act on release
                if note == F_CLEAR:
                    self._clear_page()
                elif note == F_COPY:
                    self._copy(None)
                elif note == F_PASTE:
                    self._paste(None)
            self._used.discard(note)
            self._dirty = True
            self.update()
            return
        self._fn.add(note)
        self._used.discard(note)
        if note in (F_PAGE_PREV, F_PAGE_NEXT):
            self._page_key(-1 if note == F_PAGE_PREV else 1)
        elif note == F_DOUBLE:
            self._double()
        elif note == F_RESOLUTION:
            self._res = (self._res + 1) % len(RESOLUTIONS)
            self._page = 0
            self._say('resolution %s' % RESOLUTIONS[self._res][1])
        elif note == F_FOLLOW:
            self._follow = not self._follow
            self._say('follow %s' % ('on' if self._follow else 'off'))
        elif F_VEL_1 <= note <= F_VEL_6:
            self._set_velocity(VELOCITIES[note - F_VEL_1])
        elif note in (F_VEL_DOWN, F_VEL_UP):
            levels = list(VELOCITIES)
            current = min(range(len(levels)), key=lambda i: abs(levels[i] - self._velocity))
            current = max(0, min(len(levels) - 1, current + (1 if note == F_VEL_UP else -1)))
            self._set_velocity(levels[current])
        self._dirty = True
        self.update()

    # ------------------------------------------------------------ editing
    def _ensure_clip(self):
        if self._clip is None and self._slot is not None and self._track is not None:
            try:
                if not self._slot.has_clip:
                    self._create_clip()
                self._attach_clip()
            except Exception as error:
                self._log('cannot make a clip: %s' % error)
        return self._clip is not None

    def _spec(self, pitch, start, duration, velocity, mute=False):
        return Live.Clip.MidiNoteSpecification(pitch=pitch, start_time=start, duration=duration,
                                               velocity=velocity, mute=mute)

    def _toggle(self, index):
        if not self._ensure_clip():
            self._say('no MIDI clip: select a MIDI track and a clip slot')
            return
        t = self._step_time(index)
        if t >= self._start() + self._length() - EPS:
            self._say('step %d is past the clip end (hold LENGTH + a step to extend)' % (self._page * PADS + index + 1))
            return
        try:
            self._read_notes()
            if self._notes_at(t):
                self._clip.remove_notes_extended(self._pitch, 1, t - EPS, self._step())
            else:
                self._clip.add_new_notes((self._spec(self._pitch, t, self._step(), self._velocity),))
        except Exception as error:
            self._log('cannot change step %d: %s' % (index + 1, error))
        self._dirty = True

    def _tie(self, first, last):
        """One long note from the first held step to the end of the last."""
        if last < first or not self._ensure_clip():
            return
        t0, t1 = self._step_time(first), self._step_time(last) + self._step()
        try:
            self._clip.remove_notes_extended(self._pitch, 1, t0 + self._step() - EPS, t1 - t0 - self._step())
            notes = list(self._clip.get_notes_extended(self._pitch, 1, t0 - EPS, self._step()))
            if notes:
                for n in notes:
                    n.duration = t1 - n.start_time
                self._clip.apply_note_modifications(notes)
            else:
                self._clip.add_new_notes((self._spec(self._pitch, t0, t1 - t0, self._velocity),))
            self._say('tie steps %d-%d' % (self._page * PADS + first + 1, self._page * PADS + last + 1))
        except Exception as error:
            self._log('cannot tie: %s' % error)
        self._dirty = True

    def _set_velocity(self, velocity):
        targets = [i for i in self._held]
        if not targets:
            self._velocity = velocity
            self._say('new steps: velocity %d' % velocity)
            return
        if not self._ensure_clip():
            return
        try:
            for index in targets:
                t = self._step_time(index)
                notes = list(self._clip.get_notes_extended(self._pitch, 1, t - EPS, self._step()))
                if notes:
                    for n in notes:
                        n.velocity = velocity
                    self._clip.apply_note_modifications(notes)
                else:
                    self._clip.add_new_notes((self._spec(self._pitch, t, self._step(), velocity),))
                self._consumed.add(index)
            self._say('velocity %d' % velocity)
        except Exception as error:
            self._log('cannot set velocity: %s' % error)
        self._dirty = True

    def _set_length(self, length):
        if not self._ensure_clip():
            return
        length = max(self._step(), min(MAX_LENGTH, length))
        end = self._clip.loop_start + length
        try:
            if end > self._clip.end_marker:
                self._clip.end_marker = end
                self._clip.loop_end = end
            else:
                self._clip.loop_end = end
                self._clip.end_marker = end
            self._say('clip length %.2f bars' % (length / 4.0))
        except Exception as error:
            self._log('cannot set the length: %s' % error)
        self._dirty = True

    def _double(self):
        """Like Push's Double Loop: twice as long, contents copied."""
        if not self._ensure_clip():
            return
        if self._length() * 2 > MAX_LENGTH:
            self._say('clip is already %d bars' % int(self._length() / 4))
            return
        try:
            self._clip.duplicate_loop()
            self._say('doubled: %.2f bars' % (self._length() / 4.0))
        except Exception as error:
            self._log('cannot double: %s' % error)
        self._dirty = True

    def _clear_page(self):
        if self._clip is None:
            return
        t = self._step_time(0)
        try:
            self._clip.remove_notes_extended(self._pitch, 1, t - EPS, self._span())
            self._say('cleared page %d' % (self._page + 1))
        except Exception as error:
            self._log('cannot clear: %s' % error)
        self._dirty = True

    def _copy_page(self, target):
        """Copy every note of the shown page (all drums) onto another page."""
        if self._clip is None or target < 0 or target == self._page:
            return
        span, start = self._span(), self._start()
        src, dst = start + self._page * span, start + target * span
        try:
            notes = list(self._clip.get_notes_extended(0, 128, src - EPS, span))
            if dst + span > self._clip.loop_end + EPS:
                self._set_length(dst + span - start)
            self._clip.remove_notes_extended(0, 128, dst - EPS, span)
            if notes:
                self._clip.add_new_notes(tuple(self._spec(n.pitch, n.start_time - src + dst, n.duration, n.velocity, n.mute)
                                               for n in notes))
            self._say('copied page %d to %d' % (self._page + 1, target + 1))
            self._page = target
        except Exception as error:
            self._log('cannot copy the page: %s' % error)
        self._dirty = True

    def _page_key(self, delta):
        if F_DRUM in self._fn:
            self._bank = max(0, min(112, self._bank + 16 * delta))
            self._say('drum bank %d-%d' % (self._bank, self._bank + 15))
        elif F_COPY in self._fn:
            self._used.add(F_COPY)
            self._copy_page(self._page + delta)
        else:
            limit = self._pages() - 1 + (F_LENGTH in self._fn)   # one page past the end while LENGTH is held
            self._page = max(0, min(limit, self._page + delta))
            self._say('page %d/%d' % (self._page + 1, self._pages()))

    def drum_key(self, index):
        """A drum key (0..15 of the bank) was pressed: like a drum pad on
        Move, it selects that sound for the steps (the keyboard plays it).
        With CLEAR held it deletes every note of that sound in the clip."""
        if not self._active or not 0 <= index < PADS:
            return
        note = self._bank + index
        self._log('drum key %d -> note %d' % (index, note))
        if F_CLEAR in self._fn:
            self._used.add(F_CLEAR)
            self._clear_drum(note)
        elif F_COPY in self._fn:
            self._used.add(F_COPY)
            self._copy(note)
        elif F_PASTE in self._fn:
            self._used.add(F_PASTE)
            self._paste(note)
        else:
            self._select_drum(note)
        self.update()

    # Copy / paste (0.61, like Move's Copy button): COPY copies the shown page
    # (all drums) or, with a drum key, that drum's lane on the page; PASTE
    # puts it on the shown page (a lane goes to the drum key held, else to the
    # active drum).
    def _copy(self, lane):
        if self._clip is None:
            return
        span, src = self._span(), self._step_time(0)
        try:
            if lane is None:
                notes = self._clip.get_notes_extended(0, 128, src - EPS, span)
            else:
                notes = self._clip.get_notes_extended(lane, 1, src - EPS, span)
            self._clipboard = (lane, [(n.pitch, n.start_time - src, n.duration, n.velocity, n.mute) for n in notes])
            self._say('copied %s of page %d' % ('all drums' if lane is None else 'drum %d' % lane, self._page + 1))
        except Exception as error:
            self._log('cannot copy: %s' % error)

    def _paste(self, target):
        if self._clipboard is None:
            self._say('nothing copied yet')
            return
        if not self._ensure_clip():
            return
        lane, notes = self._clipboard
        span, dst = self._span(), self._step_time(0)
        try:
            if lane is None and target is None:
                self._clip.remove_notes_extended(0, 128, dst - EPS, span)
                specs = [self._spec(p, dst + o, d, v, m) for p, o, d, v, m in notes]
            else:
                pitch = target if target is not None else (self._pitch if lane is not None else None)
                source = [n for n in notes if lane is not None or n[0] == self._pitch]
                self._clip.remove_notes_extended(pitch, 1, dst - EPS, span)
                specs = [self._spec(pitch, dst + o, d, v, m) for p, o, d, v, m in source]
            if dst + span > self._clip.loop_end + EPS:
                self._set_length(dst + span - self._start())
            if specs:
                self._clip.add_new_notes(tuple(specs))
            self._say('pasted onto page %d' % (self._page + 1))
        except Exception as error:
            self._log('cannot paste: %s' % error)
        self._dirty = True

    def _clear_drum(self, note):
        if self._clip is None:
            return
        try:
            self._clip.remove_notes_extended(note, 1, -1000.0, 1e6)
            self._say('cleared drum %d' % note)
        except Exception as error:
            self._log('cannot clear the drum: %s' % error)
        self._dirty = True

    def _select_drum(self, note):
        self._pitch = note
        name = 'note %d' % note
        if self._rack is not None:
            try:
                pad = self._rack.drum_pads[note]
                self._rack.view.selected_drum_pad = pad
                name = pad.name or name
            except Exception as error:
                self._log('cannot select the drum pad: %s' % error)
        self._say('drum: %s' % name)
        self._dirty = True

    def disconnect(self):
        self._unbind()
        self._active = False
