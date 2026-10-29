// === core\navigation.js ===
window.go = function(id, el) {
        if (!id) return;
        const targetPane = document.getElementById(id);
        if (!targetPane) return;

        document.documentElement.setAttribute('data-active-tab', id);
        try { localStorage.setItem('redlily_active_tab', id); } catch(e) {}

        document.querySelectorAll('.tab-pane').forEach(function(p) { p.classList.remove('active'); });
        targetPane.classList.add('active');

        if (el) {
            document.querySelectorAll('.nav-link').forEach(function(n) { n.classList.remove('active'); });
            el.classList.add('active');
            const pageTitle = document.getElementById('page-title');
            const span = el.querySelector('span');
            if (pageTitle && span) {
                pageTitle.innerText = span.innerText;
            }
        }

        if (window.initCustomDropdowns) {
            window.initCustomDropdowns();
        }
        if (id === 'tab-4' && window.refreshIdeFiles) window.refreshIdeFiles();
        if (id === 'tab-6' && window.refreshRuntimeLogs) window.refreshRuntimeLogs();

        if (window.innerWidth < 768 && window.closeSidebar) window.closeSidebar();
    };

        // Return to the tab the user last opened.
    document.addEventListener('DOMContentLoaded', () => {
        try {
            const requestedTab = localStorage.getItem('redlily_active_tab') || 'tab-1';
            const navLinks = Array.from(document.querySelectorAll('.nav-link'));
            const navLink = navLinks.find(el => el.dataset.tab === requestedTab) ||
                navLinks.find(el => el.dataset.tab === 'tab-1');
            const savedTab = navLink ? navLink.dataset.tab : 'tab-1';
            if (savedTab !== requestedTab) localStorage.setItem('redlily_active_tab', savedTab);
                if (navLink) {
                    window.go(savedTab, navLink);
                    if (window.innerWidth >= 768) {
                        const sb = document.getElementById('sidebar');
                        if (sb) sb.classList.remove('-translate-x-full');
                    }
                }
        } catch (err) {
            console.error("Failed to restore tab:", err);
        }

        // Reload the preview when a source file changes on disk.
        if (window.location.hostname === 'localhost' || window.location.hostname === '127.0.0.1') {
            let lastBuildVer = null;
            setInterval(() => {
                fetch('/preview/version?_t=' + Date.now())
                    .then(r => r.json())
                    .then(data => {
                        if (data && data.version) {
                            if (lastBuildVer === null) {
                                lastBuildVer = data.version;
                            } else if (lastBuildVer !== data.version) {
                                console.log('[LiveReload] New UI build detected. Auto-refreshing...');
                                window.location.reload();
                            }
                        }
                    })
                    .catch(() => {});
            }, 1200);
        }

        // Let mobile users close the sidebar from the backdrop, outside area, or a left swipe.
        const overlay = document.getElementById('overlay');
        if (overlay) {
            ['click', 'touchstart', 'pointerdown'].forEach(evt => {
                overlay.addEventListener(evt, (e) => {
                    e.preventDefault();
                    window.closeSidebar();
                }, { passive: false });
            });
        }

        // Close the mobile sidebar when the user taps outside it.
        document.addEventListener('pointerdown', (e) => {
            if (window.innerWidth >= 768) return;
            const sidebar = document.getElementById('sidebar');
            if (!sidebar || sidebar.classList.contains('-translate-x-full')) return;
            
            const hamburger = e.target.closest('[data-action="open-sidebar"]');
            if (sidebar.contains(e.target) || hamburger) return;
            
            window.closeSidebar();
        });

        // Close the sidebar when the user swipes left across it.
        const sidebar = document.getElementById('sidebar');
        if (sidebar) {
            let touchStartX = 0;
            let touchStartY = 0;
            sidebar.addEventListener('touchstart', (e) => {
                touchStartX = e.touches[0].clientX;
                touchStartY = e.touches[0].clientY;
            }, { passive: true });

            sidebar.addEventListener('touchend', (e) => {
                const touchEndX = e.changedTouches[0].clientX;
                const touchEndY = e.changedTouches[0].clientY;
                const deltaX = touchEndX - touchStartX;
                const deltaY = touchEndY - touchStartY;
                if (deltaX < -35 && Math.abs(deltaX) > Math.abs(deltaY)) {
                    window.closeSidebar();
                }
            }, { passive: true });
        }
    });

    document.addEventListener('click', (event) => {
        const tab = event.target.closest('[data-tab]');
        if (tab) {
            event.preventDefault();
            window.go(tab.dataset.tab, tab);
            return;
        }
        const action = event.target.closest('[data-action]');
        if (!action) return;
        if (action.dataset.action === 'toggle-sidebar') document.body.classList.toggle('sidebar-collapsed');
        if (action.dataset.action === 'open-sidebar') window.openSidebar();
        if (action.dataset.action === 'close-sidebar') window.closeSidebar();
    });

    window.openSidebar = function() {
        const sb = document.getElementById('sidebar');
        const ov = document.getElementById('overlay');
        if (sb) sb.classList.remove('-translate-x-full');
        if (ov) ov.classList.remove('hidden');
        document.body.classList.add('mobile-sidebar-open');
    };
    window.closeSidebar = function() {
        const sb = document.getElementById('sidebar');
        const ov = document.getElementById('overlay');
        if (sb) sb.classList.add('-translate-x-full');
        if (ov) ov.classList.add('hidden');
        document.body.classList.remove('mobile-sidebar-open');
    };

// === core\shortcuts.js ===
// Register keyboard shortcuts shared by every dashboard page.
    document.addEventListener('keydown', function(e) {
        if (e.ctrlKey) {
            var handled = false;
            var tabsMap = {
                '1': 'tab-1', // Status
                '2': 'tab-3', // Tools
                '3': 'tab-4', // Files
                '4': 'tab-8', // Remote
                'l': 'tab-6'  // Logs
            };

            if (tabsMap[e.key.toLowerCase()]) {
                var targetId = tabsMap[e.key.toLowerCase()];
                var navLink = document.querySelector('a[data-tab="' + targetId + '"]');
                if (navLink) {
                    navLink.click();
                    handled = true;
                }
            }

            if (handled) {
                e.preventDefault();
            }
        }
    });

// === core\dropdowns.js ===
// Replace marked native selects with custom dropdown controls.
    function initCustomDropdowns() {
        document.querySelectorAll('select.cdrop-select').forEach(function(sel) {
            if (sel.dataset.cdropInit) return;
            sel.dataset.cdropInit = '1';
            sel.style.display = 'none';

            var wrap = document.createElement('div');
            wrap.className = 'cdrop' + (sel.classList.contains('w-full') ? ' w-full' : '');
            sel.parentNode.insertBefore(wrap, sel);
            wrap.appendChild(sel);

            // Build the control that opens the options menu.
            var btn = document.createElement('button');
            btn.type = 'button';
            btn.className = 'cdrop-btn';
            var firstOpt = sel.options[sel.selectedIndex] || sel.options[0];
            var label = document.createElement('span');
            label.className = 'cdrop-text';
            label.textContent = firstOpt ? firstOpt.text : 'Select...';
            var chevron = document.createElement('i');
            chevron.className = 'fa-solid fa-chevron-down cdrop-chevron';
            btn.appendChild(label);
            btn.appendChild(chevron);
            wrap.insertBefore(btn, sel);

            sel.addEventListener('change', function() {
                var selectedOpt = sel.options[sel.selectedIndex] || sel.options[0];
                if (selectedOpt) btn.querySelector('.cdrop-text').textContent = selectedOpt.text;
                menu.querySelectorAll('.cdrop-item').forEach(function(el) {
                    el.classList.toggle('selected', el.dataset.value === sel.value);
                });
            });

            // Build the list of choices from the native select options.
            var menu = document.createElement('div');
            menu.className = 'cdrop-menu';
            buildMenu(sel, menu, btn);
            wrap.appendChild(menu);

            // Keep the custom control and native select in sync when opened.
            btn.addEventListener('click', function(e) {
                e.preventDefault();
                e.stopPropagation();
                var isOpen = wrap.classList.contains('open');
                closeAllDropdowns();
                if (!isOpen) wrap.classList.add('open');
            });
        });
    }

    function buildMenu(sel, menu, btn) {
        menu.innerHTML = '';
        var children = sel.children;
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            if (child.tagName === 'OPTGROUP') {
                var grp = document.createElement('div');
                grp.className = 'cdrop-group';
                grp.textContent = child.label;
                menu.appendChild(grp);
                var opts = child.children;
                for (var j = 0; j < opts.length; j++) {
                    menu.appendChild(makeItem(opts[j], sel, btn, menu));
                }
            } else if (child.tagName === 'OPTION') {
                menu.appendChild(makeItem(child, sel, btn, menu));
            }
        }
    }

    function makeItem(opt, sel, btn, menu) {
        var item = document.createElement('button');
        item.type = 'button';
        item.className = 'cdrop-item';
        if (opt.selected) item.classList.add('selected');
        item.textContent = opt.text;
        item.dataset.value = opt.value;
        item.addEventListener('click', function(e) {
            e.preventDefault();
            e.stopPropagation();
            sel.value = opt.value;
            sel.dispatchEvent(new Event('change'));
            btn.querySelector('.cdrop-text').textContent = opt.text;
            menu.querySelectorAll('.cdrop-item').forEach(function(el) { el.classList.remove('selected'); });
            item.classList.add('selected');
            closeAllDropdowns();
        });
        return item;
    }

    function closeAllDropdowns() {
        document.querySelectorAll('.cdrop.open').forEach(function(d) { d.classList.remove('open'); });
    }

    document.addEventListener('click', function(e) {
        if (!e.target.closest('.cdrop')) closeAllDropdowns();
    });

    function observeSelects() {
        var observer = new MutationObserver(function(mutations) {
            mutations.forEach(function(mutation) {
                if (mutation.target.tagName === 'SELECT' && mutation.target.classList.contains('cdrop-select')) {
                    var sel = mutation.target;
                    var wrap = sel.closest('.cdrop');
                    if (wrap) {
                        var menu = wrap.querySelector('.cdrop-menu');
                        var btn = wrap.querySelector('.cdrop-btn');
                        if (menu && btn) {
                            buildMenu(sel, menu, btn);
                            var selectedOpt = sel.options[sel.selectedIndex] || sel.options[0];
                            if (selectedOpt) {
                                btn.querySelector('.cdrop-text').textContent = selectedOpt.text;
                            }
                        }
                    }
                }
            });
        });
        document.querySelectorAll('select.cdrop-select').forEach(function(sel) {
            observer.observe(sel, { childList: true, subtree: true });
        });
    }

    window.initCustomDropdowns = initCustomDropdowns;
    document.addEventListener('DOMContentLoaded', function() {
        initCustomDropdowns();
        observeSelects();
    });

// === core\modal.js ===
window.showConfirmModal = function(title, text, confirmCallback) {
    const modal = document.getElementById('confirmModal');
    const modalContent = document.getElementById('confirmModalContent');
    const titleEl = document.getElementById('confirmModalTitle');
    const textEl = document.getElementById('confirmModalText');
    const cancelBtn = document.getElementById('confirmModalCancel');
    const confirmBtn = document.getElementById('confirmModalConfirm');

    if(!modal) return;

    titleEl.innerText = title;
    textEl.innerText = text;

    modal.classList.remove('hidden');
    // Apply the initial state before adding the transition class.
    void modal.offsetWidth;
    modal.classList.remove('opacity-0');
    modalContent.classList.remove('scale-95');

    const closeModal = () => {
        modal.classList.add('opacity-0');
        modalContent.classList.add('scale-95');
        setTimeout(() => modal.classList.add('hidden'), 300);
    };

    cancelBtn.onclick = closeModal;
    confirmBtn.onclick = () => {
        closeModal();
        if(confirmCallback) confirmCallback();
    };
};

// === core\toast-actions.js ===
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

