const REFRESH_MS = 2000;
const HISTORY_MAX = 40;

const el = (id) => document.getElementById(id);

let thresholdSaveTimer = null;

const sensorHistory = {
    labels: [],
    temp: [],
    humidity: [],
};

const CHART_COLORS = {
    temp: '#f97316',
    hum: '#38bdf8',
    grid: 'rgba(148, 163, 184, 0.15)',
    axis: '#64748b',
    text: '#94a3b8',
};

function pushSensorHistory(data) {
    if (!data.valid) {
        return;
    }

    const label = new Date().toLocaleTimeString('fr-FR', {
        hour: '2-digit',
        minute: '2-digit',
        second: '2-digit',
    });

    sensorHistory.labels.push(label);
    sensorHistory.temp.push(Number(data.temp));
    sensorHistory.humidity.push(Number(data.humidity));

    if (sensorHistory.labels.length > HISTORY_MAX) {
        sensorHistory.labels.shift();
        sensorHistory.temp.shift();
        sensorHistory.humidity.shift();
    }

    drawSensorChart();
}

function chartRange(values, fallbackMin, fallbackMax, padding = 2) {
    if (!values.length) {
        return { min: fallbackMin, max: fallbackMax };
    }
    const min = Math.min(...values);
    const max = Math.max(...values);
    if (min === max) {
        return { min: min - padding, max: max + padding };
    }
    return { min: min - padding, max: max + padding };
}

function drawSensorChart() {
    const canvas = el('sensor-chart');
    if (!canvas) {
        return;
    }

    const wrap = canvas.parentElement;
    const width = Math.max(wrap.clientWidth - 16, 280);
    const height = Math.max(wrap.clientHeight - 16, 180);
    const dpr = window.devicePixelRatio || 1;

    canvas.width = Math.round(width * dpr);
    canvas.height = Math.round(height * dpr);
    canvas.style.width = `${width}px`;
    canvas.style.height = `${height}px`;

    const ctx = canvas.getContext('2d');
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    ctx.clearRect(0, 0, width, height);

    const pad = { top: 14, right: 44, bottom: 28, left: 44 };
    const plotW = width - pad.left - pad.right;
    const plotH = height - pad.top - pad.bottom;

    ctx.fillStyle = CHART_COLORS.text;
    ctx.font = '11px "Segoe UI", system-ui, sans-serif';
    ctx.textAlign = 'center';

    if (sensorHistory.labels.length < 2) {
        ctx.fillText(
            'En attente de mesures…',
            pad.left + plotW / 2,
            pad.top + plotH / 2,
        );
        return;
    }

    const tempRange = chartRange(sensorHistory.temp, 15, 35, 1);
    const humRange = chartRange(sensorHistory.humidity, 30, 70, 2);
    const count = sensorHistory.labels.length;

    const xAt = (index) =>
        pad.left + (index / (count - 1)) * plotW;
    const yTemp = (value) =>
        pad.top + (1 - (value - tempRange.min) / (tempRange.max - tempRange.min)) * plotH;
    const yHum = (value) =>
        pad.top + (1 - (value - humRange.min) / (humRange.max - humRange.min)) * plotH;

    ctx.strokeStyle = CHART_COLORS.grid;
    ctx.lineWidth = 1;
    for (let i = 0; i <= 4; i += 1) {
        const y = pad.top + (i / 4) * plotH;
        ctx.beginPath();
        ctx.moveTo(pad.left, y);
        ctx.lineTo(pad.left + plotW, y);
        ctx.stroke();
    }

    ctx.fillStyle = CHART_COLORS.axis;
    ctx.textAlign = 'right';
    for (let i = 0; i <= 4; i += 1) {
        const ratio = 1 - i / 4;
        const tempVal = tempRange.min + ratio * (tempRange.max - tempRange.min);
        const humVal = humRange.min + ratio * (humRange.max - humRange.min);
        const y = pad.top + (i / 4) * plotH + 4;
        ctx.fillStyle = CHART_COLORS.temp;
        ctx.fillText(`${tempVal.toFixed(0)}°`, pad.left - 6, y);
        ctx.fillStyle = CHART_COLORS.hum;
        ctx.textAlign = 'left';
        ctx.fillText(`${humVal.toFixed(0)}%`, pad.left + plotW + 6, y);
        ctx.textAlign = 'right';
    }

    const drawLine = (values, yFn, color) => {
        ctx.strokeStyle = color;
        ctx.lineWidth = 2;
        ctx.lineJoin = 'round';
        ctx.beginPath();
        values.forEach((value, index) => {
            const x = xAt(index);
            const y = yFn(value);
            if (index === 0) {
                ctx.moveTo(x, y);
            } else {
                ctx.lineTo(x, y);
            }
        });
        ctx.stroke();
    };

    drawLine(sensorHistory.temp, yTemp, CHART_COLORS.temp);
    drawLine(sensorHistory.humidity, yHum, CHART_COLORS.hum);

    ctx.fillStyle = CHART_COLORS.text;
    ctx.textAlign = 'center';
    const tickIndexes = [0, Math.floor((count - 1) / 2), count - 1];
    tickIndexes.forEach((index) => {
        ctx.fillText(
            sensorHistory.labels[index],
            xAt(index),
            height - 8,
        );
    });
}

