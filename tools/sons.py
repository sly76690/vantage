# VANTAGE - writes the factory sounds shipped with the installer (menu SOUNDS) into Presets/Sounds.
# Each sound only lists what differs from the default settings: Vantage fills in the rest when it loads it.
# Every sound is programmed from scratch on Vantage by sound type: no factory data or patch of any other
# instrument is copied. A sound can carry a sequence (SEQ ...): press PLAY in SEQUENCER to hear it.
# Usage:  python3 tools/sons.py      then  bash tools/paquet.sh
import os, glob
from xml.sax.saxutils import quoteattr

# Shapes: 0 triangle, .33 saw, .66 square, 1 narrow pulse. Ranges: 0 = 64' ... 3 = 8' ... 5 = 2'.
# oscLayer: 0 = VCO 1 only, 1 = VCO 2 only. POLY-MOD: polyEnv / polyOsc (-1..1) into polyFreq / polyWidth / polyFilter.
# Sequence text: steps separated by ';', notes by ',', 'r' = rest (MIDI note numbers, 60 = C4).
# Vantage: the multimode filter (filterAType 2) at 12 dB by default; filterMode 0 = low-pass, .5 = notch, 1 = high-pass;
# filterBP = band-pass. chordOn + chord ("0,4,7": intervals above the key) = CHORD memory. voiceSpread 0 alternate, 1 pitch, 2 random.
BASE = {"mode": 1, "jupiterMix": 1, "osc1Shape": 6, "osc2Shape": 6, "envLink": 0, "chorusMode": 0,
        "filterAType": 2, "slope": 0, "hpfFreq": .27, "age": .45, "voiceWidth": .8}
SAW = dict(vco1Wave=.33, vco2Wave=.33)