// === features\dashboard\telemetry.js ===
// Poll the ESP32 telemetry endpoint and update dashboard values and charts.
    var REDLILY_DATA_CACHE_MS = 5000;
    window.redlilyData = function(force, includeFiles, full) {
        var now = Date.now();
        var requestUrl = full ? (includeFiles ? '/data.json?files=1' : '/data.json')
            : (includeFiles ? '/files' : '/telemetry');
        if (!includeFiles && !force && window._redlilyDataValue && window._redlilyDataValueUrl === requestUrl && now - window._redlilyDataAt < REDLILY_DATA_CACHE_MS) {
            return Promise.resolve(window._redlilyDataValue);
        }
        if (!window._redlilyDataRequest || window._redlilyDataRequestUrl !== requestUrl) {
            var controller = new AbortController();
            var timeout = setTimeout(function() { controller.abort(); }, 15000);
            window._redlilyDataRequestUrl = requestUrl;
            window._redlilyDataRequest = fetch(requestUrl, { cache: 'no-store', signal: controller.signal })
                .then(function(response) {
                    if (!response.ok) throw new Error('API error');
                    return response.json();
                })
                .then(function(data) {
                    window._redlilyDataValue = data;
                    window._redlilyDataValueUrl = requestUrl;
                    window._redlilyDataAt = Date.now();
                    return data;
                })
                .finally(function() {
                    clearTimeout(timeout);
                    window._redlilyDataRequest = null;
                    window._redlilyDataRequestUrl = null;
                });
        }
        return window._redlilyDataRequest;
    };

    document.addEventListener('input', function(event) {
        if (event.target && event.target.id === 'wifiSsid') event.target.dataset.userEdited = 'true';
    });
    document.addEventListener('focusin', function(event) {
        if (event.target && event.target.id === 'wifiSsid') event.target.dataset.userEdited = 'true';
    });

    var telemetryInterval;
    var telemetryInFlight = false;
    clearInterval(telemetryInterval);
    telemetryInterval = setInterval(function() {
            // Read current values from the firmware telemetry endpoint.
            if (!telemetryInFlight && (document.getElementById('sdcardText') || document.getElementById('heapLiveText') || document.getElementById('temperatureLiveText'))) {
                telemetryInFlight = true;
                window.redlilyData()
                    .then(data => {
                        try {
                        var storage = data.storage && typeof data.storage === 'object' ? data.storage : null;
                        var sdAvailable = !!(storage && storage.mounted === true);
                        // heapUsagePc is already a percentage, so no conversion is needed.
                        var realHeapPc = Number(data.heapUsagePc);
                        if (!Number.isFinite(realHeapPc)) realHeapPc = 0;

                var sdText = document.getElementById('sdcardText');
                function formatBytes(bytes) {
                    var value = Number(bytes);
                    if (!Number.isFinite(value)) return 'Unavailable';
                    if (value < 1024) return value + ' B';
                    var units = ['KB', 'MB', 'GB', 'TB'];
                    var unit = -1;
                    do { value /= 1024; unit++; } while (value >= 1024 && unit < units.length - 1);
                    return value.toFixed(value >= 10 ? 0 : 1) + ' ' + units[unit];
                }
                function setTelemetryValue(element, text, healthyClass) {
                    if (!element) return;
                    element.innerText = text;
                    element.className = healthyClass;
                }
                function formatUsedPercent(value) {
                    var percent = Number(value);
                    if (!Number.isFinite(percent)) return 'Unavailable';
                    return percent > 0 && percent < 1 ? percent.toFixed(2) : percent.toFixed(0);
                }
                if (sdText) {
                    var storageAvailable = !!(storage && storage.available);
                    setTelemetryValue(sdText, storageAvailable
                        ? formatBytes(storage.usedBytes) + ' / ' + formatBytes(storage.totalBytes) + ' · ' + formatUsedPercent(storage.usedPercent) + '% used'
                        : (storage && storage.mounted === true ? '--' : 'ERR'),
                        'text-white text-[13px] font-medium');
                }
                var sdMetric = document.getElementById('sdMetricStatus');
                setTelemetryValue(sdMetric, sdAvailable ? 'Ready' : 'ERR',
                    'text-white text-[13px] font-medium');
                var sdBar = document.querySelector('#sdcard_capacity .progress-bar');
                if (sdBar) {
                    if (storage && storage.available) {
                        var usedPercent = Math.max(0, Math.min(100, Number(storage.usedPercent || 0)));
                        sdBar.style.width = usedPercent + '%';
                        sdBar.style.minWidth = Number(storage.usedBytes || 0) > 0 ? '2px' : '0';
                    } else {
                        sdBar.style.width = '0%';
                        sdBar.style.minWidth = '0';
                    }
                }
                var temperature = Number(data.temperatureC);
                var temperatureAvailable = data.temperatureC !== null && data.temperatureC !== undefined && Number.isFinite(temperature);
                var temperatureText = temperatureAvailable ? temperature.toFixed(1) + ' \u00b0C' : 'Unavailable';
                var temperatureLive = document.getElementById('temperatureLiveText');
                var temperatureMetric = document.getElementById('temperatureMetric');
                var temperatureClass = temperature >= 80 ? 'text-red-400' : (temperature >= 70 ? 'text-amber-400' : 'text-white');
                setTelemetryValue(temperatureLive, temperatureText,
                    temperatureClass + ' text-[11px] font-mono font-bold');
                setTelemetryValue(temperatureMetric, temperatureText,
                    temperatureClass + ' font-medium text-[13px] truncate');

                        var heapLive = document.getElementById('heapLiveText');
                        var heapAvailable = Number.isFinite(Number(data.heapUsagePc)) && Number(data.heapSize) > 0;
                        setTelemetryValue(heapLive, heapAvailable ? realHeapPc + '% used' : 'Unavailable',
                            'text-white text-[11px] font-mono font-bold');

                        if (!window._lastTempWarningTime) window._lastTempWarningTime = 0;
                        if (!window._lastHeapWarningTime) window._lastHeapWarningTime = 0;
                        var now = Date.now();
                        if (temperature >= 80 && (now - window._lastTempWarningTime > 60000)) {
                            window.showToast('System temperature critical (' + temperature.toFixed(1) + ' \u00b0C)', 'clean');
                            window._lastTempWarningTime = now;
                        }
                        if (realHeapPc >= 90 && (now - window._lastHeapWarningTime > 60000)) {
                            window.showToast('System memory critical (' + realHeapPc + '%)', 'clean');
                            window._lastHeapWarningTime = now;
                        }

                        // Update the live status cards.
                        var uptimeEl = document.getElementById('uptime');
                        if (uptimeEl && data.uptime) uptimeEl.innerText = data.uptime;

                        var statusEl = document.getElementById('status');
                        if (statusEl && data.status) statusEl.innerText = data.status;
                        var safeModeBanner = document.getElementById('safeModeBanner');
                        if (safeModeBanner) safeModeBanner.classList.toggle('hidden', data.safeMode !== true);
                        if (safeModeBanner) safeModeBanner.classList.toggle('flex', data.safeMode === true);

                        var usbModeEl = document.getElementById('USBmode');
                        if (usbModeEl && data.USBmode) usbModeEl.innerText = data.USBmode;
                        var usbIcon = document.getElementById('usbModeIcon');
                        if (usbIcon) {
                            var usbDevice = Number(data.usbDeviceType);
                            var usbClass = Number(data.usbClassType);
                            usbIcon.className = 'text-zinc-600 text-[13px] ' +
                                (usbDevice === 0 ? 'fa-solid fa-circle-xmark' :
                                usbClass === 1 ? 'fa-solid fa-keyboard' :
                                usbClass === 2 ? 'fa-solid fa-sd-card' :
                                usbDevice === 2 ? 'fa-solid fa-network-wired' : 'fa-brands fa-usb');
                            usbIcon.title = data.USBmode || 'USB mode';
                            usbIcon.setAttribute('aria-label', usbIcon.title);
                        }

                        var errorCountEl = document.getElementById('errorCount');
                        if (errorCountEl && data.errorCount != null) errorCountEl.innerText = data.errorCount;

                        var wifiSsidEl = document.getElementById('wifiSsid');
                        if (wifiSsidEl && typeof data.wifiSsid === 'string' && !wifiSsidEl.dataset.userEdited && !wifiSsidEl.dataset.initialized && document.activeElement !== wifiSsidEl) {
                            wifiSsidEl.value = data.wifiSsid;
                            wifiSsidEl.dataset.initialized = 'true';
                        }
                        window.wifiPasswordConfigured = data.wifiPasswordConfigured === true;
                        var wifiPasswordStatus = document.getElementById('wifiPasswordStatus');
                        if (wifiPasswordStatus) {
                            wifiPasswordStatus.textContent = data.wifiPasswordConfigured ? 'Password configured' : 'No password configured';
                            wifiPasswordStatus.className = data.wifiPasswordConfigured
                                ? 'mt-1.5 text-[11px] text-emerald-500'
                                : 'mt-1.5 text-[11px] text-amber-500';
                        }

                        // Refresh the device log shown in the terminal panel.
                        var logBoxEl = document.getElementById('logBox');
                        if (logBoxEl && !window.showingSavedLog && Array.isArray(data.logMessages)) {
                            var logMessages = Array.isArray(data.logMessages) ? data.logMessages : [];
                            var newLogs = logMessages.map(function(line) {
                                line = String(line);
                                return line.indexOf('> ') === 0 ? line : '> ' + line;
                            }).join('\n');
                            // Avoid firing the input handler unless the displayed log changed.
                            if (logBoxEl.value !== newLogs) {
                                // Preserve locally appended actions unless the device has newer log data.
                                if (newLogs.length >= logBoxEl.value.length || newLogs.substring(0, 20) !== logBoxEl.value.substring(0, 20)) {
                                    logBoxEl.value = newLogs;
                                }
                            }
                        }

                        // Update charts only after Chart.js has created them.
                        if (window.memChart) {
                            var memPoints = window.memChart.data.datasets[0].data;
                            if (memPoints.length >= 20) memPoints.shift();
                            memPoints.push(realHeapPc);
                            window.memChart.data.labels = memPoints.map(function(_, i) { return i + 1; });
                            window.memChart.update('none');
                        }

                        // Add the latest temperature sample to the chart.
                        if (window.temperatureChart && Number.isFinite(temperature)) {
                            var tempPoints = window.temperatureChart.data.datasets[0].data;
                            if (tempPoints.length >= 20) tempPoints.shift();
                            tempPoints.push(temperature);
                            window.temperatureChart.data.labels = tempPoints.map(function(_, i) { return i + 1; });
                            window.temperatureChart.update('none');
                        }
                        } catch (renderError) {
                            console.error('Telemetry render error:', renderError);
                        }
                    })
                    .finally(function() { telemetryInFlight = false; })
                        .catch(err => {
                        ['sdcardText', 'sdMetricStatus', 'heapLiveText', 'temperatureLiveText', 'temperatureMetric', 'wifiSsid', 'wifiPasswordStatus'].forEach(function(id) {
                            var element = document.getElementById(id);
                            if (element) {
                                element.innerText = 'Unavailable';
                            }
                        });
                        var statusEl = document.getElementById('status');
                        if (statusEl) statusEl.innerText = 'Telemetry unavailable';
                    });
            }
    }, 5000); // Poll every 5s to limit ESP32 heap churn

// === features\dashboard\capabilities.js ===
function applyCapabilityState(tabId, available, title, explanation) {
        var nav = document.querySelector('a[data-tab="' + tabId + '"]');
        var tab = document.getElementById(tabId);
        if (!nav || !tab) return;
        nav.classList.toggle('capability-unavailable', !available);
        nav.setAttribute('aria-disabled', available ? 'false' : 'true');
        nav.title = available ? title : title + ' unavailable. ' + explanation;
        // Keep the page reachable; its navigation item already explains the missing capability.
    }

    function refreshCapabilityState() {
        return window.redlilyData()
            .then(function(data) {
                var capabilities = Array.isArray(data.capabilities) ? data.capabilities : [];
                applyCapabilityState('tab-3', capabilities.indexOf('MARAUDER') !== -1, 'Marauder', 'Marauder is not available in this firmware build.');
                applyCapabilityState('tab-4', capabilities.indexOf('SD') !== -1, 'Files', 'SD card is not mounted or storage service is unavailable.');

                var marauderReady = capabilities.indexOf('MARAUDER') !== -1;
                var marauderStatusEl = document.getElementById('marauderStatus');
                if (marauderStatusEl) {
                    if (marauderReady) {
                        marauderStatusEl.className = 'text-emerald-500 text-[11px] font-mono';
                        marauderStatusEl.textContent = 'Ready';
                    } else {
                        marauderStatusEl.className = 'text-amber-500 text-[11px] font-mono';
                        marauderStatusEl.textContent = 'Unavailable';
                    }
                }
            });
    }

    // Keep pages reachable and report unavailable capabilities in the navigation.
    document.addEventListener('DOMContentLoaded', function() {
        refreshCapabilityState()
            .catch(function(error) {
                var statusEl = document.getElementById('status');
                if (statusEl) statusEl.textContent = 'API Error';
                console.error('RedLilyGO API unavailable:', error);
            });
    });

// === features\dashboard\dashboard.js ===
// Create the dashboard telemetry charts.
    window.initDashboardCharts = function() {
        if (window._dashboardChartsReady || typeof Chart === 'undefined') return;
        window._dashboardChartsReady = true;
        if (typeof Chart !== 'undefined') {
            Chart.defaults.color = '#9b9b9b';
            Chart.defaults.font.family = 'Inter, sans-serif';

            // Show recent heap usage.
            var ctxMem = document.getElementById('memoryChart');
            if (ctxMem) {
                var memData = [];
                window.memChart = new Chart(ctxMem, {
                    type: 'line',
                    data: {
                        labels: [],
                        datasets: [{
                            label: 'Heap %',
                            data: memData,
                            borderColor: '#ef4444',
                            backgroundColor: 'rgba(239, 68, 68, 0.1)',
                            borderWidth: 2,
                            tension: 0.4,
                            fill: true,
                            pointRadius: 0
                        }]
                    },
                    options: {
                        responsive: true,
                        maintainAspectRatio: false,
                        animation: { duration: 0 },
                        scales: {
                            y: { min: 0, max: 100, grid: { color: '#27272a' } },
                            x: { grid: { display: false }, ticks: { display: false } }
                        },
                        plugins: { legend: { display: false } }
                    }
                });
            }

            // Show recent temperature readings.
            var ctxTemperature = document.getElementById('temperatureChart');
            if (ctxTemperature) {
                var temperatureData = [];
                window.temperatureChart = new Chart(ctxTemperature, {
                    type: 'line',
                    data: {
                        labels: [],
                        datasets: [{
                            label: '\u00b0C',
                            data: temperatureData,
                            borderColor: '#f97316',
                            backgroundColor: 'rgba(249, 115, 22, 0.1)',
                            borderWidth: 2,
                            tension: 0.4,
                            fill: true,
                            pointRadius: 0
                        }]
                    },
                    options: {
                        responsive: true,
                        maintainAspectRatio: false,
                        animation: { duration: 0 },
                        scales: {
                            y: { min: 0, max: 100, grid: { color: '#27272a' } },
                            x: { grid: { display: false }, ticks: { display: false } }
                        },
                        plugins: { legend: { display: false } }
                    }
                });
            }

        }
    };
    document.addEventListener('DOMContentLoaded', function() {
        window.initDashboardCharts();
    });

