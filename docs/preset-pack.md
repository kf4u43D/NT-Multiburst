# Pack de 16 scènes Multi Burst NT

Ces scènes proposent deux générateurs complémentaires pour les percussions,
les répétitions et les modulations lentes. Douze utilisent les temps libres
Absolute ; quatre utilisent les rapports Musical. Elles fonctionnent sans CV
externe. Hand Steps demande des steps pour avancer après sa première attaque.
Les réglages explorent les possibilités du NT et ne constituent pas une
reproduction mesurée de l'AJH Multi Burst.

Choisir **Pack preset**, puis activer **Load pack preset**. Le chargement règle
les deux voix et leur horloge, active G2 et réinitialise les deux séquences.
Il ne déclenche aucune enveloppe. Presser ensuite l'encodeur droit ou utiliser
**Trigger all** pour entendre les deux voix ; **Stop all** arrête les boucles.
On peut aussi déclencher et arrêter G1 et G2 séparément.

Les routages CV/MIDI restent ceux du patch en cours. Pour entendre le duo,
router les deux sorties Envelope vers leurs destinations, ou router Envelope
mix vers le VCA choisi. Un routage G2 ou mix resté sur None doit donc être
assigné une fois. Le pack n'écrase aucun des 32 emplacements utilisateur des
banques DROID. Pour conserver une variante, utiliser Save preset sur chaque
voix puis enregistrer le preset global NT ; ce dernier conserve aussi les
réglages communs.

Dans le tableau, `N × durée / fréquence` indique le nombre d'enveloppes, leur
largeur et la cadence des débuts de bursts. Pour une scène Musical, la durée
et l'intervalle indiqués sont des fractions de noire. Les numéros ci-dessous
vont de 1 à 16 ; l'index interne du pack va de 0 à 15.

