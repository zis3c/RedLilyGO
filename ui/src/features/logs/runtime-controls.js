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