// === features\tools\autocomplete.js ===
// Suggest Marauder and Agent commands in the terminal input.
function escapeHtml(s) {
    return String(s).replace(/[&<>"']/g, function(c) {
        return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c];
    });
}

function fuzzyMatch(query, text) {
    var q = query.toLowerCase();
    var t = text.toLowerCase();
    var indices = [];
    if (!q) return { matched: false, score: 0, indices: [] };

    if (t.indexOf(q) === 0) {
        for (var i = 0; i < q.length; i++) indices.push(i);
        return { matched: true, score: 10000 - q.length, indices: indices };
    }
    var at = t.indexOf(q);
    if (at !== -1) {
        for (var j = 0; j < q.length; j++) indices.push(at + j);
        return { matched: true, score: 5000 - at, indices: indices };
    }
    // Match characters in order even when they are not adjacent, such as "att" in "attack".
    var qi = 0, score = 0, prev = -1;
    for (var k = 0; k < t.length && qi < q.length; k++) {
        if (t[k] === q[qi]) {
            score += (prev === -1 || k === prev + 1) ? 20 : 5;
            indices.push(k);
            prev = k;
            qi++;
        }
    }
    if (qi === q.length) return { matched: true, score: score, indices: indices };
    return { matched: false, score: 0, indices: [] };
}

function highlightMatch(text, indices) {
    if (!indices.length) return escapeHtml(text);
    var html = '', last = 0;
    indices.forEach(function(i) {
        html += escapeHtml(text.slice(last, i));
        html += '<span class="text-emerald-400 font-bold">' + escapeHtml(text[i]) + '</span>';
        last = i + 1;
    });
    html += escapeHtml(text.slice(last));
    return html;
}

function attachCommandAutocomplete(input, dropdown, commands) {
    if (!input || !dropdown) return;

    dropdown.style.maxHeight = '340px';
    dropdown.style.overflowY = 'auto';

    var activeIndex = -1;
    var results = [];

    function hide() {
        dropdown.classList.add('hidden');
        dropdown.classList.remove('flex');
        activeIndex = -1;
    }
    function show() {
        dropdown.classList.remove('hidden');
        dropdown.classList.add('flex');
    }

    function setActive(idx) {
        var rows = dropdown.children;
        var activeRow = null;
        for (var i = 0; i < rows.length; i++) {
            var r = rows[i];
            if (r.dataset && r.dataset.index !== undefined) {
                var on = Number(r.dataset.index) === idx;
                r.classList.toggle('bg-zinc-700/60', on);
                if (on) activeRow = r;
            }
        }
        activeIndex = idx;
        if (activeRow && activeRow.scrollIntoView) activeRow.scrollIntoView({ block: 'nearest' });
    }

    function choose(idx) {
        if (idx < 0 || idx >= results.length) return;
        input.value = results[idx].value;
        hide();
        input.focus();
    }

    function renderItems(items) {
        dropdown.innerHTML = '';
        var lastCat = null;
        items.forEach(function(item, idx) {
            if (item.cat !== lastCat) {
                var header = document.createElement('div');
                header.className = 'px-3 pt-2 pb-1 text-[10px] font-bold uppercase tracking-widest text-zinc-500 bg-zinc-800/80 flex items-center gap-1.5 sticky top-0';
                var icon = document.createElement('i');
                icon.className = String(item.icon || '') + ' ' + String(item.tone || '') + ' text-[10px]';
                header.appendChild(icon);
                header.appendChild(document.createTextNode(String(item.cat || '')));
                dropdown.appendChild(header);
                lastCat = item.cat;
            }
            var row = document.createElement('div');
            row.className = 'px-3 py-2 cursor-pointer transition-colors flex items-start gap-3 border-b border-zinc-700/40 last:border-0';
            row.dataset.index = idx;
            row.innerHTML = '<span class="font-mono text-white text-[12px] leading-tight shrink-0">' + highlightMatch(item.value, item.indices) + '</span>' +
                '<span class="text-zinc-500 text-[11px] leading-tight ml-auto text-right shrink-0 max-w-[50%]">' + escapeHtml(item.desc) + '</span>';
            row.addEventListener('mousedown', function(e) { e.preventDefault(); choose(idx); });
            row.addEventListener('mouseenter', function() { setActive(idx); });
            dropdown.appendChild(row);
        });
    }

    function update() {
        var val = input.value;
        activeIndex = -1;
        results = [];
        if (!val.trim()) { hide(); return; }

        var q = val.trim();
        commands.forEach(function(c) {
            var m = fuzzyMatch(q, c.value);
            if (m.matched) {
                results.push({ value: c.value, desc: c.desc, cat: c.cat, icon: c.icon, tone: c.tone, score: m.score, indices: m.indices });
            }
        });
        results.sort(function(a, b) { return b.score - a.score; });

        if (!results.length) { hide(); return; }

        renderItems(results);
        show();
        setActive(0);
    }

    input.addEventListener('input', update);
    input.addEventListener('focus', update);

    input.addEventListener('keydown', function(e) {
        if (dropdown.classList.contains('hidden')) {
            if (e.key === 'ArrowDown') { update(); e.preventDefault(); }
            return;
        }
        if (e.key === 'ArrowDown') { e.preventDefault(); setActive(Math.min(activeIndex + 1, results.length - 1)); }
        else if (e.key === 'ArrowUp') { e.preventDefault(); setActive(Math.max(activeIndex - 1, 0)); }
        else if (e.key === 'Enter' || e.key === 'Tab') {
            if (activeIndex >= 0) { e.preventDefault(); choose(activeIndex); }
        }
        else if (e.key === 'Escape') { e.preventDefault(); hide(); }
    });

    document.addEventListener('click', function(e) {
        if (!dropdown.contains(e.target) && e.target !== input) hide();
    });
}

var MARAUDER_COMMANDS = [
    { cat: 'WiFi Attack', icon: 'fa-solid fa-skull', tone: 'text-red-500', value: 'attack -t deauth', desc: 'Deauth selected APs (kick clients off)' },
    { cat: 'WiFi Attack', icon: 'fa-solid fa-skull', tone: 'text-red-500', value: 'attack -t deauth -c', desc: 'Deauth APs + connected stations' },
    { cat: 'WiFi Attack', icon: 'fa-solid fa-skull', tone: 'text-red-500', value: 'attack -t beacon -l', desc: 'Beacon spam: listed SSIDs' },
    { cat: 'WiFi Attack', icon: 'fa-solid fa-skull', tone: 'text-red-500', value: 'attack -t beacon -r', desc: 'Beacon spam: random SSIDs' },
    { cat: 'WiFi Attack', icon: 'fa-solid fa-skull', tone: 'text-red-500', value: 'attack -t beacon -a', desc: 'Beacon spam: fake AP copies' },
    { cat: 'WiFi Attack', icon: 'fa-solid fa-skull', tone: 'text-red-500', value: 'attack -t probe', desc: 'Probe request flood' },
    { cat: 'WiFi Attack', icon: 'fa-solid fa-skull', tone: 'text-red-500', value: 'attack -t rickroll', desc: 'Rickroll beacon spam' },

    { cat: 'WiFi Scan', icon: 'fa-solid fa-satellite-dish', tone: 'text-sky-500', value: 'scanap', desc: 'Scan nearby access points' },
    { cat: 'WiFi Scan', icon: 'fa-solid fa-satellite-dish', tone: 'text-sky-500', value: 'scansta', desc: 'Scan stations / clients' },
    { cat: 'WiFi Scan', icon: 'fa-solid fa-satellite-dish', tone: 'text-sky-500', value: 'sniffraw', desc: 'WiFi packet capture (PCAP)' },
    { cat: 'WiFi Scan', icon: 'fa-solid fa-satellite-dish', tone: 'text-sky-500', value: 'sniffbeacon', desc: 'Sniff beacon frames' },
    { cat: 'WiFi Scan', icon: 'fa-solid fa-satellite-dish', tone: 'text-sky-500', value: 'sniffprobe', desc: 'Sniff probe requests' },
    { cat: 'WiFi Scan', icon: 'fa-solid fa-satellite-dish', tone: 'text-sky-500', value: 'sniffdeauth', desc: 'Sniff deauth frames' },
    { cat: 'WiFi Scan', icon: 'fa-solid fa-satellite-dish', tone: 'text-sky-500', value: 'sniffpmkid', desc: 'Capture WPA handshakes (PMKID)' },
    { cat: 'WiFi Scan', icon: 'fa-solid fa-satellite-dish', tone: 'text-sky-500', value: 'sniffpwn', desc: 'Sniff Pwnagotchi beacons' },
    { cat: 'WiFi Scan', icon: 'fa-solid fa-satellite-dish', tone: 'text-sky-500', value: 'sniffesp', desc: 'Sniff ESP-Marauder packets' },
    { cat: 'WiFi Scan', icon: 'fa-solid fa-satellite-dish', tone: 'text-sky-500', value: 'sigmon', desc: 'Monitor signal strength' },

    { cat: 'WiFi Targets', icon: 'fa-solid fa-list', tone: 'text-yellow-500', value: 'list -a', desc: 'List access points' },
    { cat: 'WiFi Targets', icon: 'fa-solid fa-list', tone: 'text-yellow-500', value: 'list -s', desc: 'List SSIDs' },
    { cat: 'WiFi Targets', icon: 'fa-solid fa-list', tone: 'text-yellow-500', value: 'list -c', desc: 'List stations / clients' },
    { cat: 'WiFi Targets', icon: 'fa-solid fa-list', tone: 'text-yellow-500', value: 'select -a <index>', desc: 'Select AP by index' },
    { cat: 'WiFi Targets', icon: 'fa-solid fa-list', tone: 'text-yellow-500', value: 'select -s <index>', desc: 'Select SSID by index' },
    { cat: 'WiFi Targets', icon: 'fa-solid fa-list', tone: 'text-yellow-500', value: 'select -c <index>', desc: 'Select station by index' },
    { cat: 'WiFi Targets', icon: 'fa-solid fa-list', tone: 'text-yellow-500', value: 'clearlist -a', desc: 'Clear AP list' },
    { cat: 'WiFi Targets', icon: 'fa-solid fa-list', tone: 'text-yellow-500', value: 'ssid -a -g 30', desc: 'Generate 30 SSIDs for beacon list' },
    { cat: 'WiFi Targets', icon: 'fa-solid fa-list', tone: 'text-yellow-500', value: 'ssid -r <index>', desc: 'Remove SSID by index' },
    { cat: 'WiFi Targets', icon: 'fa-solid fa-list', tone: 'text-yellow-500', value: 'save -a', desc: 'Save AP list to SD' },
    { cat: 'WiFi Targets', icon: 'fa-solid fa-list', tone: 'text-yellow-500', value: 'load -a', desc: 'Load AP list from SD' },

    { cat: 'Bluetooth', icon: 'fa-brands fa-bluetooth-b', tone: 'text-purple-500', value: 'blespam -t all', desc: 'BLE spam all devices' },
    { cat: 'Bluetooth', icon: 'fa-brands fa-bluetooth-b', tone: 'text-purple-500', value: 'sniffbt', desc: 'Sniff Bluetooth traffic' },
    { cat: 'Bluetooth', icon: 'fa-brands fa-bluetooth-b', tone: 'text-purple-500', value: 'sniffskim', desc: 'Detect card skimmers' },

    { cat: 'Evil Portal', icon: 'fa-solid fa-door-open', tone: 'text-orange-500', value: 'evilportal -c start', desc: 'Start Evil Portal captive portal' },

    { cat: 'System', icon: 'fa-solid fa-gear', tone: 'text-zinc-500', value: 'help', desc: 'Show all commands' },
    { cat: 'System', icon: 'fa-solid fa-gear', tone: 'text-zinc-500', value: 'settings', desc: 'View / change settings' },
    { cat: 'System', icon: 'fa-solid fa-gear', tone: 'text-zinc-500', value: 'channel', desc: 'Get current WiFi channel' },
    { cat: 'System', icon: 'fa-solid fa-gear', tone: 'text-zinc-500', value: 'ls /', desc: 'List SD files' },
    { cat: 'System', icon: 'fa-solid fa-gear', tone: 'text-zinc-500', value: 'led -s <hex>', desc: 'Set LED color' },
    { cat: 'System', icon: 'fa-solid fa-gear', tone: 'text-zinc-500', value: 'reboot', desc: 'Reboot device' },
    { cat: 'System', icon: 'fa-solid fa-gear', tone: 'text-zinc-500', value: 'update', desc: 'Firmware update' },
    { cat: 'System', icon: 'fa-solid fa-gear', tone: 'text-zinc-500', value: 'nmea', desc: 'NMEA output' }
];

