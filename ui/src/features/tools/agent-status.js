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
