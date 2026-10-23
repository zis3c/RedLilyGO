document.addEventListener('DOMContentLoaded', function() {
    var dropZone = document.querySelector('label.cursor-pointer');
    var fileInput = document.getElementById('fileInputBox');
    var fileText = document.getElementById('fileInputText');

    if (dropZone && fileInput && fileText) {
        ['dragenter', 'dragover', 'dragleave', 'drop'].forEach(function(name) {
            dropZone.addEventListener(name, function(event) {
                event.preventDefault();
                event.stopPropagation();
            });
        });
        ['dragenter', 'dragover'].forEach(function(name) {
            dropZone.addEventListener(name, function() {
                dropZone.classList.add('border-red-500', 'bg-red-500/10');
            });
        });
        ['dragleave', 'drop'].forEach(function(name) {
            dropZone.addEventListener(name, function() {
                dropZone.classList.remove('border-red-500', 'bg-red-500/10');
            });
        });
        dropZone.addEventListener('drop', function(event) {
            if (!event.dataTransfer.files.length) return;
            fileInput.files = event.dataTransfer.files;
            fileText.textContent = fileInput.files[0].name;
        });
    }

    window.moveSelectedFile = function(source, directory) {
        if (!source || !directory) return;
        var filename = source.substring(source.lastIndexOf('/') + 1);
        var destination = directory.replace(/\/$/, '') + '/' + filename;
        window.redlilyPost('/moveFile?from=' + encodeURIComponent(source) + '&to=' + encodeURIComponent(destination)).then(function(response) {
            if (!response.ok) return response.json().catch(function() { return {}; }).then(function(data) {
                throw new Error(data.error || 'Move failed');
            });
            return response.json();
        }).then(function() {
            if (window.showToast) window.showToast('Moved to ' + directory);
            if (window.refreshIdeFiles) window.refreshIdeFiles();
        }).catch(function(error) {
            if (window.showToast) window.showToast('Move failed: ' + error.message);
        });
    };

    var tree = document.getElementById('tree');
    if (!tree) return;
    tree.addEventListener('dragstart', function(event) {
        var file = event.target.closest('[data-path]');
        if (!file) return;
        event.dataTransfer.effectAllowed = 'move';
        event.dataTransfer.setData('text/plain', file.dataset.path);
    });
    tree.addEventListener('dragover', function(event) {
        if (!event.target.closest('[data-folder="true"]')) return;
        event.preventDefault();
        event.dataTransfer.dropEffect = 'move';
    });
    tree.addEventListener('drop', function(event) {
        var folder = event.target.closest('[data-folder="true"]');
        var source = event.dataTransfer.getData('text/plain');
        if (!folder || !source) return;
        event.preventDefault();
        window.moveSelectedFile(source, folder.dataset.folderPath);
    });
});