var AGENT_COMMANDS = [
    { cat: 'Files', icon: 'fa-solid fa-folder', tone: 'text-zinc-400', value: 'dir', desc: 'List directory contents (Windows)' },
    { cat: 'Files', icon: 'fa-solid fa-folder', tone: 'text-zinc-400', value: 'ls', desc: 'List directory contents (Unix)' },
    { cat: 'Files', icon: 'fa-solid fa-folder', tone: 'text-zinc-400', value: 'cd', desc: 'Change directory' },
    { cat: 'Files', icon: 'fa-solid fa-folder', tone: 'text-zinc-400', value: 'type', desc: 'Show file content (Windows)' },
    { cat: 'Files', icon: 'fa-solid fa-folder', tone: 'text-zinc-400', value: 'cat', desc: 'Show file content (Unix)' },
    { cat: 'Files', icon: 'fa-solid fa-folder', tone: 'text-zinc-400', value: 'copy', desc: 'Copy files (Windows)' },
    { cat: 'Files', icon: 'fa-solid fa-folder', tone: 'text-zinc-400', value: 'del', desc: 'Delete files (Windows)' },
    { cat: 'Files', icon: 'fa-solid fa-folder', tone: 'text-zinc-400', value: 'mkdir', desc: 'Create directory' },
    { cat: 'Network', icon: 'fa-solid fa-network-wired', tone: 'text-emerald-400', value: 'ipconfig', desc: 'Network configuration (Windows)' },
    { cat: 'Network', icon: 'fa-solid fa-network-wired', tone: 'text-emerald-400', value: 'ifconfig', desc: 'Network configuration (Unix)' },
    { cat: 'Network', icon: 'fa-solid fa-network-wired', tone: 'text-emerald-400', value: 'ping', desc: 'Test network connectivity' },
    { cat: 'System', icon: 'fa-solid fa-terminal', tone: 'text-sky-400', value: 'tasklist', desc: 'List running processes (Windows)' },
    { cat: 'System', icon: 'fa-solid fa-terminal', tone: 'text-sky-400', value: 'whoami', desc: 'Current user' },
    { cat: 'System', icon: 'fa-solid fa-terminal', tone: 'text-sky-400', value: 'hostname', desc: 'Machine name' },
    { cat: 'System', icon: 'fa-solid fa-terminal', tone: 'text-sky-400', value: 'systeminfo', desc: 'System info (Windows)' },
    { cat: 'System', icon: 'fa-solid fa-terminal', tone: 'text-sky-400', value: 'echo', desc: 'Print text' },
    { cat: 'System', icon: 'fa-solid fa-terminal', tone: 'text-sky-400', value: 'powershell', desc: 'Launch PowerShell' }
];

var DUCKYSCRIPT_COMMANDS = [
    { cat: 'Typing / Keys', icon: 'fa-solid fa-keyboard', tone: 'text-emerald-400', value: 'STRING <text>', desc: 'Type a line of text' },
    { cat: 'Typing / Keys', icon: 'fa-solid fa-keyboard', tone: 'text-emerald-400', value: 'DELAY <ms>', desc: 'Wait N milliseconds' },
    { cat: 'Typing / Keys', icon: 'fa-solid fa-keyboard', tone: 'text-emerald-400', value: 'ENTER', desc: 'Press Enter key' },
    { cat: 'Typing / Keys', icon: 'fa-solid fa-keyboard', tone: 'text-emerald-400', value: 'GUI <key>', desc: 'Press Windows/Command combo' },
    { cat: 'Typing / Keys', icon: 'fa-solid fa-keyboard', tone: 'text-emerald-400', value: 'TAB', desc: 'Press Tab key' },
    { cat: 'Typing / Keys', icon: 'fa-solid fa-keyboard', tone: 'text-emerald-400', value: 'SPACE', desc: 'Press Space key' },
    { cat: 'Typing / Keys', icon: 'fa-solid fa-keyboard', tone: 'text-emerald-400', value: 'ESC', desc: 'Press Escape key' },
    { cat: 'Typing / Keys', icon: 'fa-solid fa-keyboard', tone: 'text-emerald-400', value: 'REM <comment>', desc: 'Comment line (ignored)' },

    { cat: 'Mouse', icon: 'fa-solid fa-arrow-pointer', tone: 'text-sky-400', value: 'MOUSE_JIGGLE', desc: 'Jiggle the mouse' },
    { cat: 'Mouse', icon: 'fa-solid fa-arrow-pointer', tone: 'text-sky-400', value: 'MOUSE_MOVE <x> <y>', desc: 'Move mouse by delta' },

    { cat: 'Device', icon: 'fa-solid fa-microchip', tone: 'text-zinc-400', value: 'LED <hue> <sat> <lum> <bri>', desc: 'Set RGB LED colour' },
    { cat: 'Device', icon: 'fa-solid fa-microchip', tone: 'text-zinc-400', value: 'TFT_ON', desc: 'Turn display on' },
    { cat: 'Device', icon: 'fa-solid fa-microchip', tone: 'text-zinc-400', value: 'TFT_OFF', desc: 'Turn display off' },
    { cat: 'Device', icon: 'fa-solid fa-microchip', tone: 'text-zinc-400', value: 'WIFI_ON', desc: 'Enable WiFi' },
    { cat: 'Device', icon: 'fa-solid fa-microchip', tone: 'text-zinc-400', value: 'WIFI_OFF', desc: 'Disable WiFi' },
    { cat: 'Device', icon: 'fa-solid fa-microchip', tone: 'text-zinc-400', value: 'WEB_OFF', desc: 'Stop web server' },
    { cat: 'Device', icon: 'fa-solid fa-microchip', tone: 'text-zinc-400', value: 'USB_RESET', desc: 'Reset USB stack' },
    { cat: 'Device', icon: 'fa-solid fa-microchip', tone: 'text-zinc-400', value: 'RESET_SETTINGS', desc: 'Factory reset settings' },
    { cat: 'Device', icon: 'fa-solid fa-microchip', tone: 'text-zinc-400', value: 'USB_NCM_PCAP_ON', desc: 'Start packet capture' },
    { cat: 'Device', icon: 'fa-solid fa-microchip', tone: 'text-zinc-400', value: 'USB_NCM_PCAP_OFF', desc: 'Stop packet capture' },

    { cat: 'Agent', icon: 'fa-solid fa-robot', tone: 'text-purple-400', value: 'AGENT_RUN <cmd>', desc: 'Run command on agent host' },

    { cat: 'Display', icon: 'fa-solid fa-display', tone: 'text-orange-400', value: 'DISPLAY_TEXT <x> <y> <text>', desc: 'Draw text on screen' },
    { cat: 'Display', icon: 'fa-solid fa-display', tone: 'text-orange-400', value: 'DISPLAY_CLEAR', desc: 'Clear the screen' },
    { cat: 'Display', icon: 'fa-solid fa-display', tone: 'text-orange-400', value: 'DISPLAY_PNG <file>', desc: 'Show PNG image' }
];

var RAW_INPUT_COMMANDS = DUCKYSCRIPT_COMMANDS.concat(
    MARAUDER_COMMANDS.map(function(c) {
        return { cat: 'Marauder (MRD)', icon: 'fa-solid fa-terminal', tone: 'text-red-400', value: c.value, desc: c.desc };
    })
);

document.addEventListener('DOMContentLoaded', function() {
    setTimeout(function() {
        attachCommandAutocomplete(
            document.getElementById('rawCommandInput'),
            document.getElementById('autocompleteDropdown'),
            RAW_INPUT_COMMANDS
        );
        attachCommandAutocomplete(
            document.getElementById('agentCommandInput'),
            document.getElementById('agentAutocompleteDropdown'),
            AGENT_COMMANDS
        );
    }, 500);
});

// === features\tools\marauder.js ===
function formatMiniLogLines(values) {
    var lines = [];
    values.forEach(function(value) {
        String(value == null ? '' : value).split(/\r?\n|(?=>)/).forEach(function(line) {
            line = line.replace(/^>\s*/, '').trim();
            if (!line) return;
            lines.push('> ' + line);
        });
    });
    return lines;
}

function storedPortalTemplate() {
    try { return localStorage.getItem('redlily_portal_template') || ''; } catch (e) { return ''; }
}

function updateMarauderPortalTemplate() {
    var label = document.getElementById('marauderPortalTemplate');
    if (label) {
        var path = storedPortalTemplate();
        label.textContent = path ? 'Portal template: ' + path : 'Portal template: none selected';
    }
}

document.addEventListener('DOMContentLoaded', function() {
    updateMarauderPortalTemplate();
    window.addEventListener('storage', updateMarauderPortalTemplate);
    var form = document.getElementById('marauderForm');
    var log = document.getElementById('marauderMiniLog');
    if (!form || !log) return;
    log.style.whiteSpace = 'pre-wrap';

    form.addEventListener('submit', function(event) {
        event.preventDefault();
        var command = document.getElementById('marauderCmd').value;
        var portalTemplate = command === 'evilportal -c start' ? storedPortalTemplate() : '';
        var baselineLogs = [];
        if (command === 'evilportal -c start' && !portalTemplate) {
            log.classList.remove('hidden');
            log.textContent = formatMiniLogLines(['Command failed: select a valid HTML form in Files first']).join('\n');
            return;
        }
        log.classList.remove('hidden');
        log.textContent = formatMiniLogLines(['Queued: ' + command, 'Waiting for firmware...']).join('\n');
        window.redlilyData(false, false, true).then(function(before) {
            baselineLogs = Array.isArray(before.logMessages) ? before.logMessages.slice() : [];
            var endpoint = portalTemplate
                ? '/evilportal?action=start&file=' + encodeURIComponent(portalTemplate)
                : '/marauder?marauderCmd=' + encodeURIComponent(command);
            return window.redlilyPost(endpoint);
        })
            .then(function(response) {
                return response.text().then(function(body) {
                    var data = {};
                    try { data = body ? JSON.parse(body) : {}; } catch (_) {}
                    if (!response.ok) {
                        var message = data.error || ('HTTP ' + response.status);
                        if (data.safeMode) message += ' (Safe Mode active)';
                        throw new Error(message);
                    }
                    return data;
                });
            })
            .then(function() {
                if (portalTemplate) {
                    log.textContent = formatMiniLogLines([
                        'Portal start queued',
                        'Template: ' + portalTemplate,
                        'SSID: dari Settings (portal-ssid)',
                        'Connect a second test device after the dashboard AP changes.'
                    ]).join('\n');
                    return null;
                }
                log.textContent = formatMiniLogLines(['Queued: ' + command, 'Running']).join('\n');
                return new Promise(function(resolve) {
                    var attempts = 0;
                    function poll() {
                        window.redlilyData(true, false, true).then(function(data) {
                            var currentLogs = Array.isArray(data.logMessages) ? data.logMessages : [];
                            var changed = currentLogs.length > baselineLogs.length ||
                                currentLogs.slice(-4).join('\n') !== baselineLogs.slice(-4).join('\n');
                            if (!changed && attempts++ < 12) {
                                setTimeout(poll, 250);
                                return;
                            }
                            data._marauderNewLogs = currentLogs.length > baselineLogs.length
                                ? currentLogs.slice(baselineLogs.length)
                                : currentLogs.slice(-8);
                            resolve(data);
                        }).catch(resolve);
                    }
                    poll();
                });
            })
            .then(function(data) {
                if (!data) return;
                var lines = formatMiniLogLines(Array.isArray(data._marauderNewLogs) ? data._marauderNewLogs : []);
                log.textContent = formatMiniLogLines([
                    'Command: ' + command,
                    'Status: ' + (data.marauderActive ? 'Running' : 'Ready')
                ].concat(lines)).join('\n') || '> No firmware output yet';
            })
            .catch(function(error) {
            log.textContent = formatMiniLogLines(['Command failed: ' + error.message]).join('\n');
            });
    });
});

document.addEventListener('DOMContentLoaded', function() {
    var form = document.querySelector('form[action="/runagentcmd"]');
    var input = document.getElementById('agentCommandInput');
    var log = document.getElementById('agentMiniLog');
    if (!form || !input || !log) return;
    form.addEventListener('submit', function(event) {
        event.preventDefault();
        var command = input.value.trim();
        if (!command) return;
        log.classList.remove('hidden');
        log.textContent = formatMiniLogLines(['Queued: ' + command]).join('\n');
        window.redlilyPost('/runagentcmd?rawCommand=' + encodeURIComponent(command))
            .then(function(response) {
                return response.text().then(function(body) {
                    var data = {};
                    try { data = body ? JSON.parse(body) : {}; } catch (_) {}
                    if (!response.ok) throw new Error(data.error || body || ('HTTP ' + response.status));
                    return data;
                });
            })
            .then(function(data) {
                log.textContent = formatMiniLogLines([
                    'Agent command: ' + command,
                    data.status || 'agent_command_queued'
                ]).join('\n');
            })
            .catch(function(error) {
                log.textContent = formatMiniLogLines(['Command failed: ' + error.message]).join('\n');
            });
    });
});

