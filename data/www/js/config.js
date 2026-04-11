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