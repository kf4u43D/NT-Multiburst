# Référence DROID et adaptation NT

La source fonctionnelle est [droid-v2.3.1.ini](../ref/droid-v2.3.1.ini), export
MULTI BURST V2.3.1 pour DROID blue-7. Les comportements ci-dessous proviennent
des connexions et formules du patch. Les intitulés des presets servent à nommer
des réglages ; ils ne garantissent pas à eux seuls une propriété musicale.

| Fonction | Référence DROID | NT avant adaptation | Adaptation retenue |
| --- | --- | --- | --- |
| Générateurs | Deux moteurs indépendants, G1 et G2 | Un moteur par instance | Deux voix dans une instance, avec référence temporelle commune. |
| Nombre de bursts | 1–40, capturé au trigger accepté | Identique | Conservation. |
| Premier burst | Immédiat | Immédiat ou sur prochaine clock | Conservation du choix NT. |
| Retrigger | RET redémarre ; ONE attend la dernière queue d'enveloppe | Identique | Conservation. |
| Pente et vélocité | Capturées au trigger | Identique | Conservation ; convention de signe NT préservée. |
| Durée et forme | Modifiables pendant une enveloppe | Identique | Conservation. |
| Huit formes | Sinus, log descendant, log montant, expo descendant, rampe montante, triangle, rampe descendante, pulse | Même ordre ; logarithmes approchés par des cubiques | Choix des courbes de référence, sans modifier les anciens sons. |
| LOOP | Une fin naturelle relance la séquence | Absent | Boucle après la dernière enveloppe ; stop/reset interrompt. |
| MUTE | Coupe ENV et GATE, conserve séquençage et BUSY | Absent | Même rôle. |
| GLIDE | Slew linéaire distinct montant/descendant entre niveaux ; premier niveau immédiat | Absent | Temps absolus ou rapports musicaux. |
| Durée musicale | 16 rapports à la période de référence | Durée absolue en ms | Mode musical ajouté, mode absolu conservé. |
| Intervalle musical | Off ou 16 rapports ; subdivisions interpolées dans les clocks CV/MIDI | Rate absolu, division MIDI/BPM, diviseur CV | Mode musical ajouté, réglages existants conservés. |
| Source de clock | Interne, CV, MIDI, Auto | Rate interne, BPM interne, CV, MIDI, interne + CV | Auto ajouté aux possibilités NT. |
| Actions | Trigger/stop/step par voix, trigger commun et actions sur le focus | Trigger/reset/step d'une instance | Actions et focus adaptés aux contrôles NT. |
| Niveaux | Gain de pente lissé × BUSY, sans amplitude ni vélocité | Non exposés | Sortie LEVELS par voix. |
| Progression | `(index−1)/max(count−1,1)` | Index affiché seulement | Sortie PROGRESS ; fin naturelle conservée au dernier niveau. |
| Événements | GATE, CLOCK, EOC, BURST et BUSY par voix | Gate, Clock, EOC et Active | BURST distinct de CLOCK ajouté. |
| Sorties communes | Mix ENV, référence CLOCK, BUSY par OU et progression du focus | À construire avec plusieurs instances | Sorties communes intégrées. |
| Amplitude | Amplitude principale × level trim, en direct | Level en direct | Trim indépendant ajouté. |
| Vélocité | Activation indépendante par voix | Toujours active pour trigger MIDI | Activation indépendante ajoutée. |
| Modulation CV | Six destinations partagées, activation/profondeur par voix | Mappings de paramètres natifs NT | Contrôles de modulation inspirés du patch, mappings hôte conservés. |
| Presets | 16 emplacements par voix, usine ou sauvegarde utilisateur | Preset global du NT seulement | Banque de 2 × 16 sons, sauvegardée avec le preset hôte. |
| MIDI | Notes 36/37, canal 10 ; transport ; émission clock interne | Notes/canal/routage configurables, CC, notes et clock sortants | Possibilités NT conservées. |

## Grille temporelle

La durée et l'intervalle partagent les rapports suivants, exprimés en noires de
la référence commune :

`1/16, 1/12, 1/8, 1/6, 1/4, 1/3, 1/2, 2/3, 1, 3/2, 2, 3, 4, 6, 8, 16`.

Le glide ajoute Off avant cette grille. L'intervalle possède aussi Off ; cette
position empêche l'avance automatique et laisse le step manuel disponible.
Les positions du patch sont arrondies sur cette grille. Le réglage d'intervalle
`0.01` du preset Ratchet 40 sélectionne bien `1/16` : il dépasse son seuil Off
de `0.005`.

## Conventions à conserver explicitement

Dans le patch, le gain vaut `1 − 0.96 × abs(slope) × position`, où `position`
vaut la progression pour une pente positive et son complément pour une pente
négative. Une pente DROID positive descend donc de 100 % à 4 %, contrairement
à la convention du NT existant. Les presets importés inversent le signe ; le
paramètre Slope NT garde son comportement historique. COUNT=1 garde toujours
le gain complet.

Les logarithmes de référence sont `log10(1 + 9p)` et `log10(10 − 9p)`, pour une
phase `p` de 0 à 1. L'exponentielle descendante est `(1−p)^3`. Le choix des
courbes de référence concerne les formes et ne change pas le signe de pente.

