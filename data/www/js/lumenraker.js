let editor = null;
let warningThrottles = { ram: false, fs: false, psram: false };
let heapHistory = [];
let psramHistory = [];
const MAX_HISTORY = 180;
let debugInterval = null;
let lastDebugMsg = "";

const EVENTS = ["Idle", "Start Print", "Bed Heating", "Extruder Heating", "Moving", "Error", "Disconnected", "Stream"];
let sysConfig = { strips: [], zones: [] };
let availableScripts = ["Solid"];
let hwBoard = "esp32";
let hwFlashMB = 4;
let hwAppVersion = "0.0.0";
let remoteTarUrl = "";

function loadExternalScript(url) {
    return new Promise((resolve, reject) => {
        const script = document.createElement('script');
        script.src = url;
        script.async = true; 
        script.onload = resolve;
        script.onerror = reject;
        document.head.appendChild(script);
    });
}

const sleep = (ms) => new Promise(resolve => setTimeout(resolve, ms));

function hexToRgb(hex) {
    const r = parseInt(hex.slice(1, 3), 16), g = parseInt(hex.slice(3, 5), 16), b = parseInt(hex.slice(5, 7), 16);
    return { r, g, b };
}

function rgbToHex(r, g, b) {
    return "#" + ((1 << 24) + (r << 16) + (g << 8) + b).toString(16).slice(1);
}

function updateThemeVar(cssVar, val) {
    document.documentElement.style.setProperty(cssVar, val);
}

function showToast(message, type = 'info') {
    const container = document.getElementById('toast-container');
    const toast = document.createElement('div');
    toast.className = `toast toast-${type}`;
    toast.innerHTML = `<span>${message}</span>`;
    
    container.appendChild(toast);
    setTimeout(() => toast.classList.add('show'), 10);
    
    setTimeout(() => {
        toast.classList.remove('show');
        setTimeout(() => toast.remove(), 400);
    }, 4000);
}

function switchTab(tabId) {
    document.querySelectorAll('.tab-content, .tab-btn').forEach(e => e.classList.remove('active'));
    document.getElementById(tabId).classList.add('active');
    event.currentTarget.classList.add('active');
}

const originalFetch = window.fetch;
window.fetch = async function() {
    const response = await originalFetch.apply(this, arguments);
    if (response.status === 401 && !arguments[0].includes('login')) {
        document.getElementById('login-overlay').classList.remove('hidden');
        throw new Error("Unauthorized");
    }
    return response;
};

async function handleLogin(e) {
    e.preventDefault();
    const btn = document.getElementById('login-btn');
    btn.innerText = "AUTHENTICATING...";

    const formData = new URLSearchParams();
    formData.append('user', document.getElementById('login-user').value);
    formData.append('pass', document.getElementById('login-pass').value);

    try {
        const res = await fetch('/api/login', {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: formData
        });

        if (res.ok) {
            document.getElementById('login-overlay').classList.add('hidden');
            init();
        } else {
            showToast("Invalid Credentials", "error");
        }
    } catch (err) {}
    btn.innerText = "LOGIN";
}

async function handleLogout() {
    try {
        await fetch('/api/logout', { method: 'POST' });
    } catch (e) {
        window.location.reload(); 
    }
}

