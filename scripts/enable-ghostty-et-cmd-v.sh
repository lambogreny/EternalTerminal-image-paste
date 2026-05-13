#!/bin/sh
set -eu

KEYBIND='keybind = cmd+v=text:\x1b]777;et-paste\x07'

if [ -n "${GHOSTTY_CONFIG_FILE:-}" ]; then
  config_file="$GHOSTTY_CONFIG_FILE"
else
  config_dir="${XDG_CONFIG_HOME:-$HOME/.config}/ghostty"
  if [ -f "$config_dir/config.ghostty" ]; then
    config_file="$config_dir/config.ghostty"
  else
    config_file="$config_dir/config"
  fi
fi

config_dir=$(dirname "$config_file")
mkdir -p "$config_dir"
touch "$config_file"

if grep -Fqx "$KEYBIND" "$config_file"; then
  echo "Ghostty Cmd+V keybind is already enabled:"
  echo "  $config_file"
  exit 0
fi

backup_file="$config_file.bak-et-cmd-v-$(date +%Y%m%d-%H%M%S)"
cp "$config_file" "$backup_file"

{
  echo
  echo "# EternalTerminal image/text paste trigger for Cmd+V"
  echo "$KEYBIND"
} >>"$config_file"

echo "Enabled Ghostty Cmd+V keybind for patched EternalTerminal."
echo "Config: $config_file"
echo "Backup: $backup_file"
echo "Reload Ghostty with Cmd+Shift+, or restart Ghostty."
