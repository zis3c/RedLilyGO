// Read and update the Marauder channel-hop delay setting.
window.applyChannelHop = function() {
    var el = document.getElementById('channelHopDelay');
    var v = parseInt(el && el.value, 10);
    if (isNaN(v) || v < 1) v = 1;
    if (v > 60) v = 60;
    window.fireQuickAction('/channelhop?delay=' + v);
};
document.addEventListener('DOMContentLoaded', function() {
    function syncChannelHop() {
        window.redlilyData()
            .then(function(d) {
                d = d && typeof d === 'object' ? d : {};
                var el = document.getElementById('channelHopDelay');
                if (el && typeof d.channelHopDelay === 'number') el.value = d.channelHopDelay;
            })
            .catch(function() {
                var failed = document.getElementById('channelHopDelay');
                if (failed) {
                    failed.value = '';
                    failed.title = 'Channel Hop status unavailable';
                }
            });
    }
    syncChannelHop();
    setInterval(syncChannelHop, 8000);
});
