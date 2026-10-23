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