function checkPasswordStrength() {
    const pwd = document.getElementById('sec-new-pass').value;
    const meter = document.getElementById('pwd-meter');
    const fill = document.getElementById('pwd-fill');
    const feedback = document.getElementById('pwd-feedback');

    if (pwd.length === 0) {
        meter.style.display = 'none';
        feedback.style.display = 'none';
        return;
    }

    meter.style.display = 'block';
    feedback.style.display = 'block';

    let score = 0;
    let tips = [];

    if (pwd.length >= 8) score++;
    else tips.push("minimum 8 chars");

    if (/[A-Z]/.test(pwd) && /[a-z]/.test(pwd)) score++;
    else tips.push("mix upper/lowercase");

    if (/[0-9]/.test(pwd)) score++;
    else tips.push("add a number");

    if (/[^A-Za-z0-9]/.test(pwd)) score++;
    else tips.push("add a symbol");

    fill.className = 'pwd-fill';
    
    if (score <= 1) {
        fill.classList.add('pwd-weak');
        feedback.innerHTML = "Strength: <b style='color:var(--danger)'>Weak</b>. " + (tips.length ? "Tip: " + tips.join(", ") : "");
    } else if (score === 2) {
        fill.classList.add('pwd-fair');
        feedback.innerHTML = "Strength: <b style='color:#ffaa00'>Fair</b>. " + (tips.length ? "Tip: " + tips.join(", ") : "");
    } else if (score === 3) {
        fill.classList.add('pwd-good');
        feedback.innerHTML = "Strength: <b style='color:#ffee00'>Good</b>. " + (tips.length ? "Tip: " + tips.join(", ") : "");
    } else {
        fill.classList.add('pwd-strong');
        feedback.innerHTML = "Strength: <b style='color:var(--success)'>Strong</b>.";
    }
}

async function updateSecurity() {
    const oldPass = document.getElementById('sec-old-pass').value;
    const newPass = document.getElementById('sec-new-pass').value;
    const newPassConfirm = document.getElementById('sec-new-pass-confirm').value;
    const newUser = document.getElementById('sec-user').value;

    if (newPass !== newPassConfirm) {
        return showToast("New passwords do not match!", "error");
    }

    const formData = new URLSearchParams();
    formData.append('old_pass', oldPass);
    formData.append('new_pass', newPass);
    formData.append('new_user', newUser);

    const res = await fetch('/api/change_password', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: formData
    });

    if (res.ok) {
        showToast("Security Updated. Please log in again.", "info");
        setTimeout(() => window.location.reload(), 1500);
    } else {
        const err = await res.json();
        showToast(err.error || "Update Failed", "error");
    }
}

async function checkAuth() {
    try {
        const resp = await fetch('/api/version');
        const data = await resp.json();
        init();
        document.getElementById('login-overlay').classList.add('hidden');
    } catch (e) {  }
}


async function loadConfig() {
    try {
        const res = await fetch('/api/config');
        sysConfig = await res.json();
        document.getElementById('host').value = sysConfig.host || 'lumenraker';
        document.getElementById('ssid').value = sysConfig.ssid || '';
        document.getElementById('pass').value = sysConfig.pass || '';
        document.getElementById('mHost').value = sysConfig.mHost || '';
        document.getElementById('mPort').value = sysConfig.mPort || 7125;
        document.getElementById('br').value = sysConfig.br || 128;
        document.querySelector('#br + .slider-val').value = sysConfig.br || 128;
        document.getElementById('fadeDurationMs').value = sysConfig.fadeDurationMs || 1000;
        document.querySelector('#fadeDurationMs + .slider-val').value = sysConfig.fadeDurationMs || 1000;
        document.getElementById('colortemp').value = sysConfig.colorTempK || 6500;
        document.getElementById('ct-val').value = sysConfig.colorTempK || 6500;
        document.getElementById('mqttHost').value = sysConfig.mqttHost || '';
        document.getElementById('mqttPort').value = sysConfig.mqttPort || 1883;
        document.getElementById('mqttUser').value = sysConfig.mqttUser || '';
        document.getElementById('mqttPass').value = sysConfig.mqttPass || '';
        document.getElementById('uiBg').value = sysConfig.uiBg || '#0f172a';
        document.getElementById('uiPanel').value = sysConfig.uiPanel || '#1e293b';
        document.getElementById('uiCard').value = sysConfig.uiCard || '#334155';
        document.getElementById('uiText').value = sysConfig.uiText || '#f8fafc';
        document.getElementById('uiDim').value = sysConfig.uiDim || '#94a3b8';
        document.getElementById('uiAccent').value = sysConfig.uiAccent || '#3b82f6';
        document.getElementById('uiBorder').value = sysConfig.uiBorder || '#475569';
        document.getElementById('uiDanger').value = sysConfig.uiDanger || '#ef4444';
        document.getElementById('uiSuccess').value = sysConfig.uiSuccess || '#22c55e';
        
        renderStrips(); renderZones();
    } catch (e) { console.error(e); }
}

