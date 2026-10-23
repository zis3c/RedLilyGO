// Edit device files in the browser and upload the saved contents.
function previewHasUnsavedChanges() {
    var content = document.getElementById('filePreviewContent');
    var editor = document.getElementById('filePreviewEditor');
    return !!(content && editor && content.dataset.savedText !== undefined &&
        editor.value !== content.dataset.savedText);
}

window.closeFilePreview = function(force) {
    var modal = document.getElementById('filePreviewModal');
    if (!modal) return;
    if (!force && previewHasUnsavedChanges()) {
        if (window.showConfirmModal) {
            window.showConfirmModal('Discard changes?', 'Your unsaved edits will be lost.', function() {
                window.closeFilePreview(true);
            });
        }
        return;
    }
    modal._previewRequest = null;
    var panel = modal.querySelector('.file-preview-panel');
    if (modal._previewCloseTimer) clearTimeout(modal._previewCloseTimer);
    modal.classList.remove('is-open');
    modal.classList.add('is-closing');
    if (panel) {
        panel.classList.remove('is-open');
        panel.classList.add('is-closing');
    }
    modal._previewCloseTimer = setTimeout(function() {
        modal.classList.add('hidden');
        modal.classList.remove('flex');
        modal.classList.remove('is-closing');
        if (panel) panel.classList.remove('is-closing');
        modal.style.display = 'none';
    }, 220);
    modal.setAttribute('aria-hidden', 'true');
    closePreviewActions();
    setPreviewEditMode(false);
};

function setPreviewEditMode(editing) {
    var content = document.getElementById('filePreviewContent');
    var editor = document.getElementById('filePreviewEditor');
    var editorShell = document.getElementById('filePreviewEditorShell');
    var editButton = document.getElementById('filePreviewEditButton');
    var saveButton = document.getElementById('filePreviewSaveButton');
    var modeLabel = document.getElementById('filePreviewModeLabel');
    var title = document.getElementById('filePreviewTitle');
    var filename = content.dataset.path;
    if (!content || !editor || !editorShell) return;
    if (modeLabel) modeLabel.textContent = editing ? 'FILE EDIT' : 'FILE PREVIEW';
    if (title && filename) title.textContent = (editing ? 'Edit: ' : 'Preview: ') + filename;
    content.classList.toggle('hidden', editing);
    editorShell.classList.toggle('hidden', !editing);
    if (editButton) {
        editButton.innerHTML = editing
            ? '<i class="fa-solid fa-eye"></i><span>Preview</span>'
            : '<i class="fa-solid fa-pen"></i><span>Edit</span>';
        editButton.title = editing ? 'Preview changes' : 'Edit file';
    }
    if (saveButton) saveButton.classList.toggle('hidden', !editing);
    if (editing) editor.focus();
}

function updatePreviewEditorLines() {
    var editor = document.getElementById('filePreviewEditor');
    var lines = document.getElementById('filePreviewEditorLines');
    if (!editor || !lines) return;
    var count = editor.value.split('\n').length;
    lines.textContent = Array.from({ length: count }, function(_, index) { return index + 1; }).join('\n');
    lines.scrollTop = editor.scrollTop;
}

window.togglePreviewActions = function(event) {
    if (event) event.stopPropagation();
    var menu = document.getElementById('filePreviewActions');
    var trigger = document.getElementById('filePreviewMenuButton');
    if (!menu) return;
    var open = !menu.classList.contains('is-open');
    menu.classList.toggle('is-open', open);
    menu.setAttribute('aria-hidden', open ? 'false' : 'true');
    if (trigger) trigger.setAttribute('aria-expanded', open ? 'true' : 'false');
};

function closePreviewActions() {
    var menu = document.getElementById('filePreviewActions');
    var trigger = document.getElementById('filePreviewMenuButton');
    if (menu) {
        menu.classList.remove('is-open');
        menu.setAttribute('aria-hidden', 'true');
    }
    if (trigger) trigger.setAttribute('aria-expanded', 'false');
}

function escapePreviewHtml(value) {
    return String(value).replace(/[&<>"']/g, function(character) {
        return {'&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'}[character];
    });
}

