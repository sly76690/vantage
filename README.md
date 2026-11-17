# OCTANT — synthétiseur polyphonique 8 voix (dossier VSTPOLY8)

Poly analogique 8 voix à deux VCO par voix, avec Poly-Mod, passe-bas 4 pôles et passe-haut.
Projet issu d'Orbit 106 (même base JUCE, même façade, même installeur) : le cahier des charges
est dans le document « VSTPOLY8 — cahier des charges ».

## Règles

- Principes publics uniquement : aucune façade, aucun nom ni son d'usine d'un instrument existant.
- Pas de « Prophet » ni de « Sequential » dans le produit.

## Construire (macOS)

```
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
TMPDIR="$PWD/.local-cache" cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
bash tools/paquet.sh
python3 tools/verify-package.py build/Octant-0.2.0.pkg
```

## État (v0.2.0, moteur Octant)

- **Deux VCO par voix** avec un **SHAPE continu** (triangle → dent de scie → carré → impulsion étroite), sync, cross-mod.
- **MIXER** : balance VCO 1 / VCO 2, SUB (carré une octave sous le VCO 1), NOISE, VCO PWM.
- **POLY-MOD** : enveloppe du filtre et VCO 1 (dosages bipolaires) vers FREQ 2, PW 2 et FILTER (les deux filtres), voix par voix.
- **38 sons d'usine** Octant (`python3 tools/sons.py`), programmés de zéro, dont 15 sons DARK (darksynth) et 7 sons avec séquence (appuyer sur PLAY).
- Le DCO hérité d'Orbit reste dans le moteur (paramètres cachés) pour relire les anciens patchs.
- **Ladder** : au-dessus de RES 9 il auto-oscille (sinus sur FREQ) ; en dessous, courbe d'Orbit inchangée. HPF : passe-haut 2 pôles sans résonance (fader HPF du FILTER A).
- Façade : bandeaux gris acier, texte noir ; logo O8 bleu et gris clair.
- **SEQUENCER** (section ARPEGGIO / SEQUENCER) : REC, puis une note ou un accord par pas (REST = silence), 64 pas maximum ; PLAY la lance toute seule ; une touche tenue la transpose. REC comme un looper : 1er appui REC (rouge), 2e appui OVD (bleu : la séquence joue et les notes jouées s'y ajoutent), 3e appui fin de l'overdub. Dans EDIT, les notes de l'overdub sont en bleu sur une seconde ligne. EDIT : grille des pas (clic sur un pas puis une note/accord qui le remplace ou s'y ajoute avec ADD ; REST, INSERT, DELETE, + pour ajouter un pas). La séquence est enregistrée avec le patch, et 8 mémoires (menu MEMORY : LOAD / STORE, fichiers Presets/Sequences) la gardent indépendamment des sons.
- **LIVE (scène)** : LIVE allumé, la séquence garde le son qu'elle avait au moment où LIVE (ou PLAY) a été lancé — voix, LFO, effets et volume figés — et le clavier joue le son du panneau par-dessus : on peut changer de son (un pad, un lead) sans toucher à la boucle. LIVE éteint, tout revient au son du panneau (la queue du son figé s'éteint naturellement). Changer de son pendant que la séquence tourne garde la séquence, le tempo et les touches du séquenceur. **Enregistrement en direct quantisé** : REC puis PLAY (ou REC pendant PLAY) : boucle vierge de 16 pas, chaque note sur le pas le plus proche.
- **EFFECTS** : chorus (OFF / I / II / I+II, CH MIX), PHASER (RATE, DEPTH, FDBK, PH MIX) et DRIVE de sortie ; le DRIVE des voix est dans MIXER.
- **DELAY** : TIME libre jusqu'à 1,5 s, ou SYNC sur une valeur de note (1/2 à 1/16, pointées D et triolets T) jouée exactement jusqu'à 4 s (une blanche jusqu'à 30 BPM). **REVERB** : trois types. **ARPEGGIO** : UP, DOWN, UP/DOWN, RANDOM, pas de 1/4 à 1/32.
- **Presets** : un nom déjà pris est refusé, sauf remplacement demandé explicitement.
- Tests : suite audio, interface, presets, Poly-Mod et `Tests/LiveRegression.h` (transitions LIVE / OVD, référence de transposition, sauvegarde du son figé, rappel audio et plafond de sortie).

## Version auditée 0.2.0

- Binaires universels **Apple Silicon + Intel**, cible minimale **macOS 11**.
- Les sessions conservent le son figé de la séquence indépendamment du son du clavier.
- Sortie de LIVE / OVD : relâchement des notes directes, sans notes bloquées.
- L’overdub conserve la référence de transposition au rechargement ; MERGE la recalcule.
- Protection du niveau après addition des deux moteurs. Les queues LIVE peuvent durer jusqu’à 30 s, avec une extinction progressive à la limite.
- Le paquet est produit dans `build/Octant-0.2.0.pkg`. Il installe les formats AU / VST3, l’application et les 38 sons Octant. La fabrication du paquet ne l’installe pas.
- Les binaires sont signés ad hoc ; le paquet n’est pas signé Developer ID ni notarisé.
- Moteur : un bloc audio plus grand que prévu par le DAW est joué en morceaux (aucune allocation pendant la lecture) ; passer de 6 à 8 voix laisse les notes finir sur leur release au lieu de les couper ; le delay synchronisé joue la valeur choisie jusqu'à 4 s (`Tests/EngineFixes.h`).
- La banque *Midnight Signals* (sons DCO d'Orbit) est retirée d'Octant : elle reste dans Orbit 106.