function renderStrips() {
    const container = document.getElementById('strips-container');
    
    container.innerHTML = sysConfig.strips.map((s, i) => {
        const zonesOnThisStrip = sysConfig.zones.filter(z => z.sIdx === i);
        const totalLeds = s.cnt || 1;
        
        return `
            <div class="card">
                <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:10px;">
                    <h3>Physical Strip #${i}</h3>
                    <button class="btn-danger" 
                        style="padding:4px 10px; ${i === 0 ? 'opacity: 0.5; cursor: not-allowed;' : ''}" 
                        ${i === 0 ? 'disabled' : ''} 
                        onclick="removeZone(${i})">
                        Delete
                    </button>
                </div>
                
                <div style="height:12px; background:#111; border-radius:4px; margin-bottom:15px; display:flex; overflow:hidden; border:1px solid var(--border);">
                    ${zonesOnThisStrip.map(z => `
                        <div style="width:${(z.len/totalLeds)*100}%; background:var(--accent); border-right:1px solid #000; height:100%;" 
                             title="Zone ${sysConfig.zones.indexOf(z)}: ${z.len} LEDs"></div>
                    `).join('')}
                </div>

                <div class="card-grid">
                    <div class="form-group"><label>Output GPIO</label><input type="number" value="${s.pin}" onchange="sysConfig.strips[${i}].pin=parseInt(this.value)"></div>
                    <div class="form-group"><label>Total Physical LEDs</label><input type="number" value="${s.cnt}" onchange="sysConfig.strips[${i}].cnt=parseInt(this.value); renderStrips();"></div>
                </div>
                <button class="btn-add" style="margin-top:15px; width:100%; background:var(--panel); border:1px dashed var(--accent);" onclick="splitStripIntoZones(${i})">✂ SPLIT STRIP INTO NEW ZONE</button>
            </div>
        `;
    }).join('');
}

function splitStripIntoZones(stripIdx) {
    const strip = sysConfig.strips[stripIdx];

    let lastPixel = 0;
    let templateZone = null;

    sysConfig.zones.forEach(z => {
        if (z.sIdx === stripIdx) {
            const end = z.start + z.len;
            if (end >= lastPixel) {
                lastPixel = end;
                templateZone = z; 
            }
        }
    });

    const remaining = strip.cnt - lastPixel;
    if (remaining <= 0) {
        return showToast("No physical LEDs remaining on this strip to split!", "error");
    }

    let clonedEvts;
    let clonedRev = false;

    if (templateZone) {
        clonedEvts = JSON.parse(JSON.stringify(templateZone.evts));
        clonedRev = templateZone.rev;
    } else {
        clonedEvts = Array.from({length: 8}, () => ({ 
            fx: "Solid", r: 255, g: 255, b: 255, sp: 255, sz: 255, dl: 255, br: 255 
        }));
    }

    sysConfig.zones.push({
        sIdx: stripIdx,
        start: lastPixel,
        len: remaining,
        rev: clonedRev,
        evts: clonedEvts
    });

    const templateInfo = templateZone ? `Cloned from Zone ${sysConfig.zones.indexOf(templateZone)}` : "Default config used";
    showToast(`Strip ${stripIdx} sliced at ${lastPixel}. ${templateInfo}`, "success");
    
    renderStrips();
    renderZones();
    switchTab('zones');
}

