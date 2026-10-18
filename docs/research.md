# Recherche et choix de conception

Sources consultées le **14 octobre 2026**, avec correction de la cible SDK le **15 octobre 2026**. Ce document distingue les faits publiés par les fabricants, les règles de développement du disting NT et les conventions choisies pour cette implémentation indépendante.

## Versions de référence

La dernière version publiée du firmware disting NT, à cette date, est **1.18.0**, sortie le **4 septembre 2026**. La version et la date sont vérifiées dans le [tableau officiel Expert Sleepers](https://www.expert-sleepers.co.uk/distingNTfirmwareupdates.html). Ce tableau constitue la référence pour le firmware ; le numéro d'API doit être vérifié séparément dans le SDK.

Le [SDK officiel distingNT_API](https://github.com/expertsleepersltd/distingNT_API) est importé dans `third_party/distingNT_API` à la révision **f440d20c2a96a9ca43892c7418a1d8185ad47eb5**, datée du **3 août 2026**. C'est la dernière révision précédant la sortie du firmware 1.18.0. Son [en-tête api.h à cette révision](https://github.com/expertsleepersltd/distingNT_API/blob/f440d20c2a96a9ca43892c7418a1d8185ad47eb5/include/distingnt/api.h) définit `kNT_apiVersionCurrent = kNT_apiVersion13`. Le plugin matériel annonce explicitement `kNT_apiVersion13`, disponible à partir du firmware **1.15.0** ; le firmware **1.18.0** demeure la cible de validation.

**Correction du 15 octobre 2026 :** la première compilation avait importé le SDK de développement API 14 et annonçait automatiquement `kNT_apiVersionCurrent`. Or l'API 14 a été introduite dans le SDK le **29 septembre 2026**, après la sortie du firmware stable 1.18.0. Le nom de la constante « Current » n'établissait donc pas la compatibilité avec ce firmware. Le correctif épingle le SDK API 13 et fixe le contrat annoncé par l'objet ARM indépendamment des futurs changements de cette constante. La recompilation et le contrôle des artefacts sont suivis dans le [rapport de validation](validation.md) ; aucune validation physique n'est revendiquée.

Ces révisions sont figées pour rendre la compilation reproductible. Un changement de firmware cible ou de SDK demande une nouvelle vérification des signatures, des fonctions utilisées et du chargement de l'objet ARM.

## Fonctionnement publié du Multi Burst Envelopes

La [fiche officielle AJH Synth](https://ajhsynth.com/MultiBurst.html) fournit les caractéristiques suivantes :

| Fonction | Comportement publié |
| --- | --- |
| Séquence | 1 à 40 enveloppes ; une seule correspond à une enveloppe AD. |
| Formes, dans l'ordre | Sine, Log Down, Log Up, Expo Down, Ramp Up, Triangle, Ramp Down, Pulse. |
| Largeur | 20 ms à 2,5 s ; contrôle par CV. |
| Cadence interne | Arrêt, puis 0,24 à 51 Hz ; contrôle par CV. |
| Amplitude | Sortie 0 à +10 V ; pente de séquence ascendante 4 à 100 %, descendante 100 à 4 %, ou uniforme. |
| Horloge externe | Une impulsion produit l'enveloppe suivante ; cadence irrégulière acceptée ; combinaison avec le LFO interne possible. |
| Déclenchement | ONE ignore les nouveaux triggers pendant une séquence ; RET interrompt et redémarre. |
| Modulation | Forme, largeur et cadence modifiables pendant la séquence ; pente mémorisée au déclenchement. |
| Sélection de forme | CV de 0 à +5 V remplaçant le sélecteur. |
| Fin et gate | Séquence active jusqu'à extinction de la dernière enveloppe ; gate commandé par nombre, largeur et cadence. |

La fiche attribue la conception à Allan Hall et Tom Wiltshire, d'Electric Druid. Elle ne publie aucune équation de forme ni table d'échantillons. Le [guide vidéo officiel](https://player.vimeo.com/video/703216004) est lié depuis cette fiche, mais son contenu n'a pas pu être exploité dans cette recherche. Le [catalogue officiel des notices](https://ajhsynth.com/downloads.html) n'offrait pas de PDF Multi Burst Envelopes lors de la consultation.

## Portée de la reproduction

Le plugin reprend le principe musical décrit publiquement : des séquences finies d'enveloppes, un contrôle de leur amplitude au fil des répétitions et plusieurs sources de cadence. Les courbes sont une implémentation indépendante. Leur nom et leur intention suivent la référence, mais aucune équivalence des tables, des équations, de la résolution ou des détails analogiques du module AJH n'est revendiquée.

Les documents consultés ne précisent pas la loi exacte de la pente entre deux bursts, la mémorisation du nombre de bursts, le comportement des changements de forme au milieu d'une enveloppe, le premier événement après un trigger, les enveloppes qui se chevauchent ou les collisions d'horloges. Ces points demanderaient des mesures sur un module AJH pour établir une équivalence plus étroite.

## Conventions retenues pour le plugin

Les règles ci-dessous sont les choix de cette implémentation ; elles définissent des comportements observables et testables.

- **Nombre et pente** : mémorisés au déclenchement d'une séquence. Une modification s'applique à la suivante.
- **Forme, largeur et niveau** : actifs pendant la séquence. La forme peut changer sur l'enveloppe en cours ; la largeur modifie sa vitesse de progression et le niveau sa tension de sortie.
- **Premier burst** : choix entre émission immédiate au trigger et attente du prochain événement d'horloge.
- **Chevauchement** : le nouveau burst redémarre l'enveloppe ; les enveloppes ne s'additionnent pas.
- **ONE et RET** : la protection ONE couvre également la dernière enveloppe jusqu'à sa fin ; RET remet la séquence à son début.
- **Reset** : prioritaire sur les triggers et horloges reçus au même instant. Il annule la séquence, remet les phases et compteurs à zéro, abaisse les gates et libère les notes MIDI appartenant au plugin.
- **Fin de cycle** : une impulsion EOC indique la fin naturelle de la dernière enveloppe. Un reset ou une interruption ne constitue pas une fin naturelle.

Le reset dédié, les messages MIDI, les sorties d'activité et de fin de cycle ainsi que les sorties d'horloge sont des extensions pour le NT.

## CV, MIDI et synchronisation

Le routage expose des actions par voix et communes, ainsi que six entrées CV partagées pour Count, Width, Interval, Slope, Shape et Amplitude. Le chemin DROID conserve ses cibles et profondeurs par voix ; les mappings CV/MIDI natifs du NT demeurent disponibles.

Quatre sources de cadence sont proposées : **interne**, **CV**, **MIDI** et **interne + CV**. Le dernier mode conserve deux sources indépendantes pour produire des rythmes combinés ; il ne suppose pas que le LFO interne soit asservi à la cadence externe. Le mode MIDI traite l'horloge et le transport comme des messages système, distincts du canal des notes et CC.

Les sorties CV permettent de router l'enveloppe, le gate de chaque burst, l'état actif de la séquence, l'impulsion EOC et l'horloge. L'état actif reste haut lorsqu'une séquence armée attend sa prochaine horloge. Les bus optionnels doivent pouvoir être désactivés et les sorties respecter le mode d'addition ou de remplacement configuré.

Le contrôle MIDI couvre les notes de déclenchement, les CC de contrôle et les événements d'horloge/transport. Les sorties MIDI couvrent des notes associées aux bursts, une représentation du niveau par CC et une horloge. L'émission de CC doit être limitée en cadence et aux changements utiles ; une sortie MIDI ne possède pas la résolution temporelle ou d'amplitude de la sortie CV. Les changements de configuration, le reset et les messages d'arrêt doivent laisser les notes dans un état cohérent.

## Réexamen des modulations du 17 octobre 2026

La [fiche AJH relue le 17 octobre 2026](https://ajhsynth.com/MultiBurst.html) confirme l'intérêt des changements de durée, de cadence et de forme pendant la séquence, ainsi que des clocks irrégulières combinées à la source interne. Le mode Absolute et Internal + CV porte ce principe dans le NT. La fiche ne permet pas de déduire une équation exacte de réponse CV.

Le patch DROID fourni quantifie ses temps sur seize rapports, capture count et pente au trigger, mais laisse durée, forme et amplitude agir en direct. Ses destinations principalement unipolaires et sa pente multiplicative peuvent réduire l'excursion d'un LFO bipolaire. La [comparaison détaillée](reference-comparison.md) sépare ces comportements des possibilités AJH publiées.

L'amélioration NT ajoute un choix commun Bipolar additive : les tensions signées agissent autour du réglage, avec variation exponentielle des temps Absolute. Shape response=Morph ajoute un fondu continu entre profils voisins. Ces deux réponses sont des conventions nouvelles de cette implémentation ; elles ne sont pas annoncées comme des caractéristiques du module AJH. Les modes DROID et Legacy restent disponibles. Le nouveau choix CV commun est ajouté en fin de paramètres ; les banques de voix restent sur 25 valeurs et mémorisent Morph dans le champ de courbes existant.

## Syntaxe et bonnes pratiques du SDK

Le plugin exporte `pluginEntry`, qui annonce explicitement son contrat API 13 sur matériel et fournit sa `_NT_factory`. Les callbacks déclarent les besoins mémoire, construisent l'instance dans les allocations de l'hôte, traitent les changements de paramètres, calculent les blocs et reçoivent les messages MIDI. Les pointeurs de définitions et de pages de paramètres doivent être initialisés lors de la construction. Les signatures du [SDK figé](https://github.com/expertsleepersltd/distingNT_API/blob/f440d20c2a96a9ca43892c7418a1d8185ad47eb5/include/distingnt/api.h) priment sur les exemples secondaires.

Le SDK définit **64 bus logiques** : 12 entrées, 8 sorties et 44 auxiliaires. Les numéros affichés à l'utilisateur commencent à 1 ; les indices du tampon commencent à 0. Une valeur de routage 0, lorsqu'elle représente une connexion désactivée, doit être validée avant toute soustraction. Le commentaire historique de `step()` mentionnant 28 bus n'est pas cohérent avec les constantes actuelles.

`numFramesBy4` représente le nombre de trames divisé par quatre ; le traitement utilise donc `numFramesBy4 * 4`. Les bus se suivent en mémoire, chacun contenant ce nombre de trames. Les temporisations utilisent `NT_globals.sampleRate`, sans supposer une fréquence fixe de 48 kHz ni une taille fixe de bloc.

Le [Makefile des exemples officiels](https://github.com/expertsleepersltd/distingNT_API/blob/f440d20c2a96a9ca43892c7418a1d8185ad47eb5/examples/Makefile) utilise Cortex-M7, Thumb, `fpv5-d16`, ABI flottante matérielle et `-fPIC`, sans RTTI ni exceptions. Le livrable est un objet ARM relogeable `.o` ; sa structure, son symbole `pluginEntry`, ses sections et ses références externes doivent être inspectés après compilation.

Pour le calcul en temps réel :

- Utiliser des états et files de capacité fixe, et réserver la mémoire persistante avec `calculateRequirements()`.
- Éviter allocation dynamique, accès disque ou réseau, attente, journalisation et boucles non bornées dans le calcul audio et les callbacks de contrôle.
- Calculer les coefficients lorsqu'un paramètre change ; utiliser des tables constantes et une interpolation légère lorsque cela simplifie les courbes.
- Borner indices, compteurs, valeurs CV, fréquences et données MIDI avant utilisation ; gérer explicitement les valeurs non finies.
- Détecter les fronts CV avec hystérésis et définir l'ordre reset, trigger puis horloge.
- Préserver les indices de paramètres après une publication ; ajouter les futurs paramètres à la fin du tableau.
- Séparer les contrôles natifs, la compilation ARM, l'émulation et les essais physiques : chacun apporte une preuve différente.

## Apport du skill de thorinside

Le [skill disting-nt-cpp-plugin-writer de thorinside](https://github.com/thorinside/nt_mcp/blob/3047bb232d9487fe075d48d12f48115383adf728/claude-plugin/skills/disting-nt-cpp-plugin-writer/SKILL.md) est importé dans `third_party/nt_mcp`, à la révision **3047bb232d9487fe075d48d12f48115383adf728**. Il recommande notamment des contrats de comportement explicites, une mémoire fixe, un calcul borné, le traitement cohérent des bus, des tests de transitions, l'inspection de l'objet ARM et une documentation vérifiée contre le code.

Son socle déclaré est **l'API 13**, alignée sur le SDK désormais épinglé et sur le contrat du plugin matériel. Ses instructions de méthode servent de guide ; les déclarations du SDK officiel fixent les signatures et les constantes. Les conseils concernant les allocations polyphoniques ne s'appliquent que lorsqu'une véritable architecture polyphonique est utilisée.

## Validation restant à effectuer

Les essais doivent couvrir les séquences de 1 et 40 bursts, les deux pentes extrêmes, ONE/RET jusque dans la dernière enveloppe, les modifications actives de paramètres, l'absence d'horloge, les sources combinées, les événements simultanés, le reset, les messages d'arrêt et les changements de routage MIDI/CV.

**Aucune validation matérielle sur disting NT ni comparaison mesurée avec le module AJH n'a encore été réalisée.** La compatibilité réelle au chargement, la charge processeur avec d'autres algorithmes, les seuils CV, les tensions aux prises et la stabilité de la synchronisation externe doivent être confirmées sur le module avant une publication destinée à l'usage en production.
