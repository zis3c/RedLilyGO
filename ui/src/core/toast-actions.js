window.showToast = function(message, type, hideIcon = false) {
    var container = document.getElementById('toast-container');
    if (!container) return;

    var messageText = String(message);
    var lowerMessage = messageText.toLowerCase();
    if (type === 'clean') type = 'warning';
    if (!type) {
        if (/failed|error|unavailable|cannot|exceeds|empty|offline|unable|not found|could not|rejected/.test(lowerMessage)) type = 'error';
        else if (/warning|disabled|critical|must|required|configure|before/.test(lowerMessage)) type = 'warning';
        else if (/loading|testing|generating|starting|queued/.test(lowerMessage)) type = 'info';
        else type = 'success';
    }

    var toast = document.createElement('div');
    toast.className = 'bg-zinc-800 border border-zinc-700 text-zinc-200 text-[13px] px-4 py-3 rounded-xl shadow-2xl flex items-center gap-3 transform transition-all duration-300 translate-y-8 opacity-0 font-medium z-50 pointer-events-auto w-[90vw] sm:w-max max-w-sm';
    const icon = document.createElement('i');
    if (type === 'error') {
        icon.className = 'fa-solid fa-circle-exclamation text-red-500 text-[15px] shrink-0';
        toast.style.borderColor = 'rgba(239, 68, 68, 0.5)';
    } else if (type === 'warning') {
        icon.className = 'fa-solid fa-triangle-exclamation text-amber-400 text-[15px] shrink-0';
        toast.style.borderColor = 'rgba(245, 158, 11, 0.5)';
    } else if (type === 'info') {
        icon.className = 'fa-solid fa-circle-info text-sky-400 text-[15px] shrink-0';
        toast.style.borderColor = 'rgba(56, 189, 248, 0.5)';
    } else {
        icon.className = 'fa-solid fa-circle-check text-emerald-500 text-[15px] shrink-0';
        toast.style.borderColor = 'rgba(16, 185, 129, 0.5)';
    }
    const text = document.createElement('span');
    text.className = 'flex-1 break-words leading-snug';
    text.textContent = messageText;
    if (!hideIcon) toast.appendChild(icon);
    toast.appendChild(text);
    const closeBtn = document.createElement('button');
    closeBtn.className = 'text-zinc-500 hover:text-white transition-colors ml-2 shrink-0 flex items-center justify-center';
    closeBtn.innerHTML = '<i class="fa-solid fa-xmark text-sm"></i>';
    closeBtn.onclick = function() {
        toast.classList.add('translate-y-8', 'opacity-0');
        setTimeout(() => toast.remove(), 300);
    };
    toast.appendChild(closeBtn);
    container.appendChild(toast);

    requestAnimationFrame(() => {
        setTimeout(() => {
            toast.classList.remove('translate-y-8', 'opacity-0');
        }, 10);
    });

    setTimeout(() => {
        toast.classList.add('translate-y-8', 'opacity-0');
        setTimeout(() => toast.remove(), 300);
    }, 3000);
};

// Fetch the per-boot CSRF token once and reuse it. Cross-site scripts cannot
// read this response (same-origin policy) or set the X-CSRF-Token header.
window.redlilyCsrf = function() {
    if (window._csrfPromise) return window._csrfPromise;
    window._csrfPromise = fetch('/csrf-token', { cache: 'no-store' })
        .then(function(response) { return response.ok ? response.json() : {}; })
        .then(function(data) { window._csrfToken = (data && data.token) || ''; return window._csrfToken; })
        .catch(function() { window._csrfToken = ''; return ''; });
    return window._csrfPromise;
};

// POST a state-changing route with the CSRF token attached.
window.redlilyPost = function(url) {
    return window.redlilyCsrf().then(function(token) {
        return fetch(url, {
            method: 'POST',
            cache: 'no-store',
            headers: token ? { 'X-CSRF-Token': token } : {}
        });
    });
};

