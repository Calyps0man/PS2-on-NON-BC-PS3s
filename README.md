# PS2 Disc Cache

# UPDATE 19.11.2025: version 0.3

- **Dual-layer DVDs are copied completely.** The ISO header on dual-layer PS2 discs only covers layer 0, so 0.2 still copied just the first layer. The plugin now reads the disc's real size from the drive and copies both layers into one ISO. Tested with Gran Turismo 4 (PAL): 5.66 GB.
- Existing ISOs of dual-layer games are also recognized by their file size, whether this plugin or another tool made them.
- **PS2 CD support.** CDs are copied as a 2048-byte ISO of the data track, and Cobra mounts the ISO as a PS2 CD. CD audio tracks are not copied yet, so the few CD games that play music from audio tracks will be silent.
- The log shows the drive capacity next to the sector count.
- The log records the detected disc type (CD or DVD) and size.
- The boot notification now says "Insert a PS2 disc."

# Original release

A VSH plugin for PS3 custom firmware that copies an original PS2 DVD to the internal HDD as an ISO, then mounts that ISO automatically through webMAN-MOD whenever you insert the same disc again.

> [!WARNING]
> **Highly experimental.** Tested on a single PS3 Slim CFW with 2 games. Use it at your own risk.
>
> **CFW only.** It requires Cobra custom firmware (tested on Evilnat 4.93 CEX). It does **not** work on PS3HEN: enabling HEN hangs while this plugin is listed in `boot_plugins.txt`.
>
> It uses internal ps2_netemu emulator with all its limitations. It will not magically make your PS2 games perform at 60 fps with 4k resolution.

## What it does

1. You insert a PS2 DVD.
2. The plugin reads the disc's serial (for example `SLES_526.70`) and looks for a matching ISO in `/dev_hdd0/PS2ISO`.
3. **If a matching ISO exists:** it mounts it silently through webMAN-MOD. Launch the disc icon in the Game column.
4. **If not:** a notification asks you to press **R3** within 30 seconds to copy the disc. It then copies, verifies and mounts the ISO.

While it copies, the progress stays on screen:

```
Copying 35% (1.5 / 4.3 GB)
Do not launch the game until the ISO is copied and verified.
Eject the disc to cancel.
```

When it's done, you'll see "matching ISO ready". Only then is it safe to launch the game.

## Requirements

- PS3 with **Cobra CFW** (tested: Evilnat 4.93 CEX)
- **webMAN-MOD** (used to mount the ISO)
- An original **PS2 DVD**. PS2 CDs are not supported.
- Enough free HDD space for the full disc, plus 64 MB

## Installation

1. Download `ps2_disc_cache.sprx` and `boot_plugins.txt` from the Releases page.
2. Copy `ps2_disc_cache.sprx` plugin to `/dev_hdd0/plugins/`.
3. Copy and replace `boot_plugins.txt` at `/dev_hdd0/`, or add this line into your existing txt file:
   ```
    /dev_hdd0/plugins/ps2_disc_cache.sprx
   ```
4. Restart the console.

> [!IMPORTANT]
> Do not attempt to load this plugin through webMAN's VSH menu as it locks up the XMB.

## Usage notes

- **Cancel a copy:** eject the disc. The partial file is deleted.
- **Copy speed:** about 4–6 MB/s, limited by the drive. Expect roughly 12–15 minutes for a 4.3 GB disc, plus a few minutes to verify.
- **Do not launch the game while it copies.** Without the ISO mounted, the system tries to boot the physical disc directly, which ends in a black screen on consoles without PS2 hardware.
- **No R3 press within 30 seconds:** the copy is skipped. Reinsert the disc to be asked again.

## ISO names

New copies are named after the disc's volume label:

```
JAK AND DAXTER [SLES_526.70].iso
```

If the label is empty, generic or just the serial, the name falls back to `SLES_526.70.iso`.

**You can rename or move ISOs freely.** Matching is done by content (serial, size and a fingerprint of the disc's data), not by file name. Any `.iso` in `/dev_hdd0/PS2ISO`, including subfolders up to four levels deep, is recognized. That also covers plain 2048-byte-sector ISOs made with other tools.

## Log

The plugin writes `/dev_hdd0/tmp/ps2disc_cache.log`, including copy speed at every 5%. If something goes wrong, the last lines show where.

## Known limitations

- **CFW only.** Not compatible with PS3HEN.
- **No real Yes/No dialog.** The system message dialog hangs the XMB when opened from a plugin, so confirmation uses R3 instead. The XMB reads the same controller, so a press can occasionally be missed; just press again.
- **webMAN's own popups** ("… loaded") may still appear when it mounts. You can turn those off in webMAN's setup page (mount info messages).
- **Single-layer PS2 DVDs** were tested. Dual-layer discs are untested.
- Only one console and 2 games have been tested so far.

## Building from source

Requirements: `powerpc64-linux-gnu-gcc` and `-ld` (for example, Debian/Ubuntu `gcc-powerpc64-linux-gnu`), Python 3 with `pyelftools`, and a [PSL1GHT](https://github.com/ps3dev/PSL1GHT) checkout for its headers.

```sh
PSL1GHT=/path/to/PSL1GHT python3 build.py   # -> build/ps2_disc_cache.prx (unsigned)
```

Tests:

```sh
python3 tests/verify_prx.py               # PRX structure, relocations, imports
python3 tests/tocchk.py build/plugin.elf  # TOC-relative accesses
cd tests && gcc -I. test_copy.c -o test_copy && ./test_copy
gcc test_identity.c -o test_identity && ./test_identity
```

Repository layout:

| Path | Contents |
| --- | --- |
| `src/plugin.c` | The plugin: disc detection, R3 prompt, copy, verify, mount |
| `src/identity.h` | PS2 disc identification (ISO 9660, `SYSTEM.CNF`, fingerprint) |
| `src/module.S`, `src/module.ld` | Module info, exports, PRX imports, linker script |
| `build.py`, `build_prx.py` | Build, and conversion of the linked ELF into a PRX |
| `shim/` | Minimal libc headers so the PSL1GHT headers compile freestanding |
| `tests/` | Host tests and PRX checks |

Technical notes:
- System functions are linked as real PRX imports, using PSL1GHT's import-stub layout.
- Program headers match webMAN-MOD's `webftp_server.sprx`.
- `.toc` is aligned to 256 bytes, so the exported TOC base matches the linker's.

## Credits

- **[Calyps0man](https://github.com/Calyps0man)**: idea, project lead, all hardware testing
- **Claude** (Anthropic): plugin code, build tooling and tests
- **ChatGPT** (OpenAI): the initial prototype this build grew from
- [PSL1GHT](https://github.com/ps3dev/PSL1GHT): headers and the PRX import layout
- [webMAN-MOD](https://github.com/aldostools/webMAN-MOD): ISO mounting, and the reference plugin layout
- Cobra / Evilnat CFW
