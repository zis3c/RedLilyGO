// Submit the USB device-type startup setting as a CSRF-protected POST.
document.addEventListener('DOMContentLoaded', function() {
    var form = document.getElementById('usbDeviceTypeForm');
    if (!form) return;
    form.addEventListener('submit', function(event) {
        event.preventDefault();
        var name = form.elements['name'].value;
        var value = form.elements['value'].value.trim();
        var url = '/set?name=' + encodeURIComponent(name) + '&value=' + encodeURIComponent(value);
        window.redlilyPost(url)
            .then(function(response) {
                if (!response.ok) return response.text().then(function(text) { throw new Error(text || ('HTTP ' + response.status)); });
                if (window.showToast) window.showToast('Startup USB mode saved');
            })
            .catch(function(error) {
                if (window.showToast) window.showToast('Save failed: ' + error.message, 'error');
            });
    });
});
