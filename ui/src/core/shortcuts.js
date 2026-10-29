    // Register keyboard shortcuts shared by every dashboard page.
    document.addEventListener('keydown', function(e) {
        if (e.ctrlKey) {
            var handled = false;
            var tabsMap = {
                '1': 'tab-1', // Status
                '2': 'tab-3', // Tools
                '3': 'tab-4', // Files
                '4': 'tab-8', // Remote
                'l': 'tab-6'  // Logs
            };

            if (tabsMap[e.key.toLowerCase()]) {
                var targetId = tabsMap[e.key.toLowerCase()];
                var navLink = document.querySelector('a[data-tab="' + targetId + '"]');
                if (navLink) {
                    navLink.click();
                    handled = true;
                }
            }

            if (handled) {
                e.preventDefault();
            }
        }
    });
