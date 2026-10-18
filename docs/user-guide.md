# Multi Burst NT : mode d'emploi

Multi Burst NT produit deux suites indépendantes d'enveloppes pour piloter des
VCA, filtres ou autres destinations CV. Chaque générateur est monophonique : un
burst qui arrive avant la fin de son enveloppe précédente la redémarre. Les deux
générateurs peuvent partager une horloge musicale et des entrées de modulation.
G2 est désactivé au chargement pour conserver le fonctionnement des anciens
presets ; l'activer avec `G2 enabled`.

## Installation

La cible de cette version de développement est le firmware **1.18.0**. Extraire
`multi_burst_nt.zip` à la racine de la MicroSD pour obtenir
`/programs/plug-ins/multi_burst.o`, puis redémarrer le NT. Vérifier le scan sous
`Misc / Plug-ins / View info…`. Dans `Add algorithm`, choisir `Multi Burst NT`,
le charger puis l'ajouter. L'objet ARM annonce explicitement l'API 13 (minimum
firmware **1.15.0**) et utilise le SDK API 13 épinglé ; le chargement réel sur
le firmware cible 1.18.0 reste à confirmer sur matériel.

Ces étapes suivent le [manuel officiel 1.18, pages 69–70](https://www.expert-sleepers.co.uk/downloads/manuals/disting_NT_user_manual_1.18.pdf).

## Prise en main

Les entrées physiques 1, 2 et 3 reçoivent respectivement trigger, clock et reset.
Par défaut, la cadence interne est 4 Hz : un trigger lance huit enveloppes de
100 ms, à niveau maximal de 8 V. Tourner les contrôles habituels du NT pour
modifier les paramètres. Presser le potentiomètre gauche déclenche, le central
réinitialise et le droit avance manuellement d'un burst du générateur sélectionné.
Tourner l'encodeur gauche choisit le focus G1/G2 ; le presser arrête ce générateur.
Presser l'encodeur droit déclenche les deux générateurs activés.

Les actions `Trigger`, `Reset` et `Step` dans les paramètres sont momentanées :
elles reviennent à Off après activation. Step fonctionne avec toutes les sources
d'horloge et contourne le diviseur CV.

Les pages G1 et G2 règlent chaque voix. Les pages Performance et Shared inputs
proposent aussi Trigger/Reset/Stop/Step all. Stop annule la séquence sans EOC ;
reset réinitialise également ses compteurs. Les nouvelles entrées et sorties
sont réglées sur None par défaut : les assigner aux bus voulus avant utilisation.

## Enveloppe

| Paramètre | Plage | Défaut | Effet |
| --- | --- | --- | --- |
| Bursts | 1–40 | 8 | Nombre mémorisé au trigger. |
| Shape | 8 formes | Sine | Forme de l'enveloppe en cours. |
| Width | 20–2500 ms | 100 ms | Durée ; une modification change la vitesse de progression en cours. |
| Slope | −100 à +100 % | 0 % | Pente mémorisée au trigger. |
| Level | 0–10 V | 8 V | Niveau maximal, modifiable en cours. |
| Retrigger | Restart (RET), Ignore (ONE) | Restart | RET recommence ; ONE ignore jusqu'à extinction de la dernière enveloppe. |
| First burst | Immediate, On clock | Immediate | Enveloppe immédiate ou séquence armée jusqu'à la clock suivante. |

Ordre des formes : Sine, Log Down, Log Up, Expo Down, Ramp Up, Triangle,
Ramp Down, Pulse. Les formes sinusoïdale et triangulaire montent puis descendent ;
les ramps sont linéaires, les formes log/expo utilisent des profils courbes
indépendants. Pulse reste haut pendant Width. Les changements de forme, de niveau
et les redémarrages peuvent introduire une discontinuité CV.

À Slope +100 %, les niveaux vont de 4 % à 100 % ; à −100 %, de 100 % à 4 %.
La progression est linéaire entre bursts. À 0 %, tous sont égaux. Un seul burst
utilise le niveau complet. La vélocité d'un trigger MIDI multiplie ces niveaux.

Les pages de chaque générateur ajoutent :

| Paramètre | Effet |
| --- | --- |
| Loop | Relance après la fin naturelle de la dernière enveloppe. Stop/reset coupe la boucle en cours ; un nouveau trigger peut la relancer. |
| Mute | Coupe l'enveloppe et Gate ; les compteurs, Busy, Levels, Progress, Clock et Burst continuent. |
| MIDI velocity | Autorise la vélocité du trigger MIDI à multiplier l'amplitude ; Off utilise la vélocité maximale. |
| Level trim | Multiplie Level, de 0 à 100 %. |
| Glide rise / fall | Lisse les changements de gain de pente entre bursts, avec des temps séparés montant/descendant. Le premier niveau est immédiat. |
| Shape response | Legacy conserve les profils initiaux ; DROID utilise les logarithmes du patch de référence ; Morph ajoute le fondu continu entre formes voisines sous CV. |

Le glide agit sur les niveaux successifs et leur sortie Levels, pas sur la
forme temporelle de chaque enveloppe. Les changements de Level restent directs.
La convention Slope NT reste positive pour une montée ; les presets importés
du DROID adaptent automatiquement le signe.

## Temps musicaux et deux générateurs

Chaque page timing possède un choix `Absolute` / `Musical`. Absolute utilise
Width en ms, Rate, les divisions et les temps de glide en ms. Musical utilise
une référence commune sélectionnée par `Musical clock` : Internal BPM, CV,
MIDI ou Auto. Tempo et CV clock PPQN décrivent cette référence commune ; les
deux voix conservent leurs propres rapports de durée et d'intervalle.

La grille Width ratio et Interval ratio, en noires de référence, est :

`1/16, 1/12, 1/8, 1/6, 1/4, 1/3, 1/2, 2/3, 1, 3/2, 2, 3, 4, 6, 8, 16`.

Interval ratio ajoute Off : seuls les steps manuels/externes font alors avancer
la séquence. Glide rise/fall ratio ajoute Off à la même grille. À 120 BPM,
un rapport 1 correspond à 500 ms, 1/3 à environ 167 ms, et 3/2 à 750 ms.
Une durée peut dépasser l'intervalle ; le burst suivant redémarre alors
l'enveloppe de cette voix.

La référence externe est mesurée pour placer les subdivisions entre deux
clocks. Auto privilégie un CV récent, puis une clock MIDI récente dont le
transport est actif, puis l'interne. Il utilise l'activité du bus, car le NT ne fournit pas au plugin
la détection d'insertion de câble du DROID. Les rapports permettent notamment
les triolets et les périodes 3:2, au-delà des divisions binaires existantes.

Si une source externe explicite s'arrête, la dernière période mesurée reste
utilisée pour terminer les enveloppes et glides en cours. L'avance automatique
suspendue attend la reprise de la clock. En Auto, le changement de source peut
modifier cette période.

## Presets de générateur

Chaque voix dispose de 16 emplacements indépendants dans sa page presets :
choisir Preset, puis Load preset pour rappeler, Save preset pour mémoriser et
Factory preset pour retrouver les valeurs usine de cet emplacement. Ces actions
sont momentanées. La banque utilisateur est enregistrée dans le preset global
du NT ; enregistrer ce dernier pour conserver les modifications après rappel.

Les sons usine reprennent les deux banques de la référence DROID : sinus,
logarithmes, percussion, rampes, triangle, ratchets, boucles, glides, triolets et
rapports 3:2 et 7:5. Le huitième emplacement est Ratchet 40 sur G1 et Ratchet 32
sur G2. Les sons usine utilisent Musical et Shape response=DROID. Un emplacement
utilisateur conserve également le mode Absolute et ses temps si ce mode est
choisi. Le tempo commun, les routages et CV response restent ceux du preset
global NT. Shape response, y compris Morph, est mémorisé par voix.

## Pack de 16 scènes

La page **Preset pack** propose 16 scènes complètes pour G1 et G2, indépendantes
des banques utilisateur : choisir **Pack preset**, puis **Load pack preset**.
Le chargement arrête les deux séquences et ne les déclenche pas ; utiliser
**Trigger all** ou le bouton de déclenchement commun pour jouer la scène.
**Stop all** interrompt les boucles. Hand Steps attend les commandes Step après
le premier burst.

Le pack règle les deux sons, leur activation, les cadences et les paramètres
communs de clock/CV. Il conserve les routages CV/MIDI, le focus et toutes les
mémoires utilisateur. Pour entendre les deux voix, assigner G2 Envelope output
ou Envelope mix output à une sortie disponible. Les routages déjà actifs peuvent
continuer à moduler les valeurs de base de la scène.

Voir le [catalogue des 16 scènes](preset-pack.md) pour leurs durées, comportements
et suggestions de modulation. Les patches Rack fournis font entendre directement
le mix des deux voix, avec les CV de modulation désactivés au départ.

## Horloges absolues et reset

Ces réglages décrivent le mode Absolute historique. En Musical, utiliser les
rapports et la référence commune décrits plus haut.

| Paramètre | Plage | Défaut |
| --- | --- | --- |
| Clock source | Internal rate, Internal BPM, CV, MIDI, Internal + CV | Internal rate |
| Rate | Off ou 0,01–51 Hz | 4 Hz |
| Tempo | 20–300 BPM | 120 BPM |
| Burst division | 1/32, 1/16, 1/8, 1/4, 1/2, 1 bar, 2 bars | 1/16 |
| CV divider | 1–64 fronts par burst | 1 |
| CV clock PPQN | 1, 2, 4, 8, 12, 24, 48 | 1 |
| Pulse width | 1–20 ms | 5 ms |

En **Internal rate**, Rate fixe directement la fréquence des bursts ; la sortie
MIDI Clock considère chaque période comme une noire. Rate Off arrête cette
cadence. La plage sous 0,24 Hz étend celle du module AJH. En **Internal BPM**, Tempo définit les noires et Burst division les
bursts. Les valeurs de division en bars supposent quatre noires par mesure.

En **CV**, chaque front avance le diviseur CV ; un événement de burst arrive
tous les CV divider fronts. Burst division ne multiplie pas les clocks CV.
CV clock PPQN indique la résolution de la source pour reconstruire la sortie
MIDI Clock à partir de deux fronts mesurés. Cette reconstruction s'arrête après
une absence de fronts supérieure au plus grand de 2 secondes ou trois périodes
mesurées. Une reprise exige deux fronts pour réestimer le tempo.

En **MIDI**, la clock F8 arrive à 24 impulsions par noire ; Burst division choisit
3, 6, 12, 24, 48, 96 ou 192 F8 par burst. Start (FA) réinitialise puis lance une
séquence, Continue (FB) réactive la cadence après Stop. Stop (FC) annule la séquence
et les notes ; Continue ne recrée pas la séquence annulée. System Reset (FF)
réinitialise. Le filtre de canal s'applique aux notes et CC, les messages temps
réel sont des messages système.

En **Internal + CV**, les deux sources avancent la même séquence. La cadence
interne reste indépendante. Deux clocks au même échantillon sont fusionnées ;
les événements distincts sont conservés. Le MIDI Clock émis suit la cadence
interne dans ce mode.

Un **reset** remet à zéro la séquence, les compteurs et les phases ; il est
prioritaire sur un trigger ou une clock au même échantillon. Il libère les notes
MIDI du plugin et ne produit pas d'EOC.

## Routage CV

Les bus se choisissent de 1 à 64, ou None (0). Les entrées 1–12 sont physiques,
les bus 13–20 correspondent aux sorties physiques 1–8, les suivants sont
auxiliaires. Les fronts montants sont reconnus à partir de 1 V ; le détecteur se
réarme à 0,2 V ou moins. Une entrée maintenue haute ne redéclenche pas. Modifier
le routage vers un signal déjà haut exige un nouveau front.

| Sortie | Bus par défaut | Prise | Signal |
| --- | --- | --- | --- |
| Envelope | 13 | Sortie 1 | 0 à Level volts, pente et vélocité incluses. |
| Gate | 14 | Sortie 2 | 5 V pendant chaque enveloppe. |
| Active | 15 | Sortie 3 | 5 V tant que la séquence est armée ou joue, dernière enveloppe incluse. |
| EOC | 16 | Sortie 4 | Impulsion 5 V après extinction naturelle de la dernière enveloppe. |
| Clock | 17 | Sortie 5 | Impulsions 5 V à la cadence des événements de burst, même sans séquence active. |

Pulse width règle Clock et EOC ; Gate suit Width. Choisir des bus de sortie
distincts. Le mode **Add**, par défaut, ajoute la contribution à celle des
algorithmes précédents ; **Replace** la remplace. La limite 0–10 V concerne la
contribution du plugin : une somme en Add peut dépasser cette plage.

G2 propose les mêmes sorties, ainsi que Levels, Progress et Burst pour chaque
voix. Leurs bus, et ceux des sorties communes, valent None au départ.

| Sortie supplémentaire | Signal |
| --- | --- |
| Levels | 0–10 V représentant le gain de pente lissé pendant Busy, sans Level, trim, vélocité ni mute. |
| Progress | 0–10 V de progression entre le premier et le dernier burst ; conserve le dernier niveau à la fin naturelle, revient à zéro au stop/reset. Un seul burst donne 0 V. |
| Burst | Impulsion 5 V de 1 ms au début réel de chaque enveloppe, même sous mute. |
| ENV mix | Somme des enveloppes des deux voix activées, limitée à 10 V pour la contribution du plugin. |
| Busy all | 10 V tant qu'au moins un générateur activé est occupé. |
| Reference clock | Impulsions 5 V à la cadence de la noire commune ; durée Pulse width. |
| Focus progress | Progress du générateur sélectionné à l'écran. |

Les entrées Stop et Step par générateur s'ajoutent aux entrées Trigger/Reset.
Les entrées communes Trigger/Reset/Stop/Step all agissent sur les voix activées.

## Modulation CV

Shared inputs propose des bus Count, Width, Interval, Slope, Shape et Amplitude
CV. Ils sont communs aux deux générateurs, avec CV enable, CV depth et les
profondeurs Count/Time/Shape/Slope indépendants par voix. Time depth règle à la
fois Width et Interval. Une entrée sur None n'affecte pas le paramètre. Le choix
commun **CV response** sélectionne DROID reference ou Bipolar additive. Les
anciens presets conservent DROID reference.

En **DROID reference**, une tension 0–5 V choisit une
position de durée/intervalle ou une forme. Count varie entre 1 et le nombre
réglé ; Amplitude entre zéro et le niveau réglé. Slope utilise −5 à +5 V pour
multiplier la pente de base, l'inverser ou l'annuler. La profondeur effectue un
fondu entre le réglage de base et cette cible, puis les sélections discrètes
sont arrondies. Un LFO bipolaire est donc tronqué à zéro sur sa moitié négative
pour les destinations autres que Slope. À Slope=0, sa modulation multiplicative
reste nulle.

En **Bipolar additive**, 0 V conserve le réglage, les tensions négatives le
diminuent et les positives l'augmentent. La tension est bornée à −5…+5 V. Les
profondeurs limitent l'excursion suivante :

| Destination | Excursion à ±5 V, profondeur 100 % |
| --- | --- |
| Count | ±39 bursts, limité à 1–40 puis arrondi. |
| Slope | ±100 points de pourcentage, limité à −100…+100 %. |
| Amplitude | ±10 V, limité à 0–10 V avant Level trim. |
| Shape | ±7 positions, limité aux huit formes. |
| Width / Interval, Absolute | Durée / fréquence multipliée par 1/16 à 16 ; Width reste à 20–2500 ms et Rate au plus à 51 Hz. Rate Off reste Off. |
| Width / Interval, Musical | Déplacement sur la grille de rapports, avec l'arrondi musical et Off pour l'intervalle. |

Par exemple, Time depth=25 % donne une excursion d'une octave : à −5 V,
Width et Rate sont divisés par deux ; à +5 V, ils doublent. Les deux tensions
peuvent être différentes, car Width et Interval possèdent leurs propres entrées.
Count et Slope restent mémorisés au trigger dans les deux modes. Width, Rate,
Shape et Amplitude agissent pendant la séquence.

**Shape response=Morph** conserve une position fractionnaire sous Shape CV et
fait un fondu entre les deux profils adjacents à la même phase. Il peut produire
des enveloppes intermédiaires sans sauts de sélection. Les positions entières
correspondent aux formes DROID. C'est une extension expressive NT ; le patch
DROID et le [module AJH](https://ajhsynth.com/MultiBurst.html) sélectionnent huit formes discrètes. Bipolar additive
est également un choix NT, dont la loi n'est pas présentée comme celle de l'AJH.

Pour une première écoute, ouvrir le [patch Motion](../patches/README.md) : deux
boucles, trois LFO lents, temps Absolute, horloges Internal + CV et Morph. Pour
retrouver des phrases moins régulières avec une horloge externe, utiliser
Absolute puis Internal + CV : chaque front externe ajoute un burst à la
cadence interne indépendante. Pour une modulation entre voix sur matériel,
sortir Progress G1 et Levels G2 sur deux prises, puis les recâbler physiquement
vers les entrées choisies pour Width CV et Amplitude CV. Un bus de sortie de
cette même instance ne fournit pas à lui seul une boucle de rétroaction interne.
Commencer avec 20–30 % de profondeur et CV response=DROID reference pour ces signaux positifs. Un
atténuateur de moitié ramène leur plage 0–10 V à 0–5 V pour exploiter toute la
course de modulation.

Les mappings de paramètres natifs restent disponibles en complément :

Dans les mappings du NT, choisir le paramètre, le bus source, Volts et Delta.
Le CV ajoute `CV × Delta / Volts` à la valeur de base. Pour retrouver une
sélection de forme 0–5 V : régler Shape à Sine, puis mapper Shape avec Volts=5
et Delta=7. Pour Bursts : base=1, Volts=5, Delta=39. Pour Width : base=20 ms,
Volts=5, Delta=2480 ms. La modulation passe par le traitement des paramètres de
l'hôte ; les fronts trigger/clock/reset sont observés à chaque échantillon.

Les mappings sont enregistrés avec le preset. Leur fonctionnement est décrit
dans le [manuel officiel 1.18, pages 45–48](https://www.expert-sleepers.co.uk/downloads/manuals/disting_NT_user_manual_1.18.pdf).

## Contrôle MIDI

MIDI input channel vaut **1** par défaut ; **Off** désactive les notes/CC propres
au plugin, **Omni** accepte les seize canaux. Les notes par défaut sont 60
(trigger), 61 (reset), 62 (step) et 63 (stop) pour G1 ; 64, 65, 66 et 67 pour G2.
Chaque voix possède son canal et ses notes, configurables de 0 à 127.
Un Note On de vélocité zéro et un Note Off ne déclenchent rien ; la séquence
continue après relâchement du trigger.

| CC G1 | CC G2 | Destination | Valeurs 0–127 |
| --- | --- | --- | --- |
| 20 | 30 | Bursts | 1–40 |
| 21 | 31 | Shape | Sine à Pulse |
| 22 | 32 | Width | 20–2500 ms en Absolute |
| 23 | 33 | Rate | 0=Off ; 1–127=0,24–51 Hz en Absolute |
| 24 | 34 | Slope | −100 à +100 % |
| 25 | 35 | Level | 0–10 V |
| 26 | — | Tempo commun | 20–300 BPM |
| 115 | 110 | Trigger | Valeur ≥64 |
| 116 | 111 | Reset | Valeur ≥64 |
| 117 | 112 | Step | Valeur ≥64 |
| 118 | 113 | Stop | Valeur ≥64 |
| 120, 123 | 120, 123 | Annulation/reset | Toute valeur sur le canal accepté par la voix |

Les mappings MIDI natifs du NT permettent aussi d'assigner les paramètres à
d'autres CC. Éviter d'assigner simultanément ces mêmes CC à d'autres paramètres
du plugin. L'entrée MIDI du SDK ne fournit pas le port d'origine au callback :
éviter une boucle entre la destination MIDI émise et la source reçue.

## Sorties MIDI

| Paramètre | Plage | Défaut |
| --- | --- | --- |
| MIDI destination | Off, Breakout, Select Bus, USB et combinaisons | Off |
| MIDI output channel | 1–16 | 1 |
| MIDI output note | 0–127 | 60 |
| MIDI notes | Off/On | Off |
| Envelope MIDI CC | Off ou 0–119 | Off |
| MIDI CC rate | 1–100 Hz | 50 Hz |
| MIDI clock output | Off/On | Off |
| MIDI transport out | Off/On | Off |

Chaque burst de niveau non nul émet un Note On, suivi d'un Note Off à la fin de son enveloppe ou
avant le prochain burst. La vélocité représente son niveau maximal sur l'échelle
0–10 V → 0–127, avec un minimum de 1 pour un Note On. Un niveau maximal nul
n'émet pas de note. Modifier note, canal,
destination ou désactiver les notes libère la note sur son ancien routage.
Mute coupe aussi les notes de cette voix. G2 possède son propre routage de
notes et d'enveloppe CC ; ses valeurs initiales sont canal 2 et note 64.

L'enveloppe CC utilise aussi 0–10 V → 0–127 ; seuls les changements sont émis,
à la cadence maximale choisie. Un retour final à zéro est transmis sans attendre
le prochain créneau. Le CV conserve une résolution et une cadence plus élevées.

MIDI clock output et MIDI transport out utilisent la destination MIDI de G1.
Clock émet F8 à 24 PPQN : interne, reconstruit du CV ou retransmis du MIDI
suivant la source. Dès qu'une voix activée utilise Musical, l'émission suit la
référence commune ; sinon elle suit la clock absolue de G1.

Transport out émet Start au premier burst et Stop lorsque les deux séquences
sont arrêtées ou terminées. Clock peut continuer entre deux séquences.
Un retrigger RET émet Stop puis Start si l'autre voix n'est pas occupée.
Si Stop et Start arrivent avant le même bloc audio, le dernier message détermine
le transport ; System Reset reste prioritaire.

## Validation sur module

Cette version a des vérifications logicielles, mais aucune validation physique.
Lors du premier essai, vérifier le scan, les tensions des sorties, ONE/RET,
Loop/Mute/Glide, les resets simultanés, les horloges absolues et musicales,
la banque utilisateur et les Note Off. La
[comparaison avec le patch DROID](reference-comparison.md) décrit les
conventions adaptées et les limites de fidélité ; les mesures sur matériel
restent nécessaires pour comparer précisément les courbes.