function autoAlignZone(idx) {
    const stripIdx = sysConfig.zones[idx].sIdx;

    if (idx === 0) {
        sysConfig.zones[idx].start = 0;
    } else {
        const prev = sysConfig.zones[idx - 1];
        
        if (prev.sIdx === stripIdx) {
            sysConfig.zones[idx].start = prev.start + prev.len;
        } else {
            sysConfig.zones[idx].start = 0;
        }
    }
    splitStripIntoZones(stripIdx);

    showToast(`Zone ${idx} aligned and strip ${stripIdx} sliced.`, "success");
}

function renderZones() {
    const container = document.getElementById('zones-container');
    const scriptOptions = availableScripts.map(s => `<option value="${s}">${s}</option>`).join('');

    container.innerHTML = sysConfig.zones.map((z, i) => `
        <div class="card">
            <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:15px; border-bottom:1px solid var(--border); padding-bottom:8px;">
                <h3>Zone #${i}</h3>
                <button class="btn-danger" 
                    style="padding:4px 10px; ${i === 0 ? 'opacity: 0.5; cursor: not-allowed;' : ''}" 
                    ${i === 0 ? 'disabled' : ''} 
                    onclick="removeZone(${i})">
                    Delete
                </button>
            </div>
            <div class="card-grid" style="margin-bottom:15px;">
                <div class="form-group"><label>Start Pixel Index <button onclick="autoAlignZone(${i})" style="padding:0 10px; background:var(--accent); font-size:10px;">Auto</button></label><input type="number" min="0" value="${z.start}" onchange="sysConfig.zones[${i}].start=parseInt(this.value); renderStrips();"></div>
                <div class="form-group"><label>Length</label><input type="number" min="1" value="${z.len}" onchange="sysConfig.zones[${i}].len=parseInt(this.value); renderStrips();"></div>

                <div class="form-group"><label>Direction</label>
                    <select onchange="sysConfig.zones[${i}].rev=(this.value==='true')">
                        <option value="false" ${!z.rev ? 'selected' : ''}>Normal</option>
                        <option value="true" ${z.rev ? 'selected' : ''}>Inverted</option>
                    </select>
                </div>
            </div>
            
            <div class="event-grid">
                ${z.evts.map((e, ei) => `
                    <div class="event-card">
                        <div class="event-main-row">
                            <button class="toggle-details" onclick="this.closest('.event-card').classList.toggle('is-expanded')">⚙</button>
                            <span class="event-title">${EVENTS[ei]}</span>
                            <div>
                                <select onchange="sysConfig.zones[${i}].evts[${ei}].fx=this.value">
                                    <option value="${e.fx}" selected>${e.fx}</option>
                                    ${scriptOptions}
                                </select>
                            </div>
                            ${renderSlider(i, ei, 'br', 'Brightness', e.br||255, true)}
                            <input type="color" value="${rgbToHex(e.r, e.g, e.b)}" 
                                onchange="const c=hexToRgb(this.value); sysConfig.zones[${i}].evts[${ei}].r=c.r; sysConfig.zones[${i}].evts[${ei}].g=c.g; sysConfig.zones[${i}].evts[${ei}].b=c.b;">
                        </div>
                        <div class="event-extra-params">
                            ${renderSlider(i, ei, 'sp', 'Speed', e.sp||0)}
                            ${renderSlider(i, ei, 'sz', 'Scale/Size', e.sz||0)}
                            ${renderSlider(i, ei, 'dl', 'Delay', e.dl||0)}
                        </div>
                    </div>
                `).join('')}
            </div>
        </div>
    `).join('');
    renderStrips();
}

