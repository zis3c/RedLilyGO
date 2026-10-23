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