function setBadge(element, connected, onLabel, offLabel) {
    element.textContent = connected ? onLabel : offLabel;
    element.classList.toggle('ok', connected);
    element.classList.toggle('ko', !connected);
}

function formatUptime(seconds) {
    const h = Math.floor(seconds / 3600);
    const m = Math.floor((seconds % 3600) / 60);
    const s = seconds % 60;
    if (h > 0) {
        return `${h}h ${m}m ${s}s`;
    }
    if (m > 0) {
        return `${m}m ${s}s`;
    }
    return `${s}s`;
}

function formatTemp(value) {
    return `${Number(value).toFixed(1).replace('.', ',')} °C`;
}

async function fetchJson(url, options = {}) {
    const response = await fetch(url, {
        ...options,
        headers: {
            Accept: 'application/json',
            ...(options.headers || {}),
        },
    });

    if (!response.ok) {
        throw new Error(`${url} → HTTP ${response.status}`);
    }

    return response.json();
}

function updateThresholdUi(data, options = {}) {
    const { syncSlider = true } = options;
    const slider = el('temp-threshold-slider');
    if (!slider) {
        return;
    }
    const value = Number(data.threshold ?? slider.value);

    slider.min = data.min ?? slider.min;
    slider.max = data.max ?? slider.max;
    if (syncSlider && document.activeElement !== slider) {
        slider.value = value;
    }
    el('threshold-value').textContent = formatTemp(
        document.activeElement === slider ? slider.value : value,
    );

    const auto = data.auto ?? true;
    el('threshold-mode').textContent = auto
        ? `Mode auto : LED si température > ${formatTemp(value)}`
        : 'Mode manuel (boutons Allumer / Éteindre)';

    if (typeof data.led === 'boolean') {
        setBadge(el('led-badge'), data.led, 'Allumée', 'Éteinte');
    }
}

async function saveThreshold(threshold) {
    const data = await fetchJson('/api/threshold', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ threshold: Number(threshold), auto: true }),
    });

    updateThresholdUi(data);
    el('actuator-result').textContent =
        `Seuil ${formatTemp(data.threshold)} — mode auto actif`;
}

function scheduleThresholdSave(value) {
    clearTimeout(thresholdSaveTimer);
    thresholdSaveTimer = setTimeout(() => {
        saveThreshold(value).catch((error) => {
            el('actuator-result').textContent = error.message;
        });
    }, 300);
}

async function refreshStatus() {
    const data = await fetchJson('/api/status');

    el('device-id').textContent = data.device ?? '—';
    setBadge(el('wifi-badge'), data.wifi?.connected, 'Connecté', 'Déconnecté');
    el('wifi-ssid').textContent = data.wifi?.ssid || '—';
    el('wifi-ip').textContent = data.wifi?.ip || '—';
    el('wifi-rssi').textContent =
        typeof data.wifi?.rssi === 'number' ? `${data.wifi.rssi} dBm` : '—';

    setBadge(el('mqtt-badge'), data.mqtt?.connected, 'Connecté', 'Déconnecté');
    el('mqtt-broker').textContent =
        data.mqtt?.broker ? `${data.mqtt.broker}:${data.mqtt.port}` : '—';
    el('mqtt-topic').textContent = data.mqtt?.topic || '—';
    el('mqtt-cmd-topic').textContent = data.mqtt?.cmdTopic || '—';
    el('mqtt-qos').textContent = data.mqtt?.qos ?? '—';
    el('mqtt-latency').textContent =
        typeof data.mqtt?.publishLatencyMs === 'number'
            ? `${data.mqtt.publishLatencyMs} ms`
            : '—';
    el('mqtt-user').textContent = data.mqtt?.user || '—';

    el('sys-uptime').textContent = formatUptime(data.uptime ?? 0);
    el('sys-heap').textContent =
        typeof data.heapFree === 'number'
            ? `${Math.round(data.heapFree / 1024)} Ko`
            : '—';

    updateThresholdUi({
        threshold: data.actuators?.tempThreshold,
        auto: data.actuators?.autoLed,
        led: data.actuators?.led,
    }, { syncSlider: false });
}