function renderSlider(zIdx, eIdx, key, label, val, mini = false) {
    return `
        <div class="form-group" style="margin:0">
            ${mini ? '' : `<label>${label}</label>`}
            <div class="slider-container" style="${mini ? 'padding: 4px 8px; border:none;' : ''}">
                <input type="range" min="0" max="255" value="${val}" 
                    oninput="this.nextElementSibling.value=this.value; sysConfig.zones[${zIdx}].evts[${eIdx}].${key}=parseInt(this.value)">
                <input type="number" step="1" min="0" max="255" class="slider-val" value="${val}" oninput="this.previousElementSibling.value=this.value;  sysConfig.zones[${zIdx}].evts[${eIdx}].${key}=parseInt(this.value)">
            </div>
        </div>
    `;
}

function addStrip() { sysConfig.strips.push({ pin: 5, cnt: 60 }); renderStrips(); }
function removeStrip(idx) { sysConfig.strips.splice(idx, 1); renderStrips(); }

function addZone() { 
    sysConfig.zones.push({ sIdx: 0, start: 0, len: 60, rev: false, 
        evts: Array.from({length: 8}, () => ({ fx: "Solid", r: 255, g: 255, b: 255, sp: 255, sz: 255, dl: 255, br: 255 })) 
    }); 
    renderZones(); 
}
function removeZone(idx) { sysConfig.zones.splice(idx, 1); renderZones(); }

async function saveSystemConfig() {
    sysConfig.host = document.getElementById('host').value;
    sysConfig.ssid = document.getElementById('ssid').value;
    sysConfig.pass = document.getElementById('pass').value;
    sysConfig.mHost = document.getElementById('mHost').value;
    sysConfig.mPort = parseInt(document.getElementById('mPort').value);
    sysConfig.br = parseInt(document.getElementById('br').value);
    sysConfig.fadeDurationMs = parseInt(document.getElementById('fadeDurationMs').value);
    sysConfig.colorTempK = parseInt(document.getElementById('colortemp').value);
    sysConfig.mqttHost = document.getElementById('mqttHost').value;
    sysConfig.mqttPort = parseInt(document.getElementById('mqttPort').value);
    sysConfig.mqttUser = document.getElementById('mqttUser').value;
    sysConfig.mqttPass = document.getElementById('mqttPass').value;
    sysConfig.uiBg = document.getElementById('uiBg').value;
    sysConfig.uiPanel = document.getElementById('uiPanel').value;
    sysConfig.uiCard = document.getElementById('uiCard').value;
    sysConfig.uiText = document.getElementById('uiText').value;
    sysConfig.uiDim = document.getElementById('uiDim').value;
    sysConfig.uiAccent = document.getElementById('uiAccent').value;
    sysConfig.uiBorder = document.getElementById('uiBorder').value;
    sysConfig.uiDanger = document.getElementById('uiDanger').value;
    sysConfig.uiSuccess = document.getElementById('uiSuccess').value;

    const res = await fetch('/api/config', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(sysConfig) });
    if (res.ok) {
        showToast("Settings synchronized successfully.", "success");
    } else {
        showToast("Failed to save settings.", "error");
    }
}

async function loadOptionalLibraries() {
    try {
        loadExternalScript("https://unpkg.com/@fnando/sparkline/dist/sparkline.js").catch(e => console.warn("Sparkline unavailable in AP mode."));
        await loadExternalScript("https://cdnjs.cloudflare.com/ajax/libs/ace/1.32.7/ace.js");
        ace.config.set("basePath", "https://cdnjs.cloudflare.com/ajax/libs/ace/1.32.7/");
        await loadExternalScript("https://cdnjs.cloudflare.com/ajax/libs/ace/1.32.7/mode-lua.min.js");
        await loadExternalScript("https://cdnjs.cloudflare.com/ajax/libs/ace/1.32.7/theme-terminal.min.js");
        await loadExternalScript("https://cdnjs.cloudflare.com/ajax/libs/ace/1.32.7/ext-themelist.min.js");
        
        document.getElementById('fallback-editor').style.display = 'none';
        document.getElementById('lua-editor').style.display = 'block';
        
        editor = ace.edit("lua-editor");
        editor.session.setMode("ace/mode/lua");
        editor.setOptions({
            enableBasicAutocompletion: true,
            useSoftTabs: true,
            tabSize: 2,
            showPrintMargin: false,
            fontSize: "14px"
        });
        
        const themelist = ace.require("ace/ext/themelist");
        const themeSelect = document.getElementById("editor-theme");
        themeSelect.innerHTML = themelist.themes.map(t => `<option value="${t.theme}">${t.caption}</option>`).join('');
        
        const savedTheme = localStorage.getItem('ace-theme') || "ace/theme/terminal";
        themeSelect.value = savedTheme;
        editor.setTheme(savedTheme);
        
        const fallbackText = document.getElementById('fallback-editor').value;
        if (fallbackText) editor.setValue(fallbackText, -1);
        
    } catch (error) {
        console.warn("Offline/AP Mode detected: Utilizing native textarea fallback.");
    }
}

