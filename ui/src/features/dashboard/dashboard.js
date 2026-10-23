    // Create the dashboard telemetry charts.
    window.initDashboardCharts = function() {
        if (window._dashboardChartsReady || typeof Chart === 'undefined') return;
        window._dashboardChartsReady = true;
        if (typeof Chart !== 'undefined') {
            Chart.defaults.color = '#9b9b9b';
            Chart.defaults.font.family = 'Inter, sans-serif';

            // Show recent heap usage.
            var ctxMem = document.getElementById('memoryChart');
            if (ctxMem) {
                var memData = [];
                window.memChart = new Chart(ctxMem, {
                    type: 'line',
                    data: {
                        labels: [],
                        datasets: [{
                            label: 'Heap %',
                            data: memData,
                            borderColor: '#ef4444',
                            backgroundColor: 'rgba(239, 68, 68, 0.1)',
                            borderWidth: 2,
                            tension: 0.4,
                            fill: true,
                            pointRadius: 0
                        }]
                    },
                    options: {
                        responsive: true,
                        maintainAspectRatio: false,
                        animation: { duration: 0 },
                        scales: {
                            y: { min: 0, max: 100, grid: { color: '#27272a' } },
                            x: { grid: { display: false }, ticks: { display: false } }
                        },
                        plugins: { legend: { display: false } }
                    }
                });
            }

            // Show recent temperature readings.
            var ctxTemperature = document.getElementById('temperatureChart');
            if (ctxTemperature) {
                var temperatureData = [];
                window.temperatureChart = new Chart(ctxTemperature, {
                    type: 'line',
                    data: {
                        labels: [],
                        datasets: [{
                            label: '\u00b0C',
                            data: temperatureData,
                            borderColor: '#f97316',
                            backgroundColor: 'rgba(249, 115, 22, 0.1)',
                            borderWidth: 2,
                            tension: 0.4,
                            fill: true,
                            pointRadius: 0
                        }]
                    },
                    options: {
                        responsive: true,
                        maintainAspectRatio: false,
                        animation: { duration: 0 },
                        scales: {
                            y: { min: 0, max: 100, grid: { color: '#27272a' } },
                            x: { grid: { display: false }, ticks: { display: false } }
                        },
                        plugins: { legend: { display: false } }
                    }
                });
            }

        }
    };
    document.addEventListener('DOMContentLoaded', function() {
        window.initDashboardCharts();
    });
