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