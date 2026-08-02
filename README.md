# 🌌 Hyprland AMOLED Minimalist Dotfiles

Sleek, modern, and high-performance **AMOLED Minimalist Dotfiles** for **Hyprland** on Arch Linux. Featuring custom GTK3 Python layer-shell popups, interactive CAVA audio spectrum visualizer, GTK wallpaper picker, dashboard app launcher, and a minimal aesthetic lockscreen.

---

## 🎨 Features & Highlights

- **Aesthetic Waybar**: Floating black-and-white AMOLED pill design with interactive process tooltips (`top 5 CPU`, `top 5 RAM`, `CPU Temp`, `Battery Status`).
- **Real-Time Volume Menu & Audio Visualizer**: Interactive GTK volume slider integrated with a 60 FPS **CAVA** audio spectrum visualizer.
- **Dashboard Launcher (`Super + R`)**: Fast custom application launcher with weather forecast, system stats, favorite web shortcuts, volume & brightness sliders, and MPRIS media player controls.
- **Wallpaper Picker (`Super + W`)**: Clean GTK grid thumbnail wallpaper picker powered by `awww`/`swww`.
- **Aesthetic Lockscreen (`Super + L`)**: Stacked silver gradient clock, minimal date, and pill password bar with smooth fade-in/fade-out animations and non-blocking `sudo` authentication.
- **Dunst Notifications**: Crisp dark AMOLED notification theme with Nerd Font icons.
- **Pixel-Perfect 1080p Resolution**: Native 1:1 scaling with `force_zero_scaling` for zero blurriness.

---

## ⌨️ Keybindings

| Keybinding | Action |
| --- | --- |
| `Super + Q` | Open Terminal (`kitty`) |
| `Super + E` | Open File Manager (`dolphin`) |
| `Super + R` | Toggle Dashboard App Launcher |
| `Super + W` | Open Wallpaper Picker |
| `Super + L` | Lock Screen |
| `Super + V` | Toggle Window Floating Mode |
| `Super + C` | Close Active Window |
| `Super + Shift + S` | Take Region Screenshot (copies to clipboard) |

---

## 📦 Dependencies

Ensure the following packages are installed on Arch Linux:

```bash
sudo pacman -S hyprland waybar dunst cava kitty dolphin python-gobject gtk3 gtk-layer-shell python-cairo python-pillow pam
```

---

## 🚀 One-Line Quick Installation

Clone this repository and run the automated installer:

```bash
git clone https://github.com/YOUR_GITHUB_USERNAME/dotfiles.git ~/.dotfiles
cd ~/.dotfiles
chmod +x install.sh
./install.sh
```

---

## 📁 Repository Structure

```
.
├── config/
│   ├── hypr/
│   │   └── hyprland.lua       # Main Hyprland Lua Configuration
│   ├── waybar/
│   │   ├── config.jsonc       # Waybar Module Layout
│   │   └── style.css          # Waybar AMOLED CSS Styling
│   └── dunst/
│       └── dunstrc            # Dunst Notification Theme
├── local/
│   └── bin/                   # Custom Python & GTK Utilities
│       ├── dashboard-launcher # App Launcher & Dashboard
│       ├── volume-menu        # CAVA Audio Spectrum & Volume Slider
│       ├── lockscreen         # Aesthetic Stacked Clock Lockscreen
│       ├── wallpaper-picker   # Grid Wallpaper Selection Menu
│       ├── battery-info       # Battery Details Notification
│       ├── take-screenshot    # Interactive Screenshot Utility
│       ├── waybar-cpu         # CPU Usage & Top Processes Script
│       ├── waybar-memory      # RAM Usage & Top Processes Script
│       └── waybar-temp        # CPU Temperature Monitor Script
├── install.sh                 # Automatic Installation Script
└── README.md                  # Documentation
```

---

## 📄 License
Licensed under the [MIT License](LICENSE).
