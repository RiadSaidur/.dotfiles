#!/usr/bin/env bash
# Apply green Papirus folder color (matches vine theme). Needs sudo once.
set -euo pipefail
sudo papirus-folders -C green --theme Papirus-Dark
echo "Done — restart Nautilus or log out for folder icons to update."