document.addEventListener('DOMContentLoaded', function() {
    var form = document.querySelector('form[action="/rawinput"]');
    var input = document.getElementById('rawCommandInput');
    var log = document.getElementById('rawMiniLog');
    if (!form || !input || !log) return;
    form.addEventListener('submit', function(event) {
        event.preventDefault();
        var command = input.value.trim();
        if (!command) return;
        log.classList.remove('hidden');
        log.textContent = formatMiniLogLines(['Queued: ' + command]).join('\n');
        window.redlilyPost('/rawinput?rawCommand=' + encodeURIComponent(command))
            .then(function(response) {
                return response.text().then(function(body) {
                    var data = {};
                    try { data = body ? JSON.parse(body) : {}; } catch (_) {}
                    if (!response.ok) throw new Error(data.error || ('HTTP ' + response.status));
                    return data;
                });
            })
            .then(function(data) {
                var status = data.status || 'queued';
                var label = data.kind === 'marauder' ? 'Marauder radio command' : 'Command';
                log.textContent = formatMiniLogLines([label + ': ' + command, status]).join('\n');
            })
            .catch(function(error) { log.textContent = formatMiniLogLines(['Command failed: ' + error.message]).join('\n'); });
    });
});

// === features\tools\payloads.js ===
var portalTemplateFiles = [];

function getStoredPortalTemplate() {
    try { return localStorage.getItem('redlily_portal_template') || ''; } catch (e) { return ''; }
}

function setStoredPortalTemplate(path) {
    try {
        if (path) localStorage.setItem('redlily_portal_template', path);
        else localStorage.removeItem('redlily_portal_template');
    } catch (e) {}
}

function updatePortalTemplateLabel() {
    var label = document.getElementById('marauderPortalTemplate');
    if (!label) return;
    var path = getStoredPortalTemplate();
    label.textContent = path ? 'Portal template: ' + path : 'Portal template: none selected';
}

function refreshPortalTemplateFiles() {
    return fetch('/evilportal/files', { cache: 'no-store' })
        .then(function(response) {
            if (!response.ok) throw new Error('HTTP ' + response.status);
            return response.json();
        })
        .then(function(data) {
            var files = Array.isArray(data) ? data : (Array.isArray(data.files) ? data.files : []);
            portalTemplateFiles = files.filter(function(file) {
                var path = String(file && (file.path || file.name) || '');
                return path.indexOf('/Portal/') === 0 && file.form === true && file.available !== false;
            }).map(function(file) { return String(file.path || file.name); });
            var stored = getStoredPortalTemplate();
            if (stored && portalTemplateFiles.indexOf(stored) === -1) setStoredPortalTemplate('');
            updatePortalTemplateLabel();
            return portalTemplateFiles;
        })
        .catch(function() {
            portalTemplateFiles = [];
            updatePortalTemplateLabel();
            return [];
        });
}

window.useSelectedPortalTemplate = function() {
    var box = document.getElementById('fileSelectionBox');
    var path = box ? box.value : '';
    if (!path || portalTemplateFiles.indexOf(path) === -1) {
        if (window.showToast) window.showToast('Select a valid HTML form inside /Portal first.', 'warning');
        return;
    }
    setStoredPortalTemplate(path);
    updatePortalTemplateLabel();
    if (window.showToast) window.showToast('Portal template selected: ' + path);
    var marauderTab = document.querySelector('a[data-tab="tab-3"]');
    if (marauderTab) marauderTab.click();
};

document.addEventListener('DOMContentLoaded', function() {
    function syncUploadDirectories(files, listedDirectories) {
        var select = document.getElementById('fileUploadDirectory');
        if (!select) return;
        var previous = select.value || 'Auto';
        var folders = Object.create(null);

        function addFolder(path) {
            path = String(path || '').replace(/\/+$/, '');
            if (path && path[0] === '/') folders[path] = true;
        }

        (Array.isArray(listedDirectories) ? listedDirectories : []).forEach(addFolder);
        (Array.isArray(files) ? files : []).forEach(function(path) {
            var parts = String(path || '').split('/').filter(Boolean);
            for (var i = 1; i < parts.length; i++) addFolder('/' + parts.slice(0, i).join('/'));
        });

        select.replaceChildren(new Option('Auto folder', 'Auto'),
            ...Object.keys(folders).sort().map(function(path) { return new Option(path, path); }));
        select.value = folders[previous] ? previous : 'Auto';
        select.dispatchEvent(new Event('change'));
    }

    function refreshIdeFiles() {
        var treeEl = document.getElementById('tree');
        if (!treeEl) return;
        var fileRequest = {};
        treeEl._fileRequest = fileRequest;
        refreshFileButtons('');
        treeEl.textContent = 'Loading files...';

        var knownFiles = Object.create(null);
        var knownDirectories = Object.create(null);

        function syncUploads() {
            syncUploadDirectories(Object.keys(knownFiles), Object.keys(knownDirectories));
        }

        function fetchDirectory(path) {
            return fetch('/files?directory=' + encodeURIComponent(path), { cache: 'no-store' })
                .then(function(response) {
                    if (!response.ok) throw new Error('HTTP ' + response.status);
                    return response.json();
                });
        }

        function createFile(file, parent, depth) {
            var item = document.createElement('div');
            item.className = 'py-1.5 px-2 rounded hover:bg-zinc-800 cursor-pointer text-zinc-400';
            item.style.paddingLeft = (8 + depth * 16) + 'px';
            item.setAttribute('role', 'treeitem');
            item.tabIndex = -1;
            item.dataset.path = file.path;
            item.draggable = true;

            var icon = document.createElement('i');
            icon.className = 'fa-regular fa-file file-tree-icon file-tree-file-icon';
            item.appendChild(icon);
            var name = document.createElement('span');
            name.className = 'file-tree-name';
            name.textContent = file.name || file.path.substring(file.path.lastIndexOf('/') + 1);
            item.appendChild(name);
            if (Number.isFinite(Number(file.size))) {
                var size = document.createElement('span');
                size.className = 'file-tree-size';
                size.textContent = formatFileSize(Number(file.size));
                item.appendChild(size);
            }
            item.addEventListener('click', function(event) {
                event.stopPropagation();
                item.focus();
                refreshFileButtons(file.path);
            });
            parent.appendChild(item);
            knownFiles[file.path] = true;
        }

        function renderContents(path, parent, depth, data) {
            if (treeEl._fileRequest !== fileRequest) return;
            parent.innerHTML = '';
            var directories = Array.isArray(data.directories) ? data.directories : [];
            var files = Array.isArray(data.fileDetails) ? data.fileDetails : [];
            directories.forEach(function(directory) { knownDirectories[directory] = true; });
            if (path !== '/') knownDirectories[path] = true;
            syncUploads();

            directories.sort().forEach(function(directory) {
                createDirectory(directory, directory.substring(directory.lastIndexOf('/') + 1), parent, depth);
            });
            files.sort(function(a, b) { return String(a.name).localeCompare(String(b.name)); }).forEach(function(file) {
                if (file && file.path) createFile(file, parent, depth);
            });

            if (!directories.length && !files.length) parent.textContent = 'Empty folder';
            if (data.fileListingTruncated || data.directoryListingTruncated) {
                var notice = document.createElement('div');
                notice.className = 'px-2 py-1.5 text-[11px] text-amber-400';
                notice.textContent = 'Some entries are hidden because this folder is large.';
                parent.appendChild(notice);
            }
        }

        function createDirectory(path, label, parent, depth) {
            var folder = document.createElement('div');
            folder.className = 'file-tree-folder py-1.5 px-2 rounded text-zinc-300 font-medium';
            folder.style.paddingLeft = (8 + depth * 16) + 'px';
            folder.setAttribute('role', 'treeitem');
            folder.setAttribute('aria-expanded', 'false');
            folder.tabIndex = -1;
            folder.dataset.folder = 'true';
            folder.dataset.folderPath = path;
            folder.dataset.folderName = label;

            var icon = document.createElement('i');
            icon.className = 'fa-solid fa-folder file-tree-icon file-tree-folder-icon';
            folder.appendChild(icon);
            var name = document.createElement('span');
            name.textContent = label;
            folder.appendChild(name);
            parent.appendChild(folder);

            var children = document.createElement('div');
            children.className = 'file-tree-children is-collapsed';
            children.setAttribute('role', 'group');
            parent.appendChild(children);

            folder.addEventListener('click', function(event) {
                event.stopPropagation();
                var expanded = folder.getAttribute('aria-expanded') === 'true';
                folder.setAttribute('aria-expanded', expanded ? 'false' : 'true');
                children.classList.toggle('is-collapsed', expanded);
                icon.classList.toggle('fa-folder-open', !expanded);
                icon.classList.toggle('fa-folder', expanded);

                var uploadDirectory = document.getElementById('fileUploadDirectory');
                if (uploadDirectory) uploadDirectory.value = path;
                refreshFileButtons('');

                if (!expanded && !folder.dataset.loaded && !folder.dataset.loading) {
                    folder.dataset.loading = 'true';
                    children.textContent = 'Loading...';
                    fetchDirectory(path)
                        .then(function(data) {
                            folder.dataset.loaded = 'true';
                            renderContents(path, children, depth + 1, data);
                        })
                        .catch(function(error) {
                            children.textContent = 'Folder unavailable: ' + error.message;
                        })
                        .then(function() { delete folder.dataset.loading; });
                }
            });
        }

        fetchDirectory('/')
            .then(function(data) {
                if (treeEl._fileRequest !== fileRequest) return;
                treeEl.innerHTML = '';
                if (!data.storage || !data.storage.available) {
                    treeEl.textContent = 'SD card unavailable';
                    return;
                }
                refreshPortalTemplateFiles();
                renderContents('/', treeEl, 0, data);
            })
            .catch(function(error) {
                if (treeEl._fileRequest === fileRequest) treeEl.textContent = 'Files unavailable: ' + error.message;
            });
    }    window.refreshIdeFiles = refreshIdeFiles;
    // File/SD scan only when Files tab opens. Dashboard must stay light.

    function formatFileSize(bytes) {
        if (bytes < 1024) return bytes + ' B';
        if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(bytes < 10240 ? 1 : 0) + ' KB';
        return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
    }

    function refreshFileButtons(path) {
        var box = document.getElementById('fileSelectionBox');
        if (box) box.value = path || '';
        document.querySelectorAll('#tree [data-path]').forEach(function(item) {
            var selected = !!path && item.dataset.path === path;
            item.classList.toggle('file-tree-selected', selected);
            item.setAttribute('aria-selected', selected ? 'true' : 'false');
        });
        var run = document.getElementById('fileRunButton');
        var edit = document.getElementById('fileEditButton');
        var portal = document.getElementById('filePortalButton');
        var download = document.getElementById('fileDownloadButton');
        var del = document.getElementById('fileDeleteButton');
        var hasPath = !!path;
        var isDuckyScript = hasPath && /\.ds$/i.test(path);
        var isPreviewable = hasPath && /\.(html?|ds|txt|log|csv|json|jsonl|css|js|md|ya?ml)$/i.test(path);
        var isPortalTemplate = portalTemplateFiles.indexOf(path) !== -1;
        if (run) run.disabled = !isDuckyScript;
        if (edit) edit.disabled = !isPreviewable;
        if (portal) portal.disabled = !isPortalTemplate;
        if (download) download.disabled = !hasPath;
        if (del) del.disabled = !hasPath;
    }

    function treeIsFolder(item) {
        var next = item.nextElementSibling;
        return !!(next && next.classList && next.classList.contains('collapse'));
    }

    document.addEventListener('click', function(event) {
        var target = event.target;
        var item = target && target.closest ? target.closest('#tree [role="treeitem"]') : null;
        if (!item) return;
        var insideTree = item.closest ? item.closest('#tree') : null;
        if (!insideTree) return;
        // Folders are not valid payloads/images; never offer file actions.
        if (treeIsFolder(item) || item.dataset.folder === 'true') {
            refreshFileButtons('');
            return;
        }
        refreshFileButtons(item.dataset.path || '');
    }, false);

    function visibleTreeItems(treeEl) {
        return Array.from(treeEl.querySelectorAll('[role="treeitem"]')).filter(function(item) {
            return !item.closest('.file-tree-children.is-collapsed');
        });
    }

    var treeEl = document.getElementById('tree');
    if (treeEl) {
        treeEl.setAttribute('tabindex', '0');
        treeEl.addEventListener('keydown', function(event) {
            var items = visibleTreeItems(treeEl);
            if (!items.length) return;
            var current = document.activeElement && document.activeElement.closest
                ? document.activeElement.closest('#tree [role="treeitem"]') : null;
            var selected = treeEl.querySelector('.file-tree-selected');
            var index = items.indexOf(current || selected);
            if (event.key === 'ArrowDown' || event.key === 'ArrowUp') {
                event.preventDefault();
                var next = index < 0 ? items[0] : items[Math.max(0, Math.min(items.length - 1, index + (event.key === 'ArrowDown' ? 1 : -1)))];
                items.forEach(function(item) { item.tabIndex = -1; });
                next.tabIndex = 0;
                next.focus();
                if (next.dataset.folder === 'true') refreshFileButtons('');
                else refreshFileButtons(next.dataset.path || '');
            } else if (event.key === 'Enter') {
                event.preventDefault();
                var target = current || selected || items[0];
                if (target.dataset.folder === 'true') target.click();
                else if (target.dataset.path) {
                    refreshFileButtons(target.dataset.path);
                    var preview = document.getElementById('fileEditButton');
                    if (preview && !preview.disabled) preview.click();
                }
            }
        });
    }

    var portalButton = document.getElementById('filePortalButton');
    if (portalButton) portalButton.addEventListener('click', window.useSelectedPortalTemplate);
    var runButton = document.getElementById('fileRunButton');
    if (runButton) runButton.addEventListener('click', function() {
        var path = document.getElementById('fileSelectionBox')?.value || '';
        if (!/\.ds$/i.test(path)) return;
        window.fireQuickAction('/runfile?filename=' + encodeURIComponent(path)).then(function(ok) {
            if (window.showToast) window.showToast(ok ? 'Queued ' + path : 'Run failed', ok ? 'success' : 'error');
        });
    });
    window.deleteSelectedFile = function() {
        var path = document.getElementById('fileSelectionBox')?.value || '';
        if (!path) return;
        window.redlilyPost('/delete?filename=' + encodeURIComponent(path)).then(function(response) {
            if (!response.ok) return response.text().then(function(text) { throw new Error(text || ('HTTP ' + response.status)); });
            if (window.showToast) window.showToast('Deleted ' + path);
            if (window.refreshIdeFiles) window.refreshIdeFiles();
        }).catch(function(error) {
            if (window.showToast) window.showToast('Delete failed: ' + error.message, 'error');
        });
    };
    updatePortalTemplateLabel();
});

