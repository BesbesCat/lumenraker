async function updateSysStats() {
    try {
        const [sysRes, fpsRes] = await Promise.all([
            fetch('/api/sysinfo'),
            fetch('/api/fps')
        ]);
        
        const sys = await sysRes.json();
        const fps = await fpsRes.json();

        document.getElementById('stat-fps').innerText = `${fps.fps} FPS`;
        document.getElementById('stat-min-heap').innerText = `${Math.round(sys.heap_min / 1024)} KB`;
        document.getElementById('stat-rssi').innerText = `${sys.wifi_rssi} dBm`;
        document.getElementById('stat-uptime').innerText = `${Math.floor(sys.uptime / 60)}m ${sys.uptime % 60}s`;
        if (sys.board) {
            document.getElementById('stat-board').innerText = sys.board;
            hwBoard = sys.board.toLowerCase().replace(/-/g, ""); // Converts "ESP32-S3" to "esp32s3"
        }
        if (sys.flash_size) {
            document.getElementById('stat-flash').innerText = `${sys.flash_size} MB`;
            hwFlashMB = sys.flash_size;
        }
        const ramUsed = sys.heap_total - sys.heap_free;
        const ramPct = (ramUsed / sys.heap_total) * 100;
        const ramEl = document.getElementById('stat-ram');
        ramEl.innerText = `${Math.round(ramUsed / 1024)} / ${Math.round(sys.heap_total / 1024)} KB`;

        heapHistory.push(ramUsed);
        if (heapHistory.length > MAX_HISTORY) heapHistory.shift();

        const sparkElement = document.getElementById("spark-ram");
        if (typeof sparkline !== 'undefined' && sparkline.sparkline && sparkElement) {
            sparkline.sparkline(sparkElement, heapHistory);
        }

        if (ramPct >= 90) {
            ramEl.classList.add('text-warning');
            if (!warningThrottles.ram) {
                showToast("System Memory is critically full (>90%)!", "error");
                warningThrottles.ram = true;
            }
        } else {
            ramEl.classList.remove('text-warning');
            warningThrottles.ram = false;
        }

        if (sys.psram_total && sys.psram_total > 0) {
            document.getElementById('row-psram').style.display = 'table-row';
            
            const psramUsed = sys.psram_total - sys.psram_free;
            const psramPct = (psramUsed / sys.psram_total) * 100;
            const psramEl = document.getElementById('stat-psram');
            psramEl.innerText = `${Math.round(psramUsed / 1024)} / ${Math.round(sys.psram_total / 1024)} KB`;

            psramHistory.push(psramUsed);
            if (psramHistory.length > MAX_HISTORY) psramHistory.shift();

            const psramSpark = document.getElementById("spark-psram");
            if (typeof sparkline !== 'undefined' && sparkline.sparkline && psramSpark) {
                sparkline.sparkline(psramSpark, psramHistory);
            }

            if (psramPct >= 90) {
                psramEl.classList.add('text-warning');
                if (!warningThrottles.psram) {
                    showToast("PSRAM is critically full (>90%)!", "error");
                    warningThrottles.psram = true;
                }
            } else {
                psramEl.classList.remove('text-warning');
                warningThrottles.psram = false;
            }
        } else {
            document.getElementById('row-psram').style.display = 'none';
        }

        const fsPct = (sys.fs_used / sys.fs_total) * 100;
        const fsEl = document.getElementById('stat-fs');
        fsEl.innerText = `${Math.round(sys.fs_used / 1024)} / ${Math.round(sys.fs_total / 1024)} KB`;

        if (fsPct >= 90) {
            fsEl.classList.add('text-warning');
            if (!warningThrottles.fs) {
                showToast("Flash Storage is nearly full (>90%)!", "error");
                warningThrottles.fs = true;
            }
        } else {
            fsEl.classList.remove('text-warning');
            warningThrottles.fs = false;
        }

    } catch (e) {
        console.error("Stats poll failed", e);
    }
}

async function pollDebug() {
    const isEnabled = document.getElementById('debug-enabled').checked;
    if (!isEnabled) return;

    try {
        const res = await fetch('/api/debug');
        const text = await res.text();

        if (text && text.trim() !== "") {
            const consoleEl = document.getElementById('debug-console');
            const now = new Date().toLocaleTimeString([], { hour12: false, hour: '2-digit', minute: '2-digit', second: '2-digit' });
            
            const entry = document.createElement('div');
            entry.className = 'debug-entry';
            entry.innerHTML = `<span class="debug-time">[${now}]</span><span>${text}</span>`;
            
            consoleEl.appendChild(entry);
            lastDebugMsg = text;

            consoleEl.scrollTop = consoleEl.scrollHeight;

            while (consoleEl.children.length > 50) {
                consoleEl.removeChild(consoleEl.firstChild);
            }
        }
    } catch (e) {
        console.error("Debug poll failed:", e);
    }
}

function toggleDebugPolling() {
    const isEnabled = document.getElementById('debug-enabled').checked;
    if (isEnabled) {
        debugInterval = setInterval(pollDebug, 2000);
        showToast("Debug polling active", "info");
    } else {
        clearInterval(debugInterval);
        debugInterval = null;
        showToast("Debug polling paused", "info");
    }
}

function clearConsole() {
    const consoleEl = document.getElementById('debug-console');
    consoleEl.innerHTML = '<div style="color: #666">-- Logs cleared.</div>';
    lastDebugMsg = "";
}

async function getVer() {
    try {
        const resp = await fetch('/api/version');
        const data = await resp.json();
        document.getElementById('app-version').innerText = `v${data.version}`;
        
        hwAppVersion = data.version;
        document.getElementById('current-version-display').innerText = `v${hwAppVersion}`;
    } catch (e) { console.error("Could not fetch version"); }
}

function showRebootModal() { document.getElementById('reboot-modal').style.display = 'flex'; }
function hideRebootModal() { document.getElementById('reboot-modal').style.display = 'none'; }

async function executeReboot() {
    document.querySelector('.modal-content').innerHTML = "<h2>Rebooting...</h2><p>Connection will be lost.</p>";
    try {
        await fetch('/api/reboot', { method: 'POST' });
    } catch (e) {}
    
    setTimeout(() => { window.location.reload(); }, 6000);
}