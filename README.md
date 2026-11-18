# VANTAGE — synthétiseur polyphonique 8 voix (dossier VSTOB)

Poly analogique 8 voix dans l'esprit des grands polys à filtre multimode, avec son propre design.
Projet issu d'Octant (même base JUCE, même séquenceur, mêmes effets, même installeur).

## Règles

- Principes publics uniquement : aucune façade, aucun nom ni son d'usine d'un instrument existant.
- Pas de « OB », « Oberheim » ni de nom de modèle d'une autre marque dans le produit.

## Construire (macOS)

```
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
TMPDIR="$PWD/.local-cache" cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
bash tools/paquet.sh
python3 tools/verify-package.py build/Vantage-0.1.0.pkg
```

## État (v0.1.0)

- **Filtre MULTI** (filtre A, par défaut) : 12 dB à variables d'état, potard **LP N HP** continu (passe-bas → coupe-bande → passe-haut), touche **BP** (passe-bande), pente 12 / 24 dB, résonance douce. LADDER et SVF restent disponibles. Le filtre B peut aussi être en MULTI (passe-bas).
- **Voix** : SPREAD (ALT / PITCH / RAND) à côté de WIDTH place les voix dans la stéréo ; **AGE** écarte davantage les voix (accord, filtre, enveloppes, LFO). HOLD et CHORD sont à gauche du clavier.
- **MODULATION** : POLY-MOD (sources FILT ENV et VCO 1, bipolaires) vers 6 destinations : FREQ 2, PW 2, SHAPE 2, FILTER, MODE (mode du filtre MULTI) et BP (mélange vers le passe-bande). LFO à 6 formes (TRI, SIN, SAW, RAMP, SQR, S&H) vers PITCH (VCO 1+2, VCO 1 ou VCO 2 : P>VCO), MODE, AMP (trémolo), filtre et PWM. Pression vers FILT, MODE, AMP, LFO et DRV.
- **CHORD** (mémoire d'accord) : tenir un accord et appuyer sur CHORD l'enregistre (8 notes max) ; chaque touche joue ensuite l'accord. Seul, CHORD allume ou éteint. L'accord est sauvegardé avec les sessions et les presets ; arpège et séquenceur reçoivent l'accord entier.
- **24 sons d'usine** Vantage (`python3 tools/sons.py`), programmés de zéro : cuivres, nappes, accords (CHORD), claviers, basses, leads, séquences (appuyer sur PLAY) et effets.
- Repris d'Octant : deux VCO par voix avec SHAPE continu, POLY-MOD, mixer, deux filtres routables, séquenceur 64 pas (REC / OVD / EDIT / LIVE, mémoires), arpège, effets (chorus, phaser, drive), delay, reverb.
- Façade : logo V8, sections de jeu en violet, sections de synthèse en gris acier.
- Tests : suite audio, interface, presets, et `Tests/Multimode.h`, `Tests/VoiceModules.h`, `Tests/ChordMemory.h`.