SOUNDS = {
    # ---- brass: the wide 12 dB low-pass opening on the filter envelope
    "BRASS - Vantage Brass": dict(BASE, **SAW, detune=8, cutoff=520, resonance=.18, env=.55, keytrack=.5,
        attack=.03, decay=1.0, sustain=.85, release=.45, fattack=.08, fdecay=.9, fsustain=.5, frelease=.45, velFilter=.3,
        atFilter=.35, wheelDepth=.3, reverbType=1, reverbMix=.14, gain=.5),
    "BRASS - Arena Stabs": dict(BASE, **SAW, detune=11, cutoff=700, resonance=.2, env=.65,
        attack=.004, decay=.5, sustain=.55, release=.25, fattack=.003, fdecay=.35, fsustain=.25, frelease=.22, velFilter=.4,
        voiceDrive=.45, reverbType=1, reverbSize=.6, reverbMix=.16, gain=.6),
    "BRASS - Soft Horns": dict(BASE, vco1Wave=.33, vco2Wave=.5, detune=6, cutoff=380, resonance=.12, env=.45, keytrack=.6,
        attack=.12, decay=1.4, sustain=.8, release=.7, fattack=.35, fdecay=1.2, fsustain=.55, frelease=.7,
        atFilter=.4, atVolume=.25, chorusMode=1, chorus=.3, reverbMix=.18, gain=0.8),
    "BRASS - Hollow Notch": dict(BASE, **SAW, detune=9, cutoff=900, resonance=.45, env=.4, filterMode=.5,
        attack=.02, decay=.9, sustain=.75, release=.4, fattack=.05, fdecay=.8, fsustain=.4, frelease=.4,
        atFilter=.3, reverbMix=.15, gain=.55),
    # ---- pads
    "PAD - Wide Strings": dict(BASE, **SAW, detune=13, cutoff=1900, resonance=.1, env=.15, hpfFreq=.4,
        attack=.8, decay=1.5, sustain=.9, release=2.4, fattack=.9, fdecay=2.0, fsustain=.7, frelease=2.2,
        age=.6, chorusMode=2, chorus=.45, atVolume=.3, wheelDepth=.25, reverbType=2, reverbSize=.8, reverbMix=.24, gain=0.55),
    "PAD - Notch Drift": dict(BASE, **SAW, detune=10, cutoff=1100, resonance=.55, env=.1, filterMode=.5,
        attack=1.2, decay=2.0, sustain=.9, release=3.0, rate=.15, lfoFilter=.9, lfoShape=1,
        chorusMode=1, chorus=.3, reverbType=2, reverbSize=.85, reverbMix=.28, gain=.55),
    "PAD - Band Choir": dict(BASE, vco1Wave=.4, vco2Wave=.66, detune=7, cutoff=850, resonance=.7, env=.15, filterBP=1,
        attack=1.0, decay=2.0, sustain=.85, release=2.8, fattack=1.6, fdecay=2.5, fsustain=.6, frelease=2.5,
        vcoPwm=.3, rate=.4, chorusMode=2, chorus=.4, reverbType=2, reverbSize=.8, reverbMix=.26, gain=.6),
    "PAD - Highpass Air": dict(BASE, **SAW, detune=12, cutoff=700, resonance=.3, env=.2, filterMode=1, vcoNoise=.12,
        attack=1.5, decay=2.0, sustain=.9, release=3.5, fattack=2.0, fdecay=3.0, fsustain=.5, frelease=3.0,
        phaserOn=1, phaserRate=.1, phaserDepth=.7, phaserFeedback=.5, phaserMix=.5, reverbType=2, reverbSize=.9, reverbMix=.3, gain=0.8),
    "PAD - Eight Rivers": dict(BASE, **SAW, detune=9, cutoff=1300, resonance=.2, env=.25, age=.9, voiceWidth=1,
        attack=.6, decay=2.0, sustain=.85, release=2.5, fattack=1.2, fdecay=2.5, fsustain=.55, frelease=2.2,
        reverbType=2, reverbSize=.85, reverbMix=.26, delaySync=5, delayFeedback=.35, delayMix=.12, gain=0.6),
    # ---- chord memory: one key, the whole chord (CHORD is on)
    "CHORD - Minor Seventh Stabs": dict(BASE, **SAW, detune=9, cutoff=800, resonance=.22, env=.6, chordOn=1, chord="0,3,7,10",
        attack=.003, decay=.35, sustain=.2, release=.25, fattack=.002, fdecay=.28, fsustain=.1, frelease=.2, velFilter=.4,
        voiceDrive=.4, reverbType=1, reverbMix=.16, delaySync=6, delayFeedback=.25, delayMix=.1, gain=.4),
    "CHORD - Power Fifths": dict(BASE, **SAW, detune=10, vcoSub=.3, cutoff=650, resonance=.2, env=.55, chordOn=1, chord="0,7,12",
        attack=.005, decay=.8, sustain=.6, release=.3, fattack=.004, fdecay=.5, fsustain=.3, frelease=.3,
        voiceDrive=.6, fxDrive=.25, reverbMix=.14, gain=.4),
    "CHORD - Sus Horizon": dict(BASE, **SAW, detune=11, cutoff=1200, resonance=.25, env=.25, chordOn=1, chord="0,5,7,12",
        attack=.7, decay=2.0, sustain=.85, release=2.5, fattack=1.0, fdecay=2.0, fsustain=.6, frelease=2.2,
        chorusMode=2, chorus=.35, reverbType=2, reverbSize=.85, reverbMix=.28, gain=.4),
    "CHORD - Major Ninth Keys": dict(BASE, vco1Wave=.4, vco2Wave=.5, detune=6, cutoff=1500, resonance=.15, env=.4, chordOn=1, chord="0,4,7,11,14",
        attack=.003, decay=.9, sustain=.35, release=.5, fattack=.002, fdecay=.6, fsustain=.25, frelease=.4, velFilter=.4,
        chorusMode=1, chorus=.3, reverbMix=.18, gain=0.7),
    # ---- keys
    "KEYS - Bright Poly": dict(BASE, **SAW, detune=7, cutoff=1400, resonance=.18, env=.45, keytrack=.6,
        attack=.003, decay=1.2, sustain=.4, release=.5, fattack=.002, fdecay=.7, fsustain=.3, frelease=.45, velFilter=.45,
        chorusMode=1, chorus=.25, reverbMix=.15, gain=.6),
    "KEYS - Band Clav": dict(BASE, vco1Wave=.85, vco2Wave=.7, detune=4, cutoff=1300, resonance=.55, env=.5, filterBP=1, keytrack=.7,
        attack=.002, decay=.45, sustain=.15, release=.2, fattack=.001, fdecay=.25, fsustain=.1, frelease=.18, velFilter=.5,
        phaserOn=1, phaserRate=.5, phaserDepth=.5, phaserFeedback=.4, phaserMix=.35, gain=0.9),
    # ---- basses
    "BASS - Round Twelve": dict(BASE, **SAW, detune=5, vcoSub=.65, voiceDrive=.55, fxDrive=.2, cutoff=280, resonance=.3, env=.6, keytrack=.4, voiceMode=1, legato=1, portamento=.03,
        attack=.002, decay=.5, sustain=.6, release=.15, fattack=.001, fdecay=.3, fsustain=.1, frelease=.15, velFilter=.35,
        voiceWidth=.2, gain=1.0),
    "BASS - Unison Wall": dict(BASE, **SAW, detune=9, vcoSub=.4, voiceMode=2, unisonSpread=16, cutoff=420, resonance=.25, env=.5,
        attack=.002, decay=.6, sustain=.55, release=.2, fattack=.001, fdecay=.4, fsustain=.15, frelease=.18, legato=1,
        voiceDrive=.5, voiceWidth=.5, gain=.35),
    "BASS - Band Pulse": dict(BASE, vco1Wave=.75, vco2Wave=.66, osc2Octave=2, cutoff=500, resonance=.6, env=.55, filterBP=1, voiceMode=1, legato=1,
        attack=.002, decay=.35, sustain=.4, release=.12, fattack=.001, fdecay=.25, fsustain=.1, frelease=.12,
        voiceDrive=.75, fxDrive=.3, voiceWidth=.2, gain=1.0),
    # ---- leads
    "LEAD - Sync Horn": dict(BASE, oscLayer=.8, oscSync=1, vco1Wave=.33, vco2Wave=.33, osc2Tune=7, polyEnv=.45, cutoff=1800, resonance=.25, env=.35,
        attack=.01, decay=.7, sustain=.85, release=.35, fattack=.005, fdecay=.8, fsustain=.4, frelease=.35, voiceMode=1, legato=1, portamento=.05,
        lfoShape=1, rate=5.8, lfoDelay=.45, lfoPitch=.25, wheelDepth=.5, delaySync=5, delayFeedback=.35, delayMix=.15, reverbMix=.14, gain=1.0),
    "LEAD - Singing Band": dict(BASE, **SAW, detune=6, cutoff=1100, resonance=.8, env=.35, filterBP=1, voiceMode=1, legato=1, portamento=.07,
        attack=.01, decay=.8, sustain=.85, release=.4, fattack=.01, fdecay=1.0, fsustain=.5, frelease=.4,
        lfoShape=1, rate=5.5, lfoDelay=.5, lfoPitch=.3, atFilter=.4, delaySync=5, delayFeedback=.4, delayMix=.18, reverbType=2, reverbMix=.2, gain=1.0),
    "LEAD - Unison Solo": dict(BASE, **SAW, detune=10, voiceMode=2, unisonSpread=18, cutoff=2200, resonance=.25, env=.3,
        attack=.003, decay=.5, sustain=.9, release=.3, fattack=.001, fdecay=.4, fsustain=.6, frelease=.3, legato=1, portamento=.05,
        voiceDrive=.6, fxDrive=.25, lfoShape=1, rate=6, lfoDelay=.35, lfoPitch=.2, wheelDepth=.6, delaySync=6, delayFeedback=.35, delayMix=.16, reverbMix=.18, gain=.3),
    # ---- sequences and effects (SEQ: press PLAY)
    "SEQ - Chord Steps": dict(BASE, **SAW, detune=9, cutoff=900, resonance=.25, env=.55, chordOn=1, chord="0,7,12",
        attack=.003, decay=.25, sustain=0, release=.15, fattack=.002, fdecay=.2, fsustain=0, frelease=.12,
        arpTempo=118, arpDivision=2, arpGate=.5, reverbMix=.16, delaySync=5, delayFeedback=.35, delayMix=.15, gain=.55,
        sequence="45,52,57;r;45,52,57;r;r;45,52,57;r;43,50,55;r;43,50,55;r;r;41,48,53;r;43,50,55;r"),
    "SEQ - Notch Pulse": dict(BASE, **SAW, detune=6, vcoSub=.3, cutoff=650, resonance=.5, env=.6, filterMode=.35,
        attack=.002, decay=.2, sustain=.2, release=.1, fattack=.001, fdecay=.16, fsustain=0, frelease=.1, rate=.12, lfoFilter=.6,
        arpTempo=122, arpDivision=2, arpGate=.55, voiceMode=1, delaySync=5, delayFeedback=.4, delayMix=.18, gain=1.0,
        sequence="33;33;45;33;40;33;43;45;33;33;45;33;38;40;43;45"),
    "FX - Filter Wind": dict(BASE, oscLayer=1, vcoNoise=.9, vco1Wave=0, vco2Wave=0, cutoff=900, resonance=.85, env=.3, filterBP=1,
        attack=1.5, decay=3.0, sustain=.8, release=3.5, fattack=2.5, fdecay=3.0, fsustain=.4, frelease=3.0, rate=.08, lfoFilter=1.5, lfoShape=1,
        reverbType=2, reverbSize=.95, reverbMix=.35, gain=.6),
}

folder = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Presets", "Sounds")
os.makedirs(folder, exist_ok=True)
for old in glob.glob(os.path.join(folder, "*.orbitpreset")):   # the bank is rewritten as a whole
    os.remove(old)
for name, settings in SOUNDS.items():
    settings = dict(settings); sequence = settings.pop("sequence", None); chord = settings.pop("chord", None)
    extra = (' sequence=%s' % quoteattr(sequence) if sequence else '') + (' chord=%s' % quoteattr(chord) if chord else '')
    head = '<OrbitState stateVersion="5" program="0" modified="0"%s>' % extra
    lines = ['<?xml version="1.0" encoding="UTF-8"?>', head]
    lines += ['  <PARAM id=%s value="%s"/>' % (quoteattr(key), repr(float(value))) for key, value in settings.items()]
    lines.append('</OrbitState>')
    with open(os.path.join(folder, name + ".orbitpreset"), "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")
    print("écrit :", name)
