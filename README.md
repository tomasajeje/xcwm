# Xorg Coordinates Window Manager (xcwm)
An Xorg hybrid window manager combining dynamic tiling and infinite panning.
## Features
* **Hybrid Layout:** Half tiling (with gaps), half panning/infinite canvas per workspace.
* **Independent Workspaces:** 3 tags that store their own layout mode and camera coordinates (`vx`, `vy`).
* **Sloppy Focus:** Window focus follows the mouse pointer.
* **Gaming Ready:** EWMH support (`_NET_WM_STATE_FULLSCREEN`) for unhindered fullscreen apps and games.
* **Media & Brightness Controls:** Built-in shortcuts for daily laptop usage.
## Keybindings
The modifier key is `Mod4` (Windows/Super key).
| Keybinding | Action |
| :--- | :--- |
| `Win + v` | Toggle between Tiling and Panning mode |
| `Win + f` | Toggle Fullscreen mode |
| `Win + j` / `k` | Cycle window focus |
| `Win + Shift + j` / `k` | Move focused window up/down in the stack |
| `Win + Shift + ← / → / ↑ / ↓` | Move focused window and camera together (Panning mode) |
| `Win + c` | Center camera on focused window (Panning mode only) |
| `Win + 1` / `2` / `3` | Switch workspace (Tag) |
| `Win + Shift + 1` / `2` / `3` | Move focused window to another Tag |
| `Win + q` | Launch terminal (`st`) |
| `Win + d` | Launch application launcher (`rofi`) |
| `Win + w` | Close focused window |
| `Win + Shift + e` | Quit xcwm |
| `Volume Up / Down / Mute` | Adjust system volume |
| `Brightness Up / Down` | Adjust screen brightness |
### Mouse Controls
**In Panning Mode:**
* `Win + Left Click + Drag`: Move window freely.
* `Win + Right Click + Drag`: Resize window.
* `Win + Shift + Right Click + Drag`: Move the camera across the infinite canvas.
**In Tiling Mode:**
* `Win + Left Click + Drag`: Drag and drop a window to swap its position in the dynamic tiling layout.
## Dependencies
To compile `xcwm`, you need the X11 development headers and a C compiler (`gcc`). We also recommend installing `dunst` for notifications:
* **Debian/Ubuntu:** `sudo apt install libx11-dev gcc make st rofi wireplumber brightnessctl dunst`
* **Arch Linux:** `sudo pacman -S libx11 base-devel st rofi wireplumber brightnessctl dunst` (install `st` from the AUR, e.g. `yay -S st`)
* **Fedora:** `sudo dnf install libX11-devel gcc make st rofi wireplumber brightnessctl dunst`
* **Void Linux:** `sudo xbps-install -S libX11-devel gcc make st rofi wireplumber brightnessctl dunst`
## Polybar Setup (Recommended)
In your Polybar configuration file (`~/.config/polybar/config.ini`), make sure to add these lines under your main bar section (e.g., `[bar/your_bar_name]`):
```ini
[bar/your_bar_name]
dock = true
wm-restack = generic
override-redirect = true
```
## Installation
```bash
git clone https://codeberg.org/tomasajeje/xcwm.git
cd xcwm
make
sudo/doas make install
```
## .xinitrc (Recommended)
To start `xcwm` along with your preferred tools, create or edit your `~/.xinitrc` file. Ensure the script is executable (`chmod +x ~/.xinitrc`).
```bash
#!/bin/sh
# Notification Daemon
dunst &
# Status Bar
polybar &
# Launch Window Manager
exec xcwm
```