loadOptionalLibraries();

function changeEditorTheme(themePath) {
    if (editor) {
        editor.setTheme(themePath);
        localStorage.setItem('ace-theme', themePath);
    }
}

function setEditorContent(text) {
    if (editor) {
        editor.setValue(text, -1);
    } else {
        document.getElementById('fallback-editor').value = text;
    }
}

function getEditorContent() {
    if (editor) {
        return editor.getValue();
    } else {
        return document.getElementById('fallback-editor').value;
    }
}

async function loadScriptsList() {
    try {
        const res = await fetch('/api/scripts');
        const files = await res.json();
        availableScripts = files.map(f => f.replace('.lua', ''));
        renderFileList(files);
    } catch (e) { console.error(e); }
}

async function loadScriptContent(name) {
    const res = await fetch(`/api/read_script?name=${name.replace('.lua','')}`);
    const text = await res.text();
    setEditorContent(text);
    document.getElementById('script-name').value = name.replace('.lua','');
}

function newScript() {
    document.getElementById('script-name').value = '';
    setEditorContent('-- New Lua Effect\nlocal count = get_count()\nfor i=0, count-1 do\n    set_rgb(i, 255, 255, 255)\nend');
    showToast("Editor ready for new script", "info");
}

async function saveScript() {
    const name = document.getElementById('script-name').value;
    const code = getEditorContent();
    if (!name) return showToast("Filename is required!", "error");
    
    showToast("Saving, Please wait ...", "info");
    
    const blob = new Blob([code], { type: 'text/plain' });
    fetch(`/api/save_script?name=${name}`, { method: 'POST', headers: { 'Content-Type': 'application/octet-stream' }, body: blob }).catch(() => {
    });
    
    let attempts = 0;
    const maxAttempts = 20;
    
    const poll = setInterval(async () => {
        attempts++;
        if (attempts > maxAttempts) {
            clearInterval(poll);
            showToast("Reconnection timed out. Check your hardware.", "error");
            return;
        }

        try {
            const res = await fetch('/api/scripts', { signal: AbortSignal.timeout(1000) });
            if (res.ok) {
                clearInterval(poll);
                clearConsole();
                showToast(`Successfully saved and reconnected!`, "success");
                
                await loadScriptsList();
                await loadScriptContent(name);
            }
        } catch (e) {
            // Still rebooting...
        }
    }, 1500);
}

function renderFileList(files) {
    document.getElementById('file-list').innerHTML = files.map(f => `
        <div class="script-item" onclick="loadScriptContent('${f}')">
            <span>${f}</span>
            <button class="btn-danger" style="padding:2px 8px; font-size:0.7rem" onclick="event.stopPropagation(); deleteScript('${f.replace('.lua', '')}')">X</button>
        </div>
    `).join('');
}

