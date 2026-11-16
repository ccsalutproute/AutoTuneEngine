#!/bin/bash
# Installe / débloque VoxTune (VST3 + AU) pour FL Studio sur macOS.
# Usage : curl -fsSL https://raw.githubusercontent.com/ccsalutproute/AutoTuneEngine/main/install-mac.sh | bash
#
# 1. Si VoxTune est déjà dans le dossier des plugins : retire simplement le blocage de macOS.
# 2. Sinon : le cherche dans Téléchargements / Bureau (dossier ou zip), puis l'installe.
# 3. Sinon : le télécharge depuis la dernière Release GitHub (si elle existe).

VST3="$HOME/Library/Audio/Plug-Ins/VST3"
AU="$HOME/Library/Audio/Plug-Ins/Components"
URL="https://github.com/ccsalutproute/AutoTuneEngine/releases/latest/download/VoxTune-macOS.zip"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

mkdir -p "$VST3" "$AU"

newest() {  # newest() <dossier de recherche> <nom>  -> chemin le plus récent
  local best="" f
  while IFS= read -r -d '' f; do
    if [ -z "$best" ] || [ "$f" -nt "$best" ]; then best="$f"; fi
  done < <(find "$1" -maxdepth 6 -name "$2" -print0 2>/dev/null)
  printf '%s' "$best"
}

unpack() {  # décompresse un zip (et le zip qui est éventuellement dedans)
  unzip -q -o "$1" -d "$TMP/x" || return 1
  inner="$(find "$TMP/x" -name 'VoxTune-macOS*.zip' | head -n 1)"
  [ -n "$inner" ] && unzip -q -o "$inner" -d "$TMP/x"
  return 0
}

SRC_VST3=""; SRC_AU=""

if [ ! -d "$VST3/VoxTune.vst3" ]; then
  for dir in "$HOME/Downloads" "$HOME/Desktop"; do
    [ -z "$SRC_VST3" ] && SRC_VST3="$(newest "$dir" VoxTune.vst3)"
    [ -z "$SRC_AU" ]   && SRC_AU="$(newest "$dir" VoxTune.component)"
  done

  if [ -z "$SRC_VST3" ]; then
    ZIP=""
    for dir in "$HOME/Downloads" "$HOME/Desktop"; do
      [ -z "$ZIP" ] && ZIP="$(newest "$dir" 'VoxTune-macOS*.zip')"
    done
    if [ -z "$ZIP" ]; then
      echo "⬇️  Téléchargement de VoxTune..."
      curl -fsSL "$URL" -o "$TMP/dl.zip" 2>/dev/null && ZIP="$TMP/dl.zip"
    fi
    if [ -n "$ZIP" ] && unpack "$ZIP"; then
      SRC_VST3="$(find "$TMP/x" -name VoxTune.vst3 -type d | head -n 1)"
      SRC_AU="$(find "$TMP/x" -name VoxTune.component -type d | head -n 1)"
    fi
  fi

  if [ -z "$SRC_VST3" ]; then
    echo "❌ VoxTune introuvable. Télécharge d'abord « VoxTune-macOS » depuis l'onglet Actions"
    echo "   de https://github.com/ccsalutproute/AutoTuneEngine puis relance cette commande."
    exit 1
  fi

  echo "📦 Installation de VoxTune..."
  rm -rf "$VST3/VoxTune.vst3"; cp -R "$SRC_VST3" "$VST3/"
  if [ -n "$SRC_AU" ]; then rm -rf "$AU/VoxTune.component"; cp -R "$SRC_AU" "$AU/"; fi
fi

echo "🔓 Déblocage par macOS..."
for f in "$VST3/VoxTune.vst3" "$AU/VoxTune.component"; do
  [ -e "$f" ] && xattr -cr "$f"
done

# Copies éventuellement placées dans le dossier système
for f in /Library/Audio/Plug-Ins/VST3/VoxTune.vst3 /Library/Audio/Plug-Ins/Components/VoxTune.component; do
  if [ -e "$f" ] && ! xattr -cr "$f" 2>/dev/null; then
    echo "   Mot de passe du Mac demandé pour $f"
    echo "   (il ne s'affiche pas pendant que tu tapes, c'est normal — puis Entrée)"
    sudo xattr -cr "$f"
  fi
done

killall -9 AudioComponentRegistrar 2>/dev/null

echo ""
echo "✅ VoxTune est prêt !"
echo "   Dans FL Studio : Cmd+Q pour quitter, rouvre-le, puis"
echo "   Options → Manage plugins → coche « Rescan previously verified plugins » → Find installed plugins."