// === features\tools\agent-status.js ===
// Display the Windows Agent connection and capability status.
document.addEventListener('DOMContentLoaded', function() {
    function pollAgentStatus() {
        window.redlilyData()
            .then(function(d) {
                d = d && typeof d === 'object' ? d : {};
                var connected = d.agentConnected === true;
                var el = document.getElementById('agentStatus');
                if (el) {
                    el.textContent = connected ? 'Connected' : 'Offline';
                    el.className = 'text-white font-medium text-[13px] truncate';
                }
                var mn = document.getElementById('machineName');
                if (mn) mn.textContent = connected && d.machineName ? d.machineName : 'Not connected';
                var av = document.getElementById('agentVersion');
                if (av) av.textContent = d.agentVersion || 'Unknown';
                var ac = document.getElementById('agentCompatibility');
                if (ac) {
                    if (!connected) ac.textContent = 'Unknown';
                    else ac.textContent = d.agentProtocolCompatible ? 'Compatible' : 'Incompatible';
                }
                var errors = document.getElementById('errorCount');
                if (errors) errors.textContent = d.errorCount != null ? String(d.errorCount) : 'Unknown';

            })
            .catch(function() {
                var unavailable = document.getElementById('agentStatus');
                if (unavailable) {
                    unavailable.textContent = 'Unavailable';
                    unavailable.className = 'text-white font-medium text-[13px] truncate';
                }
            });
    }
    pollAgentStatus();
    setInterval(pollAgentStatus, 5000);
});

// === features\files\editor-upload.js ===
// Edit device files in the browser and upload the saved contents.
function previewHasUnsavedChanges() {
    var content = document.getElementById('filePreviewContent');
    var editor = document.getElementById('filePreviewEditor');
    return !!(content && editor && content.dataset.savedText !== undefined &&
        editor.value !== content.dataset.savedText);
}

window.closeFilePreview = function(force) {
    var modal = document.getElementById('filePreviewModal');
    if (!modal) return;
    if (!force && previewHasUnsavedChanges()) {
        if (window.showConfirmModal) {
            window.showConfirmModal('Discard changes?', 'Your unsaved edits will be lost.', function() {
                window.closeFilePreview(true);
            });
        }
        return;
    }
    modal._previewRequest = null;
    var panel = modal.querySelector('.file-preview-panel');
    if (modal._previewCloseTimer) clearTimeout(modal._previewCloseTimer);
    modal.classList.remove('is-open');
    modal.classList.add('is-closing');
    if (panel) {
        panel.classList.remove('is-open');
        panel.classList.add('is-closing');
    }
    modal._previewCloseTimer = setTimeout(function() {
        modal.classList.add('hidden');
        modal.classList.remove('flex');
        modal.classList.remove('is-closing');
        if (panel) panel.classList.remove('is-closing');
        modal.style.display = 'none';
    }, 220);
    modal.setAttribute('aria-hidden', 'true');
    closePreviewActions();
    setPreviewEditMode(false);
};

function setPreviewEditMode(editing) {
    var content = document.getElementById('filePreviewContent');
    var editor = document.getElementById('filePreviewEditor');
    var editorShell = document.getElementById('filePreviewEditorShell');
    var editButton = document.getElementById('filePreviewEditButton');
    var saveButton = document.getElementById('filePreviewSaveButton');
    var modeLabel = document.getElementById('filePreviewModeLabel');
    var title = document.getElementById('filePreviewTitle');
    var filename = content.dataset.path;
    if (!content || !editor || !editorShell) return;
    if (modeLabel) modeLabel.textContent = editing ? 'FILE EDIT' : 'FILE PREVIEW';
    if (title && filename) title.textContent = (editing ? 'Edit: ' : 'Preview: ') + filename;
    content.classList.toggle('hidden', editing);
    editorShell.classList.toggle('hidden', !editing);
    if (editButton) {
        editButton.innerHTML = editing
            ? '<i class="fa-solid fa-eye"></i><span>Preview</span>'
            : '<i class="fa-solid fa-pen"></i><span>Edit</span>';
        editButton.title = editing ? 'Preview changes' : 'Edit file';
    }
    if (saveButton) saveButton.classList.toggle('hidden', !editing);
    if (editing) editor.focus();
}

function updatePreviewEditorLines() {
    var editor = document.getElementById('filePreviewEditor');
    var lines = document.getElementById('filePreviewEditorLines');
    if (!editor || !lines) return;
    var count = editor.value.split('\n').length;
    lines.textContent = Array.from({ length: count }, function(_, index) { return index + 1; }).join('\n');
    lines.scrollTop = editor.scrollTop;
}

window.togglePreviewActions = function(event) {
    if (event) event.stopPropagation();
    var menu = document.getElementById('filePreviewActions');
    var trigger = document.getElementById('filePreviewMenuButton');
    if (!menu) return;
    var open = !menu.classList.contains('is-open');
    menu.classList.toggle('is-open', open);
    menu.setAttribute('aria-hidden', open ? 'false' : 'true');
    if (trigger) trigger.setAttribute('aria-expanded', open ? 'true' : 'false');
};

function closePreviewActions() {
    var menu = document.getElementById('filePreviewActions');
    var trigger = document.getElementById('filePreviewMenuButton');
    if (menu) {
        menu.classList.remove('is-open');
        menu.setAttribute('aria-hidden', 'true');
    }
    if (trigger) trigger.setAttribute('aria-expanded', 'false');
}

function escapePreviewHtml(value) {
    return String(value).replace(/[&<>"']/g, function(character) {
        return {'&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'}[character];
    });
}

function setPreviewContent(content, text, saved = true) {
    if (!content) return;
    text = String(text == null ? '' : text).replace(/\r\n/g, '\n');
    content.dataset.rawText = text;
    if (saved) content.dataset.savedText = text;
    content.innerHTML = text.split('\n').map(function(line, index) {
        return '<span class="file-preview-line"><span class="file-preview-line-number">' +
            (index + 1) + '</span><span class="file-preview-line-text">' +
            (escapePreviewHtml(line) || '&nbsp;') + '</span></span>';
    }).join('');
}

window.editPreviewFile = function() {
    var content = document.getElementById('filePreviewContent');
    var editor = document.getElementById('filePreviewEditor');
    var editorShell = document.getElementById('filePreviewEditorShell');
    if (!content || !editor || !editorShell) return;
    if (!editorShell.classList.contains('hidden')) {
        setPreviewContent(content, editor.value, false);
        closePreviewActions();
        setPreviewEditMode(false);
        return;
    }
    if (content.dataset.rawText === undefined) return;
    editor.value = content.dataset.rawText;
    updatePreviewEditorLines();
    closePreviewActions();
    setPreviewEditMode(true);
};

window.savePreviewFile = function() {
    var content = document.getElementById('filePreviewContent');
    var modal = document.getElementById('filePreviewModal');
    var filename = content && content.dataset.path;
    var editor = document.getElementById('filePreviewEditor');
    var saveButton = document.getElementById('filePreviewSaveButton');
    if (!filename || !editor || content.dataset.savedText === undefined) return Promise.reject(new Error('No file loaded'));
    if (content._pendingSave) return content._pendingSave;
    var previewRequest = modal && modal._previewRequest;
    var submittedText = editor.value;
    if (saveButton) saveButton.disabled = true;
    var formData = new FormData();
    formData.append('file', new Blob([submittedText], { type: 'text/plain' }), filename.split('/').pop());
    var url = '/uploadFile?replace=1&path=' + encodeURIComponent(filename);
    content._pendingSave = window.redlilyCsrf().then(function(token) {
        return fetch(url, { method: 'POST', body: formData, headers: token ? { 'X-CSRF-Token': token } : {} });
    })
        .then(function(response) {
            if (!response.ok) throw new Error('HTTP ' + response.status);
            return response.json();
        })
        .then(function(result) {
            if (!result.ok) throw new Error(result.error || 'Save failed');
            if (modal && modal._previewRequest === previewRequest && content.dataset.path === filename) {
                setPreviewContent(content, submittedText);
                closePreviewActions();
                setPreviewEditMode(editor.value !== submittedText);
            }
            if (window.showToast) window.showToast('Saved ' + filename);
            return result;
        })
        .catch(function(error) {
            if (window.showToast) window.showToast('Save failed: ' + error.message);
            throw error;
        })
        .finally(function() {
            content._pendingSave = null;
            if (saveButton) saveButton.disabled = false;
        });
    return content._pendingSave;
};

window.previewSelectedFile = function(force) {
    var filename = document.getElementById('fileSelectionBox').value;
    var modal = document.getElementById('filePreviewModal');
    var panel = modal && modal.querySelector('.file-preview-panel');
    var title = document.getElementById('filePreviewTitle');
    var content = document.getElementById('filePreviewContent');
    if (!filename || !modal || !content) return;
    if (!force && previewHasUnsavedChanges()) {
        if (window.showConfirmModal) window.showConfirmModal('Discard changes?', 'Your unsaved edits will be lost.', function() {
            window.previewSelectedFile(true);
        });
        return;
    }
    var previewRequest = {};
    modal._previewRequest = previewRequest;
    content.dataset.path = filename;
    if (title) title.textContent = 'Preview: ' + filename;
    content.textContent = 'Loading...';
    delete content.dataset.rawText;
    delete content.dataset.savedText;
    var editor = document.getElementById('filePreviewEditor');
    if (editor) editor.value = '';
    setPreviewEditMode(false);
    if (modal._previewCloseTimer) clearTimeout(modal._previewCloseTimer);
    modal.style.display = 'flex';
    modal.classList.remove('hidden');
    modal.classList.add('flex');
    modal.classList.remove('is-closing');
    void modal.offsetWidth;
    modal.setAttribute('aria-hidden', 'false');
    modal.classList.add('is-open');
    if (panel) {
        panel.classList.remove('is-closing');
        panel.classList.add('is-open');
    }
    return fetch('/download?filename=' + encodeURIComponent(filename), { cache: 'no-store' })
        .then(function(response) {
            if (!response.ok) throw new Error('HTTP ' + response.status);
            return response.text();
        })
        .then(function(text) {
            if (modal._previewRequest !== previewRequest) return;
            setPreviewContent(content, text);
            if (editor) editor.value = content.dataset.rawText;
        })
        .catch(function(error) {
            if (modal._previewRequest === previewRequest) content.textContent = 'Preview unavailable: ' + error.message;
        });
};

document.addEventListener('click', function(event) {
    var modal = document.getElementById('filePreviewModal');
    if (modal && event.target === modal) window.closeFilePreview();
    if (!event.target.closest('.file-preview-menu')) closePreviewActions();
});

document.addEventListener('input', function(event) {
    if (event.target && event.target.id === 'filePreviewEditor') updatePreviewEditorLines();
});

document.addEventListener('scroll', function(event) {
    if (event.target && event.target.id === 'filePreviewEditor') updatePreviewEditorLines();
}, true);

document.addEventListener('keydown', function(event) {
    if (event.key === 'Escape') window.closeFilePreview();
});

