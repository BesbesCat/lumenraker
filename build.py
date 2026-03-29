import os
import shutil
import json
import tarfile
import struct

VERSION = "0.0.8-alpha"
BUILD_DIR = os.path.join(".pio", "build/lumenraker")
UPDATE_DIR = os.path.join(BUILD_DIR, "update")
DATA_DIR = "data"
DOCS_DIR = "docs"
FW_DIR = os.path.join(DOCS_DIR, "firmware")

FILES_TO_PROCESS = [
    ("firmware.bin", "firmware.bin"),
    ("bootloader.bin", "bootloader.bin"),
    ("partitions.bin", "partitions.bin")
]

def get_littlefs_offset(partition_bin_path):
    """Parses partitions.bin to find the LittleFS/SPIFFS offset."""
    if not os.path.exists(partition_bin_path):
        return None
    
    with open(partition_bin_path, "rb") as f:
        while True:
            data = f.read(32) # Each partition entry is 32 bytes
            if len(data) < 32: break
            
            # ESP32 Partition Entry Format:
            # Magic (2), Type (1), Subtype (1), Offset (4), Size (4), Label (16), Flags (4)
            magic, p_type, p_subtype, offset, size, label = struct.unpack("<HBBII16s", data[:28])
            label = label.strip(b'\x00').decode('utf-8')
            
            if label in ["spiffs", "littlefs"]:
                return offset
    return None

def main():
    if not os.path.exists(BUILD_DIR):
        print(f"Error: Build directory not found at '{BUILD_DIR}'")
        return

    # 1. Automatic Offset Discovery
    partition_bin = os.path.join(BUILD_DIR, "partitions.bin")
    discovered_offset = get_littlefs_offset(partition_bin)
    
    if discovered_offset:
        print(f"Found LittleFS offset in partition table: {discovered_offset} (0x{discovered_offset:02X})")
    else:
        discovered_offset = 2686976 # Fallback to default
        print(f"Warning: Could not find LittleFS in partitions.bin. Using fallback: {discovered_offset}")

    # 2. Prepare Update Directory and Tarball
    if os.path.exists(UPDATE_DIR):
        shutil.rmtree(UPDATE_DIR)
    os.makedirs(UPDATE_DIR, exist_ok=True)

    if os.path.exists(DATA_DIR):
        for item in os.listdir(DATA_DIR):
            s = os.path.join(DATA_DIR, item)
            d = os.path.join(UPDATE_DIR, item)
            if os.path.isdir(s):
                shutil.copytree(s, d)
            else:
                shutil.copy2(s, d)

    fw_src = os.path.join(BUILD_DIR, "firmware.bin")
    if os.path.exists(fw_src):
        shutil.copy2(fw_src, os.path.join(UPDATE_DIR, "firmware.bin"))

    tar_path = os.path.join(UPDATE_DIR, "update.tar")
    with tarfile.open(tar_path, "w") as tar:
        for item in os.listdir(UPDATE_DIR):
            if item == "update.tar": continue
            tar.add(os.path.join(UPDATE_DIR, item), arcname=item)
    
    for item in os.listdir(UPDATE_DIR):
        if item != "update.tar":
            item_path = os.path.join(UPDATE_DIR, item)
            if os.path.isdir(item_path): shutil.rmtree(item_path)
            else: os.remove(item_path)

    # 3. Process Web Installer Files
    os.makedirs(FW_DIR, exist_ok=True)
    for src_name, dst_name in FILES_TO_PROCESS:
        src_path = os.path.join(BUILD_DIR, src_name)
        if os.path.exists(src_path):
            shutil.copy2(src_path, os.path.join(FW_DIR, dst_name))

    littlefs_src = "littlefs.bin"
    if os.path.exists(littlefs_src):
        shutil.move(littlefs_src, os.path.join(FW_DIR, "littlefs.bin"))

    shutil.copy2(tar_path, os.path.join(FW_DIR, "update.tar"))

    # 4. Generate Manifest with discovered offset
    manifest = {
        "name": "Lumenraker",
        "version": VERSION,
        "home_assistant_domain": "lumenraker",
        "builds": [
            {
                "chipFamily": "ESP32",
                "parts": [
                    { "path": "firmware/bootloader.bin", "offset": 4096 },
                    { "path": "firmware/partitions.bin", "offset": 32768 },
                    { "path": "firmware/firmware.bin",   "offset": 65536 },
                    { "path": "firmware/littlefs.bin",   "offset": discovered_offset }
                ]
            }
        ]
    }
    
    with open(os.path.join(DOCS_DIR, "manifest.json"), "w") as f:
        json.dump(manifest, f, indent=2)

    # 5. Generate index.html
    index_path = os.path.join(DOCS_DIR, "index.html")
    if not os.path.exists(index_path):
        html_content = """<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Lumenraker Installer</title>
    <style>
        body { font-family: sans-serif; background-color: #121212; color: #fff; display: flex; flex-direction: column; align-items: center; justify-content: center; height: 100vh; margin: 0; }
        .container { text-align: center; background: #1e1e1e; padding: 40px; border-radius: 12px; }
        h1 { color: #00bcd4; }
        a { color: #00bcd4; text-decoration: none; font-size: 0.9em; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Lumenraker Installer</h1>
        <esp-web-install-button manifest="manifest.json"></esp-web-install-button>
        <div style="margin-top: 25px;"><a href="firmware/update.tar">Download update.tar</a></div>
    </div>
    <script type="module" src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module"></script>
</body>
</html>"""
        with open(index_path, "w", encoding="utf-8") as f:
            f.write(html_content)

    print("Success: Manifest synced with partition table. Files moved and TAR generated.")

if __name__ == "__main__":
    main()