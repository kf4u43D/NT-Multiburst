# Vérification locale du 18 octobre 2026

La version de développement **0.4** ajoute un pack de 16 scènes à deux voix,
indépendant des banques utilisateur, aux fonctions de modulation de la version 0.3.
Le SDK reste figé à l'API 13, commit
`f440d20c2a96a9ca43892c7418a1d8185ad47eb5` ; firmware minimum 1.15.0,
cible de validation 1.18.0. Le chargement sur matériel reste à effectuer.

| Vérification | Résultat |
| --- | --- |
| Moteur natif | Réussite : anciens comportements, boucle/EOC, mute/glide, courbes, morphing sans remise à zéro de phase et modulation temporelle continue. |
| Horloge musicale | Grille des 16 rapports, triolets, deux timings communs, CV/MIDI/Auto, PPQN 1 à 48, transport et changements de fréquence d'échantillonnage vérifiés. |
| Adaptation SDK | 34 cas réussis à 44,1, 48 et 96 kHz : fonctions historiques, modulation, transport, mémoires et les 16 scènes du pack. |
| Presets usine | Comparaison indépendante des 32 × 19 valeurs avec l'INI DROID réussie ; signe de pente adapté à la convention NT. |
| Persistance | 169 paramètres ; les 167 indices précédents sont inchangés. Banques JSON de 25 champs conservées ; charger une scène ne modifie pas les mémoires utilisateur. |
| Compilation native | Clang 22.1, C++17, avertissements traités comme erreurs. |
| Analyse statique | clang-tidy sur les deux sources du projet : aucune erreur ; diagnostics des en-têtes externes filtrés. |
| Compilation ARM | Arm GNU GCC 14.3, Cortex-M7, Thumb, hard float, PIC : réussite. |
| Inspection ARM | ELF32 little endian ARM REL, EABI5, arguments VFP et pluginEntry global ; contrat API 13 conservé. |
| Archive | Un seul fichier, programs/plug-ins/multi_burst.o ; contenu et empreinte vérifiés par le script. |
| Émulateur | DLL Windows reconstruite ; DSP des bibliothèques Rack/Fundamental/nt_emu installées et aller-retour des banques JSON vérifiés. |
| Disting NT physique | Non testé : chargement, interface, tensions, jitter, MIDI externe et charge CPU à confirmer. |

L'objet ARM livré fait **61 600 octets** avec ses métadonnées ELF et relocations.
Ses sections contiennent **18 728 octets de code**, **6 190 octets de constantes**,
**3 444 octets de métadonnées initialisées** et aucune .bss statique.
L'état des voix et banques est réservé par instance auprès de l'hôte.
Ces tailles ne sont pas une mesure de charge CPU.

SHA-256 de l'objet livré :

`17448fc85a4fc62a9ff9182037c0dec1e3d6972dc9a2b33afbb78033daf6a9ec`

Le vérificateur accepte uniquement les exports NT du SDK figé et les signatures
exactes des neuf méthodes JSON utilisées, également déclarées dans son en-tête
serialisation.h. Aucun helper de conversion/division 64 bits, libm, allocation
ou runtime C++ supplémentaire n'est importé ; memcpy, memset et le pseudo-symbole PIC
sont conservés. Les empreintes des en-têtes figurent dans le rapport ARM.

## Vérification Rack / nt_emu

Le test utilise libRack, Fundamental 2.6.4 et nt_emu 2.0.2 réellement installés,
sans fenêtre graphique, périphérique audio ou modification du profil utilisateur.
La DLL annonce l'API 8 pour l'émulateur et limite les routages à 28 bus.

Le patch restaure les 169 paramètres. Le banc vérifie huit enveloppes de
100 ms, le crescendo de 0,320 à 7,999 V, l'EOC de 5 ms et le reset silencieux.
Les périodes CV et internes sont de 12 000 échantillons, à un échantillon près ;
le décalage observé dans la chaîne CV est de quatre échantillons. La voie
sinus/VCA produit une valeur RMS d'environ 0,097 V.

Deux sons utilisateur différents sont ensuite sauvegardés, remplacés, rechargés
par le vrai parseur JSON de nt_emu et rappelés : les valeurs des deux banques
sont retrouvées. Le format plat évite la limitation des tableaux imbriqués du
parseur de cet émulateur. Les banques du firmware utilisent le même schéma.
Le contrôle MIDI externe et l'affichage physique restent hors de ces tests.

Le patch Motion est également exécuté pendant 12 secondes dans les bibliothèques
réelles : les trois entrées de modulation reçoivent des tensions bipolaires,
les deux voix restent actives et leurs sorties restent dans 0–10 V. G2 atteint
7 V ; les intervalles entre fronts de clock varient de 290 à 17 776 échantillons.
Les segments hauts de Gate vont de 1 830 à 46 720 échantillons. Ces segments
peuvent contenir plusieurs enveloppes qui se redéclenchent et ne constituent pas
une mesure de Width. Ce test démontre la variation temporelle du patch, sans
prétendre mesurer le caractère sonore d'un AJH physique.

Le journal Motion local est `build/rack-motion-smoke/result.txt` ; le
[patch à ouvrir](../patches/Multi_Burst_NT-motion.vcv) possède ses propres scopes
et laisse le choix du périphérique audio à l'utilisateur.

## Pack de 16 scènes

Les 16 scènes sont calculées depuis le même en-tête C++ pour le plugin et les
exports Rack. Le contrôle natif joue chacune sans CV externe et vérifie les deux
voix, le mix borné, le chargement silencieux et Stop. Un autre cas remplace deux
voix dont des notes MIDI sont tenues : les Note Off sont envoyés, les triggers et
sauvegardes déjà en attente sont annulés, les routages et les banques sauvegardées
restent identiques.

Les 16 patches exportés ont été instanciés dans les vraies bibliothèques Rack
avec restauration des 169 paramètres et vérification des ports. Neuf ont aussi
été joués : Drum Pair, Crossing Levels, Slow Glide, Morph Breathing, Hand Steps,
Triplet Weave, Seven Against Five, Dotted Lanterns et Long Tide. Les sorties des
deux voix et du mix restent bornées, la voie audio est non nulle, les boucles
continuent et Stop/Reset n'émettent pas d'EOC. Hand Steps avance avec trois
impulsions réelles du troisième bouton PULSES après sa première attaque.

Les journaux locaux sont dans `build/rack-pack-smoke/`. Le ZIP du
pack a été extrait puis contrôlé : 16 fichiers VCV, catalogue, guide et sources,
avec concordance des SHA-256 du manifeste, de l'en-tête et de la DLL utilisée.
Le [catalogue](preset-pack.md) décrit les gestes et modulations facultatives.

Le rapport d'inspection publié est [object_report.json](../downloads/v0.4/object_report.json),
avec le [manifeste portable](../downloads/v0.4/manifest.json). Le manifeste complet
de compilation reste dans `dist/build_manifest.json`, et le journal local du
test Rack dans `build/rack-smoke/result.txt`. Les choix
fonctionnels sont détaillés dans la [comparaison DROID/NT](reference-comparison.md).
Aucun tag ni release distant n'a été créé.