window.uploadFile = function() {
    const input = document.getElementById('fileInputBox');
    const progress = document.querySelector('#fileUploadProgressBar > div');
    if (input && input._uploadRequest) return input._uploadRequest;
    const file = input && input.files && input.files[0];
    if (!file) {
        if (window.showToast) window.showToast('Select a file first.');
        return;
    }
    if (file.size > 4 * 1024 * 1024) {
        if (window.showToast) window.showToast('File exceeds 4 MB limit.');
        return;
    }

    const formData = new FormData();
    formData.append('file', file, file.name);
    const directory = document.getElementById('fileUploadDirectory');
    formData.append('directory', directory ? directory.value : 'Auto');

    const xhr = new XMLHttpRequest();
    if (progress) {
        clearTimeout(progress._hideTimer);
        progress.style.width = '0%';
        if (progress.parentElement) progress.parentElement.classList.remove('opacity-0');
    }
    xhr.open('POST', '/uploadFile', true);
    xhr.timeout = 60000;
    input._uploadRequest = xhr;

    function uploadFailed(message) {
        if (progress) {
            progress.style.width = '0%';
            if (progress.parentElement) progress.parentElement.classList.add('opacity-0');
        }
        if (message && window.showToast) window.showToast(message);
    }

    xhr.addEventListener('loadend', function() {
        if (input._uploadRequest === xhr) input._uploadRequest = null;
    });

    xhr.upload.addEventListener('progress', function(e) {
        if (!progress || !e.lengthComputable || e.total === 0) return;
        var pct = Math.round((e.loaded / e.total) * 100);
        progress.style.width = Math.max(0, Math.min(100, pct)) + '%';
    });

    xhr.addEventListener('load', function() {
        if (xhr.status >= 200 && xhr.status < 300) {
            if (progress) {
                progress.style.width = '100%';
                progress._hideTimer = setTimeout(() => { if (progress.parentElement) progress.parentElement.classList.add('opacity-0'); }, 1000);
            }
            if (window.showToast) window.showToast('Uploaded ' + file.name);
            if (input.files[0] === file) {
                const inputText = document.getElementById('fileInputText');
                if (inputText) inputText.innerText = 'Choose a file';
                input.value = '';
            }
            if (window.refreshIdeFiles) window.refreshIdeFiles();
        } else {
            var message = 'Upload failed: HTTP ' + xhr.status;
            if (xhr.status === 503) message = 'SD card unavailable.';
            else if (xhr.status === 413) message = 'File exceeds 4 MB limit.';
            uploadFailed(message);
        }
    });

    xhr.addEventListener('error', function() {
        uploadFailed('Upload failed (network error).');
    });

    xhr.addEventListener('abort', function() {
        uploadFailed();
    });

    xhr.addEventListener('timeout', function() {
        uploadFailed('Upload timed out. Try again.');
    });

    window.redlilyCsrf().then(function(token) {
        if (token) { try { xhr.setRequestHeader('X-CSRF-Token', token); } catch (_) {} }
        try {
            xhr.send(formData);
        } catch (_) {
            input._uploadRequest = null;
            uploadFailed('Upload failed (network error).');
        }
    });
    return xhr;
};

// Provide the shared confirmation dialog used by destructive file actions.

// === features\files\drag-drop.js ===
document.addEventListener('DOMContentLoaded', function() {
    var dropZone = document.querySelector('label.cursor-pointer');
    var fileInput = document.getElementById('fileInputBox');
    var fileText = document.getElementById('fileInputText');

    if (dropZone && fileInput && fileText) {
        ['dragenter', 'dragover', 'dragleave', 'drop'].forEach(function(name) {
            dropZone.addEventListener(name, function(event) {
                event.preventDefault();
                event.stopPropagation();
            });
        });
        ['dragenter', 'dragover'].forEach(function(name) {
            dropZone.addEventListener(name, function() {
                dropZone.classList.add('border-red-500', 'bg-red-500/10');
            });
        });
        ['dragleave', 'drop'].forEach(function(name) {
            dropZone.addEventListener(name, function() {
                dropZone.classList.remove('border-red-500', 'bg-red-500/10');
            });
        });
        dropZone.addEventListener('drop', function(event) {
            if (!event.dataTransfer.files.length) return;
            fileInput.files = event.dataTransfer.files;
            fileText.textContent = fileInput.files[0].name;
        });
    }

    window.moveSelectedFile = function(source, directory) {
        if (!source || !directory) return;
        var filename = source.substring(source.lastIndexOf('/') + 1);
        var destination = directory.replace(/\/$/, '') + '/' + filename;
        window.redlilyPost('/moveFile?from=' + encodeURIComponent(source) + '&to=' + encodeURIComponent(destination)).then(function(response) {
            if (!response.ok) return response.json().catch(function() { return {}; }).then(function(data) {
                throw new Error(data.error || 'Move failed');
            });
            return response.json();
        }).then(function() {
            if (window.showToast) window.showToast('Moved to ' + directory);
            if (window.refreshIdeFiles) window.refreshIdeFiles();
        }).catch(function(error) {
            if (window.showToast) window.showToast('Move failed: ' + error.message);
        });
    };

    var tree = document.getElementById('tree');
    if (!tree) return;
    tree.addEventListener('dragstart', function(event) {
        var file = event.target.closest('[data-path]');
        if (!file) return;
        event.dataTransfer.effectAllowed = 'move';
        event.dataTransfer.setData('text/plain', file.dataset.path);
    });
    tree.addEventListener('dragover', function(event) {
        if (!event.target.closest('[data-folder="true"]')) return;
        event.preventDefault();
        event.dataTransfer.dropEffect = 'move';
    });
    tree.addEventListener('drop', function(event) {
        var folder = event.target.closest('[data-folder="true"]');
        var source = event.dataTransfer.getData('text/plain');
        if (!folder || !source) return;
        event.preventDefault();
        window.moveSelectedFile(source, folder.dataset.folderPath);
    });
});

// === features\logs\filter.js ===
// Filter device log lines as the user types.
document.addEventListener('DOMContentLoaded', function() {
    var logBox = document.getElementById('logBox');
    var filterInput = document.getElementById('logFilterInput');
    var categoryFilter = document.getElementById('logCategoryFilter');

    if (!logBox || !filterInput || !categoryFilter) return;

    var originalLogData = '';
    var originalDescriptor = Object.getOwnPropertyDescriptor(HTMLTextAreaElement.prototype, 'value');

    function compactLogLine(line) {
        var component, message;
        line = String(line || '').replace(/^>\s*/, '').trim();
        if (!line) return '';

        line = line.replace(/^(\d{2}:\d{2})\s+\1\s+/, '$1 ');
        const compact = line.match(/^(\d{2}:\d{2})\s+([A-Za-z0-9+_-]+)(?:\s+(.*))?$/);
        if (compact) {
            message = (compact[3] || '').trim();
            if (!message) return '';
            component = {
                BOARD: 'BOOT', WiFi: 'WIFI', MAIN: 'SYS', main: 'SYS',
                ESP32M: 'MRD', MARAUDER: 'MRD', MMC: 'SD', STORAGE: 'SD', DISPLAY: 'TFT'
            }[compact[2]] || compact[2];
            return compact[1] + ' ' + component + ' ' + message;
        }

        var parts = line.split(' | ');
        var time = (parts[0] || '').match(/^\d{2}:\d{2}/);
        component = '';
        message = line;
        if (parts.length >= 4) {
            component = parts[2];
            message = parts.slice(3).join(' | ');
        } else if (parts.length >= 3) {
            component = parts[1];
            message = parts.slice(2).join(' | ');
        }

        var shortComponent = { BOARD: 'BOOT', WiFi: 'WIFI', main: 'SYS', WEB: 'WEB', USB: 'USB' }[component] || component;
        var shortMessage = message
            .replace(/^Reset reason code:\s*/i, 'Reset ')
            .replace(/^WiFi mode:\s*AP, password configured$/i, 'AP + PW')
            .replace(/^WiFi mode:\s*Station, password configured$/i, 'STA + PW')
            .replace(/^Timeout waiting for USB to mount$/i, 'Mount timeout')
            .replace(/^Marauder will start when requested from web UI\.?$/i, 'Marauder ready')
            .replace(/^Starting web server\.?$/i, 'Web server on')
            .replace(/^System ready\.?$/i, 'Ready')
            .replace(/^Input client rejected:\s*USB HID is unavailable\.?$/i, 'HID unavailable')
            .replace(/^Version:\s*([a-f0-9]{7})[a-f0-9]+$/i, 'FW $1')
            .replace(/[.]$/, '');

        return (time ? time[0] + ' ' : '') + (shortComponent ? shortComponent + ' ' : '') + shortMessage;
    }

    function matchesCategory(rawLine, displayLine, category) {
        if (category === 'all') return true;
        var line = (rawLine + ' ' + displayLine).toLowerCase();
        if (category === 'marauder') return /\b(mrd|marauder|esp32m)\b/.test(line);
        if (category === 'ducky') return /\bducky\b/.test(line);
        if (category === 'agent') return /\bagent\b/.test(line);
        if (category === 'system') return !/\b(mrd|marauder|esp32m|ducky|agent)\b/.test(line);
        return true;
    }

    function applyFilter() {
        var query = filterInput.value.trim().toLowerCase();
        var category = categoryFilter.value;
        var filtered = originalLogData.split('\n').filter(function(rawLine) {
            var displayLine = compactLogLine(rawLine);
            var haystack = (rawLine + ' ' + displayLine).toLowerCase();
            return (!query || haystack.includes(query)) && matchesCategory(rawLine, displayLine, category);
        }).map(compactLogLine).filter(Boolean).join('\n');

        originalDescriptor.set.call(logBox, filtered);
        var autoScroll = document.getElementById('autoScrollLogs');
        if (autoScroll && autoScroll.checked) logBox.scrollTop = logBox.scrollHeight;
    }

    Object.defineProperty(logBox, 'value', {
        configurable: true,
        get: function() { return originalDescriptor.get.call(this); },
        set: function(value) {
            originalLogData = String(value == null ? '' : value);
            applyFilter();
        }
    });

    filterInput.addEventListener('input', applyFilter);
    categoryFilter.addEventListener('change', applyFilter);
});

// === features\logs\saved.js ===
window.loadSavedLog = function() {
    var logBox = document.getElementById('logBox');
    var status = document.getElementById('savedLogStatus');
    if (!logBox) return;
    var request = {};
    logBox._logRequest = request;
    if (status) status.textContent = 'Loading...';
    return fetch('/systemlog?tail=200', { cache: 'no-store' })
        .then(function(response) {
            if (!response.ok) throw new Error(response.status === 503 ? 'SD unavailable' : 'Saved log unavailable');
            return response.text();
        })
        .then(function(text) {
            if (logBox._logRequest !== request) return;
            window.showingSavedLog = true;
            logBox.value = text || 'No saved SD logs';
            if (status) status.textContent = 'SD log loaded';
            if (window.showToast) window.showToast('Loaded saved SD log');
        })
        .catch(function(error) {
            if (logBox._logRequest !== request) return;
            if (status) status.textContent = error.message;
            if (window.showToast) window.showToast(error.message);
        });
};

window.clearRuntimeLogs = function() {
    var logBox = document.getElementById('logBox');
    var request = {};
    if (logBox) logBox._logRequest = request;
    return window.redlilyPost('/clearlogs')
        .then(function(response) { if (!response.ok) throw new Error('Clear failed'); return response.json(); })
        .then(function(result) {
            if (!result.ok) throw new Error(result.error || 'Clear failed');
            if (logBox && logBox._logRequest !== request) return;
            window.showingSavedLog = false;
            if (logBox) logBox.value = '';
            var status = document.getElementById('savedLogStatus');
            if (status) status.textContent = 'Runtime logs';
            if (window.showToast) window.showToast('Runtime logs cleared. SD log kept.');
        })
        .catch(function(error) { if (window.showToast) window.showToast(error.message); });
};

// === features\logs\runtime-controls.js ===
window.refreshRuntimeLogs = function() {
        var logBox = document.getElementById('logBox');
        if (!logBox || window.showingSavedLog || document.documentElement.getAttribute('data-active-tab') !== 'tab-6') return;
        if (logBox._runtimePending) return logBox._runtimePending;
        var request = logBox._logRequest;
        logBox._runtimePending = fetch('/runtime-logs', { cache: 'no-store' })
            .then(function(response) { if (!response.ok) throw new Error('Runtime logs unavailable'); return response.json(); })
            .then(function(data) {
                if (window.showingSavedLog || logBox._logRequest !== request || document.documentElement.getAttribute('data-active-tab') !== 'tab-6') return;
                if (!Array.isArray(data.logMessages)) throw new Error('Invalid runtime logs');
                var lines = data.logMessages;
                var text = lines.map(function(line) { return '> ' + String(line); }).join('\n');
                if (logBox.value !== text) logBox.value = text;
            })
            .catch(function() {})
            .finally(function() { logBox._runtimePending = null; });
        return logBox._runtimePending;
    };

    // Keep the log view pinned to new output unless the user scrolls away.
    document.addEventListener('DOMContentLoaded', function() {
        var logBox = document.getElementById('logBox');
        var autoScrollToggle = document.getElementById('autoScrollLogs');
        var autoScrollIcon = document.getElementById('autoScrollIcon');

        if (logBox && autoScrollToggle && autoScrollIcon) {
            function updateAutoScrollState() {
                if (autoScrollToggle.checked) {
                    autoScrollIcon.classList.remove('text-zinc-500');
                    autoScrollIcon.classList.add('text-emerald-400');
                    logBox.scrollTop = logBox.scrollHeight;
                } else {
                    autoScrollIcon.classList.remove('text-emerald-400');
                    autoScrollIcon.classList.add('text-zinc-500');
                }
            }

            autoScrollToggle.addEventListener('change', updateAutoScrollState);
            updateAutoScrollState();

            // Detect log updates made by code that dispatches DOM mutations.
            var logObserver = new MutationObserver(function() {
                if (autoScrollToggle.checked) {
                    logBox.scrollTop = logBox.scrollHeight;
                }
            });
            logObserver.observe(logBox, { characterData: true, childList: true, subtree: true });

            // Setting a textarea's value does not trigger MutationObserver, so briefly poll its length.
            var lastLen = logBox.value.length;
            setInterval(function() {
                if (logBox.value.length !== lastLen) {
                    lastLen = logBox.value.length;
                    if (autoScrollToggle.checked) {
                        logBox.scrollTop = logBox.scrollHeight;
                    }
                }
            }, 500);
        }
        window.refreshRuntimeLogs();
        setInterval(window.refreshRuntimeLogs, 2000);
    });

