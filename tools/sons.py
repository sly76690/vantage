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
BASE = {"mode": 1, "jupiterMix": 1, "osc1Shape": 6, "osc2Shape": 6, "envLink": 0, "chorusMode": 0,
        "filterAType": 0, "slope": 1, "hpfFreq": .27, "age": .35, "voiceWidth": .6}

SOUNDS = {
    # ---- brass and keys
    "BRASS - Vantage Brass": dict(BASE, vco1Wave=.33, vco2Wave=.33, detune=7, cutoff=650, resonance=.12, env=.55, keytrack=.5,
        attack=.04, decay=.8, sustain=.8, release=.35, fattack=.09, fdecay=.9, fsustain=.45, frelease=.4, velFilter=.3,
        atFilter=.35, atVolume=.25, wheelDepth=.3, reverbMix=.12, gain=.52),
    "BRASS - Fanfare Stabs": dict(BASE, vco1Wave=.36, vco2Wave=.33, detune=11, cutoff=900, resonance=.15, env=.6,
        attack=.004, decay=.35, sustain=.3, release=.2, fattack=.002, fdecay=.28, fsustain=.15, frelease=.2, velFilter=.4,
        atFilter=.3, delaySync=6, delayFeedback=.25, delayMix=.12, gain=.95),
    "KEYS - Glass Clav": dict(BASE, vco1Wave=.92, vco2Wave=.85, detune=4, cutoff=1900, resonance=.3, env=.5, keytrack=.6,
        attack=.002, decay=.55, sustain=.2, release=.25, fattack=.001, fdecay=.25, fsustain=.1, frelease=.2, velFilter=.5, atFilter=.3,
        phaserOn=1, phaserRate=.6, phaserDepth=.5, phaserFeedback=.4, phaserMix=.4, gain=.9),
    "KEYS - Soft Poly": dict(BASE, vco1Wave=.2, vco2Wave=.45, detune=6, cutoff=1500, resonance=.1, env=.35, keytrack=.5,
        attack=.003, decay=1.4, sustain=.45, release=.6, fattack=.002, fdecay=.7, fsustain=.3, frelease=.5, velFilter=.4,
        chorusMode=1, chorus=.3, reverbMix=.15, atFilter=.25, gain=1.0),
    # ---- POLY-MOD
    "BELL - Poly Mod Bells": dict(BASE, oscLayer=1, vco1Wave=0, vco2Wave=0, osc1Octave=4, osc2Octave=3, polyOsc=.42, polyEnv=.18,
        cutoff=7000, resonance=.08, env=.15, attack=.002, decay=2.6, sustain=0, release=2.2, fattack=.001, fdecay=1.4, fsustain=0, frelease=1.5,
        velFilter=.3, atLfo=.2, lfoPitch=.08, rate=4.5, reverbType=2, reverbSize=.7, reverbMix=.25, delaySync=5, delayFeedback=.3, delayMix=.12),
    "BELL - Clock Tower": dict(BASE, oscLayer=.85, vco1Wave=.1, vco2Wave=0, osc1Octave=2, osc2Octave=4, osc2Tune=-5, polyOsc=.6, polyEnv=.3,
        cutoff=5000, resonance=.15, env=.2, attack=.002, decay=4.0, sustain=0, release=4.0, fattack=.001, fdecay=2.5, fsustain=0, frelease=3.0,
        reverbType=2, reverbSize=.9, reverbMix=.3, gain=.65),
    "LEAD - Sync Rider": dict(BASE, oscLayer=1, oscSync=1, vco1Wave=.33, vco2Wave=.33, osc2Tune=5, polyEnv=.55,
        cutoff=3200, resonance=.2, env=.25, attack=.003, decay=.6, sustain=.85, release=.3, fattack=.001, fdecay=.9, fsustain=.25, frelease=.4,
        voiceMode=1, legato=1, portamento=.05, wheelDepth=.4, lfoDelay=.5, rate=5.5, atFilter=.25, delaySync=5, delayFeedback=.35, delayMix=.15),
    "LEAD - Poly Growl": dict(BASE, vco1Wave=.4, vco2Wave=.66, detune=6, polyOsc=.22, polyFilter=1, polyFreq=0,
        cutoff=1100, resonance=.5, env=.45, attack=.004, decay=.5, sustain=.7, release=.25, fattack=.003, fdecay=.6, fsustain=.35, frelease=.3,
        voiceDrive=.6, voiceMode=1, legato=1, portamento=.06, atDrive=.5, atFilter=.25, gain=.85),
    "FX - Metal Rain": dict(BASE, oscLayer=.8, vco1Wave=.66, vco2Wave=0, osc2Octave=5, polyOsc=.7, polyFilter=1, vcoNoise=.2,
        cutoff=2400, resonance=.4, env=.3, attack=.01, decay=1.8, sustain=.3, release=2.5, fattack=.005, fdecay=1.5, fsustain=.2, frelease=2.0,
        atLfo=.3, rate=7, lfoFilter=.5, reverbType=2, reverbSize=.9, reverbMix=.32, delaySync=7, delayFeedback=.5, delayMix=.2),
    "FX - Siren Drop": dict(BASE, oscLayer=1, vco1Wave=.66, vco2Wave=.33, osc1Octave=1, polyEnv=-.8, polyOsc=.15,
        cutoff=2600, resonance=.35, env=.4, attack=.005, decay=1.2, sustain=.6, release=1.2, fattack=.001, fdecay=1.6, fsustain=0, frelease=1.0,
        voiceMode=1, delaySync=4, delayFeedback=.55, delayMix=.25, reverbMix=.2),
    # ---- the singing ladder
    "LEAD - Singing Filter": dict(BASE, vco1Wave=.33, vco2Wave=.33, detune=5, oscLayer=.5, cutoff=700, resonance=.95, env=.35, keytrack=1,
        attack=.01, decay=.8, sustain=.8, release=.4, fattack=.01, fdecay=1.2, fsustain=.4, frelease=.5, voiceMode=1, legato=1, portamento=.08,
        atFilter=.4, wheelDepth=.4, lfoDelay=.6, rate=5, delaySync=5, delayFeedback=.3, delayMix=.15, vcoSub=.35, gain=1.0),
    # ---- pads and strings
    "PAD - Phase Sky": dict(BASE, vco1Wave=.33, vco2Wave=.4, detune=12, cutoff=1800, resonance=.15, env=.2,
        attack=1.5, decay=2.0, sustain=.85, release=3.5, fattack=1.8, fdecay=3.0, fsustain=.6, frelease=3.5,
        phaserOn=1, phaserRate=.12, phaserDepth=.8, phaserFeedback=.6, phaserMix=.6, reverbType=2, reverbSize=.8, reverbMix=.25,
        atFilter=.3, atVolume=.25, wheelDepth=.25, gain=.65),
    "PAD - Night Choir": dict(BASE, vco1Wave=.66, vco2Wave=.8, detune=8, polyEnv=-.35, polyFreq=0, polyWidth=1,
        cutoff=1200, resonance=.3, env=.25, attack=1.0, decay=2.5, sustain=.8, release=3.5, fattack=2.2, fdecay=3.5, fsustain=.4, frelease=3.0,
        vcoPwm=.35, pwmSource=0, rate=.45, atFilter=.3, wheelDepth=.25, reverbType=2, reverbSize=.8, reverbMix=.26, gain=.45),
    "PAD - Slow Sweep": dict(BASE, vco1Wave=.33, vco2Wave=.2, detune=14, osc2Octave=2, polyEnv=.12, cutoff=320, resonance=.55, env=.45,
        attack=1.8, decay=3.0, sustain=.85, release=4.5, fattack=3.5, fdecay=4.0, fsustain=.3, frelease=4.0, rate=.18, lfoFilter=.3,
        atFilter=.35, atLfo=.25, reverbType=2, reverbSize=.85, reverbMix=.28, delaySync=4, delayFeedback=.4, delayMix=.12),
    "STRINGS - Eight Voices": dict(BASE, vco1Wave=.33, vco2Wave=.33, detune=13, cutoff=3200, resonance=.08, env=.12, hpfFreq=.45,
        attack=.7, decay=1.5, sustain=.9, release=2.4, fattack=.8, fdecay=2.0, fsustain=.7, frelease=2.2, chorusMode=2, chorus=.5,
        atVolume=.3, wheelDepth=.25, reverbMix=.2, gain=.5),
    # ---- basses
    "BASS - Unison Sub": dict(BASE, voiceMode=2, unisonSpread=14, vco1Wave=.33, vco2Wave=.45, vcoSub=.5, cutoff=380, resonance=.35, env=.6,
        keytrack=.4, attack=.002, decay=.5, sustain=.6, release=.15, fattack=.001, fdecay=.3, fsustain=0, frelease=.15, velFilter=.35,
        legato=1, portamento=.03, atDrive=.4, voiceWidth=.3),
    "BASS - Sync Punch": dict(BASE, oscLayer=.7, oscSync=1, osc1Octave=2, osc2Octave=2, osc2Tune=3, polyEnv=.35, vcoSub=.4,
        cutoff=900, resonance=.3, env=.5, attack=.002, decay=.4, sustain=.5, release=.12, fattack=.001, fdecay=.22, fsustain=0, frelease=.12,
        voiceMode=1, legato=1, atFilter=.25, voiceWidth=.2, gain=1.0),
    # ---- sequences (press PLAY in SEQUENCER; hold a key to transpose)
    "SEQ - Night Drive": dict(BASE, vco1Wave=.33, vco2Wave=.66, detune=5, vcoSub=.4, cutoff=420, resonance=.45, env=.55, keytrack=.4,
        attack=.002, decay=.25, sustain=.3, release=.1, fattack=.001, fdecay=.18, fsustain=0, frelease=.1, voiceMode=1,
        arpTempo=118, arpDivision=2, arpGate=.55, delaySync=8, delayFeedback=.3, delayMix=.12, atFilter=.3, gain=1.0,
        sequence="36;36;48;36;39;36;43;36;36;36;48;36;41;39;36;43"),
    "SEQ - Chord Cycle": dict(BASE, vco1Wave=.4, vco2Wave=.33, detune=9, cutoff=1300, resonance=.2, env=.4,
        attack=.005, decay=.6, sustain=.4, release=.5, fattack=.003, fdecay=.5, fsustain=.2, frelease=.4,
        arpTempo=100, arpDivision=1, arpGate=.75, chorusMode=1, chorus=.35, reverbMix=.18, delaySync=5, delayFeedback=.35, delayMix=.15,
        sequence="48,55,60,63;r;46,53,58,62;r;44,51,56,60;r;43,50,55,59;r"),
    "SEQ - Poly Mod Run": dict(BASE, vco1Wave=.2, vco2Wave=.5, osc2Octave=4, polyOsc=.3, cutoff=2200, resonance=.35, env=.4,
        attack=.002, decay=.3, sustain=.4, release=.2, fattack=.001, fdecay=.2, fsustain=0, frelease=.15,
        arpTempo=110, arpDivision=2, arpGate=.6, atFilter=.3, delaySync=6, delayFeedback=.4, delayMix=.18, reverbMix=.12, gain=1.0,
        sequence="60;67;72;67;63;70;75;70;58;65;70;65;62;69;74;69"),

    # ---- DARKSYNTH: the hard-driven 80s horror-synth style (genre traits only: driven saws, octave basslines at
    # 16ths, sync screams, gated stabs). Original patches and sequences, no melody of any record.
    "DARK - Chase Bass": dict(BASE, voiceMode=1, vco1Wave=.33, vco2Wave=.33, detune=8, vcoSub=.6, osc2Octave=2,
        cutoff=520, resonance=.4, env=.6, keytrack=.5, attack=.001, decay=.18, sustain=.35, release=.06, fattack=.001, fdecay=.14, fsustain=.1, frelease=.08,
        voiceDrive=.8, fxDrive=.35, arpTempo=124, arpDivision=2, arpGate=.5, atFilter=.3, voiceWidth=.2, gain=.8,
        sequence="30;42;30;42;30;42;30;42;28;40;28;40;33;45;31;43"),
    "DARK - Razor Lead": dict(BASE, voiceMode=2, unisonSpread=16, oscLayer=.65, oscSync=1, osc2Tune=7, polyEnv=.4, vco1Wave=.33, vco2Wave=.33,
        cutoff=2800, resonance=.25, env=.3, attack=.002, decay=.5, sustain=.8, release=.25, fattack=.001, fdecay=.7, fsustain=.3, frelease=.3,
        voiceDrive=.7, fxDrive=.3, legato=1, portamento=.04, wheelDepth=.5, lfoDelay=.4, rate=6, delaySync=5, delayFeedback=.35, delayMix=.15,
        reverbType=1, reverbMix=.15, atDrive=.4, gain=.3),
    "DARK - Neon Scream": dict(BASE, voiceMode=2, unisonSpread=22, oscLayer=.5, vco1Wave=.33, vco2Wave=.33, osc2Octave=4, detune=8,
        cutoff=3600, resonance=.18, env=.35, keytrack=.5, attack=.003, decay=.6, sustain=.9, release=.35, fattack=.002, fdecay=.5, fsustain=.5, frelease=.4,
        voiceDrive=.6, fxDrive=.35, legato=1, portamento=.06, wheelDepth=.5, lfoDelay=.5, rate=5.5, atLfo=.3, atFilter=.25,
        delaySync=5, delayFeedback=.4, delayMix=.22, reverbType=2, reverbSize=.75, reverbMix=.22, gain=.3),
    "DARK - Laser Scream": dict(BASE, voiceMode=1, oscLayer=.8, oscSync=1, vco1Wave=.33, vco2Wave=.4, osc2Tune=9, polyEnv=.8, vcoSub=.3,
        cutoff=3000, resonance=.35, env=.4, attack=.002, decay=.7, sustain=.85, release=.3, fattack=.001, fdecay=1.2, fsustain=.2, frelease=.4,
        voiceDrive=.8, fxDrive=.3, legato=1, portamento=.05, wheelDepth=.5, lfoDelay=.4, rate=6, atDrive=.4,
        phaserOn=1, phaserRate=.35, phaserDepth=.6, phaserFeedback=.5, phaserMix=.3,
        delaySync=5, delayFeedback=.45, delayMix=.2, reverbType=2, reverbSize=.8, reverbMix=.2, gain=.6),
    "DARK - Wailing Lead": dict(BASE, voiceMode=1, oscLayer=.55, vco1Wave=.33, vco2Wave=.6, osc2Octave=4, detune=6, vcoSub=.25,
        cutoff=3400, resonance=.3, env=.3, keytrack=.5, attack=.004, decay=.6, sustain=.9, release=.35, fattack=.002, fdecay=.45, fsustain=.55, frelease=.4,
        voiceDrive=.8, fxDrive=.35, legato=1, portamento=.07, lfoShape=1, rate=5.8, lfoDelay=.45, lfoPitch=.3, wheelDepth=.6, atDrive=.4, atFilter=.25,
        delaySync=5, delayFeedback=.4, delayMix=.2, reverbType=2, reverbSize=.8, reverbMix=.22, gain=.4),
    "DARK - Chrome Vibrato": dict(BASE, voiceMode=2, unisonSpread=18, vco1Wave=.33, vco2Wave=.33, detune=10, cutoff=2900, resonance=.3, env=.3,
        attack=.003, decay=.5, sustain=.9, release=.3, fattack=.001, fdecay=.4, fsustain=.6, frelease=.3,
        voiceDrive=.7, fxDrive=.3, legato=1, portamento=.05, lfoShape=1, rate=6.2, lfoDelay=.35, lfoPitch=.22, wheelDepth=.6, atDrive=.4,
        delaySync=6, delayFeedback=.35, delayMix=.18, reverbType=2, reverbSize=.75, reverbMix=.2, gain=.3),
    "PAD - Titan Wall": dict(BASE, vco1Wave=.33, vco2Wave=.33, detune=14, vcoSub=.35, cutoff=2300, resonance=.15, env=.25, keytrack=.4,
        attack=.25, decay=2.0, sustain=.9, release=2.6, fattack=.3, fdecay=2.0, fsustain=.7, frelease=2.4,
        voiceDrive=.45, fxDrive=.25, chorusMode=2, chorus=.45, lfoShape=1, rate=5.2, lfoPitch=.04, atFilter=.3, wheelDepth=.3,
        reverbType=2, reverbSize=.9, reverbMix=.3, gain=.28),
    "PAD - Black Cathedral": dict(BASE, vco1Wave=.33, vco2Wave=.55, detune=12, osc2Octave=2, cutoff=500, resonance=.3, env=.55, keytrack=.4,
        attack=.9, decay=3.0, sustain=.85, release=3.5, fattack=2.2, fdecay=3.5, fsustain=.5, frelease=3.0,
        voiceDrive=.5, phaserOn=1, phaserRate=.08, phaserDepth=.7, phaserFeedback=.55, phaserMix=.45, atFilter=.35,
        reverbType=2, reverbSize=.95, reverbMix=.35, delaySync=4, delayFeedback=.4, delayMix=.12, gain=.7),
    "BRASS - Doom Brass": dict(BASE, vco1Wave=.33, vco2Wave=.36, detune=9, vcoSub=.3, cutoff=800, resonance=.18, env=.6, keytrack=.5,
        attack=.02, decay=1.2, sustain=.85, release=.8, fattack=.06, fdecay=1.0, fsustain=.5, frelease=.7, velFilter=.35,
        voiceDrive=.65, fxDrive=.3, chorusMode=1, chorus=.25, atFilter=.35, atDrive=.3, wheelDepth=.4,
        reverbType=2, reverbSize=.8, reverbMix=.22, gain=.3),
    "DARK - Blood Stabs": dict(BASE, vco1Wave=.36, vco2Wave=.36, detune=12, cutoff=1400, resonance=.2, env=.55,
        attack=.002, decay=.22, sustain=0, release=.12, fattack=.001, fdecay=.18, fsustain=0, frelease=.1, velFilter=.4,
        voiceDrive=.6, fxDrive=.3, reverbType=1, reverbSize=.6, reverbMix=.2, delaySync=6, delayFeedback=.25, delayMix=.12, gain=1.0),
    "DARK - Low Blood Stabs": dict(BASE, vco1Wave=.36, vco2Wave=.36, detune=12, osc1Octave=2, osc2Octave=2, vcoSub=.5, cutoff=900, resonance=.25, env=.6,
        attack=.002, decay=.25, sustain=0, release=.14, fattack=.001, fdecay=.2, fsustain=0, frelease=.12, velFilter=.4,
        voiceDrive=.75, fxDrive=.35, reverbType=1, reverbSize=.5, reverbMix=.15, delaySync=6, delayFeedback=.2, delayMix=.08, voiceWidth=.4, gain=1.0),
    "DARK - Steel Blood Stabs": dict(BASE, vco1Wave=.36, vco2Wave=.5, detune=12, cutoff=1100, resonance=.7, env=.65, keytrack=.5,
        attack=.002, decay=.25, sustain=0, release=.14, fattack=.001, fdecay=.2, fsustain=0, frelease=.12, velFilter=.45,
        voiceDrive=.6, fxDrive=.3, reverbType=1, reverbSize=.6, reverbMix=.2, delaySync=6, delayFeedback=.25, delayMix=.12, gain=1.0),
    "DARK - Chase Stabs": dict(BASE, vco1Wave=.36, vco2Wave=.36, detune=12, cutoff=1400, resonance=.2, env=.55,
        attack=.002, decay=.2, sustain=0, release=.1, fattack=.001, fdecay=.16, fsustain=0, frelease=.08, velFilter=.4,
        voiceDrive=.6, fxDrive=.3, reverbType=1, reverbSize=.6, reverbMix=.18, delaySync=6, delayFeedback=.25, delayMix=.1,
        arpTempo=140, arpDivision=2, arpGate=.5, gain=1.0,
        sequence="53,56,60;r;r;53,56,60;r;r;53,56,60;r;49,53,56;r;r;49,53,56;r;r;49,53,56;r;51,55,58;r;r;51,55,58;r;r;51,55,58;r;48,51,55;r;r;48,51,55;r;48,51,55;48,51,55;r"),
    "DARK - Titan Stab": dict(BASE, voiceMode=2, unisonSpread=24, vco1Wave=.36, vco2Wave=.36, detune=12, vcoSub=.35, cutoff=1500, resonance=.2, env=.55,
        attack=.002, decay=.3, sustain=0, release=.18, fattack=.001, fdecay=.22, fsustain=0, frelease=.15, velFilter=.4,
        voiceDrive=.7, fxDrive=.35, reverbType=2, reverbSize=.7, reverbMix=.22, delaySync=6, delayFeedback=.3, delayMix=.12, gain=.7),
    "DARK - Gated Chords": dict(BASE, vco1Wave=.5, vco2Wave=.4, detune=9, cutoff=1700, resonance=.25, env=.35,
        attack=.002, decay=.12, sustain=.6, release=.05, fattack=.001, fdecay=.15, fsustain=.4, frelease=.05,
        arpTempo=124, arpDivision=2, arpGate=.4, voiceDrive=.5, chorusMode=1, chorus=.25, reverbMix=.12, gain=.9,
        sequence="54,57,61;54,57,61;r;54,57,61;52,56,59;r;52,56,59;r;50,54,57;50,54,57;r;50,54,57;49,52,56;r;49,52,56;r"),
    "DARK - Siren Sweep": dict(BASE, oscLayer=1, vco1Wave=.66, vco2Wave=.33, osc1Octave=2, polyOsc=.35, polyEnv=.6, polyFilter=1,
        cutoff=1800, resonance=.6, env=.5, attack=.01, decay=2.5, sustain=.5, release=1.5, fattack=1.5, fdecay=2.0, fsustain=.3, frelease=1.5,
        voiceDrive=.6, rate=.3, lfoFilter=.6, reverbType=2, reverbSize=.85, reverbMix=.3, delaySync=4, delayFeedback=.5, delayMix=.2, gain=.55),
    "DARK - Fog Cathedral": dict(BASE, vco1Wave=.33, vco2Wave=.66, detune=15, osc2Octave=2, cutoff=900, resonance=.35, env=.3,
        attack=2.0, decay=3.0, sustain=.9, release=5.0, fattack=3.0, fdecay=4.0, fsustain=.5, frelease=5.0, voiceDrive=.4,
        phaserOn=1, phaserRate=.08, phaserDepth=.7, phaserFeedback=.55, phaserMix=.45, reverbType=2, reverbSize=.95, reverbMix=.35, gain=.6),
    "DARK - Night Pulse": dict(BASE, vco1Wave=.8, vco2Wave=.66, detune=6, cutoff=1100, resonance=.45, env=.45,
        attack=.001, decay=.2, sustain=.2, release=.1, fattack=.001, fdecay=.16, fsustain=0, frelease=.1, voiceDrive=.5,
        arpTempo=124, arpDivision=2, arpGate=.45, delaySync=8, delayFeedback=.45, delayMix=.18, reverbMix=.15, gain=1.0,
        sequence="66;73;78;73;66;73;76;73;64;71;76;71;61;68;73;68"),
}

folder = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Presets", "Sounds")
os.makedirs(folder, exist_ok=True)
for old in glob.glob(os.path.join(folder, "*.orbitpreset")):   # the bank is rewritten as a whole
    os.remove(old)
for name, settings in SOUNDS.items():
    settings = dict(settings); sequence = settings.pop("sequence", None)
    head = '<OrbitState stateVersion="5" program="0" modified="0"%s>' % (' sequence=%s' % quoteattr(sequence) if sequence else '')
    lines = ['<?xml version="1.0" encoding="UTF-8"?>', head]
    lines += ['  <PARAM id=%s value="%s"/>' % (quoteattr(key), repr(float(value))) for key, value in settings.items()]
    lines.append('</OrbitState>')
    with open(os.path.join(folder, name + ".orbitpreset"), "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")
    print("écrit :", name)
