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