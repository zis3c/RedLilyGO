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
