# Développement

## Récupérer les sources et les dépendances

Le dépôt [NT-Multiburst](https://github.com/kf4u43D/NT-Multiburst) contient les
sources du plugin, ses tests, les scripts de compilation et les presets.

```powershell
git clone https://github.com/kf4u43D/NT-Multiburst.git
cd NT-Multiburst
```

Un instantané minimal du SDK officiel est inclus dans
`third_party/distingNT_API` : ses cinq en-têtes et sa
[licence MIT](../third_party/distingNT_API/LICENSE), sans historique Git ni
exemples. Il provient du commit `f440d20c2a96a9ca43892c7418a1d8185ad47eb5`
du 3 août 2026, avec l'API 13. Aucun sous-module ni téléchargement supplémentaire
n'est nécessaire pour compiler le plugin ou exécuter les tests natifs.

`sdk.lock.json` conserve le commit amont, le contrat API et l'empreinte SHA-256
de `api.h` après normalisation des fins de ligne en LF. Avant la compilation
ARM, `scripts/build.ps1` contrôle l'API et cette empreinte. Si le répertoire du
SDK contient aussi un checkout Git complet, son commit est vérifié également.
L'inspection de l'objet contrôle les imports contre `api.h` et les signatures
JSON exactes déclarées dans `serialisation.h`.

Les dépôts amont complets restent utiles pour consulter leurs exemples ou
travailler sur l'émulateur. Ils peuvent être clonés à côté du projet, sans
modifier l'instantané utilisé par les compilations :

```powershell
git clone https://github.com/expertsleepersltd/distingNT_API.git ../distingNT_API-complet
git -C ../distingNT_API-complet checkout --detach f440d20c2a96a9ca43892c7418a1d8185ad47eb5
git clone https://github.com/thorinside/nt_emu.git ../nt_emu
git -C ../nt_emu checkout --detach ad2aa4b610f39c32903e3b7403cf2f2335508ca1
git clone https://github.com/thorinside/nt_mcp.git ../nt_mcp
git -C ../nt_mcp checkout --detach 3047bb232d9487fe075d48d12f48115383adf728
```

Les ressources `nt_mcp` et son skill
`claude-plugin/skills/disting-nt-cpp-plugin-writer/SKILL.md` ont servi de
documentation pendant le développement ; ils ne sont pas des dépendances de
compilation. Le clone de `nt_emu` n'est pas utilisé par le build de la DLL du
plugin, qui se connecte à l'émulateur installé. Les PDF des manuels ne sont pas
redistribués ; leurs liens officiels figurent dans [research.md](research.md).

Le 15 octobre 2026, l'audit a corrigé l'import initial du SDK de développement
API 14 : cette API a été introduite le 29 septembre, après la sortie du firmware
stable 1.18.0. L'objet ARM fixe désormais explicitement son contrat à
`kNT_apiVersion13` au lieu de suivre `kNT_apiVersionCurrent`. L'API 13 existe
depuis le firmware 1.15.0 ; la cible de validation reste 1.18.0. Le [rapport de
validation](validation.md) suit la recompilation et les contrôles des artefacts
après correction ; le chargement sur NT physique reste à effectuer.

Les livrables publiés sont regroupés dans
[`downloads/v0.4`](../downloads/v0.4/). Les répertoires `build` et `dist` restent
des sorties locales régénérables, exclues du suivi Git. La référence DROID
utilisée pour les sons usine est conservée sous `ref/droid-v2.3.1.ini`.

## Architecture

`include/multi_burst/engine.h` et `src/engine.cpp` définissent un moteur indépendant
de l'hôte. `src/multi_burst.cpp` porte le contrat du SDK : métadonnées, paramètres,
bus, MIDI et affichage. `include/multi_burst/parameters.h` fige les indices des
paramètres et les divisions.

Depuis la version 0.2, deux moteurs partagent `MusicalClock` et possèdent chacun
un `MusicalTiming` pour leur rapport d'intervalle. Le mode absolu historique reste
disponible. `include/multi_burst/presets.h` décrit les valeurs usine extraites
de `ref/droid-v2.3.1.ini` et les valeurs propres aux mémoires de générateur.
`include/multi_burst/performance_presets.h` ajoute les 16 scènes du pack 0.4,
avec leurs deux voix et les réglages communs, indépendamment des banques DROID.
Les [écarts de référence](reference-comparison.md) explicitent les conventions
préservées et l'adaptation de la détection Auto aux bus NT.

L'instance est construite par placement new dans la DTC demandée à l'hôte.
Le calcul audio ne réserve pas de mémoire dynamique. Les événements UI passent
par des indicateurs atomiques ; les callbacks MIDI et le moteur s'exécutent sur
le thread audio. Les états d'affichage sont des instantanés atomiques.

Les 32 mémoires utilisateur utilisent un stockage atomique fixe. Leur lecture
audio est bornée : en cas de modification concurrente, un rappel conserve les
paramètres en place. La sérialisation hors audio attend un instantané cohérent.
Le JSON versionné contient des enregistrements plats de 27 nombres (voix,
emplacement et 25 champs), compatibles avec le parseur nt_emu 2.0.2. Un document
invalide ne remplace pas partiellement les banques. Tous les CV d'un échantillon
sont lus avant les écritures, même lorsque des bus d'entrée/sortie se recouvrent.

Les formes utilisent des polynômes à coût borné et des phases entières Q48 ;
les conversions et divisions de temporisation sont recalculées quand leurs
entrées changent. Il n'y a pas de dépendance CMSIS ni de fonctions trigonométriques
ou exponentielles de bibliothèque dans le calcul.

Les logarithmes DROID optionnels utilisent une réduction d'intervalle et une
série de longueur fixe. Le glide lisse uniquement le gain de pente. Les valeurs
de count, slope et vélocité restent capturées au trigger ; le mute laisse les
événements et la progression actifs. L'EOC conserve sa largeur pendant une boucle.

La version 0.3 ajoute le paramètre global `CvResponse` au dernier indice et étend
le champ de courbe existant à `Legacy / DROID / Morph`. Aucun champ de banque
n'est ajouté. Le morphing évalue deux formes à la même phase et interpole leur
résultat. La réponse temporelle bipolaire utilise une approximation bornée de
`2^x` sur ±4 octaves ; aucun appel libm n'est introduit. En mode Absolute, les
CV de largeur et cadence conservent leurs fractions et phases. Le mode Musical
garde ses rapports quantifiés ; le fondu Interval CV se calcule dans la position
normalisée avant le choix Off.

Le pack 0.4 ajoute `PackPreset` et `LoadPackPreset` aux indices 167 et 168. Son
chargement applique les deux sons et les réglages communs sur le thread audio,
puis réinitialise les séquences sans les déclencher. Les événements de trigger,
step et sauvegarde déjà en attente sont annulés pour ce chargement. Les routages,
le focus et les banques utilisateur restent en place. Aucun format JSON de banque
n'est modifié. L'export Rack évalue le même en-tête C++ pour produire les 16 patches
et le catalogue ; les valeurs des sons ne sont pas dupliquées en Python.

Les callbacks SDK rendent les horloges MIDI au niveau du bloc, sans timestamp
d'échantillon. La clock CV est détectée par échantillon ; l'émission MIDI dépend
ensuite de la mise en file par le firmware. Ne pas assimiler une mesure native
à une mesure de jitter matériel.

## Compiler et tester sous Windows

Prérequis : Python 3, Arm GNU Toolchain (`arm-none-eabi-g++` et binutils),
Clang++ avec runtime C++ Windows ou MinGW. Les outils disponibles lors de cette
création étaient ARM GCC 14.3 et LLVM Clang 22.1.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/test.ps1
clang-tidy -p build/native src/engine.cpp src/multi_burst.cpp
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1
```

Le premier script compile et exécute les suites moteur et adaptation SDK.
Il enregistre aussi les arguments réels dans `build/native/compile_commands.json`
pour l'analyse statique. `.clang-tidy` documente les exceptions propres au SDK
et aux arrondis positifs bornés.
Le second produit l'objet ARM relogeable, l'inspecte puis prépare l'archive
MicroSD. Les avertissements sont traités comme erreurs.

Sorties :

- `build/native/engine_tests.exe` et `plugin_tests.exe` ;
- `build/arm/object_report.json` avec sections, symboles et empreinte ;
- `dist/programs/plug-ins/multi_burst.o` ;
- `dist/multi_burst_nt.zip`, contenant exactement ce chemin ;
- `dist/build_manifest.json` avec outil et SHA-256.

L'objet est compilé Cortex-M7, Thumb, FPU `fpv5-d16`, hard float, `-fPIC`, `-Os`,
sans RTTI, exceptions ni runtime d'initialisation statique. Deux unités sont
liées par `-r -nostdlib` pour conserver un objet relogeable. Le vérificateur exige
ELF32 little endian ARM REL EABI5, ABI VFP et un `pluginEntry` global.
Il refuse les imports imprévus ; `_GLOBAL_OFFSET_TABLE_` est le pseudo-symbole
du linker GNU pour le code PIC. Les fonctions de l'hôte utilisées sont contrôlées
contre le SDK figé ; les routines mémoire C autorisées restent visibles dans le
rapport.

Les méthodes C++ JSON utilisées sont vérifiées par signature exacte après
démanglage et contre `serialisation.h` du SDK figé ; ce contrôle n'autorise
aucun autre import C++. Le bridge Windows résout explicitement ces neuf méthodes
dans les exports de l'émulateur pour le rappel des banques.

## Contrats à préserver

Le GUID est `MbNt`. Les paramètres sont persistés par indice : ajouter les
nouveaux à la fin et changer seulement leur présentation dans les pages.
Préserver également le contrat API 13 explicite de l'objet ARM : une montée de
SDK ne doit pas modifier à elle seule l'API annoncée à l'hôte.
Les sorties Add/Replace s'appliquent à chaque bus ; None doit être traité avant
`bus - 1`. Le dernier bus légal est donné par `kNT_lastBus`, actuellement 64.
Reset gagne sur les événements simultanés. Les notes sortantes conservent leur
ancienne destination jusqu'au Note Off, même si les paramètres changent.

Les profils de courbe exacts et les détails non publiés du matériel AJH sont
documentés comme choix indépendants dans `research.md`.

## Niveaux de preuve

Le banc VCV Rack est décrit dans `patches/README.md`. La compilation desktop
utilise `emu/multi_burst_compat.h` et `emu/nt_emu_bridge.cpp` ; le moteur et
l'adaptateur firmware restent les mêmes sources. Pour nt_emu 2.0.2, la DLL
annonce l'API 8 minimale nécessaire à ses callbacks et limite les routages à
28 bus. L'objet ARM annonce explicitement l'API 13 et conserve 64 bus.

Le bridge résout les exports Windows de nt_emu et remet les écritures de
paramètres et leurs callbacks sur l'instance concernée. Cette adaptation est
contenue dans le projet ; elle n'utilise aucun fichier d'un autre plugin.
L'export global de fréquence de nt_emu 2.0.2 annonce 48 kHz ; le patch doit être
joué à cette fréquence. Les destinations MIDI matérielles du NT sont envoyées
au port MIDI choisi dans l'émulateur.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build-emu.ps1
python scripts/create-rack-patch.py
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/test-rack.ps1
```

Les essais Rack nécessitent VCV Rack 2, Fundamental, nt_emu et le Rack SDK
Windows correspondant, installés séparément. Le script utilise le paramètre
`-RackSdkPath` ou la variable d'environnement `RACK_SDK_PATH`. En leur absence,
il cherche successivement `Rack-SDK`, `third_party/Rack-SDK`, puis
`third_party/nt_emu/vcv-plugin/Rack-SDK` sous la racine du projet. Le SDK doit
contenir `include/rack.hpp` et `libRack.dll.a` ; un chemin absent ou incomplet
produit une erreur explicite. `-RackDirectory`, `-RackPluginsPath` et `-Compiler`
permettent d'adapter les autres installations.

Après avoir déplacé ou cloné le projet, régénérer les patches avec les scripts
du projet pour qu'ils enregistrent le chemin local de la DLL. Pour réexporter
les 16 scènes et vérifier le pack :

```powershell
python scripts/create-rack-patch.py --motion
python scripts/export-preset-pack.py --library dist/nt_emu/multi_burst.dll
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/test-rack.ps1 -PresetPack
```

Le test Rack charge les bibliothèques installées et exécute leur DSP sans ouvrir
la fenêtre de Rack ni remplacer ses réglages ou son autosave. Son rapport est
dans `build/rack-smoke/result.txt` et son journal dans `rack-smoke.log`.

Les tests natifs contrôlent les transitions, la durée et le routage via une
simulation de l'API. L'inspection ARM contrôle le format et les dépendances.
Le chargement, l'UI physique, les tensions, le jitter externe, la charge CPU et
la coexistence avec d'autres algorithmes doivent être vérifiés sur un NT.

`nt_emu` facilite les essais interactifs, avec des routes limitées à 28 bus et
des morceaux d'API simulés. Les essais du DSP de ses bibliothèques installées
apportent une preuve distincte des tests natifs ; ils ne vérifient pas les prises
physiques ou le transport MIDI du disting NT.

Chaque mise à jour doit conserver les contrats persistés, reconstruire les
livrables et actualiser le rapport de validation. Les vérifications locales
décrites ici ne remplacent pas les essais sur un disting NT physique.
