# Blurred Files

A fast, lightweight, borderless GTK4 file manager for Linux built with C and Cairo, featuring a custom superellipse silhouette, a 3D concave-curved warm orange header bar, and a translucent slate-charcoal sidebar designed to pair with Wayland compositor blur (such as Wayfire).

## Features

- **Custom Superellipse UI & 3D Depth:**
  - Borderless superellipse window silhouette (`n = 3.8` / `4.0`) with a custom multi-pass superellipse drop shadow and 8-way interactive edge resizing.
  - Translucent slate-charcoal sidebar rail that lets desktop blur shine through, paired with a 3D top-lit concave orange header bar.
  - Matching corner-matched squircle modals for **Properties**, **Rename**, **New Folder / New File**, **Open With**, and **Preferences**.
- **Flexible Navigation & Views:**
  - **Grid View & List View** toggle with sortable columns (**Name**, **Type**, **Modified**, **Size**).
  - **5-Tier Nautilus-Style Zoom (`Ctrl + Mouse Wheel` or `Ctrl + +/-/0`)** with automatic responsive column reflow.
  - Interactive breadcrumb trail with clickable `/` root pill, horizontal scroll wheel support, and right-click path copying.
  - Back/Forward history navigation (`Alt + Left/Right` or mouse side buttons 8 & 9).
  - Dynamic sidebar with quick-access places, pinned folders, mounted/unmounted drives (`GVolumeMonitor`), and live directory refresh (`GFileMonitor`).
- **Complete File Operations:**
  - Single-click or double-click open modes (configurable in Preferences).
  - Drag-and-drop onto folders, sidebar bookmarks, Trash, and across windows with a floating icon preview.
  - Whitespace rubberband marquee selection, `Ctrl + Click` multi-select, and `Ctrl + A` select all.
  - Copy, Cut, Paste, Rename, Move to Trash, Restore from Trash, and non-blocking background **Empty Trash**.
  - Built-in archive creation (`.zip` and `.tar.gz`) and one-click extraction (`Extract Here`).
  - Undo (`Ctrl + Z`) and Redo (`Ctrl + Y` / `Ctrl + Shift + Z`) history stack.
  - Asynchronous image thumbnail generation with disk/memory caching.

## Keyboard Shortcuts

| Shortcut | Action |
| :--- | :--- |
| `Ctrl + Mouse Wheel` | Zoom icon size & grid columns (5 discrete tiers) |
| `Ctrl + +` / `Ctrl + -` | Zoom in / Zoom out |
| `Ctrl + 0` | Reset zoom to default tier |
| `Ctrl + C` / `Ctrl + X` / `Ctrl + V` | Copy / Cut / Paste selected items |
| `Ctrl + Z` | Undo last move, rename, or trash action |
| `Ctrl + Y` / `Ctrl + Shift + Z` | Redo action |
| `Ctrl + Shift + N` | Create New Folder |
| `Ctrl + A` | Select all items in current folder |
| `Ctrl + F` | Focus search filter bar |
| `Ctrl + H` | Toggle hidden files (`.*`) |
| `Ctrl + Alt + T` | Open current directory in terminal |
| `Space` | Open Properties modal for selection or current folder |
| `Delete` | Move selected items to Trash (or delete permanently in Trash) |
| `Backspace` | Navigate to parent directory |
| `Alt + Left` / `Alt + Right` | Navigate Back / Forward |
| `Escape` | Clear selection or clear active search query |

## Dependencies

### openSUSE
```bash
sudo zypper install -y gcc make pkgconf-pkg-config gtk4-devel zip unzip tar
```

### Debian / Ubuntu
```bash
sudo apt update
sudo apt install -y build-essential pkg-config libgtk-4-dev zip unzip tar
```

## Building & Installing

### Build & Run Locally
```bash
make clean
make -j$(nproc)
make run
```

### Install System-Wide (`/usr/local`)
```bash
sudo make install
```

### Build `.rpm` or `.deb` Packages
```bash
# openSUSE / Fedora RPM
./build_rpm.sh

# Debian / Ubuntu DEB
./build_deb.sh
```

## Wayfire Blur & Shadow Configuration (Optional)

To enable background blur behind the translucent sidebar and disable Wayfire's default rectangular window shadow in favor of the built-in superellipse shadow, add the following to `~/.config/wayfire.ini`:

```ini
[blur]
method = kawase
kawase_degrade = 2
kawase_iterations = 3

[window-rules]
rule_blurred_files = on created if app_id is "com.github.blurredfiles" then set decoration none
```

## Configuration

User preferences (view mode, zoom tier, hidden files, thumbnails, single/double-click behavior, sort column, and pinned sidebar folders) are saved automatically to:

```text
~/.config/blurred_files/settings.ini
```

## License

MIT License