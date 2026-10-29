    window.go = function(id, el) {
        if (!id) return;
        const targetPane = document.getElementById(id);
        if (!targetPane) return;

        document.documentElement.setAttribute('data-active-tab', id);
        try { localStorage.setItem('redlily_active_tab', id); } catch(e) {}

        document.querySelectorAll('.tab-pane').forEach(function(p) { p.classList.remove('active'); });
        targetPane.classList.add('active');

        if (el) {
            document.querySelectorAll('.nav-link').forEach(function(n) { n.classList.remove('active'); });
            el.classList.add('active');
            const pageTitle = document.getElementById('page-title');
            const span = el.querySelector('span');
            if (pageTitle && span) {
                pageTitle.innerText = span.innerText;
            }
        }

        if (window.initCustomDropdowns) {
            window.initCustomDropdowns();
        }
        if (id === 'tab-4' && window.refreshIdeFiles) window.refreshIdeFiles();
        if (id === 'tab-6' && window.refreshRuntimeLogs) window.refreshRuntimeLogs();

        if (window.innerWidth < 768 && window.closeSidebar) window.closeSidebar();
    };

        // Return to the tab the user last opened.
    document.addEventListener('DOMContentLoaded', () => {
        try {
            const requestedTab = localStorage.getItem('redlily_active_tab') || 'tab-1';
            const navLinks = Array.from(document.querySelectorAll('.nav-link'));
            const navLink = navLinks.find(el => el.dataset.tab === requestedTab) ||
                navLinks.find(el => el.dataset.tab === 'tab-1');
            const savedTab = navLink ? navLink.dataset.tab : 'tab-1';
            if (savedTab !== requestedTab) localStorage.setItem('redlily_active_tab', savedTab);
                if (navLink) {
                    window.go(savedTab, navLink);
                    if (window.innerWidth >= 768) {
                        const sb = document.getElementById('sidebar');
                        if (sb) sb.classList.remove('-translate-x-full');
                    }
                }
        } catch (err) {
            console.error("Failed to restore tab:", err);
        }

        // Reload the preview when a source file changes on disk.
        if (window.location.hostname === 'localhost' || window.location.hostname === '127.0.0.1') {
            let lastBuildVer = null;
            setInterval(() => {
                fetch('/preview/version?_t=' + Date.now())
                    .then(r => r.json())
                    .then(data => {
                        if (data && data.version) {
                            if (lastBuildVer === null) {
                                lastBuildVer = data.version;
                            } else if (lastBuildVer !== data.version) {
                                console.log('[LiveReload] New UI build detected. Auto-refreshing...');
                                window.location.reload();
                            }
                        }
                    })
                    .catch(() => {});
            }, 1200);
        }

        // Let mobile users close the sidebar from the backdrop, outside area, or a left swipe.
        const overlay = document.getElementById('overlay');
        if (overlay) {
            ['click', 'touchstart', 'pointerdown'].forEach(evt => {
                overlay.addEventListener(evt, (e) => {
                    e.preventDefault();
                    window.closeSidebar();
                }, { passive: false });
            });
        }

        // Close the mobile sidebar when the user taps outside it.
        document.addEventListener('pointerdown', (e) => {
            if (window.innerWidth >= 768) return;
            const sidebar = document.getElementById('sidebar');
            if (!sidebar || sidebar.classList.contains('-translate-x-full')) return;
            
            const hamburger = e.target.closest('[data-action="open-sidebar"]');
            if (sidebar.contains(e.target) || hamburger) return;
            
            window.closeSidebar();
        });

        // Close the sidebar when the user swipes left across it.
        const sidebar = document.getElementById('sidebar');
        if (sidebar) {
            let touchStartX = 0;
            let touchStartY = 0;
            sidebar.addEventListener('touchstart', (e) => {
                touchStartX = e.touches[0].clientX;
                touchStartY = e.touches[0].clientY;
            }, { passive: true });

            sidebar.addEventListener('touchend', (e) => {
                const touchEndX = e.changedTouches[0].clientX;
                const touchEndY = e.changedTouches[0].clientY;
                const deltaX = touchEndX - touchStartX;
                const deltaY = touchEndY - touchStartY;
                if (deltaX < -35 && Math.abs(deltaX) > Math.abs(deltaY)) {
                    window.closeSidebar();
                }
            }, { passive: true });
        }
    });

    document.addEventListener('click', (event) => {
        const tab = event.target.closest('[data-tab]');
        if (tab) {
            event.preventDefault();
            window.go(tab.dataset.tab, tab);
            return;
        }
        const action = event.target.closest('[data-action]');
        if (!action) return;
        if (action.dataset.action === 'toggle-sidebar') document.body.classList.toggle('sidebar-collapsed');
        if (action.dataset.action === 'open-sidebar') window.openSidebar();
        if (action.dataset.action === 'close-sidebar') window.closeSidebar();
    });

    window.openSidebar = function() {
        const sb = document.getElementById('sidebar');
        const ov = document.getElementById('overlay');
        if (sb) sb.classList.remove('-translate-x-full');
        if (ov) ov.classList.remove('hidden');
        document.body.classList.add('mobile-sidebar-open');
    };
    window.closeSidebar = function() {
        const sb = document.getElementById('sidebar');
        const ov = document.getElementById('overlay');
        if (sb) sb.classList.add('-translate-x-full');
        if (ov) ov.classList.add('hidden');
        document.body.classList.remove('mobile-sidebar-open');
    };
