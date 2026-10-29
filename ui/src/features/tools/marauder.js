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
