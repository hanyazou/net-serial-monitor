# Net & Serial Monitor (Raspberry Pi OS, C++/FLTK)

## Purpose
A tiny GUI utility for Raspberry Pi 4 that repeatedly runs two background checks and shows their live status:

- **Network reachability** via `test_network.sh`
- **Serial connectivity** via `test_serial.sh`

The window displays:
- A one-line status string, e.g. `network=OK, serial=OK`
- Three traffic-light style filled circles (left to right):
  - **network** (green=OK, red=down, gray=unknown at startup)
  - **serial** (green=connected, red=failed, gray=unknown at startup)
  - **reserved** (always gray for future use)
- An **[Exit]** button to quit.

## Runtime Environment
- Raspberry Pi 4
- Raspberry Pi OS (Bullseye/Bookworm)
- X11 desktop available (FLTK requires a running GUI session)
- `test_network.sh` and `test_serial.sh` installed in the same **install bindir** as this app (typically `/usr/local/bin`) or somewhere in PATH, and returns exit code **0** on success

## Build Instructions

### Install dependencies
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libfltk1.3-dev
```

### Configure & build
```bash
# From the project root containing CMakeLists.txt and main.cpp
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### Run (without installing)
```bash
./build/net_serial_monitor
```

---

## Install (binary, desktop entry, icon, helper script)

The project supports `cmake --install` to place files under your chosen prefix (default: `/usr/local`).

```bash
# Install everything (requires sudo for system locations)
sudo cmake --install build
```

What gets installed (typical paths):
- App binary: `/usr/local/bin/net_serial_monitor`
- Desktop entry: `/usr/local/share/applications/net-serial-monitor.desktop`
- Icon (PNG): `/usr/local/share/pixmaps/net-serial-monitor-128.png`
- Helper script: `/usr/local/bin/test_network.sh` and `/usr/local/bin/test_serial.sh`

> The app first searches your PATH for the sctrips.
> If not found there, it falls back to /usr/local/bin and then /usr/bin.
> If still not found, the status is set to unknown and the worker thread exits.

### Show in GUI menus
Placing the `.desktop` file is enough. If it does not appear immediately:
```bash
lxpanelctl restart   # or log out/in
```
`update-desktop-database` is **not required** unless you add a `MimeType=` to the desktop file.

---

## test_serial.sh (helper script)

The app calls:
```bash
test_serial.sh
```
**Return code semantics**
- `0` → serial is considered **connected** (green)
- non-zero → **failed** (red)

**Minimal example script** (put under `scripts/test_serial.sh` in the repo; it will be installed executable):
```sh
#!/bin/sh
# Simple probe: open the device and do a quick check (customize as needed)
DEV="${1:-/dev/ttyUSB0}"

# Example: just test we can open it for reading/writing
if [ -c "$DEV" ] && exec 3<>"$DEV"; then
  # Optional: send a probe and read a short reply here
  # printf 'PING\r\n' >&3; sleep 0.1; head -c 1 <&3 >/dev/null 2>&1
  exec 3>&-
  exit 0
fi

exit 1
```

---

## Usage
Launch from the **Application Menu** (after install) or run directly:
```bash
net_serial_monitor
```

The UI updates several times per second:
- **Network** circle reflects the `test_network.sh`.
- **Serial** circle reflects the exit code of `test_serial.sh`.
- **Reserved** stays gray.

Click **[Exit]** to stop workers and close the window.

---

## Configuration & Customization

- **Change ping target**  
  Edit `test_network.sh` in `scripts` and replace the IP address.

- **Change serial device or command**  
  Edit `test_serial.sh` in `scripts` and replace the IP address.

- **Install prefix**  
  Use `-DCMAKE_INSTALL_PREFIX=/opt/netmon` to install elsewhere. The app embeds the bindir so it can find `test_serial.sh`.

- **Desktop category/icon**  
  Edit `misc/net-serial-monitor.desktop` and `misc/net-serial-monitor-128.png` as desired.

---

## Troubleshooting

- **Menu item does not appear**
  - Ensure `.desktop` file installed under `/usr/local/share/applications/`.
  - Run `lxpanelctl restart` or re-login.
  - Check `Exec=` path exists and is executable.

- **Serial status stays `unknown`**
  - Ensure `/usr/local/bin/test_serial.sh` (or your install bindir) exists and is executable (`chmod +x`).
  - Confirm it has a valid shebang (e.g., `#!/bin/sh`) and returns **0** on success.
  - If you installed to a non-default prefix, rebuild/reinstall so the embedded bindir matches.

- **Serial shows `failed` (red)**
  - Your script returned non-zero. Test manually:
    ```bash
    /usr/local/bin/test_serial.sh; echo $?
    ```
  - Check permissions: your user may need to be in group `dialout`:
    ```bash
    sudo usermod -aG dialout "$USER"
    # log out/in for group change to take effect
    ```

- **Network shows `down` (red)**
  - Verify `192.168.0.1` is reachable in your network or change the target.
  - Confirm `ping` exists: `which ping`.

- **No GUI**
  - Ensure you are in a graphical X11 session on Raspberry Pi OS.

---

## Uninstall

```bash
# Remove installed files (adjust if you changed prefix)
sudo rm -f /usr/local/bin/net_serial_monitor
sudo rm -f /usr/local/bin/test_serial.sh
sudo rm -f /usr/local/share/applications/net-serial-monitor.desktop
sudo rm -f /usr/local/share/pixmaps/net-serial-monitor.png

lxpanelctl restart  # refresh menu
```

---

## Project Structure (suggested)
```
.
├─ CMakeLists.txt
├─ main.cpp
├─ misc/
│  ├─ net-serial-monitor.desktop
│  └─ net-serial-monitor.png
└─ scripts/
   ├─ test_network.sh
   └─ test_serial.sh
```

---

## Notes
- The app never runs blocking probes on the UI thread.
- `update-desktop-database` is optional and only relevant if you add `MimeType=`.
- Icons under `pixmaps` do not require `gtk-update-icon-cache`.
