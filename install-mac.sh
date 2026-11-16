#!/bin/bash
# Installe VoxTune (VST3 + AU) pour FL Studio sur macOS.
# Usage : curl -fsSL https://raw.githubusercontent.com/ccsalutproute/AutoTuneEngine/main/install-mac.sh | bash
set -e

URL="https://github.com/ccsalutproute/AutoTuneEngine/releases/latest/download/VoxTune-macOS.zip"
VST3="$HOME/Library/Audio/Plug-Ins/VST3"
AU="$HOME/Library/Audio/Plug-Ins/Components"
TMP="$(mktemp -d)"

echo "⬇️  Téléchargement de VoxTune..."
curl -fsSL "$URL" -o "$TMP/VoxTune.zip"
unzip -q "$TMP/VoxTune.zip" -d "$TMP"

echo "📦 Installation..."
mkdir -p "$VST3" "$AU"
rm -rf "$VST3/VoxTune.vst3" "$AU/VoxTune.component"
cp -R "$TMP/VoxTune.vst3" "$VST3/"
cp -R "$TMP/VoxTune.component" "$AU/"
xattr -cr "$VST3/VoxTune.vst3" "$AU/VoxTune.component"

# Anciennes copies (bloquées par macOS) dans le dossier système
for f in /Library/Audio/Plug-Ins/VST3/VoxTune.vst3 /Library/Audio/Plug-Ins/Components/VoxTune.component; do
  if [ -e "$f" ]; then
    echo "🧹 Suppression d'une ancienne copie : $f"
    echo "   (tape le mot de passe de ton Mac puis Entrée — il ne s'affiche pas, c'est normal)"
    sudo rm -rf "$f"
  fi
done

rm -rf "$TMP"
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo ""
echo "✅ VoxTune est installé !"
echo "   Relance FL Studio (Cmd+Q puis rouvrir), puis Options → Manage plugins → Find installed plugins."