| Nº / scène | Temps G1 / G2 | Comportement et action | Modulation conseillée, facultative |
| --- | --- | --- | --- |
| 1 — Drum Pair | 1 × 140 ms / 1 × 70 ms | Une percussion expo descendante et un accent log montant plus doux. Un Trigger all joue le duo une fois. | Width CV lent ou enveloppe ; profondeur master 25 %, Time 50 %. Count depth=0 garde une attaque par voix. |
| 2 — Bead Cascade | 6 × 90 ms / 6 Hz ; 5 × 75 ms / 4 Hz | Deux chapelets courts : sinus décroissant et rampe descendante aux niveaux croissants. Un seul déclenchement. | Count CV lent pour changer les longueurs au prochain trigger ; Width CV pour détacher ou rapprocher les perles. |
| 3 — Eight Ratchet | 8 × 20 ms / 24 Hz ; 4 × 45 ms / 12 Hz | Huit pulses rapides, accompagnés de quatre accents expo. Fin en moins de 0,35 s sans CV. | Interval CV modéré pour resserrer ou élargir la rafale ; Count depth=0 conserve les nombres annoncés. |
| 4 — Flam Duo | 2 × 55 ms / 12,5 Hz ; 1 × 220 ms | Deux attaques expo espacées de 80 ms sur G1, avec un triangle doux sur G2. Trigger all joue le flam et son contour. | Width CV ; Count depth=0 garde le flam à deux attaques. Interval CV change leur écart. |
| 5 — Velvet Crescendo | 8 × 100 ms / 5 Hz ; 6 × 160 ms / 3,75 Hz | Deux suites finies de niveaux croissants, log montante et sinus, avec glides montants de 70/120 ms. | Slope CV lent ; sa valeur est capturée au prochain déclenchement. Amplitude CV fait varier le niveau pendant la phrase. |
| 6 — Falling Sparks | 12 × 30 ms / 18 Hz ; 6 × 95 ms / 7 Hz | Une pluie expo rapide et des accents log descendants plus espacés ; les deux niveaux diminuent. | Width CV ou Shape CV discret pour varier les queues. Trigger répété redémarre la pluie. |
| 7 — Crossing Levels | 8 × 220 ms / 2,4 Hz ; 8 × 320 ms / 1,7 Hz | Boucles aux pentes opposées, glides de 180/260 ms. Les cycles d'environ 3,14/4,44 s se décalent naturellement. Trigger all lance, Stop all termine. | Slope CV lent ou Amplitude CV pour faire ressortir alternativement les voix. Le décalage est déterministe. |
| 8 — Slow Glide | 6 × 1,6 s / 0,60 Hz ; 7 × 2,2 s / 0,47 Hz | Triangle et sinus en boucles lentes, lissages de niveaux de 1,4/1,8 s. Prévoir 30 s pour entendre deux cycles de chaque voix. | Width CV très lent ; Time depth=50 % limite son excursion. Levels convient à un balayage de filtre. |
| 9 — Morph Breathing | 7 × 850 ms / 1,1 Hz ; 9 × 550 ms / 1,7 Hz | Boucles log montante et triangle, glides de 400/300 ms ; Shape response=Morph sur les deux voix. | Un LFO lent dans Shape CV révèle les formes intermédiaires. Sans CV, les formes de base restent fixes. |
| 10 — Pulse And Sweep | 9 × 65 ms / 8 Hz ; 5 × 950 ms / 1,2 Hz | Une trame de pulses courts sous un balayage montant plus lent ; deux boucles indépendantes. | Width CV allonge les pulses ou les rampes ; régler Time depth par voix selon le rôle souhaité. |
| 11 — Hand Steps | 4 × 180 ms / Rate Off ; 4 × 300 ms / Rate Off | Trigger all joue la première enveloppe. Trois Step all jouent les trois suivantes ; le rythme vient des gestes. Aucune boucle automatique. | Jouer Step à la main ou router Step all input vers une gate. Shape/Width CV modifie les contours sans fixer le rythme. |
| 12 — Clock Weave | 13 × 95 ms / 3,3 Hz ; 9 × 150 ms / 2,1 Hz | Boucles Morph en Internal + CV. Sans clock externe, les deux cadences internes suffisent ; chaque front CV ajoute un événement de burst. | Une clock indépendante dans Clock input enrichit le placement des attaques. Shape CV est une seconde option. Aucun aléatoire n'est généré. |
| 13 — Triplet Weave | 108 BPM ; 12 × 1/3 noire / 8 × 1/2 noire | Expo et sinus bouclés : triolets contre croches, avec une phrase d'environ quatre noires pour chaque voix sans modulation. | Amplitude CV conserve le dessin rythmique. Time CV transpose les rapports sur la grille et change ce dessin. |
| 14 — Seven Against Five | 96 BPM ; 7 × 1 noire / 5 × 1 noire | Triangle croissant et sinus décroissant : boucles de sept et cinq temps, glide d'une demi-noire sur leurs niveaux. | Slope CV très lent renouvelle chaque phrase. Éviter Count/Time CV si l'on veut garder le rapport 7:5. |
| 15 — Dotted Lanterns | 120 BPM ; 4 × 3/2 noire / 6 × 1 noire | Accents log descendants pointés contre triangles aux niveaux croissants. Deux phrases de six noires, glides de 1/4 et 1/3 noire. | Amplitude ou Shape CV laisse les deux durées de phrase intactes ; Time CV les transforme. |
| 16 — Long Tide | 60 BPM ; 4 × 2 noires / 3 × 3 noires | Boucles log montante et rampe descendante, Morph et glides de 1,5/2 s. Deux marées de huit et neuf secondes environ. | Shape CV très lent pour fondre entre profils ; Time depth=50 % donne des déplacements plus contenus. |

## Régler la modulation

Le pack choisit **CV response=Bipolar additive** : 0 V conserve le réglage,
une tension négative le diminue, une tension positive l'augmente. Les entrées
CV peuvent rester sur None. Aucun LFO n'est produit par le pack lui-même.

La profondeur master est généralement de 35 %. Les profondeurs propres aux
destinations la multiplient : par exemple, Time depth=70 % donne une profondeur
effective de 24,5 %. À ±5 V, cela approche une octave de variation de fréquence
ou de durée en Absolute. Les scènes de percussion réduisent certaines
profondeurs ; les nombres de Drum Pair, Eight Ratchet, Flam Duo et Hand Steps
restent fixes sous Count CV. Amplitude utilise directement le master depth.

Count et Slope sont capturés au trigger accepté, y compris au début d'un
nouveau cycle de boucle. Width, Interval/Rate, Shape et Amplitude restent
modifiables pendant la séquence. La forme Morph ne se déplace que si Shape CV
ou un mapping de l'hôte fait évoluer sa position.

Les niveaux de base des deux voix totalisent au plus 9 V. Leur somme peut
monter sous modulation ; Envelope mix limite la contribution du plugin à
10 V. Pour conserver les nuances séparément, utiliser les deux sorties
Envelope. En mode Add, les contributions d'autres algorithmes s'ajoutent
encore à cette tension.

Dans les quatre scènes Musical, la référence est Internal BPM. Pour les
synchroniser à une source externe, choisir ensuite Musical clock=CV ou MIDI
et régler sa résolution ; le chargement suivant du pack rétablit sa référence
interne. Dans Clock Weave, Clock input accepte une clock additionnelle et
reste facultatif.