function setPreviewContent(content, text, saved = true) {
    if (!content) return;
    text = String(text == null ? '' : text).replace(/\r\n/g, '\n');
    content.dataset.rawText = text;
    if (saved) content.dataset.savedText = text;
    content.innerHTML = text.split('\n').map(function(line, index) {
        return '<span class="file-preview-line"><span class="file-preview-line-number">' +
            (index + 1) + '</span><span class="file-preview-line-text">' +
            (escapePreviewHtml(line) || '&nbsp;') + '</span></span>';
    }).join('');
}

window.editPreviewFile = function() {
    var content = document.getElementById('filePreviewContent');
    var editor = document.getElementById('filePreviewEditor');
    var editorShell = document.getElementById('filePreviewEditorShell');
    if (!content || !editor || !editorShell) return;
    if (!editorShell.classList.contains('hidden')) {
        setPreviewContent(content, editor.value, false);
        closePreviewActions();
        setPreviewEditMode(false);
        return;
    }
    if (content.dataset.rawText === undefined) return;
    editor.value = content.dataset.rawText;
    updatePreviewEditorLines();
    closePreviewActions();
    setPreviewEditMode(true);
};

window.savePreviewFile = function() {
    var content = document.getElementById('filePreviewContent');
    var modal = document.getElementById('filePreviewModal');
    var filename = content && content.dataset.path;
    var editor = document.getElementById('filePreviewEditor');
    var saveButton = document.getElementById('filePreviewSaveButton');
    if (!filename || !editor || content.dataset.savedText === undefined) return Promise.reject(new Error('No file loaded'));
    if (content._pendingSave) return content._pendingSave;
    var previewRequest = modal && modal._previewRequest;
    var submittedText = editor.value;
    if (saveButton) saveButton.disabled = true;
    var formData = new FormData();
    formData.append('file', new Blob([submittedText], { type: 'text/plain' }), filename.split('/').pop());
    var url = '/uploadFile?replace=1&path=' + encodeURIComponent(filename);
    content._pendingSave = window.redlilyCsrf().then(function(token) {
        return fetch(url, { method: 'POST', body: formData, headers: token ? { 'X-CSRF-Token': token } : {} });
    })
        .then(function(response) {
            if (!response.ok) throw new Error('HTTP ' + response.status);
            return response.json();
        })
        .then(function(result) {
            if (!result.ok) throw new Error(result.error || 'Save failed');
            if (modal && modal._previewRequest === previewRequest && content.dataset.path === filename) {
                setPreviewContent(content, submittedText);
                closePreviewActions();
                setPreviewEditMode(editor.value !== submittedText);
            }
            if (window.showToast) window.showToast('Saved ' + filename);
            return result;
        })
        .catch(function(error) {
            if (window.showToast) window.showToast('Save failed: ' + error.message);
            throw error;
        })
        .finally(function() {
            content._pendingSave = null;
            if (saveButton) saveButton.disabled = false;
        });
    return content._pendingSave;
};

window.previewSelectedFile = function(force) {
    var filename = document.getElementById('fileSelectionBox').value;
    var modal = document.getElementById('filePreviewModal');
    var panel = modal && modal.querySelector('.file-preview-panel');
    var title = document.getElementById('filePreviewTitle');
    var content = document.getElementById('filePreviewContent');
    if (!filename || !modal || !content) return;
    if (!force && previewHasUnsavedChanges()) {
        if (window.showConfirmModal) window.showConfirmModal('Discard changes?', 'Your unsaved edits will be lost.', function() {
            window.previewSelectedFile(true);
        });
        return;
    }
    var previewRequest = {};
    modal._previewRequest = previewRequest;
    content.dataset.path = filename;
    if (title) title.textContent = 'Preview: ' + filename;
    content.textContent = 'Loading...';
    delete content.dataset.rawText;
    delete content.dataset.savedText;
    var editor = document.getElementById('filePreviewEditor');
    if (editor) editor.value = '';
    setPreviewEditMode(false);
    if (modal._previewCloseTimer) clearTimeout(modal._previewCloseTimer);
    modal.style.display = 'flex';
    modal.classList.remove('hidden');
    modal.classList.add('flex');
    modal.classList.remove('is-closing');
    void modal.offsetWidth;
    modal.setAttribute('aria-hidden', 'false');
    modal.classList.add('is-open');
    if (panel) {
        panel.classList.remove('is-closing');
        panel.classList.add('is-open');
    }
    return fetch('/download?filename=' + encodeURIComponent(filename), { cache: 'no-store' })
        .then(function(response) {
            if (!response.ok) throw new Error('HTTP ' + response.status);
            return response.text();
        })
        .then(function(text) {
            if (modal._previewRequest !== previewRequest) return;
            setPreviewContent(content, text);
            if (editor) editor.value = content.dataset.rawText;
        })
        .catch(function(error) {
            if (modal._previewRequest === previewRequest) content.textContent = 'Preview unavailable: ' + error.message;
        });
};

