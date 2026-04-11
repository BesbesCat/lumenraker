async function checkForUpdates() {
    const btn = document.getElementById('check-update-btn');
    const display = document.getElementById('latest-version-display');
    const installBtn = document.getElementById('install-update-btn');
    const warningBox = document.getElementById('update-warning');
    
    btn.innerText = "Checking...";
    btn.disabled = true;
    
    try {
        const manifestUrl = `https://besbescat.github.io/lumenraker/manifest_${hwFlashMB}mb.json`;
        const res = await fetch(manifestUrl);
        
        if (!res.ok) throw new Error("Manifest not found. You may be on an unsupported flash size.");
        
        const manifest = await res.json();
        const latestVer = manifest.version;
        
        display.innerText = `v${latestVer}`;
        
        if (latestVer !== hwAppVersion) {
            display.style.color = "var(--success)";
            display.innerText += " (Update Available!)";
            
            remoteTarUrl = `https://besbescat.github.io/lumenraker/firmware/${hwBoard}_${hwFlashMB}mb/update.tar`;
            
            installBtn.style.display = "block";
            warningBox.style.display = "block";
        } else {
            display.style.color = "var(--text-dim)";
            display.innerText += " (Up to date)";
            installBtn.style.display = "none";
            warningBox.style.display = "none";
        }
    } catch (e) {
        display.innerText = "Error checking repository";
        display.style.color = "var(--danger)";
        showToast(e.message, "error");
    }
    
    btn.innerText = "Check for Updates";
    btn.disabled = false;
}

async function performOTAUpgrade() {
    const btn = document.getElementById('install-update-btn');
    const checkBtn = document.getElementById('check-update-btn');
    const log = document.getElementById('install-progress');
    
    if (!confirm(`Are you sure you want to install the latest firmware for ${hwBoard} (${hwFlashMB}MB)?`)) return;
    
    btn.disabled = true;
    checkBtn.disabled = true;
    log.innerText = "Downloading update bundle (this may take a moment)...";
    
    try {
        const response = await fetch(remoteTarUrl);
        if (!response.ok) throw new Error("Failed to download update bundle.");
        
        const arrayBuffer = await response.arrayBuffer();
        log.innerText = "Download complete. Extracting & Installing...";
        
        await processInstaller(arrayBuffer); 
        
    } catch (e) {
        log.innerText = "Error: " + e.message;
        btn.disabled = false;
        checkBtn.disabled = false;
    }
}

async function handleLocalUpload() {
    const fileInput = document.getElementById('local-tar-file');
    const status = document.getElementById('install-local-progress');

    if (fileInput.files.length === 0) {
        status.innerText = "Error: Please select a .tar file first.";
        return;
    }

    const file = fileInput.files[0];
    status.innerText = "Reading local file...";
    const arrayBuffer = await file.arrayBuffer();
    processInstaller(arrayBuffer);
}

async function processInstaller(arrayBuffer) {
    const status = document.getElementById('install-local-progress') || document.getElementById('install-progress');

    try {
        status.innerText = "Unpacking bundle...";
        const files = await untar(arrayBuffer);
        
        let extractedConfig = null;

        for (const file of files) {
            if (file.name === 'config.json' || file.name === '/config.json') {
                const decoder = new TextDecoder();
                extractedConfig = decoder.decode(file.buffer);
                continue;                
            }

            status.innerText = `Installing: ${file.name}`;
        
            let endpoint = "";
            let headers = {};

            if (file.name === "firmware.bin") {
                endpoint = '/api/install/firmware';
            } else {
                endpoint = '/api/install/file';
                let dest = '/' + file.name;
                headers = { 'X-Dest-Path': dest };
            }

            const response = await fetch(endpoint, { 
                method: 'POST', 
                headers: headers,
                body: file.buffer 
            });

            if (!response.ok) throw new Error(`Failed on ${file.name}`);

            await sleep(500); 
        }

        if (extractedConfig) {
            status.innerText = "Processing configuration backup...";
            try {
                const newConfig = JSON.parse(extractedConfig);
                
                let currentConfig = {};
                const configRes = await fetch('/api/config');
                if (configRes.ok) {
                    currentConfig = await configRes.json();
                }

                const mergedConfig = JSON.parse(JSON.stringify(currentConfig));
                
                for (const key in newConfig) {
                    mergedConfig[key] = newConfig[key];
                }

                status.innerText = "Uploading merged configuration...";
                
                await fetch('/api/config', { 
                    method: 'POST', 
                    headers: { 'Content-Type': 'application/json' }, 
                    body: JSON.stringify(mergedConfig) 
                }).catch(() => {  });

                status.innerText = "Config applied. Waiting 6 seconds for reboot...";
                await sleep(6000);
            } catch (err) {
                console.error("Failed to merge configuration:", err);
                status.innerText = "Warning: Failed to process config. Continuing...";
                await sleep(2000);
            }
        } else {
            status.innerText = "Installation Successful! Rebooting...";
            await fetch('/api/reboot', { method: 'POST' }).catch(()=>{});
        }
        
        setTimeout(() => { window.location.reload(); }, extractedConfig ? 2000 : 6000);

    } catch (err) {
        status.innerText = "Critical Error: " + err.message;
        console.error(err);
    }
}