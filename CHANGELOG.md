# Changelog (KamoteKeys for FM-1)

## KamoteKeys 0.48-FM1 (development snapshot, untested on hardware)

- First FM-1 edition of KamoteKeys 0.48, on FM1 OPEN V14.
- Split keyboard in Normal and Key Scale Mode: white keys 01-09 are the chord pads (MAJ MIN DIM SUS4 | 7 MAJ7 6 9 | LOCK), the black keys between them are CC 102-107 buttons, keys G#-G play.
- Pad Scale (SEL), Drum (FX) and Clip Mode (EDIT): white keys 01-16 are pads 1-16; key LEDs show pad state (sounding notes, Pad Scale roots, clip playing/queued/recording).
- Buttons: ENV Sustain, ARP, SEQ Key Scale, LFO settings / Bluetooth / diagnostics, PLAY Start, GLO Stop, REC Session Record, SAVE Capture MIDI, HOME Undo (HOME + SAVE Redo).
- Knobs replace the SMK-37 faders: SELECT scale, ALGORITHM root, PRESETS fixed velocity (FM-1 keys have no velocity); saved with the other settings.
- KamoteKeys settings stored in a new record after FM1 OPEN's session settings (journaled; older journals still recover).