async function deleteScript(name) {
    showToast("Confirm deletion by clicking X again within 3s", "info");
    const btn = event.currentTarget;
    if(btn.dataset.confirmed === "true") {
        const res = await fetch(`/api/delete_script?name=${name}`, { method: 'DELETE' });
        if (res.ok) {
            showToast("Script purged from flash.", "success");
            loadScriptsList();
        }
    } else {
        btn.dataset.confirmed = "true";
        btn.style.background = "var(--danger)";
        btn.style.color = "#fff";
        setTimeout(() => { 
            btn.dataset.confirmed = "false"; 
            btn.style.background = ""; 
            btn.style.color = "";
        }, 3000);
    }
}


class TarBuilder {
    constructor() { this.buffers = []; }

    addFile(name, contentString) {
        const encoder = new TextEncoder();
        const content = encoder.encode(contentString);
        const header = new Uint8Array(512);
        let offset = 0;

        const writeString = (str, len) => {
            for (let i = 0; i < str.length && i < len; i++) header[offset + i] = str.charCodeAt(i);
            offset += len;
        };

        writeString(name, 100);
        writeString("0000644\0", 8);
        writeString("0000000\0", 8);
        writeString("0000000\0", 8);
        writeString(content.length.toString(8).padStart(11, '0') + '\0', 12);
        writeString(Math.floor(Date.now() / 1000).toString(8).padStart(11, '0') + '\0', 12);        
        const chksumOffset = offset;
        writeString("        ", 8);
        writeString("0", 1);
        writeString("ustar\0", 6);
        writeString("00", 2);

        let chksum = 0;
        for (let i = 0; i < 512; i++) chksum += header[i];
        const chksumStr = chksum.toString(8).padStart(6, '0') + '\0 ';
        for (let i = 0; i < 8; i++) header[chksumOffset + i] = chksumStr.charCodeAt(i);

        this.buffers.push(header);
        this.buffers.push(content);
        
        const paddingLength = (512 - (content.length % 512)) % 512;
        if (paddingLength > 0) this.buffers.push(new Uint8Array(paddingLength));
    }

    getBlob() {
        this.buffers.push(new Uint8Array(1024)); 
        return new Blob(this.buffers, { type: 'application/x-tar' });
    }
}

async function exportBackup() {
    const exportConfig = document.getElementById('export-config').checked;
    const exportEffects = document.getElementById('export-effects').checked;

    if (!exportConfig && !exportEffects) {
        showToast("Please select at least one item to export.", "error");
        return;
    }

    const btn = document.getElementById('btn-export');
    const originalText = btn.innerText;
    btn.innerText = "Generating Bundle ...";
    btn.disabled = true;

    try {
        const tar = new TarBuilder();

        if (exportConfig) {
            const res = await fetch('/api/config');
            if (res.ok) {
                const configData = await res.text();
                tar.addFile('config.json', configData);
            } else {
                showToast("Warning: Failed to read config data.", "error");
            }
        }

        if (exportEffects) {
            const res = await fetch('/api/scripts');
            if (res.ok) {
                const scripts = await res.json();
                for (const scriptName of scripts) {
                    const scriptRes = await fetch(`/api/read_script?name=${scriptName}`);
                    if (scriptRes.ok) {
                        const scriptContent = await scriptRes.text();
                        tar.addFile(`fx/${scriptName}.lua`, scriptContent);
                    }
                }
            } else {
                showToast("Warning: Failed to fetch script list.", "error");
            }
        }

        const blob = tar.getBlob();
        const dateStr = new Date().toISOString().split('T')[0];
        const url = URL.createObjectURL(blob);
        
        const a = document.createElement('a');
        a.href = url;
        a.download = `backup_${dateStr}.tar`;
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        URL.revokeObjectURL(url);

        showToast("Backup exported successfully!", "success");

    } catch (e) {
        console.error("Export failed:", e);
        showToast("An error occurred during export.", "error");
    } finally {
        btn.innerText = originalText;
        btn.disabled = false;
    }
}


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