// === features\settings\toggles.js ===
// Sync control toggles with the firmware's /data.json state.
var syncingWifiMode = false;
document.addEventListener('DOMContentLoaded', function() {
    var wifiMode = document.getElementById('wifiMode');
    if (wifiMode) {
        wifiMode.addEventListener('change', function() {
            if (!syncingWifiMode) wifiMode.dataset.userEdited = 'true';
        });
    }
    var portalOpenEl = document.getElementById('portalOpen');
    if (portalOpenEl) {
        portalOpenEl.addEventListener('change', function() {
            portalOpenEl.dataset.userEdited = 'true';
        });
    }
    document.addEventListener('input', function(event) {
        var target = event.target;
        if (target && target.id === 'value') target.dataset.userEdited = 'true';
        if (target && (target.id === 'wifiSsid' || target.id === 'wifiPass' ||
                       target.id === 'portalSsid' || target.id === 'portalPass')) {
            target.dataset.userEdited = 'true';
        }
    });

    var prefilled = false;
    function prefill(id, value) {
        var el = document.getElementById(id);
        if (!el || typeof value !== 'string') return;
        if (prefilled && el.dataset.userEdited === 'true') return;
        if (document.activeElement === el) return;
        if (el.value !== value) el.value = value;
    }

    function syncToggles() {
        window.redlilyData()
            .then(function(d) {
                d = d && typeof d === 'object' ? d : {};
                prefill('wifiSsid', d.wifiSsid);
                prefill('wifiPass', d.wifiPassword);
                prefill('portalSsid', d.portalSsid);
                prefill('portalPass', d.portalPassword);
                var portalOpen = document.getElementById('portalOpen');
                if (portalOpen && typeof d.portalPassword === 'string' && portalOpen.dataset.userEdited !== 'true') {
                    portalOpen.checked = d.portalPassword.length === 0;
                }
                prefilled = true;
                var stealth = document.getElementById('stealthToggle');
                var autorun = document.getElementById('autorunToggle');
                var powersave = document.getElementById('powersaveToggle');
                var screensaver = document.getElementById('screensaverToggle');
                if (stealth && typeof d.stealth === 'boolean') stealth.checked = d.stealth;
                if (autorun && typeof d.autorun === 'boolean') autorun.checked = d.autorun;
                if (powersave && typeof d.powersave === 'boolean') powersave.checked = d.powersave;
                if (screensaver && typeof d.screensaver === 'boolean') screensaver.checked = d.screensaver;
                window.wifiPasswordConfigured = d.wifiPasswordConfigured === true;
                if (wifiMode && typeof d.wifiApMode === 'boolean' && !wifiMode.dataset.userEdited) {
                    var mode = d.wifiApMode ? 'ap' : 'client';
                    if (wifiMode.value !== mode) {
                        syncingWifiMode = true;
                        wifiMode.value = mode;
                        wifiMode.dispatchEvent(new Event('change'));
                        syncingWifiMode = false;
                    }
                }
                var usbDeviceType = document.getElementById('currentUsbDeviceType');
                var usbDeviceValue = document.getElementById('value');
                if (usbDeviceType && Number.isInteger(d.usbDeviceType)) {
                    var formattedUsbType = '0x' + d.usbDeviceType.toString(16).padStart(4, '0');
                    usbDeviceType.textContent = formattedUsbType;
                    var configuredType = Number.isInteger(d.configuredUsbDeviceType) ? d.configuredUsbDeviceType : d.usbDeviceType;
                    var formattedConfiguredType = '0x' + configuredType.toString(16).padStart(4, '0');
                    if (usbDeviceValue && !usbDeviceValue.dataset.userEdited && document.activeElement !== usbDeviceValue) {
                        usbDeviceValue.value = formattedConfiguredType;
                    }
                }
            })
            .catch(function() {
                var usbDeviceType = document.getElementById('currentUsbDeviceType');
                if (usbDeviceType) {
                    usbDeviceType.textContent = 'Unavailable';
                    usbDeviceType.title = 'USB device type could not be read from the device.';
                }
            });
    }
    syncToggles();
    setInterval(syncToggles, 5000);
});

// === features\settings\channel-hop.js ===
// Read and update the Marauder channel-hop delay setting.
window.applyChannelHop = function() {
    var el = document.getElementById('channelHopDelay');
    var v = parseInt(el && el.value, 10);
    if (isNaN(v) || v < 1) v = 1;
    if (v > 60) v = 60;
    window.fireQuickAction('/channelhop?delay=' + v);
};
document.addEventListener('DOMContentLoaded', function() {
    function syncChannelHop() {
        window.redlilyData()
            .then(function(d) {
                d = d && typeof d === 'object' ? d : {};
                var el = document.getElementById('channelHopDelay');
                if (el && typeof d.channelHopDelay === 'number') el.value = d.channelHopDelay;
            })
            .catch(function() {
                var failed = document.getElementById('channelHopDelay');
                if (failed) {
                    failed.value = '';
                    failed.title = 'Channel Hop status unavailable';
                }
            });
    }
    syncChannelHop();
    setInterval(syncChannelHop, 8000);
});

// === features\settings\setup.js ===
// Configure first-run defaults and check firmware health.
(function() {
    function requestWithRetry(url, options, attempts) {
        var remaining = attempts || 3;
        return fetch(url, options).then(function(response) {
            if (!response.ok && response.status >= 500 && remaining > 1) {
                return new Promise(function(resolve) { setTimeout(resolve, 500); })
                    .then(function() { return requestWithRetry(url, options, remaining - 1); });
            }
            return response;
        }).catch(function(error) {
            if (remaining <= 1) throw error;
            return new Promise(function(resolve) { setTimeout(resolve, 500); })
                .then(function() { return requestWithRetry(url, options, remaining - 1); });
        });
    }

    function setSetupMessage(message, isError) {
        var target = document.getElementById('setupMessage');
        if (!target) return;
        target.textContent = message;
        target.className = 'mt-4 min-h-5 text-xs ' + (isError ? 'text-red-400' : 'text-zinc-400');
    }

    function initializeSetup() {
        var gate = document.getElementById('setupGate');
        var form = document.getElementById('setupForm');
        var submit = document.getElementById('setupSubmit');
        if (!gate || !form || !submit) return;
        var setupNonce = '';

        requestWithRetry('/setup/status', { cache: 'no-store' }, 3)
            .then(function(response) {
                if (!response.ok) throw new Error('Setup status unavailable');
                return response.json();
            })
            .then(function(status) {
                if (!status || typeof status.required !== 'boolean') {
                    throw new Error('Invalid setup status');
                }
                if (!status.required) {
                    gate.classList.add('hidden');
                    return;
                }
                if (typeof status.nonce !== 'string' || !status.nonce) {
                    throw new Error('Invalid setup nonce');
                }
                setupNonce = status.nonce;
                submit.disabled = false;
                setSetupMessage('Use 3 to 32 characters for username and at least 12 characters for password.', false);
                document.getElementById('setupUsername').focus();
            })
            .catch(function(error) {
                setSetupMessage(error.message + '. Reload to retry.', true);
            });

        form.addEventListener('submit', function(event) {
            event.preventDefault();
            var username = document.getElementById('setupUsername').value.trim();
            var password = document.getElementById('setupPassword').value;
            var confirmation = document.getElementById('setupConfirmation').value;
            if (password !== confirmation) { setSetupMessage('Passwords do not match.', true); return; }
            submit.disabled = true;
            setSetupMessage('Securing device...', false);
            fetch('/setup', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                 body: JSON.stringify({ username: username, password: password, confirmation: confirmation, nonce: setupNonce })
            }).then(function(response) {
                if (!response.ok) return response.json().then(function(body) { throw new Error(body.error || 'Setup failed'); });
                setSetupMessage('Setup complete. Reloading into protected dashboard...', false);
                setTimeout(function() { window.location.reload(); }, 700);
            }).catch(function(error) {
                submit.disabled = false;
                setSetupMessage(error.message, true);
            });
        });
    }

    document.addEventListener('DOMContentLoaded', function() {
        initializeSetup();
    });
})();

// === features\settings\settings-actions.js ===
// Submit the USB device-type startup setting as a CSRF-protected POST.
document.addEventListener('DOMContentLoaded', function() {
    var form = document.getElementById('usbDeviceTypeForm');
    if (!form) return;
    form.addEventListener('submit', function(event) {
        event.preventDefault();
        var name = form.elements['name'].value;
        var value = form.elements['value'].value.trim();
        var url = '/set?name=' + encodeURIComponent(name) + '&value=' + encodeURIComponent(value);
        window.redlilyPost(url)
            .then(function(response) {
                if (!response.ok) return response.text().then(function(text) { throw new Error(text || ('HTTP ' + response.status)); });
                if (window.showToast) window.showToast('Startup USB mode saved');
            })
            .catch(function(error) {
                if (window.showToast) window.showToast('Save failed: ' + error.message, 'error');
            });
    });
});

// === features\about\report.js ===
// Collect diagnostic details for the user-initiated security report.
window.generateReport = function() {
    if (window.showToast) window.showToast('Generating report...');

    Promise.all([
        window.redlilyData(true, true, true).catch(function(error) { return { __error: error.message }; }),
        fetch('/wifi/scan').then(function(r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); }).catch(function(error) { return { __error: error.message }; }),
        fetch('/evilportal').then(function(r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); }).catch(function(error) { return { __error: error.message }; }),
        fetch('/deauth').then(function(r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); }).catch(function(error) { return { __error: error.message }; })
    ]).then(function(res) {
        var data = res[0] || {};
        var aps = Array.isArray(res[1]) ? res[1] : [];
        var portal = res[2] || {};
        var deauth = res[3] || {};

        var lines = [];
        lines.push('==========================================');
        lines.push(' RedLilyGO - Security Audit Report');
        lines.push(' Generated: ' + new Date().toISOString());
        lines.push('==========================================');
        lines.push('');
        lines.push('== Device ==');
        lines.push('Device data       : ' + (data.__error ? 'unavailable (' + data.__error + ')' : 'ok'));
        lines.push('Uptime            : ' + (data.uptime || 'n/a'));
        lines.push('Firmware (commit) : ' + (data.version || 'n/a'));
        lines.push('Chip              : ' + (data.chipModel || 'n/a') + ' (' + (data.numCores || '?') + ' cores)');
        lines.push('Heap used         : ' + (data.heapUsagePc != null ? data.heapUsagePc + '%' : 'n/a'));
        lines.push('SD used           : ' + (data.sdCardPercentFull != null ? data.sdCardPercentFull + '%' : 'n/a'));
        lines.push('Agent connected   : ' + (data.agentConnected ? 'yes (' + (data.machineName || '?') + ')' : 'no'));
        lines.push('');
        lines.push('== Access Points (' + (res[1].__error ? 'unavailable (' + res[1].__error + ')' : aps.length) + ') ==');
        aps.forEach(function(ap, i) {
            lines.push((i + 1) + '. ' + (ap.ssid || '(hidden)') + '  CH:' + ap.channel + '  RSSI:' + ap.rssi + ' dBm  ' + ap.bssid + (ap.selected ? '  [selected]' : ''));
        });
        lines.push('');
        lines.push('== Evil Portal ==');
        lines.push('Credentials harvested : ' + (res[2].__error ? 'unavailable (' + res[2].__error + ')' : (portal.username ? 'yes' : 'no')));
        lines.push('');
        lines.push('== Deauth Detector ==');
        lines.push('Frames detected : ' + (res[3].__error ? 'unavailable (' + res[3].__error + ')' : (deauth.count || 0)));
        (Array.isArray(deauth.events) ? deauth.events : []).forEach(function(ev) { lines.push('  - ' + ev); });
        lines.push('');
        lines.push('== Captures (SD files) ==');
        var files = (Array.isArray(data.fileListing) ? data.fileListing : []).filter(function(f) { return /\.(pcap|csv|log|ds)$/i.test(f); });
        if (files.length) files.forEach(function(f) { lines.push('  - ' + f); });
        else lines.push('  (none)');
        lines.push('');
        lines.push('== Log Messages ==');
        var tail = (Array.isArray(data.logMessages) ? data.logMessages : []).slice(-50);
        if (tail.length) tail.forEach(function(l) { lines.push('  ' + l); });
        else lines.push('  (none)');
        lines.push('');
        lines.push('-- end of report --');

        var blob = new Blob([lines.join('\n')], { type: 'text/plain' });
        var a = document.createElement('a');
        a.href = URL.createObjectURL(blob);
        a.download = 'redlilygo-audit-' + new Date().toISOString().replace(/[:.]/g, '-') + '.txt';
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        if (window.showToast) window.showToast('Report downloaded');
    });
};
