# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
"""COPY / PASTE key logic shared by Clip Mode and Step Mode (0.65).

COPY: tap (or hold), then press what to copy: it replaces the clipboard.
PASTE: tap, then the next target press pastes once; hold PASTE and every
target pressed meanwhile gets pasted. Tapping an armed key again cancels."""


class CopyPaste(object):
    COPY_ARMED, PASTE_ARMED = 32, 64      # flags added to the clipboard kind on the screen

    def __init__(self):
        self.reset()

    def reset(self):
        self.copy_armed = self.paste_armed = False
        self.copy_held = self.paste_held = False
        self.copy_used = self.paste_used = False

    def copy_key(self, pressed):
        if pressed:
            if self.copy_armed and not self.copy_held:
                self.copy_armed = False                       # second tap: cancel
                return
            self.copy_armed, self.copy_held, self.copy_used, self.paste_armed = True, True, False, False
        else:
            self.copy_held = False
            if self.copy_used:
                self.copy_armed = False

    def paste_key(self, pressed):
        if pressed:
            if self.paste_armed and not self.paste_held:
                self.paste_armed = False
                return
            self.paste_armed, self.paste_held, self.paste_used, self.copy_armed = True, True, False, False
        else:
            self.paste_held = False
            if self.paste_used:
                self.paste_armed = False

    def target(self):
        """'copy', 'paste' or None: what the next pad / key press does."""
        return 'copy' if self.copy_armed else 'paste' if self.paste_armed else None

    def done(self, action):
        if action == 'copy':
            self.copy_used = True
            if not self.copy_held:
                self.copy_armed = False
        else:
            self.paste_used = True
            if not self.paste_held:
                self.paste_armed = False

    def flags(self):
        return (self.COPY_ARMED if self.copy_armed else 0) | (self.PASTE_ARMED if self.paste_armed else 0)
