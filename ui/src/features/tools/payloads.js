var portalTemplateFiles = [];

function getStoredPortalTemplate() {
    try { return localStorage.getItem('redlily_portal_template') || ''; } catch (e) { return ''; }
}

function setStoredPortalTemplate(path) {
    try {
        if (path) localStorage.setItem('redlily_portal_template', path);
        else localStorage.removeItem('redlily_portal_template');
    } catch (e) {}
}

function updatePortalTemplateLabel() {
    var label = document.getElementById('marauderPortalTemplate');
    if (!label) return;
    var path = getStoredPortalTemplate();
    label.textContent = path ? 'Portal template: ' + path : 'Portal template: none selected';
}

function refreshPortalTemplateFiles() {
    return fetch('/evilportal/files', { cache: 'no-store' })
        .then(function(response) {
            if (!response.ok) throw new Error('HTTP ' + response.status);
            return response.json();
        })
        .then(function(data) {
            var files = Array.isArray(data) ? data : (Array.isArray(data.files) ? data.files : []);
            portalTemplateFiles = files.filter(function(file) {
                var path = String(file && (file.path || file.name) || '');
                return path.indexOf('/Portal/') === 0 && file.form === true && file.available !== false;
            }).map(function(file) { return String(file.path || file.name); });
            var stored = getStoredPortalTemplate();
            if (stored && portalTemplateFiles.indexOf(stored) === -1) setStoredPortalTemplate('');
            updatePortalTemplateLabel();
            return portalTemplateFiles;
        })
        .catch(function() {
            portalTemplateFiles = [];
            updatePortalTemplateLabel();
            return [];
        });
}

window.useSelectedPortalTemplate = function() {
    var box = document.getElementById('fileSelectionBox');
    var path = box ? box.value : '';
    if (!path || portalTemplateFiles.indexOf(path) === -1) {
        if (window.showToast) window.showToast('Select a valid HTML form inside /Portal first.', 'warning');
        return;
    }
    setStoredPortalTemplate(path);
    updatePortalTemplateLabel();
    if (window.showToast) window.showToast('Portal template selected: ' + path);
    var marauderTab = document.querySelector('a[data-tab="tab-3"]');
    if (marauderTab) marauderTab.click();
};

