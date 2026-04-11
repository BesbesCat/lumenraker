async function init() { 
    await getVer(); 
    await loadScriptsList(); 
    await loadConfig(); 
    setInterval(updateSysStats, 3000); 
}

window.onload = checkAuth;