MUTE ne remet ni le compteur ni la phase à zéro. LEVELS ne contient ni la
vélocité capturée, ni l'amplitude, ni le level trim, ni le mute. PROGRESS reste
au dernier niveau après une fin naturelle ; stop remet l'index à zéro. Pour une
séquence d'un seul burst, cette progression reste à zéro.

Le patch émet CLOCK pendant 5 ms pour les événements d'horloge, même lorsque
le générateur n'est pas occupé ; BURST est une impulsion de 1 ms au début réel
d'une enveloppe. Un stop n'émet pas d'EOC. Ces événements ont donc des usages
différents.

## Modulation du patch

Chaque voix possède CV enable, CV depth, count depth, time depth, shape depth
et slope depth. Le master depth et CV enable multiplient les profondeurs de
destination. Width et Interval partagent time depth. Une entrée absente n'agit
pas sur le paramètre. Le mode commun `CV response=DROID reference` conserve
ce fonctionnement.

La modulation de référence effectue un fondu entre la valeur de base et une
cible CV bornée. Ce fonctionnement diffère du mapping additif natif du NT :

| Destination | Cible à profondeur maximale |
| --- | --- |
| Count | `1 + (Count−1) × clamp(CV/5, 0, 1)`, puis arrondi. |
| Width et Interval | Position `clamp(CV/5, 0, 1)` sur la grille. |
| Slope | `Slope × clamp(CV/5, −1, 1)`. |
| Shape | `min(floor(8 × clamp(CV/5, 0, 1)), 7)`, puis arrondi du fondu. |
| Amplitude | `Amplitude × clamp(CV/5, 0, 1)`. |

Pour Interval, le fondu porte sur la position normalisée, avant d'ajouter
l'offset du choix Off dans l'interface NT. La position résultante est Off si
elle vaut au plus `0.005` ; sinon son indice musical est `round(15 × position)`.
Cela conserve les seuils du DROID, y compris avec une profondeur partielle.

## Comparaison avec le module AJH et expression NT

Le patch DROID est une adaptation. La [description officielle AJH](https://ajhsynth.com/MultiBurst.html)
met en avant le changement de largeur, de cadence et de forme pendant une
séquence, les clocks externes irrégulières et leur combinaison avec le LFO
interne. Pour ce dernier comportement, utiliser NT `Absolute` et
`Internal + CV` : les deux sources restent indépendantes. Le mode musical DROID
reste utile pour des rapports synchronisés ; il impose une grille.

Le chemin DROID module vers une cible positive pour cinq destinations : un
LFO −5…+5 V y passe une moitié de période sur la cible minimale. La pente,
multiplicative, ne varie pas si son réglage est zéro. Ces lois ne démontrent
pas celles des entrées CV analogiques AJH.

Deux choix optionnels complètent cette adaptation :

| Choix NT | Comportement | Compatibilité |
| --- | --- | --- |
| CV response=Bipolar additive | 0 V neutre ; déplacement signé pour Count, Slope, Shape et Amplitude. En Absolute, Width et Rate varient selon `2^(4 × CV/5 × profondeur)`. En Musical, les temps restent sur la grille. | Paramètre commun ajouté après les 166 anciens indices ; les banques de voix gardent leur format. |
| Shape response=Morph | Interpolation des deux profils DROID voisins à phase égale ; position continue sous CV. | Valeur 2 ajoutée au champ Reference curves existant ; 0=Legacy et 1=DROID sont conservés, ainsi que les 25 valeurs de chaque son. |

Ces lois sont des extensions NT. Elles ne sont attribuées ni au DROID ni au
module AJH. Count et Slope restent capturés au trigger ; Width, Rate, Shape et
Amplitude restent actifs pendant la séquence. La convention de signe Slope NT
reste inchangée.

## Banque usine

Les deux banques reprennent les 16 ensembles de réglages du patch : Sinus
classique, Log positif, Log négatif, Expo percussion, Rampe montante, Triangle
lent, Rampe descendante, Ratchet, Boucle sinus, Glide montée, Glide descente,
Boucle expo, Binaire/Pointe, Triolets vélocité, Polyrythme 3:2, Polymètre 7:5.
G1 utilise Ratchet 40 et Binaire ; G2 utilise Ratchet 32 et Pointe.

Les 19 valeurs propres à chaque générateur sont reprises : count, shape, slope,
amplitude, retrigger, loop, mute, width, interval, glide up/down, CV enable,
velocity enable, CV depth, level trim et les quatre profondeurs de modulation.
Les six extensions NT mémorisent aussi les valeurs temporelles absolues, le
mode temporel et le choix des courbes. Les valeurs absolues des sons usine
correspondent à leur grille à 120 BPM, dans les limites des paramètres NT.

## Limites de la comparaison

La sélection Auto du DROID détecte un câble CV branché, puis utilise le MIDI
si son transport fonctionne, puis l'interne. Le NT reçoit un bus et ne peut
pas déduire directement l'insertion d'un câble physique ; sa sélection Auto
repose donc sur l'activité reçue. Le patch conserve une période externe
mesurée et limite sa fraction à `0.9999` ; cela ne constitue pas une clock
prédictive continue lorsque la source s'arrête.

Les contrôleurs DROID DB8E/B32/P2B8/E4, leurs LED, le rappel par appui court et
la sauvegarde par maintien de 1,5 s sont des choix matériels. Les pages,
paramètres et boutons du NT portent leurs fonctions sans reproduire cette
chaîne physique. Le fichier INI permet une comparaison logicielle ; il ne
remplace pas une mesure sur les deux matériels.