document.addEventListener('click', function(event) {
    var modal = document.getElementById('filePreviewModal');
    if (modal && event.target === modal) window.closeFilePreview();
    if (!event.target.closest('.file-preview-menu')) closePreviewActions();
});

document.addEventListener('input', function(event) {
    if (event.target && event.target.id === 'filePreviewEditor') updatePreviewEditorLines();
});

document.addEventListener('scroll', function(event) {
    if (event.target && event.target.id === 'filePreviewEditor') updatePreviewEditorLines();
}, true);

document.addEventListener('keydown', function(event) {
    if (event.key === 'Escape') window.closeFilePreview();
});

window.uploadFile = function() {
    const input = document.getElementById('fileInputBox');
    const progress = document.querySelector('#fileUploadProgressBar > div');
    if (input && input._uploadRequest) return input._uploadRequest;
    const file = input && input.files && input.files[0];
    if (!file) {
        if (window.showToast) window.showToast('Select a file first.');
        return;
    }
    if (file.size > 4 * 1024 * 1024) {
        if (window.showToast) window.showToast('File exceeds 4 MB limit.');
        return;
    }

    const formData = new FormData();
    formData.append('file', file, file.name);
    const directory = document.getElementById('fileUploadDirectory');
    formData.append('directory', directory ? directory.value : 'Auto');

    const xhr = new XMLHttpRequest();
    if (progress) {
        clearTimeout(progress._hideTimer);
        progress.style.width = '0%';
        if (progress.parentElement) progress.parentElement.classList.remove('opacity-0');
    }
    xhr.open('POST', '/uploadFile', true);
    xhr.timeout = 60000;
    input._uploadRequest = xhr;

    function uploadFailed(message) {
        if (progress) {
            progress.style.width = '0%';
            if (progress.parentElement) progress.parentElement.classList.add('opacity-0');
        }
        if (message && window.showToast) window.showToast(message);
    }

    xhr.addEventListener('loadend', function() {
        if (input._uploadRequest === xhr) input._uploadRequest = null;
    });

    xhr.upload.addEventListener('progress', function(e) {
        if (!progress || !e.lengthComputable || e.total === 0) return;
        var pct = Math.round((e.loaded / e.total) * 100);
        progress.style.width = Math.max(0, Math.min(100, pct)) + '%';
    });

    xhr.addEventListener('load', function() {
        if (xhr.status >= 200 && xhr.status < 300) {
            if (progress) {
                progress.style.width = '100%';
                progress._hideTimer = setTimeout(() => { if (progress.parentElement) progress.parentElement.classList.add('opacity-0'); }, 1000);
            }
            if (window.showToast) window.showToast('Uploaded ' + file.name);
            if (input.files[0] === file) {
                const inputText = document.getElementById('fileInputText');
                if (inputText) inputText.innerText = 'Choose a file';
                input.value = '';
            }
            if (window.refreshIdeFiles) window.refreshIdeFiles();
        } else {
            var message = 'Upload failed: HTTP ' + xhr.status;
            if (xhr.status === 503) message = 'SD card unavailable.';
            else if (xhr.status === 413) message = 'File exceeds 4 MB limit.';
            uploadFailed(message);
        }
    });

    xhr.addEventListener('error', function() {
        uploadFailed('Upload failed (network error).');
    });

    xhr.addEventListener('abort', function() {
        uploadFailed();
    });

    xhr.addEventListener('timeout', function() {
        uploadFailed('Upload timed out. Try again.');
    });

    window.redlilyCsrf().then(function(token) {
        if (token) { try { xhr.setRequestHeader('X-CSRF-Token', token); } catch (_) {} }
        try {
            xhr.send(formData);
        } catch (_) {
            input._uploadRequest = null;
            uploadFailed('Upload failed (network error).');
        }
    });
    return xhr;
};

// Provide the shared confirmation dialog used by destructive file actions.
