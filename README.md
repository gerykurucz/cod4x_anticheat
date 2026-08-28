# Nektum Shield

**Version:** 3.2  
**Developer:** XV9K ([@sudoxv9k](https://github.com/sudoxv9k))  
**Platform:** Call of Duty 4: Modern Warfare (COD4X server)

A comprehensive server‑side anti‑cheat and administration plugin for COD4X. It monitors player behavior in real time, detects cheats, manages bans, mutes, and persistent identities, and logs important events to Discord.

---

## Features

### Anti‑Cheat Detections
- **Aimbot / Snap** – sudden unrealistic view angle changes
- **No‑Recoil** – pitch variance and yaw stability analysis
- **Silent Aim** – mismatch between shot direction and target position
- **No‑Spread** – hit dispersion pattern analysis
- **Target Switch** – rapid camera turns between kills
- **Macro / Rapid Fire** – click timing, rate, and consistency checks
- **ESP / Wallhack suspicion** – tracking of occluded enemies, pre‑aiming

All detections use a scoring system (configurable via cvars) and trigger automatic bans when thresholds are exceeded.

### Administration Tools
- Ban by name, PlayerID, Nektum ID, or IP subnet
- Unban using the same identifiers
- Search users in online and offline databases
- Mute / unmute players
- CGNAT detection – prevents subnet bans for mobile networks

### Identity Management
- Each player receives a unique **Nektum ID**
- Name theft prevention (duplicate names cause kick)
- Ban evasion detection (new account with same name/subnet)

### Persistent Storage
- `nektumshield_banlist.txt`
- `nektumshield_users.txt`
- `nektumshield_mutes.txt`
- `nektumshield.log`

### Discord Webhook
- Automatic notifications for detections, bans, mutes
- Webhook URL configurable via `ns_webhook_url` cvar (no recompilation needed)

---

## Installation

1. Download or compile the plugin (see below).
2. Place the compiled `anticheat.so` (Linux) or `anticheat.dll` (Windows) into the server's `plugins` folder.
3. Restart the server or load the plugin manually.
4. The plugin creates all data files in the server's `fs_homepath` directory.

---

## Configuration

All detection settings are cvars prefixed with `ac_`. The webhook uses `ns_webhook_url`.

All configurable cvars are included in cfg/server.cfg

---

## Commands

### Chat (in‑game, power ≥ 80)
- `!bb <target> <reason>` – ban a player
- `!ub <target>` – unban
- `!fu <target>` – find user

**Target can be:** name (partial), PlayerID, Nektum ID, IP/subnet.

For using these admin tools B3 is required, use b3/codxcommands.py in your B3 setup.
If you skip this, automatic detections will still work, but these manual admin tools won't.

---

## Building on Linux

The plugin is 32‑bit (COD4X server requirement).

### Requirements
- GCC with 32‑bit support
- 32‑bit glibc development headers
- COD4X Plugin SDK (`api/` folder)
- Cloning the repository

### Debian / Ubuntu

```bash
sudo dpkg --add-architecture i386
sudo apt update
sudo apt install gcc-multilib libc6-dev-i386
cd cod4x_anticheat
make clean && make`

### Fedora

```bash
sudo dnf install glibc-devel.i686 libgcc.i686
cd cod4x_anticheat
make clean && make`

### Arch

```bash
sudo pacman -S lib32-gcc-libs lib32-glibc`
cd cod4x_anticheat
make clean && make`

Output: build/anticheat.so

### Troubleshooting

No Discord messages: Verify ns_webhook_url is set and the webhook is valid.

Plugin doesn't load: Ensure file is in plugins and the server is 32‑bit.

Too many false positives: Increase thresholds / decrease sensitivity in cvars.

Banlist not saved: Check write permissions in fs_homepath.

## Credits

Developer: XV9K / @sudoxv9k

Platform: COD4X (https://cod4x.me)

Disclaimer: Server‑side anti‑cheat sofwares are safe to use but does not provide 100% protection.
Use alongside other security layers. With using this software you agree with the licensing.