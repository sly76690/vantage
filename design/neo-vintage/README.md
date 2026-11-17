# Orbit 106 — référence Neo Vintage

`reference-approved.png` est la maquette validée. Le rendu réel doit être comparé à cette image, pas à une nouvelle génération de maquette.

## Matières intégrées

`Source/Assets/neo-knob.png` : sprite de potentiomètre, créé avec l'outil intégré imagegen, fond transparent. Essai conservé pour référence uniquement : le rendu final utilise des composants dessinés avec une lumière commune, sans charger ce sprite. Le repère de valeur reste vectoriel et interactif. Le corps ne tourne pas : l'éclairage reste cohérent.

Prompt de génération :

> Use case: product-mockup. Asset type: isolated photorealistic synthesizer rotary knob UI sprite on genuinely transparent background. ONE knob only at exact center, top-down orthographic view, a perfect circular satin warm brushed aluminum flat cap with a tiny fine beveled rim, nearly no black skirt visible, thin dark sidewall and soft realistic shadow cast 3 pixels down-right relative to a 48 pixel knob. Closely match elegant vintage 1970s studio synth knob: softly warm silver ivory metal, NOT chrome, no starburst, no concentric decorations, no screws, NO pointer, NO index line, NO text, NO markings or surrounding scale. Top face softly lit from upper-left, natural fine directional brush texture, subtle broad soft highlight. Knob fills 75 percent of square canvas width, centered exactly, generous transparent margin contains the full soft shadow. Body is stationary; pointer will be drawn separately in software. Premium photoreal material but restrained dimension, no perspective tilt, no surrounding panel. Transparent pixels outside the knob and soft shadow.

## Adaptations fonctionnelles explicites

La maquette générée invente plusieurs commandes de l'oscillateur II et omet des paramètres réels. L'interface conserve les vrais paramètres : pas d'ajout fictif de SUB/NOISE séparés pour OSC II, conservation du VCA, de HOLD, de la vélocité, du modèle A, de la polarité d'enveloppe, du bend filtre et des divisions/octaves de l'arpégiateur.

Les légendes, touches et repères restent dessinés et liés aux paramètres ; aucune photo complète ne remplace les contrôles.

## Atlas de référence intégré

Le panneau approuvé est embarqué via `OrbitReference`. `ReferenceMaterials` prélève les capuchons de faders, les touches et le corps de potentiomètre directement dans cette image. Le texte des touches et le repère fixe du potentiomètre sont remplacés en mémoire, sans modifier le fichier de référence. Le masque circulaire enlève le fond autour du potentiomètre. Les bindings audio sont inchangés.
