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
