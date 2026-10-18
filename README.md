# Multi Burst NT

Plugin C++ pour Expert Sleepers disting NT, inspiré du principe du Multi Burst
Envelopes d'AJH Synth et de la version DROID fournie dans `ref` : deux générateurs
de 1 à 40 enveloppes, huit formes, pente d'amplitude, contrôle CV/MIDI et
synchronisation interne ou externe.

Cette version de développement **0.4** cible le firmware **1.18.0** avec le
SDK officiel **API 13**, figé dans [sdk.lock.json](sdk.lock.json). L'objet ARM
annonce explicitement ce contrat API, disponible à partir du firmware **1.15.0**.
Le chargement et le fonctionnement sur matériel restent à valider. Les courbes
constituent une reproduction indépendante ; les équations originales AJH ne
sont pas publiques.

Téléchargements de la version 0.4 :

- [Archive pour la MicroSD du NT](downloads/v0.4/multi_burst_nt.zip)
- [Pack de 16 presets pour Rack et catalogue](downloads/v0.4/multi_burst_motion_presets.zip)
- [DLL Windows pour nt_emu](downloads/v0.4/multi_burst.dll)
- [Empreintes SHA-256](downloads/v0.4/SHA256SUMS.txt)

Le dépôt [NT-Multiburst](https://github.com/kf4u43D/NT-Multiburst) contient les
sources et les en-têtes SDK nécessaires. Pour travailler depuis un autre dossier,
régénérer les patches Rack avec le chemin local de la DLL ; les fichiers exportés
conservent le chemin absolu choisi lors de leur création.

- Sorties CV : enveloppe, gate, activité, fin de séquence et horloge.
- Sorties MIDI : notes par burst, enveloppe par CC et clock à 24 PPQN.
- Sources : cadence libre, BPM interne, clock CV, clock MIDI, cadence interne + CV.
- Reset CV/MIDI/manuellement, modes ONE/RET et départ immédiat ou sur horloge.
- Deux générateurs, focus, trigger/stop/step communs ou indépendants.
- Loop, mute, glide asymétrique, niveaux, progression et mix d'enveloppes.
- Horloge musicale commune, mode Auto et 16 rapports de durée/intervalle.
- 16 presets par générateur, sons usine DROID et mémoires utilisateur.
- Six entrées de modulation partagées, profondeurs indépendantes par générateur.
- Réponse CV bipolaire optionnelle : variations continues des temps en mode absolu.
- Morphing continu entre les huit formes, en complément de leur sélection discrète.
- Pack de 16 scènes à deux voix, accessible dans la page Preset pack.
- Modulation des paramètres par les mappings CV/MIDI natifs du NT.

G2 est désactivé par défaut ; les 42 anciens indices de paramètres et le mode
temporel absolu sont conservés. Les sons usine utilisent le nouveau mode musical.
La [comparaison DROID/NT](docs/reference-comparison.md) décrit les adaptations
et les différences de convention.

Pour retrouver les variations de largeur/cadence et les horloges combinées mises
en avant par AJH, essayer le [patch Motion](patches/Multi_Burst_NT-motion.vcv) et
ses [instructions](patches/README.md). Il combine deux voix en temps libres,
trois CV bipolaires lents et le morphing ; les sons usine DROID gardent leur grille.

Le [pack de 16 scènes](docs/preset-pack.md) couvre percussions, ratchets, flams,
glides et phrases ternaires. Choisir Pack preset puis Load pack preset, et lancer
Trigger all. Les mémoires utilisateur et les routages sont conservés. Les
16 patches Rack et leur catalogue sont dans `dist/multi_burst_motion_presets.zip`.

[Mode d'emploi](docs/user-guide.md) · [Recherche et sources](docs/research.md) ·
[Développement et vérification](docs/development.md)

Pour les essais sur ordinateur : [patch VCV Rack avec nt_emu](patches/Multi_Burst_NT-nt_emu.vcv)
et [instructions du patch](patches/README.md). La bibliothèque Windows associée
se trouve dans `dist/nt_emu/multi_burst.dll`.

Le [rapport de validation locale](docs/validation.md) détaille les tests et le
format de l'objet livré.

Le livrable local est `dist/multi_burst_nt.zip`, à extraire à la racine de la
MicroSD. L'objet se trouve sous `programs/plug-ins/multi_burst.o`.
Les tests natifs et l'inspection ARM sont décrits dans la documentation de
développement. Le chargement, les tensions et la charge CPU restent à vérifier
sur un disting NT physique.

Pour actualiser les téléchargements versionnés après les compilations et
l'export des presets : `python scripts/prepare-downloads.py`.
