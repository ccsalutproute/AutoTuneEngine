# VoxTune 🎤

Plugin de correction de hauteur (type « auto-tune ») en **VST3** pour **FL Studio** (Windows et macOS), aussi disponible en AU (macOS) et en application autonome.

Du réglage naturel et transparent jusqu'à l'effet robotique « hard-tune ».

## Fonctionnalités

- **Détection de hauteur YIN** avec raffinement à pleine résolution (précision de l'ordre de quelques cents) et filtrage des erreurs d'octave.
- **Transposition TD-PSOLA** : la voix est déplacée période par période, ce qui **préserve les formants**. Pas d'effet « chipmunk ».
- **13 gammes** : chromatique, majeure, mineures (naturelle, harmonique, mélodique), pentatoniques, blues, modes (dorien, phrygien, lydien, mixolydien) et **gamme personnalisée** (clic sur les notes).
- **Retune Speed** : 0 ms pour l'effet T-Pain, 20 à 80 ms pour une correction naturelle qui garde le vibrato.
- **Stabilité** (hystérésis) : évite que la note saute entre deux notes voisines.
- **Correction** (0–100 %), **Transpose** (±12 demi-tons), **Mix** dry/wet, **Diapason** (430–450 Hz).
- **Type de voix** : Soprano, Alto/Ténor, Baryton, Basse, Instrument.
- **Graphique en temps réel** de la hauteur chantée (gris) et corrigée (vert), avec un accordeur.
- Latence d'environ 50 ms, **déclarée à FL Studio, qui la compense automatiquement**.
- Faible charge CPU : environ 1,5 % d'un cœur à 44,1 kHz.

## Télécharger (sans rien compiler)

GitHub Actions compile automatiquement le plugin à chaque modification :

1. Va dans l'onglet **Actions** du dépôt, puis ouvre le dernier build réussi (✅).
2. En bas de la page, télécharge **VoxTune-Windows** (ou **VoxTune-macOS**).
3. Pour une version officielle, crée un tag `v1.0.0` : le zip apparaît dans **Releases**.

## Installation dans FL Studio

**Windows**
1. Décompresse le zip.
2. Copie le dossier `VoxTune.vst3` dans `C:\Program Files\Common Files\VST3\`.
3. Dans FL Studio : **Options → Manage plugins → Find installed plugins**.
4. Sur la piste de voix du Mixer, ouvre un slot et choisis **VoxTune**.

**macOS** (Apple Silicon et Intel) — **méthode rapide** : colle cette ligne dans le Terminal, puis appuie sur Entrée :
```bash
curl -fsSL https://raw.githubusercontent.com/ccsalutproute/AutoTuneEngine/main/install-mac.sh | bash
```

Méthode manuelle :
1. Décompresse le zip.
2. Dans le Finder, menu **Aller → Aller au dossier…**, tape `~/Library/Audio/Plug-Ins/`.
3. Copie `VoxTune.vst3` dans le dossier `VST3` et `VoxTune.component` dans le dossier `Components`.
4. Le plugin n'est pas notarisé par Apple : retire le blocage de macOS en collant ceci dans le **Terminal** :
   ```bash
   xattr -cr ~/Library/Audio/Plug-Ins/VST3/VoxTune.vst3 ~/Library/Audio/Plug-Ins/Components/VoxTune.component
   ```
5. Dans FL Studio : **Options → Manage plugins → Find installed plugins**, puis ajoute VoxTune sur la piste de voix du Mixer.

## Réglages conseillés

| Style | Retune Speed | Correction | Stabilité |
|---|---|---|---|
| Hard-tune / T-Pain | 0 ms | 100 % | 0–10 c |
| Pop moderne | 10–25 ms | 100 % | 15 c |
| Naturel / transparent | 50–120 ms | 70–90 % | 20–30 c |

Choisis toujours la **tonalité et la gamme du morceau**. En cas de doute, commence en chromatique.
Place VoxTune **en premier** dans la chaîne (avant la compression et la réverbération), sur une voix **mono et sèche**.

## Compiler soi-même

Il faut CMake 3.22 ou plus récent, ainsi que Visual Studio 2022 (Windows) ou Xcode (macOS). JUCE est téléchargé automatiquement.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Le VST3 se trouve dans `build/VoxTune_artefacts/Release/VST3/`.

## Structure

```
Source/AutoTuneEngine.h    Moteur DSP (YIN + quantification + PSOLA), C++ pur
Source/PluginProcessor.*   Paramètres, liaison avec l'hôte
Source/PluginEditor.*      Interface graphique
tests/engine_test.cpp      Test automatique de justesse (lancé par la CI)
.github/workflows/         Build Windows + macOS
```

## Limites

- Traitement mono : les canaux sont additionnés, puis le résultat est envoyé sur la sortie stéréo. C'est le cas normal pour une voix.
- Pas encore d'entrée MIDI pour imposer les notes cibles.