document.addEventListener('DOMContentLoaded', function() {
    function syncUploadDirectories(files, listedDirectories) {
        var select = document.getElementById('fileUploadDirectory');
        if (!select) return;
        var previous = select.value || 'Auto';
        var folders = Object.create(null);

        function addFolder(path) {
            path = String(path || '').replace(/\/+$/, '');
            if (path && path[0] === '/') folders[path] = true;
        }

        (Array.isArray(listedDirectories) ? listedDirectories : []).forEach(addFolder);
        (Array.isArray(files) ? files : []).forEach(function(path) {
            var parts = String(path || '').split('/').filter(Boolean);
            for (var i = 1; i < parts.length; i++) addFolder('/' + parts.slice(0, i).join('/'));
        });

        select.replaceChildren(new Option('Auto folder', 'Auto'),
            ...Object.keys(folders).sort().map(function(path) { return new Option(path, path); }));
        select.value = folders[previous] ? previous : 'Auto';
        select.dispatchEvent(new Event('change'));
    }

    function refreshIdeFiles() {
        var treeEl = document.getElementById('tree');
        if (!treeEl) return;
        var fileRequest = {};
        treeEl._fileRequest = fileRequest;
        refreshFileButtons('');
        treeEl.textContent = 'Loading files...';

        var knownFiles = Object.create(null);
        var knownDirectories = Object.create(null);

        function syncUploads() {
            syncUploadDirectories(Object.keys(knownFiles), Object.keys(knownDirectories));
        }

        function fetchDirectory(path) {
            return fetch('/files?directory=' + encodeURIComponent(path), { cache: 'no-store' })
                .then(function(response) {
                    if (!response.ok) throw new Error('HTTP ' + response.status);
                    return response.json();
                });
        }

        function createFile(file, parent, depth) {
            var item = document.createElement('div');
            item.className = 'py-1.5 px-2 rounded hover:bg-zinc-800 cursor-pointer text-zinc-400';
            item.style.paddingLeft = (8 + depth * 16) + 'px';
            item.setAttribute('role', 'treeitem');
            item.tabIndex = -1;
            item.dataset.path = file.path;
            item.draggable = true;

            var icon = document.createElement('i');
            icon.className = 'fa-regular fa-file file-tree-icon file-tree-file-icon';
            item.appendChild(icon);
            var name = document.createElement('span');
            name.className = 'file-tree-name';
            name.textContent = file.name || file.path.substring(file.path.lastIndexOf('/') + 1);
            item.appendChild(name);
            if (Number.isFinite(Number(file.size))) {
                var size = document.createElement('span');
                size.className = 'file-tree-size';
                size.textContent = formatFileSize(Number(file.size));
                item.appendChild(size);
            }
            item.addEventListener('click', function(event) {
                event.stopPropagation();
                item.focus();
                refreshFileButtons(file.path);
            });
            parent.appendChild(item);
            knownFiles[file.path] = true;
        }

        function renderContents(path, parent, depth, data) {
            if (treeEl._fileRequest !== fileRequest) return;
            parent.innerHTML = '';
            var directories = Array.isArray(data.directories) ? data.directories : [];
            var files = Array.isArray(data.fileDetails) ? data.fileDetails : [];
            directories.forEach(function(directory) { knownDirectories[directory] = true; });
            if (path !== '/') knownDirectories[path] = true;
            syncUploads();

            directories.sort().forEach(function(directory) {
                createDirectory(directory, directory.substring(directory.lastIndexOf('/') + 1), parent, depth);
            });
            files.sort(function(a, b) { return String(a.name).localeCompare(String(b.name)); }).forEach(function(file) {
                if (file && file.path) createFile(file, parent, depth);
            });

            if (!directories.length && !files.length) parent.textContent = 'Empty folder';
            if (data.fileListingTruncated || data.directoryListingTruncated) {
                var notice = document.createElement('div');
                notice.className = 'px-2 py-1.5 text-[11px] text-amber-400';
                notice.textContent = 'Some entries are hidden because this folder is large.';
                parent.appendChild(notice);
            }
        }

        function createDirectory(path, label, parent, depth) {
            var folder = document.createElement('div');
            folder.className = 'file-tree-folder py-1.5 px-2 rounded text-zinc-300 font-medium';
            folder.style.paddingLeft = (8 + depth * 16) + 'px';
            folder.setAttribute('role', 'treeitem');
            folder.setAttribute('aria-expanded', 'false');
            folder.tabIndex = -1;
            folder.dataset.folder = 'true';
            folder.dataset.folderPath = path;
            folder.dataset.folderName = label;

            var icon = document.createElement('i');
            icon.className = 'fa-solid fa-folder file-tree-icon file-tree-folder-icon';
            folder.appendChild(icon);
            var name = document.createElement('span');
            name.textContent = label;
            folder.appendChild(name);
            parent.appendChild(folder);

            var children = document.createElement('div');
            children.className = 'file-tree-children is-collapsed';
            children.setAttribute('role', 'group');
            parent.appendChild(children);

            folder.addEventListener('click', function(event) {
                event.stopPropagation();
                var expanded = folder.getAttribute('aria-expanded') === 'true';
                folder.setAttribute('aria-expanded', expanded ? 'false' : 'true');
                children.classList.toggle('is-collapsed', expanded);
                icon.classList.toggle('fa-folder-open', !expanded);
                icon.classList.toggle('fa-folder', expanded);

                var uploadDirectory = document.getElementById('fileUploadDirectory');
                if (uploadDirectory) uploadDirectory.value = path;
                refreshFileButtons('');

                if (!expanded && !folder.dataset.loaded && !folder.dataset.loading) {
                    folder.dataset.loading = 'true';
                    children.textContent = 'Loading...';
                    fetchDirectory(path)
                        .then(function(data) {
                            folder.dataset.loaded = 'true';
                            renderContents(path, children, depth + 1, data);
                        })
                        .catch(function(error) {
                            children.textContent = 'Folder unavailable: ' + error.message;
                        })
                        .then(function() { delete folder.dataset.loading; });
                }
            });
        }

        fetchDirectory('/')
            .then(function(data) {
                if (treeEl._fileRequest !== fileRequest) return;
                treeEl.innerHTML = '';
                if (!data.storage || !data.storage.available) {
                    treeEl.textContent = 'SD card unavailable';
                    return;
                }
                refreshPortalTemplateFiles();
                renderContents('/', treeEl, 0, data);
            })
            .catch(function(error) {
                if (treeEl._fileRequest === fileRequest) treeEl.textContent = 'Files unavailable: ' + error.message;
            });
    }    window.refreshIdeFiles = refreshIdeFiles;
    // File/SD scan only when Files tab opens. Dashboard must stay light.

    function formatFileSize(bytes) {
        if (bytes < 1024) return bytes + ' B';
        if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(bytes < 10240 ? 1 : 0) + ' KB';
        return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
    }

    function refreshFileButtons(path) {
        var box = document.getElementById('fileSelectionBox');
        if (box) box.value = path || '';
        document.querySelectorAll('#tree [data-path]').forEach(function(item) {
            var selected = !!path && item.dataset.path === path;
            item.classList.toggle('file-tree-selected', selected);
            item.setAttribute('aria-selected', selected ? 'true' : 'false');
        });
        var run = document.getElementById('fileRunButton');
        var edit = document.getElementById('fileEditButton');
        var portal = document.getElementById('filePortalButton');
        var download = document.getElementById('fileDownloadButton');
        var del = document.getElementById('fileDeleteButton');
        var hasPath = !!path;
        var isDuckyScript = hasPath && /\.ds$/i.test(path);
        var isPreviewable = hasPath && /\.(html?|ds|txt|log|csv|json|jsonl|css|js|md|ya?ml)$/i.test(path);
        var isPortalTemplate = portalTemplateFiles.indexOf(path) !== -1;
        if (run) run.disabled = !isDuckyScript;
        if (edit) edit.disabled = !isPreviewable;
        if (portal) portal.disabled = !isPortalTemplate;
        if (download) download.disabled = !hasPath;
        if (del) del.disabled = !hasPath;
    }

    function treeIsFolder(item) {
        var next = item.nextElementSibling;
        return !!(next && next.classList && next.classList.contains('collapse'));
    }

    document.addEventListener('click', function(event) {
        var target = event.target;
        var item = target && target.closest ? target.closest('#tree [role="treeitem"]') : null;
        if (!item) return;
        var insideTree = item.closest ? item.closest('#tree') : null;
        if (!insideTree) return;
        // Folders are not valid payloads/images; never offer file actions.
        if (treeIsFolder(item) || item.dataset.folder === 'true') {
            refreshFileButtons('');
            return;
        }
        refreshFileButtons(item.dataset.path || '');
    }, false);

    function visibleTreeItems(treeEl) {
        return Array.from(treeEl.querySelectorAll('[role="treeitem"]')).filter(function(item) {
            return !item.closest('.file-tree-children.is-collapsed');
        });
    }

    var treeEl = document.getElementById('tree');
    if (treeEl) {
        treeEl.setAttribute('tabindex', '0');
        treeEl.addEventListener('keydown', function(event) {
            var items = visibleTreeItems(treeEl);
            if (!items.length) return;
            var current = document.activeElement && document.activeElement.closest
                ? document.activeElement.closest('#tree [role="treeitem"]') : null;
            var selected = treeEl.querySelector('.file-tree-selected');
            var index = items.indexOf(current || selected);
            if (event.key === 'ArrowDown' || event.key === 'ArrowUp') {
                event.preventDefault();
                var next = index < 0 ? items[0] : items[Math.max(0, Math.min(items.length - 1, index + (event.key === 'ArrowDown' ? 1 : -1)))];
                items.forEach(function(item) { item.tabIndex = -1; });
                next.tabIndex = 0;
                next.focus();
                if (next.dataset.folder === 'true') refreshFileButtons('');
                else refreshFileButtons(next.dataset.path || '');
            } else if (event.key === 'Enter') {
                event.preventDefault();
                var target = current || selected || items[0];
                if (target.dataset.folder === 'true') target.click();
                else if (target.dataset.path) {
                    refreshFileButtons(target.dataset.path);
                    var preview = document.getElementById('fileEditButton');
                    if (preview && !preview.disabled) preview.click();
                }
            }
        });
    }

    var portalButton = document.getElementById('filePortalButton');
    if (portalButton) portalButton.addEventListener('click', window.useSelectedPortalTemplate);
    var runButton = document.getElementById('fileRunButton');
    if (runButton) runButton.addEventListener('click', function() {
        var path = document.getElementById('fileSelectionBox')?.value || '';
        if (!/\.ds$/i.test(path)) return;
        window.fireQuickAction('/runfile?filename=' + encodeURIComponent(path)).then(function(ok) {
            if (window.showToast) window.showToast(ok ? 'Queued ' + path : 'Run failed', ok ? 'success' : 'error');
        });
    });
    window.deleteSelectedFile = function() {
        var path = document.getElementById('fileSelectionBox')?.value || '';
        if (!path) return;
        window.redlilyPost('/delete?filename=' + encodeURIComponent(path)).then(function(response) {
            if (!response.ok) return response.text().then(function(text) { throw new Error(text || ('HTTP ' + response.status)); });
            if (window.showToast) window.showToast('Deleted ' + path);
            if (window.refreshIdeFiles) window.refreshIdeFiles();
        }).catch(function(error) {
            if (window.showToast) window.showToast('Delete failed: ' + error.message, 'error');
        });
    };
    updatePortalTemplateLabel();
});
