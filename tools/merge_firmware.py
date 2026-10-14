#!/usr/bin/env python3
"""
Build the ESP Web Tools artefacts from what PlatformIO produced.

Two images are emitted, because merging the filesystem into the firmware image
would force esptool to pad the gap up to the spiffs partition - turning a
1.3 MB download into a 4 MB one:

  desk-buddy-firmware.bin   bootloader + partition table + application -> 0x0
  littlefs.bin              the on-device web panel -> the spiffs offset

manifest.json is regenerated as well, so the offsets here and the offsets the
browser uses can never drift apart.

Run tools/build_release.sh instead of calling this directly.
"""
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(ROOT, "firmware", ".pio", "build", "esp32-c3-super-mini")
PARTITIONS = os.path.join(ROOT, "firmware", "partitions.csv")
CONFIG_H = os.path.join(ROOT, "firmware", "include", "config.h")
OUT_DIR = os.path.join(ROOT, "web-flasher")

BOOTLOADER_OFFSET = 0x0        # ESP32-C3 puts the bootloader at 0x0
PARTITION_OFFSET = 0x8000
FLASH_SIZE = 0x400000          # 4 MB


def firmware_version():
    with open(CONFIG_H) as fh:
        m = re.search(r'#define\s+FW_VERSION\s+"([^"]+)"', fh.read())
    return m.group(1) if m else "0.0.0"


def partition_offsets():
    by_name, by_subtype = {}, {}
    with open(PARTITIONS) as fh:
        for raw in fh:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            cols = [c.strip() for c in line.split(",")]
            if len(cols) < 5:
                continue
            try:
                addr = int(cols[3], 16)
            except ValueError:
                continue          # offset left blank for the generator
            by_name[cols[0]] = addr
            by_subtype.setdefault(cols[2], addr)
    return by_name, by_subtype


def check(offset, size, placed, label):
    for other_off, other_size, other_name in placed:
        if offset < other_off + other_size and other_off < offset + size:
            raise SystemExit("%s at 0x%x overlaps %s at 0x%x - check partitions.csv"
                             % (label, offset, other_name, other_off))
    if offset + size > FLASH_SIZE:
        raise SystemExit("%s at 0x%x (+%d bytes) runs past the 4 MB flash"
                         % (label, offset, size))


def merge(out_name, entries):
    """entries: list of (offset, filename). Returns the output path."""
    out = os.path.join(OUT_DIR, out_name)
    cmd = [
        sys.executable, "-m", "esptool",
        "--chip", "esp32c3",
        "merge-bin",
        "-o", out,
        "--flash-mode", "dio",
        "--flash-freq", "80m",
        "--flash-size", "4MB",
    ]

    placed, total = [], 0
    for offset, name in entries:
        path = os.path.join(BUILD, name)
        if not os.path.exists(path):
            raise SystemExit(
                "%s missing - run `pio run`"
                "%s (looked in %s)"
                % (name, "" if name != "littlefs.bin" else " -t buildfs", BUILD)
            )
        size = os.path.getsize(path)
        check(offset, size, placed, name)
        placed.append((offset, size, name))
        cmd += [hex(offset), path]
        total += size
        print("    %-18s 0x%-7x %8d bytes" % (name, offset, size))

    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL)
    size = os.path.getsize(out)
    print("    -> %-15s %8d bytes (%.2f MB)\n"
          % (out_name, size, size / 1048576.0))
    return size


def write_manifest(parts, version):
    import json
    manifest = {
        "name": "Desk Buddy",
        "version": version,
        "new_install_prompt_erase": True,
        "builds": [
            {
                "chipFamily": "ESP32-C3",
                "improv": False,
                "parts": parts,
            }
        ],
    }
    path = os.path.join(OUT_DIR, "manifest.json")
    with open(path, "w") as fh:
        json.dump(manifest, fh, indent=2)
        fh.write("\n")
    return path


def main():
    by_name, by_subtype = partition_offsets()

    app_off = by_name.get("app0", by_subtype.get("ota_0"))
    fs_off = by_name.get("spiffs", by_subtype.get("spiffs"))
    if app_off is None or fs_off is None:
        raise SystemExit("partitions.csv is missing an app0 or spiffs entry")

    os.makedirs(OUT_DIR, exist_ok=True)
    version = firmware_version()
    print("  Desk Buddy %s\n" % version)

    print("  system image")
    merge("desk-buddy-firmware.bin", [
        (BOOTLOADER_OFFSET, "bootloader.bin"),
        (PARTITION_OFFSET, "partitions.bin"),
        (app_off, "firmware.bin"),
    ])

    print("  filesystem image")
    # Deliberately NOT merged: esptool would pad every byte from 0x0 up to the
    # spiffs offset, turning a 320 KB image into a 4 MB one. The browser flashes
    # it at the offset the manifest specifies, so a straight copy is correct.
    src = os.path.join(BUILD, "littlefs.bin")
    if not os.path.exists(src):
        raise SystemExit("littlefs.bin missing - run `pio run -t buildfs`")
    size = os.path.getsize(src)
    check(fs_off, size, [], "littlefs.bin")
    dst = os.path.join(OUT_DIR, "littlefs.bin")
    shutil.copyfile(src, dst)
    print("    %-18s 0x%-7x %8d bytes" % ("littlefs.bin", fs_off, size))
    print("    -> %-15s %8d bytes (%.2f MB)\n"
          % ("littlefs.bin", size, size / 1048576.0))

    parts = [
        {"path": "desk-buddy-firmware.bin", "offset": 0},
        {"path": "littlefs.bin", "offset": fs_off},
    ]
    path = write_manifest(parts, version)
    print("  manifest -> %s" % os.path.relpath(path, ROOT))
    print("\n  Serve web-flasher/ over HTTPS (or localhost) and open index.html.")


if __name__ == "__main__":
    main()
