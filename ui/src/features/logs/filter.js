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