async function scanWifi() {
    const btn = document.getElementById('wifi-scan-btn');
    const dd = document.getElementById('wifi-dropdown');
    
    btn.innerText = "⏳ Scanning ...";
    btn.style.pointerEvents = 'none';
    dd.style.display = 'block';
    dd.innerHTML = '<div style="padding:20px; text-align:center; color:var(--text-dim);">Scanning ...</div>';

    const poll = async () => {
        try {
            const res = await fetch('/api/wifi/scan');
            
            if (res.status === 202) {
                setTimeout(poll, 1000);
                return;
            }
            
            if (!res.ok) throw new Error("Scan failed");
            
            const networks = await res.json();
            
            if (networks.length === 0) {
                dd.innerHTML = '<div style="padding:20px; text-align:center; color:var(--text-dim);">No networks found</div>';
            } else {
                const uniqueNets = {};
                networks.forEach(n => {
                    if (n.ssid && (!uniqueNets[n.ssid] || uniqueNets[n.ssid].rssi < n.rssi)) {
                        uniqueNets[n.ssid] = n;
                    }
                });

                dd.innerHTML = Object.values(uniqueNets).sort((a,b) => b.rssi - a.rssi).map(n => {
                    const isEncrypted = n.enc !== 0 && n.enc !== 7; 
                    const lockIcon = isEncrypted ? '<span style="font-size:0.8rem" title="Encrypted">🔒</span>' : '<span style="font-size:0.8rem; opacity:0.5;" title="Open">🔓</span>';
                    
                    let bars = 1;
                    if (n.rssi > -60) bars = 4;
                    else if (n.rssi > -70) bars = 3;
                    else if (n.rssi > -80) bars = 2;

                    const safeSsid = n.ssid.replace(/'/g, "\\'");

                    return `
                        <div class="wifi-item" onclick="selectWifi('${safeSsid}')">
                            <span style="font-weight:600; color:var(--text);">${n.ssid}</span>
                            <div class="wifi-info">
                                ${lockIcon}
                                <div class="wifi-bars" title="${n.rssi} dBm">
                                    <div class="w-bar ${bars>=1 ? 'active':''}" style="height:4px"></div>
                                    <div class="w-bar ${bars>=2 ? 'active':''}" style="height:8px"></div>
                                    <div class="w-bar ${bars>=3 ? 'active':''}" style="height:12px"></div>
                                    <div class="w-bar ${bars>=4 ? 'active':''}" style="height:16px"></div>
                                </div>
                            </div>
                        </div>
                    `;
                }).join('');
            }
            
            btn.innerText = "🔄 Scan";
            btn.style.pointerEvents = 'auto';

        } catch (e) {
            dd.innerHTML = '<div style="padding:20px; text-align:center; color:var(--danger);">Scan failed</div>';
            btn.innerText = "🔄 Scan";
            btn.style.pointerEvents = 'auto';
        }
    };
    
    poll();
}

function selectWifi(ssid) {
    document.getElementById('ssid').value = ssid;
    document.getElementById('wifi-dropdown').style.display = 'none';
    document.getElementById('pass').focus();
}

document.addEventListener('click', function(e) {
    const dd = document.getElementById('wifi-dropdown');
    if (dd && dd.style.display === 'block' && !e.target.closest('#wifi-dropdown') && !e.target.closest('#wifi-scan-btn')) {
        dd.style.display = 'none';
    }
});


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
        
        // Pass the specific element ID for OTA upgrades
        await processInstaller(arrayBuffer, 'install-progress'); 
        
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
    
    // Pass the specific element ID for local uploads
    await processInstaller(arrayBuffer, 'install-local-progress');
}

// Added the optional statusElementId parameter
async function processInstaller(arrayBuffer, statusElementId = 'install-progress') {
    const status = document.getElementById(statusElementId);

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


async function init() { 
    await getVer(); 
    await loadScriptsList(); 
    await loadConfig(); 
    setInterval(updateSysStats, 3000); 
}

window.onload = checkAuth;