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