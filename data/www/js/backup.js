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