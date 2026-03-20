
import os
import shutil
import json
import tarfile

VERSION = "0.0.2-alpha"
PARTITION_SCHEME = "arduino_ide_current" 

BUILD_DIR = os.path.join("build", "esp32.esp32.d1_mini32")
UPDATE_DIR = os.path.join(BUILD_DIR, "update")
DATA_DIR = "data"
DOCS_DIR = "docs"
FW_DIR = os.path.join(DOCS_DIR, "firmware")

OFFSETS = {
    "arduino_ide_current": 3997696,
    "default": 2686976,   
    "huge_app": 3211264,  
    "minimal": 4063232,   
    "no_ota": 2097152     
}

FILES_TO_PROCESS = [
    ("lumenraker.ino.bin", "firmware.bin"),
    ("lumenraker.ino.bootloader.bin", "bootloader.bin"),
    ("lumenraker.ino.partitions.bin", "partitions.bin")
]

def main():
    if not os.path.exists(BUILD_DIR):
        print(f"Error: Build directory not found at '{BUILD_DIR}'")
        return

    # 1. Prepare Update Directory and Tarball
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

    fw_src = os.path.join(BUILD_DIR, "lumenraker.ino.bin")
    if os.path.exists(fw_src):
        shutil.copy2(fw_src, os.path.join(UPDATE_DIR, "lumenraker.ino.bin"))

    tar_path = os.path.join(UPDATE_DIR, "update.tar")
    with tarfile.open(tar_path, "w") as tar:
        for item in os.listdir(UPDATE_DIR):
            if item == "update.tar": continue
            item_path = os.path.join(UPDATE_DIR, item)
            tar.add(item_path, arcname=item)
    
    # Cleanup: Delete everything in update/ EXCEPT update.tar
    for item in os.listdir(UPDATE_DIR):
        if item != "update.tar":
            item_path = os.path.join(UPDATE_DIR, item)
            if os.path.isdir(item_path):
                shutil.rmtree(item_path)
            else:
                os.remove(item_path)
    print(f"Generated and Cleaned: {tar_path}")

    # 2. Process Web Installer Files (Docs)
    os.makedirs(FW_DIR, exist_ok=True)

    for src_name, dst_name in FILES_TO_PROCESS:
        src_path = os.path.join(BUILD_DIR, src_name)
        dst_path = os.path.join(FW_DIR, dst_name)
        if os.path.exists(src_path):
            shutil.copy2(src_path, dst_path)
            print(f"Copied: {src_name} -> {dst_name}")

    littlefs_src = "mklittlefs.bin"
    littlefs_dest = os.path.join(FW_DIR, "littlefs.bin")
    if os.path.exists(littlefs_src):
        shutil.move(littlefs_src, littlefs_dest)
        print(f"Moved: {littlefs_src} -> littlefs.bin")

    # Copy update.tar to docs/firmware/
    shutil.copy2(tar_path, os.path.join(FW_DIR, "update.tar"))
    print("Copied update.tar to docs/firmware/")

    # 3. Generate Manifest
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
                    { "path": "firmware/littlefs.bin",   "offset": OFFSETS[PARTITION_SCHEME] }
                ]
            }
        ]
    }
    
    with open(os.path.join(DOCS_DIR, "manifest.json"), "w") as f:
        json.dump(manifest, f, indent=2)
    print("Generated: manifest.json")

    # 4. Generate index.html
    index_path = os.path.join(DOCS_DIR, "index.html")
    if not os.path.exists(index_path):
        html_content = """<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Lumenraker Web Installer</title>
    <style>
        body { font-family: sans-serif; background-color: #121212; color: #fff; display: flex; flex-direction: column; align-items: center; justify-content: center; height: 100vh; margin: 0; }
        .container { text-align: center; background: #1e1e1e; padding: 40px; border-radius: 12px; }
        h1 { color: #00bcd4; }
        p { margin-bottom: 10px; color: #aaa; }
        a { color: #00bcd4; text-decoration: none; font-size: 0.9em; }
        a:hover { text-decoration: underline; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Lumenraker Installer</h1>
        <p>Connect ESP32 via USB and click install.</p>
        <esp-web-install-button manifest="manifest.json"></esp-web-install-button>
        <div style="margin-top: 25px;">
            <a href="firmware/update.tar">Download update.tar (OTA)</a>
        </div>
    </div>
    <script type="module" src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module"></script>
</body>
</html>"""
        with open(index_path, "w", encoding="utf-8") as f:
            f.write(html_content)
        print("Generated: index.html")

if __name__ == "__main__":
    main()