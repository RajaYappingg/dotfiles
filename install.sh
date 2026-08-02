#!/usr/bin/env bash
# Hyprland AMOLED Dotfiles Installer Script
set -e

DOTFILES_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "🚀 Installing Hyprland AMOLED Dotfiles..."

# Create target directories
mkdir -p "$HOME/.config/hypr"
mkdir -p "$HOME/.config/waybar"
mkdir -p "$HOME/.config/dunst"
mkdir -p "$HOME/.local/bin"

# Copy configurations
echo "📦 Copying configuration files..."
cp -r "$DOTFILES_DIR/config/hypr/"* "$HOME/.config/hypr/"
cp -r "$DOTFILES_DIR/config/waybar/"* "$HOME/.config/waybar/"
cp -r "$DOTFILES_DIR/config/dunst/"* "$HOME/.config/dunst/"

# Copy scripts and set permissions
echo "🔧 Copying scripts to ~/.local/bin..."
cp -r "$DOTFILES_DIR/local/bin/"* "$HOME/.local/bin/"
chmod +x "$HOME/.local/bin/"*

echo "✅ Installation complete!"
echo "Press Super+R for Launcher, Super+L for Lockscreen, Super+W for Wallpaper Picker."