window.fireQuickAction = function(url) {
    return window.redlilyPost(url).then(async response => {
        const body = await response.text();
        if (!response.ok) {
            let detail = body;
            try {
                const payload = JSON.parse(body);
                detail = payload.error || payload.status || body;
            } catch (_) {}
            throw new Error('HTTP ' + response.status + (detail ? ': ' + detail : ''));
        }
        var lb = document.getElementById('logBox');
        if(lb) {
            lb.value += '\n> Dispatched: ' + url + '\n';
            lb.scrollTop = lb.scrollHeight;
        }
        if (window.showToast) {
            window.showToast("Command Dispatched!");
        }
        return true;
    }).catch(e => {
        console.error(e);
        var lb = document.getElementById('logBox');
        if (lb) lb.value += '\n> Action failed: ' + url + ' (' + e.message + ')\n';
        if (window.showToast) window.showToast("Action failed: " + e.message);
        return false;
    });
};

window.saveNetworkConfig = function() {
    var modeEl = document.getElementById('wifiMode');
    var ssidEl = document.getElementById('wifiSsid');
    var passEl = document.getElementById('wifiPass');
    var mode = (modeEl && modeEl.value === 'ap') ? 'true' : 'false';
    var ssid = ssidEl ? ssidEl.value.trim() : '';
    var pass = passEl ? passEl.value : '';

    if (mode !== 'true' && !ssid) {
        if (window.showToast) window.showToast("SSID cannot be empty!");
        return;
    }

    if (mode === 'true' && ((pass.length > 0 && pass.length < 8) || (!pass.length && !window.wifiPasswordConfigured))) {
        if (window.showToast) window.showToast("AP password must be at least 8 characters!");
        return;
    }

    var url = '/settings?network=save&apmode=' + mode + '&pass=' + encodeURIComponent(pass);
    if (ssid) url += '&ssid=' + encodeURIComponent(ssid);
    window.redlilyPost(url)
        .then(function(response) {
            if (!response.ok) throw new Error('HTTP ' + response.status);
            return response.text();
        })
        .then(function(message) {
            if (window.showToast) window.showToast(message || 'Wi-Fi configuration saved.');
        })
        .catch(function(error) {
            if (window.showToast) window.showToast('Wi-Fi save failed: ' + error.message, 'error');
        });
};

window.savePortalConfig = function() {
    var ssidEl = document.getElementById('portalSsid');
    var passEl = document.getElementById('portalPass');
    var openEl = document.getElementById('portalOpen');
    var ssid = ssidEl ? ssidEl.value.trim() : '';
    var pass = passEl ? passEl.value : '';
    var open = !!(openEl && openEl.checked);

    if (!ssid || ssid.length > 29) {
        if (window.showToast) window.showToast('Portal SSID must be 1-29 characters!');
        return;
    }
    if (!open && pass.length > 0 && pass.length < 8) {
        if (window.showToast) window.showToast('Portal password must be at least 8 characters!');
        return;
    }

    var url = '/settings?portal=save&ssid=' + encodeURIComponent(ssid);
    if (open) url += '&open=true';
    else if (pass) url += '&pass=' + encodeURIComponent(pass);
    window.redlilyPost(url)
        .then(function(response) {
            if (!response.ok) throw new Error('HTTP ' + response.status);
            return response.text();
        })
        .then(function(message) {
            if (window.showToast) window.showToast(message || 'Portal AP saved.');
        })
        .catch(function(error) {
            if (window.showToast) window.showToast('Portal save failed: ' + error.message, 'error');
        });
};

window.togglePasswordVisibility = function(inputId, btn) {
    var input = document.getElementById(inputId);
    var icon = btn ? btn.querySelector('i') : null;
    if (!input) return;
    if (input.type === 'password') {
        input.type = 'text';
        if (icon) {
            icon.classList.remove('fa-eye');
            icon.classList.add('fa-eye-slash');
        }
    } else {
        input.type = 'password';
        if (icon) {
            icon.classList.remove('fa-eye-slash');
            icon.classList.add('fa-eye');
        }
    }
};
