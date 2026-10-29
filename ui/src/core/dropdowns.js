    // Replace marked native selects with custom dropdown controls.
    function initCustomDropdowns() {
        document.querySelectorAll('select.cdrop-select').forEach(function(sel) {
            if (sel.dataset.cdropInit) return;
            sel.dataset.cdropInit = '1';
            sel.style.display = 'none';

            var wrap = document.createElement('div');
            wrap.className = 'cdrop' + (sel.classList.contains('w-full') ? ' w-full' : '');
            sel.parentNode.insertBefore(wrap, sel);
            wrap.appendChild(sel);

            // Build the control that opens the options menu.
            var btn = document.createElement('button');
            btn.type = 'button';
            btn.className = 'cdrop-btn';
            var firstOpt = sel.options[sel.selectedIndex] || sel.options[0];
            var label = document.createElement('span');
            label.className = 'cdrop-text';
            label.textContent = firstOpt ? firstOpt.text : 'Select...';
            var chevron = document.createElement('i');
            chevron.className = 'fa-solid fa-chevron-down cdrop-chevron';
            btn.appendChild(label);
            btn.appendChild(chevron);
            wrap.insertBefore(btn, sel);

            sel.addEventListener('change', function() {
                var selectedOpt = sel.options[sel.selectedIndex] || sel.options[0];
                if (selectedOpt) btn.querySelector('.cdrop-text').textContent = selectedOpt.text;
                menu.querySelectorAll('.cdrop-item').forEach(function(el) {
                    el.classList.toggle('selected', el.dataset.value === sel.value);
                });
            });

            // Build the list of choices from the native select options.
            var menu = document.createElement('div');
            menu.className = 'cdrop-menu';
            buildMenu(sel, menu, btn);
            wrap.appendChild(menu);

            // Keep the custom control and native select in sync when opened.
            btn.addEventListener('click', function(e) {
                e.preventDefault();
                e.stopPropagation();
                var isOpen = wrap.classList.contains('open');
                closeAllDropdowns();
                if (!isOpen) wrap.classList.add('open');
            });
        });
    }

    function buildMenu(sel, menu, btn) {
        menu.innerHTML = '';
        var children = sel.children;
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            if (child.tagName === 'OPTGROUP') {
                var grp = document.createElement('div');
                grp.className = 'cdrop-group';
                grp.textContent = child.label;
                menu.appendChild(grp);
                var opts = child.children;
                for (var j = 0; j < opts.length; j++) {
                    menu.appendChild(makeItem(opts[j], sel, btn, menu));
                }
            } else if (child.tagName === 'OPTION') {
                menu.appendChild(makeItem(child, sel, btn, menu));
            }
        }
    }

    function makeItem(opt, sel, btn, menu) {
        var item = document.createElement('button');
        item.type = 'button';
        item.className = 'cdrop-item';
        if (opt.selected) item.classList.add('selected');
        item.textContent = opt.text;
        item.dataset.value = opt.value;
        item.addEventListener('click', function(e) {
            e.preventDefault();
            e.stopPropagation();
            sel.value = opt.value;
            sel.dispatchEvent(new Event('change'));
            btn.querySelector('.cdrop-text').textContent = opt.text;
            menu.querySelectorAll('.cdrop-item').forEach(function(el) { el.classList.remove('selected'); });
            item.classList.add('selected');
            closeAllDropdowns();
        });
        return item;
    }

    function closeAllDropdowns() {
        document.querySelectorAll('.cdrop.open').forEach(function(d) { d.classList.remove('open'); });
    }

    document.addEventListener('click', function(e) {
        if (!e.target.closest('.cdrop')) closeAllDropdowns();
    });

    function observeSelects() {
        var observer = new MutationObserver(function(mutations) {
            mutations.forEach(function(mutation) {
                if (mutation.target.tagName === 'SELECT' && mutation.target.classList.contains('cdrop-select')) {
                    var sel = mutation.target;
                    var wrap = sel.closest('.cdrop');
                    if (wrap) {
                        var menu = wrap.querySelector('.cdrop-menu');
                        var btn = wrap.querySelector('.cdrop-btn');
                        if (menu && btn) {
                            buildMenu(sel, menu, btn);
                            var selectedOpt = sel.options[sel.selectedIndex] || sel.options[0];
                            if (selectedOpt) {
                                btn.querySelector('.cdrop-text').textContent = selectedOpt.text;
                            }
                        }
                    }
                }
            });
        });
        document.querySelectorAll('select.cdrop-select').forEach(function(sel) {
            observer.observe(sel, { childList: true, subtree: true });
        });
    }

    window.initCustomDropdowns = initCustomDropdowns;
    document.addEventListener('DOMContentLoaded', function() {
        initCustomDropdowns();
        observeSelects();
    });
