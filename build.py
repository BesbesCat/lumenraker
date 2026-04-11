import os
import shutil
import json
import tarfile
import struct
import re

VERSION = "0.0.8-alpha7_HF3"
PIO_BUILD_DIR = os.path.join(".pio", "build")
DATA_DIR = "data"
DOCS_DIR = "docs"
FW_BASE_DIR = os.path.join(DOCS_DIR, "firmware")

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
    if not os.path.exists(PIO_BUILD_DIR):
        print(f"Error: Build directory not found at '{PIO_BUILD_DIR}'. Run 'pio run' first.")
        return

    os.makedirs(DOCS_DIR, exist_ok=True)
    os.makedirs(FW_BASE_DIR, exist_ok=True)

    # 1. Discover all environments and group them by flash size
    available_envs = [d for d in os.listdir(PIO_BUILD_DIR) if os.path.isdir(os.path.join(PIO_BUILD_DIR, d))]
    flash_groups = {} # e.g., {'16mb': ['esp32_16mb', 'esp32s3_16mb']}
    
    for env in available_envs:
        env_dir = os.path.join(PIO_BUILD_DIR, env)
        if not os.path.exists(os.path.join(env_dir, "firmware.bin")):
            continue # Skip non-firmware folders
            
        # Extract flash size from the environment name (e.g., esp32s3_16mb -> 16mb)
        match = re.search(r'_(4mb|8mb|16mb|32mb)$', env, re.IGNORECASE)
        size_label = match.group(1).lower() if match else "4mb"
        
        if size_label not in flash_groups:
            flash_groups[size_label] = []
        flash_groups[size_label].append(env)

    if not flash_groups:
        print("No compiled firmware found. Please run 'pio run' first.")
        return

    # 2. Process Files & Generate Manifests per Flash Size
    manifest_files = {}
    tar_links = []

    for size_label, env_list in flash_groups.items():
        manifest_name = f"manifest_{size_label}.json"
        manifest = {
            "name": f"Lumenraker ({size_label.upper()})",
            "version": VERSION,
            "home_assistant_domain": "lumenraker",
            "builds": []
        }
        
        for env in env_list:
            print(f"Processing environment: {env}")
            env_dir = os.path.join(PIO_BUILD_DIR, env)
            env_fw_dir = os.path.join(FW_BASE_DIR, env)
            os.makedirs(env_fw_dir, exist_ok=True)
            
            # Determine chip family and bootloader offset
            # ESP Web Tools uses standard names, and ESP32-S3 bootloader starts at 0x0 instead of 0x1000
            is_s3 = "s3" in env.lower()
            chip_family = "ESP32-S3" if is_s3 else "ESP32"
            bootloader_offset = 0 if is_s3 else 4096
            
            # Discover LittleFS Offset
            offset = get_littlefs_offset(os.path.join(env_dir, "partitions.bin"))
            if not offset:
                offset = 2686976
                print(f"  -> Warning: Using fallback LittleFS offset: {offset}")
            
            # Prepare Update Directory (for OTA TAR)
            update_dir = os.path.join(env_dir, "update")
            if os.path.exists(update_dir): shutil.rmtree(update_dir)
            os.makedirs(update_dir, exist_ok=True)
            
            if os.path.exists(DATA_DIR):
                for item in os.listdir(DATA_DIR):
                    s = os.path.join(DATA_DIR, item)
                    d = os.path.join(update_dir, item)
                    if os.path.isdir(s): shutil.copytree(s, d)
                    else: shutil.copy2(s, d)
            
            shutil.copy2(os.path.join(env_dir, "firmware.bin"), os.path.join(update_dir, "firmware.bin"))
            
            # Create TAR Ball for this specific environment
            tar_path = os.path.join(env_fw_dir, "update.tar")
            with tarfile.open(tar_path, "w") as tar:
                for item in os.listdir(update_dir):
                    tar.add(os.path.join(update_dir, item), arcname=item)
            tar_links.append({"env": env, "path": f"firmware/{env}/update.tar"})
            
            # Copy all Web Installer binaries to the docs folder
            for f_name in ["bootloader.bin", "partitions.bin", "firmware.bin", "littlefs.bin"]:
                src = os.path.join(env_dir, f_name)
                if os.path.exists(src):
                    shutil.copy2(src, os.path.join(env_fw_dir, f_name))
            
            # Add entry to the manifest block
            manifest["builds"].append({
                "chipFamily": chip_family,
                "parts": [
                    { "path": f"firmware/{env}/bootloader.bin", "offset": bootloader_offset },
                    { "path": f"firmware/{env}/partitions.bin", "offset": 32768 },
                    { "path": f"firmware/{env}/firmware.bin",   "offset": 65536 },
                    { "path": f"firmware/{env}/littlefs.bin",   "offset": offset }
                ]
            })

        # Save manifest
        with open(os.path.join(DOCS_DIR, manifest_name), "w") as f:
            json.dump(manifest, f, indent=2)
        manifest_files[size_label] = manifest_name

    # 3. Generate Smart index.html
    generate_html(manifest_files, tar_links)
    print("\nSuccess: Web Installer updated with multi-environment support.")


