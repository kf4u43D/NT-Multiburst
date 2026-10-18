# Patches Multi Burst NT / nt_emu

Ouvrir **Multi_Burst_NT-nt_emu.vcv** dans VCV Rack 2. Le patch utilise les modules
Core, Fundamental et nt_emu déjà installés. Il charge automatiquement le plugin
desktop `dist/nt_emu/multi_burst.dll` du projet. Régler **Engine → Sample rate à
48 kHz**, correspondant à la fréquence annoncée par nt_emu 2.0.2.

**Multi_Burst_NT-motion.vcv** propose une seconde écoute avec deux générateurs
en boucle, trois LFO lents bipolaires et le morphing des formes. Le patch de
test initial conserve ses réglages et ses cinq sorties.

## Pack de 16 scènes

Les [16 patches prêts à ouvrir](../dist/preset-pack/README.md) et leur
[archive ZIP](../dist/multi_burst_motion_presets.zip) proposent des sons courts,
ratchets, duos de niveaux, boucles lentes et rapports musicaux. Le
[guide du pack](../docs/preset-pack.md) décrit chaque scène. Le
[catalogue JSON](../dist/preset-pack/catalog.json) contient leurs réglages exacts.

Chaque patch fait écouter les deux voix : ENV MIX, sortie NT 8, commande le VCA
du sinus. PULSES 1 lance, PULSES 2 réinitialise, PULSES 3 avance d'un burst.
Ce troisième bouton permet notamment d'explorer Hand Steps, dont la cadence
interne est arrêtée. Les LFO lents sont câblés, mais leurs entrées CV sont sur
None au départ pour entendre le son de base. Activer Width CV input=4,
Interval CV input=5 ou Shape CV input=6 pour ajouter du mouvement.

La clock externe 2,7 Hz est également disponible sur l'entrée 2. Dans les scènes
Absolute, choisir Clock input=2 et Clock source=Internal + CV pour la combiner
à la cadence interne. Le réglage initial Clock input=None laisse chaque scène
jouer à sa cadence propre.

Sur le NT, le même pack se rappelle dans **Preset pack** : choisir **Pack
preset**, puis **Load pack preset**. Il règle les deux voix et leurs paramètres
communs ; les routages et les deux banques DROID utilisateur restent ceux du
preset NT.

## Démo Motion : modulations en mouvement

1. Ouvrir **Multi_Burst_NT-motion.vcv**, puis cliquer le bouton **2 de PULSES**.
2. Cliquer **1 de PULSES** pour lancer les deux voix. G1 joue huit bursts, G2
   treize ; les deux bouclent avec des pentes et durées différentes.
3. Choisir le pilote et le périphérique dans AUDIO 2 pour écouter G1, un sinus
   passé dans le VCA d'enveloppe. Le volume commence à 15 %.
4. Observer les formes, les espacements et la largeur qui évoluent ; presser
   PULSES 2 arrête les deux séquences, PULSES 1 les relance.

Le mode Absolute conserve les temps continus. Une clock externe à 2,7 Hz se
combine aux cadences internes variables, initialement 4 Hz et 1,6 Hz.
CV response=Bipolar additive garde les réglages au passage à 0 V ; Shape
response=Morph transforme les profils sans paliers de sélection. Les trois
LFO de modulation sont libres et continuent après le reset des séquences.

| Entrée NT | Source | Destination | Profondeur G1 / G2 |
| --- | --- | --- | --- |
| 1 | PULSES 1 | Trigger all | — |
| 2 | LFO clock 2,7 Hz | Clock des deux voix | — |
| 3 | PULSES 2 | Reset all | — |
| 4 | Triangle bipolaire 0,125 Hz | Width | 35 % / 20 % |
| 5 | Sinus bipolaire 0,088 Hz | Interval / Rate | 35 % / 20 % |
| 6 | Triangle bipolaire 0,051 Hz | Shape | 35 % / 25 % |

Les trois scopes de la première rangée affichent G1 ENV/Gate, Active/EOC et
Clock/clock externe. Les deux nouveaux scopes affichent G2 ENV/Shape CV, puis
Width CV/Interval CV. G2 ENV utilise la sortie NT 6.

