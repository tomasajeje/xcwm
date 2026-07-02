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

To compile `xcwm`, you need the X11 development headers and a C compiler (`gcc`):

* **Debian/Ubuntu:** `sudo apt install libx11-dev gcc make alacritty dmenu wireplumber brightnessctl`
* **Arch Linux:** `sudo pacman -S libx11 base-devel alacritty dmenu wireplumber brightnessctl`
* **Fedora:** `sudo dnf install libX11-devel gcc make alacritty dmenu wireplumber brightnessctl`
* **Void Linux:** `sudo xbps-install -S libX11-devel gcc make alacritty dmenu wireplumber brightnessctl`

## Polybar Setup (Recommended)

In your Polybar configuration file (`~/.config/polybar/config.ini`), make sure to add these lines under your main bar section (e.g., `[bar/your_bar_name]`):

```ini
[bar/your_bar_name]
dock = true
wm-restack = bspwm
```

## Installation

```bash
git clone https://codeberg.org/tomasajeje/xcwm.git
cd xcwm
make
sudo/doas make install
