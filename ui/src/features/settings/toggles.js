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
