const sleep = (ms) => new Promise(resolve => setTimeout(resolve, ms));
async function runInstaller() {
    const status = document.getElementById('install-progress');
    const TAR_LIB_URL = "https://cdn.jsdelivr.net/npm/js-untar@latest/build/dist/untar.js";
    const BUNDLE_URL = "https://cdn.jsdelivr.net/gh/BesbesCat/lumenraker@master/docs/firmware/update.tar";

    try {
        // 1. Load the untar library if it's not already there
        if (typeof untar === 'undefined') {
            status.innerText = "Loading extraction tools...";
            await new Promise((resolve, reject) => {
                const script = document.createElement('script');
                script.src = TAR_LIB_URL;
                script.onload = resolve;
                script.onerror = reject;
                document.head.appendChild(script);
            });
        }

        // 2. Fetch the latest.tar
        status.innerText = "Downloading package...";
        const response = await fetch(BUNDLE_URL);
        const arrayBuffer = await response.arrayBuffer();

        // 3. Unpack
        status.innerText = "Unpacking...";
        const files = await untar(arrayBuffer);

        for (const file of files) {
            status.innerText = `Installing: ${file.name}`;
            
            // Route files to correct endpoints
            if (file.name === "firmware.bin") {
                await fetch('/api/install/firmware', { 
                    method: 'POST', 
                    body: file.buffer 
                });
            } else {
                let dest = '/' + file.name;
                await fetch('/api/install/file', {
                    method: 'POST',
                    headers: { 'X-Dest-Path': dest },
                    body: file.buffer
                });
            }
            await sleep(3000);
        }
    // Path D: Custom Configs (Optional)
//    status.innerText = "Applying recommended zone defaults...";
//    await fetch('/api/config', { 
//        method: 'POST', 
//        body: JSON.stringify({ /* custom preset data */ }) 
//    });

        status.innerText = "Installation Successful! Rebooting...";
        await fetch('/api/reboot', { method: 'POST' });
        
        // Refresh the page after 6 seconds
        setTimeout(() => { window.location.reload(); }, 6000);

    } catch (err) {
        status.innerText = "Critical Error: " + err.message;
        console.error(err);
    }
}

// Execute immediately
runInstaller();