def generate_html(manifest_files, tar_links):
    # Sort flash sizes naturally (4mb, 8mb, 16mb)
    sorted_sizes = sorted(manifest_files.keys(), key=lambda x: int(x.replace('mb', '')))
    default_manifest = manifest_files[sorted_sizes[0]]

    # Build Dropdown Options
    options_html = ""
    for size in sorted_sizes:
        options_html += f'                <option value="{manifest_files[size]}">{size.upper()} Flash</option>\n'

    # Build TAR links
    tar_html = ""
    for link in sorted_sizes:
        # Group tar links by env
        env_links = [t for t in tar_links if link in t['env']]
        for t in env_links:
            tar_html += f'            <div class="dl-link"><a href="{t["path"]}">Download {t["env"]} update.tar</a></div>\n'

    html_content = f"""<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Lumenraker Installer</title>
    <style>
        body {{ font-family: sans-serif; background-color: #121212; color: #fff; display: flex; flex-direction: column; align-items: center; justify-content: center; min-height: 100vh; margin: 0; }}
        .container {{ text-align: center; background: #1e1e1e; padding: 40px; border-radius: 12px; max-width: 400px; width: 100%; }}
        h1 {{ color: #00bcd4; margin-bottom: 5px; }}
        p.subtitle {{ color: #aaa; font-size: 0.9em; margin-bottom: 25px; }}
        select {{ background: #2c2c2c; color: white; border: 1px solid #444; padding: 10px; border-radius: 6px; width: 100%; margin-bottom: 20px; font-size: 16px; cursor: pointer; }}
        a {{ color: #00bcd4; text-decoration: none; font-size: 0.9em; transition: color 0.2s; }}
        a:hover {{ color: #5eead4; }}
        .dl-section {{ margin-top: 30px; border-top: 1px solid #333; padding-top: 15px; text-align: left; }}
        .dl-link {{ margin-top: 8px; }}
        esp-web-install-button {{ margin-bottom: 20px; }}
    </style>
</head>
<body>
    <div class="container">
        <h1>Lumenraker</h1>
        <p class="subtitle">Select your board's physical flash size. The installer will auto-detect your chip architecture (ESP32 vs ESP32-S3).</p>
        
        <select id="flash-select">
{options_html}
        </select>

        <esp-web-install-button id="installer" manifest="{default_manifest}"></esp-web-install-button>
        
        <div class="dl-section">
            <h4 style="margin:0 0 10px 0; color:#888;">Manual OTA Updates</h4>
{tar_html}
        </div>
    </div>

    <script type="module" src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module"></script>
    <script>
        const select = document.getElementById('flash-select');
        const installer = document.getElementById('installer');
        select.addEventListener('change', (e) => {{
            installer.setAttribute('manifest', e.target.value);
        }});
    </script>
</body>
</html>"""

    with open(os.path.join(DOCS_DIR, "index.html"), "w", encoding="utf-8") as f:
        f.write(html_content)

if __name__ == "__main__":
    main()