async function refreshSensors() {
    const data = await fetchJson('/api/sensors');

    setBadge(el('sensor-badge'), data.valid, 'Valide', 'Erreur');
    el('sensor-temp').textContent =
        data.valid ? Number(data.temp).toFixed(1) : '—';
    el('sensor-hum').textContent =
        data.valid ? Number(data.humidity).toFixed(1) : '—';
    el('sensor-pot').textContent =
        data.valid ? Number(data.potentiometer ?? 0).toFixed(0) : '—';
    el('sensor-button').textContent = data.buttonPressed ? 'Appuyé' : 'Relâché';
    pushSensorHistory(data);
}

async function refreshConfig() {
    const data = await fetchJson('/api/config');

    el('cfg-broker-input').value = data.broker || '';
    el('cfg-port-input').value = data.port ?? 1883;
    el('cfg-user-input').value = data.user || '';
    el('cfg-topic').textContent = data.topic || '—';
    el('config-note').textContent = data.note || '';
}

async function refreshThreshold() {
    const data = await fetchJson('/api/threshold');
    updateThresholdUi(data);
}

async function saveConfig(event) {
    event.preventDefault();

    const result = el('config-result');
    result.textContent = 'Enregistrement…';

    const payload = {
        broker: el('cfg-broker-input').value.trim(),
        port: Number(el('cfg-port-input').value),
        user: el('cfg-user-input').value.trim(),
        password: el('cfg-password-input').value,
    };

    try {
        const data = await fetchJson('/api/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload),
        });
        result.textContent = JSON.stringify(data, null, 2);
        el('cfg-password-input').value = '';
        await refreshConfig();
    } catch (error) {
        result.textContent = error.message;
    }
}

async function sendActuator(action) {
    const result = el('actuator-result');
    result.textContent = 'Envoi…';

    try {
        const data = await fetchJson('/api/actuators', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ action }),
        });
        setBadge(el('led-badge'), data.led, 'Allumée', 'Éteinte');
        el('threshold-mode').textContent =
            'Mode manuel (boutons Allumer / Éteindre)';
        result.textContent = data.led ? 'LED allumée (manuel)' : 'LED éteinte (manuel)';
    } catch (error) {
        result.textContent = error.message;
    }
}

async function refreshAll() {
    try {
        await Promise.all([
            refreshStatus(),
            refreshSensors(),
            refreshConfig(),
            refreshThreshold(),
        ]);
        el('last-update').textContent =
            `Mise à jour ${new Date().toLocaleTimeString('fr-FR')}`;
    } catch (error) {
        el('last-update').textContent = `Erreur : ${error.message}`;
    }
}

document.querySelectorAll('[data-action]').forEach((button) => {
    button.addEventListener('click', () => {
        sendActuator(button.dataset.action);
    });
});

document.addEventListener('DOMContentLoaded', () => {
    el('config-form').addEventListener('submit', saveConfig);

    const slider = el('temp-threshold-slider');
    if (slider) {
        slider.addEventListener('input', () => {
            el('threshold-value').textContent = formatTemp(slider.value);
            el('threshold-mode').textContent =
                `Mode auto : LED si température > ${formatTemp(slider.value)}`;
        });
        slider.addEventListener('change', () => {
            scheduleThresholdSave(slider.value);
        });
    }

    window.addEventListener('resize', () => {
        drawSensorChart();
    });

    refreshAll();
    setInterval(refreshAll, REFRESH_MS);
});
