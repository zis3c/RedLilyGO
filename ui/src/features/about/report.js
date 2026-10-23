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
