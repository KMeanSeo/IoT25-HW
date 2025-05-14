document.addEventListener('DOMContentLoaded', function() {
    // 탭 전환 기능
    function switchTab(tabName) {
        document.querySelectorAll('.nav-btn').forEach(btn => {
            btn.classList.remove('active');
            if (btn.getAttribute('data-tab') === tabName) btn.classList.add('active');
        });
        document.querySelectorAll('.content-section').forEach(section => {
            section.style.display = 'none';
            if (section.id === `${tabName}Section`) section.style.display = 'block';
        });
        setTimeout(() => {
            if (window.posChart) window.posChart.resize();
            if (window.compChart) window.compChart.resize();
        }, 100);
    }

    // 차트 초기화
    function initPositionChart() {
        const ctx = document.getElementById('positionChart').getContext('2d');
        window.posChart = new Chart(ctx, {
            type: 'scatter',
            data: {
                datasets: [{
                    label: 'Devices',
                    data: [],
                    backgroundColor: 'rgba(75, 192, 192, 0.5)',
                    pointRadius: 8
                }]
            },
            options: {
                scales: {
                    x: { type: 'linear', min: -10, max: 10, title: { display: true, text: 'X (meters)' } },
                    y: { type: 'linear', min: -10, max: 10, title: { display: true, text: 'Y (meters)' } }
                }
            }
        });
    }

    function initComparisonChart() {
        const ctx = document.getElementById('comparisonChart').getContext('2d');
        window.compChart = new Chart(ctx, {
            type: 'bar',
            data: {
                labels: [],
                datasets: [{
                    label: 'Actual',
                    data: [],
                    backgroundColor: 'rgba(54, 162, 235, 0.5)'
                }, {
                    label: 'Measured',
                    data: [],
                    backgroundColor: 'rgba(255, 99, 132, 0.5)'
                }]
            },
            options: {
                responsive: true,
                scales: {
                    y: { beginAtZero: true, title: { display: true, text: 'Distance (meters)' } }
                }
            }
        });
    }

    // 클라이언트 선택 관리
    let selectedClients = new Set();
    document.querySelectorAll('.client-btn').forEach(btn => {
        btn.addEventListener('click', () => {
            const clientId = btn.dataset.client;
            if (selectedClients.has(clientId)) {
                selectedClients.delete(clientId);
                btn.classList.remove('active');
            } else {
                selectedClients.add(clientId);
                btn.classList.add('active');
            }
        });
    });

    // 교정 데이터 제출
    async function submitCalibration() {
        const actualDist = parseFloat(document.getElementById('actualDist').value);
        if (selectedClients.size === 0) {
            alert('Please select at least one client!');
            return;
        }
        if (isNaN(actualDist) || actualDist <= 0) {
            alert('Please enter a valid distance greater than 0!');
            document.getElementById('actualDist').focus();
            return;
        }
        try {
            const requests = Array.from(selectedClients).map(clientId =>
                fetch('/calibrate', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ client_id: clientId, actual_dist: actualDist })
                }).then(async (res) => {
                    if (!res.ok) {
                        const err = await res.json();
                        alert('Server error: ' + (err.error || 'Unknown error'));
                    }
                })
            );
            await Promise.all(requests);
            updateResults();
        } catch (error) {
            console.error('Calibration error:', error);
        }
    }

    // 결과 업데이트
    async function updateResults() {
        try {
            const response = await fetch('/get_calibration');
            const data = await response.json();
            const tbody = document.querySelector('#resultsTable tbody');
            tbody.innerHTML = '';
            window.compChart.data.labels = [];
            window.compChart.data.datasets[0].data = [];
            window.compChart.data.datasets[1].data = [];
            for (const [client, values] of Object.entries(data)) {
                tbody.innerHTML += `
                    <tr>
                        <td>${client}</td>
                        <td>${values.actual}</td>
                        <td>${values.measured}</td>
                        <td>${values.error}%</td>
                    </tr>
                `;
                window.compChart.data.labels.push(client);
                window.compChart.data.datasets[0].data.push(values.actual);
                window.compChart.data.datasets[1].data.push(values.measured);
            }
            window.compChart.update();
        } catch (error) {
            console.error('Update error:', error);
        }
    }

    // 실시간 위치 업데이트
    function setupSSE() {
        const evtSource = new EventSource("/stream");
        evtSource.onmessage = function(e) {
            const data = JSON.parse(e.data);
            const dataset = window.posChart.data.datasets[0];
            const index = dataset.data.findIndex(p => p.client_id === data.client_id);
            const newPoint = { x: data.x, y: data.y, client_id: data.client_id };
            if (index > -1) {
                dataset.data[index] = newPoint;
            } else {
                dataset.data.push(newPoint);
            }
            window.posChart.update();
        };
    }

    // 이벤트 바인딩 및 초기화
    document.getElementById('submitBtn').addEventListener('click', submitCalibration);
    document.querySelectorAll('.nav-btn').forEach(btn => {
        btn.addEventListener('click', () => switchTab(btn.dataset.tab));
    });

    // 기본 탭, 차트, SSE 초기화
    switchTab('position');
    initPositionChart();
    initComparisonChart();
    setupSSE();
});
