    function applyCapabilityState(tabId, available, title, explanation) {
        var nav = document.querySelector('a[data-tab="' + tabId + '"]');
        var tab = document.getElementById(tabId);
        if (!nav || !tab) return;
        nav.classList.toggle('capability-unavailable', !available);
        nav.setAttribute('aria-disabled', available ? 'false' : 'true');
        nav.title = available ? title : title + ' unavailable. ' + explanation;
        // Keep the page reachable; its navigation item already explains the missing capability.
    }

    function refreshCapabilityState() {
        return window.redlilyData()
            .then(function(data) {
                var capabilities = Array.isArray(data.capabilities) ? data.capabilities : [];
                applyCapabilityState('tab-3', capabilities.indexOf('MARAUDER') !== -1, 'Marauder', 'Marauder is not available in this firmware build.');
                applyCapabilityState('tab-4', capabilities.indexOf('SD') !== -1, 'Files', 'SD card is not mounted or storage service is unavailable.');

                var marauderReady = capabilities.indexOf('MARAUDER') !== -1;
                var marauderStatusEl = document.getElementById('marauderStatus');
                if (marauderStatusEl) {
                    if (marauderReady) {
                        marauderStatusEl.className = 'text-emerald-500 text-[11px] font-mono';
                        marauderStatusEl.textContent = 'Ready';
                    } else {
                        marauderStatusEl.className = 'text-amber-500 text-[11px] font-mono';
                        marauderStatusEl.textContent = 'Unavailable';
                    }
                }
            });
    }

    // Keep pages reachable and report unavailable capabilities in the navigation.
    document.addEventListener('DOMContentLoaded', function() {
        refreshCapabilityState()
            .catch(function(error) {
                var statusEl = document.getElementById('status');
                if (statusEl) statusEl.textContent = 'API Error';
                console.error('RedLilyGO API unavailable:', error);
            });
    });