Pour comparer, passer Shape response de Morph à DROID sur G1 : les formes
redeviennent discrètes. Mettre Time CV depth à zéro stabilise les durées et les
cadences internes. Passer CV response à DROID reference montre la modulation
vers des cibles positives ; les tensions négatives gardent alors leur cible
sur la borne basse. Ces choix NT sont explicités dans le [mode d'emploi](../docs/user-guide.md#modulation-cv).

## Premier essai

1. Cliquer le **bouton 2 de PULSES** pour remettre le plugin et le LFO à zéro.
2. Cliquer le **bouton 1 de PULSES** pour déclencher une séquence.
3. Observer les huit enveloppes, dont le niveau augmente de 4 à 100 % de 8 V.
4. Pour écouter, choisir le pilote et le périphérique de sortie dans AUDIO 2.
   Le VCA de droite règle le volume, initialement à 15 %.

Le réglage initial est huit bursts, largeur 100 ms, pente +100 %, horloge CV à
4 Hz. La séquence s'arrête après la dernière enveloppe. Les impulsions d'horloge
continuent entre les séquences.

| Scope | X | Y |
| --- | --- | --- |
| 1 | Enveloppe, sortie NT 1 | Gate, sortie NT 2 |
| 2 | Active, sortie NT 3 | EOC, sortie NT 4 |
| 3 | Clock, sortie NT 5 | Clock externe du LFO |

La seconde rangée fournit un sinus à environ 130,8 Hz, un VCA contrôlé par
l'enveloppe, un VCA de volume et AUDIO 2. Les deux canaux audio reçoivent le
même signal. Le périphérique audio est laissé à choisir dans le patch.

## Variantes à essayer

Dans le menu des paramètres du NT, changer Shape, Width et Level en cours de
séquence. Slope et Bursts sont mémorisés au trigger : une modification attend
la séquence suivante. À Slope 0, les bursts sont uniformes ; à −100 %, ils
décroissent. ONE ignore un nouveau trigger pendant une séquence, RET recommence.

Changer Clock source en **Internal rate** pour utiliser Rate, ou **Internal BPM**
pour utiliser Tempo et Burst division. **Internal + CV** combine Rate et le LFO.
Rate=0 arrête la cadence interne. Presser le potentiomètre droit du NT déclenche
un Step manuel ; on peut aussi utiliser le paramètre Step dans le menu.
Le bouton 2 de PULSES doit toujours ramener Envelope, Gate et Active à zéro,
sans produire d'EOC. EOC indique uniquement la fin naturelle.

Pour le MIDI, choisir les périphériques **MIDI input/output** dans le menu de
nt_emu. Le plugin accepte le canal 1 : notes 60=trigger, 61=reset, 62=step ;
CC 20–26 contrôlent Bursts, Shape, Width, Rate, Slope, Level et Tempo. Le patch
active les sorties de notes, enveloppe CC 11, clock et transport, mais aucun
périphérique n'est sélectionné automatiquement. Pour synchroniser directement
au MIDI Clock, choisir Clock source=MIDI puis envoyer Start et F8. Les routages
de CV demeurent disponibles. La sortie MIDI émise doit utiliser un port qui ne
boucle pas sur l'entrée du plugin.

## Chargement et reproduction

Le fichier `.o` ARM destiné au module physique ne se charge pas comme bibliothèque
desktop : ce patch utilise sa compilation Windows `.dll`. Le chemin enregistré
est absolu. Si le projet est déplacé, reconstruire et régénérer le patch :

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build-emu.ps1
python scripts/create-rack-patch.py
python scripts/create-rack-patch.py --motion
python scripts/export-preset-pack.py
```

Tous les routages du patch utilisent les entrées/sorties physiques, dans la plage
de 28 bus de nt_emu 2.0.2. L'adaptation desktop conserve le moteur DSP du plugin
et ajuste le contrat d'hôte pour cet émulateur ; elle n'est pas destinée au
chargement sur le disting NT.

L'export du pack compile un petit programme C++17 à partir de
`include/multi_burst/performance_presets.h` : les valeurs des scènes ne sont pas
recopiées dans le script Python. Un compilateur natif clang++ ou g++ est requis.
L'export produit les 16 `.vcv`, le catalogue, les sources de réglages et un
manifeste avec les empreintes SHA-256. Les patches contiennent le chemin absolu
de la DLL ; relancer l'export après déplacement ou reconstruction du projet.

## Vérification effectuée

Le patch a été chargé dans les bibliothèques DSP réellement installées de Rack,
Fundamental et nt_emu. Les vérifications couvrent la restauration des 42
paramètres historiques et des ajouts, les cinq sorties,
huit enveloppes de 100 ms, le crescendo, EOC 5 ms,
le reset CV et les horloges CV/interne. La voie sinus/VCA produit bien un signal
audio. Aucun périphérique audio ou MIDI externe n'a été activé par ces tests.
L'interface graphique et le transport MIDI vers un périphérique restent à
essayer dans Rack. La démo Motion est un patch distinct ; sa validation est
suivie dans le [rapport de validation](../docs/validation.md).

Son test DSP sur 12 secondes confirme les trois CV bipolaires, les deux voix,
les variations des segments de Gate et les intervalles irréguliers de clock.

La vérification couvre également les banques utilisateur G1/G2 : sauvegarde de
deux sons, remplacement, rechargement par le parseur JSON réel de nt_emu puis
rappel des valeurs initiales. Le patch démarre avec G2 désactivé. Pour essayer
les fonctions DROID, activer G2, choisir les bus de sortie et charger un son dans
les pages presets ; le [mode d'emploi](../docs/user-guide.md) décrit le mode
musical, les boucles et le glide.

Sources du format et des ports : [VCV Rack](https://github.com/VCVRack/Rack/blob/v2/src/patch.cpp),
[VCV Fundamental](https://github.com/VCVRack/Fundamental/tree/v2/src),
[nt_emu](https://github.com/thorinside/nt_emu/tree/ad2aa4b610f39c32903e3b7403cf2f2335508ca1/vcv-plugin/src).
