#include "web_assets.h"
#include <pgmspace.h>
#include <cstring>

namespace {

struct AssetEntry {
    const char* path;
    const char* contentType;
    const char* data;
};

const char kIndexHtml[] PROGMEM = R"webembed0(﻿<!DOCTYPE html>
<html lang="fr">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>IoT Secure Station</title>
    <link rel="stylesheet" href="/style.css">
</head>
<body>
    <div class="layout">
        <header class="header">
            <div>
                <p class="eyebrow">Firmware ESP32</p>
                <h1>IoT Secure Station</h1>
            </div>
            <div class="header-meta">
                <span id="device-id">—</span>
                <span id="ui-version">Dashboard v4</span>
                <span id="last-update">Mise à jour…</span>
            </div>
        </header>

        <section class="status-grid">
            <article class="card" id="card-wifi">
                <h2>WiFi</h2>
                <p class="badge" id="wifi-badge">—</p>
                <dl>
                    <div><dt>SSID</dt><dd id="wifi-ssid">—</dd></div>
                    <div><dt>IP</dt><dd id="wifi-ip">—</dd></div>
                    <div><dt>Signal</dt><dd id="wifi-rssi">—</dd></div>
                </dl>
            </article>

            <article class="card" id="card-mqtt">
                <h2>MQTT</h2>
                <p class="badge" id="mqtt-badge">—</p>
                <dl>
                    <div><dt>Broker</dt><dd id="mqtt-broker">—</dd></div>
                    <div><dt>Topic</dt><dd id="mqtt-topic">—</dd></div>
                    <div><dt>Topic cmd</dt><dd id="mqtt-cmd-topic">—</dd></div>
                    <div><dt>QoS</dt><dd id="mqtt-qos">—</dd></div>
                    <div><dt>Latence pub</dt><dd id="mqtt-latency">—</dd></div>
                    <div><dt>Utilisateur</dt><dd id="mqtt-user">—</dd></div>
                </dl>
            </article>

            <article class="card" id="card-system">
                <h2>Système</h2>
                <dl>
                    <div><dt>Uptime</dt><dd id="sys-uptime">—</dd></div>
                    <div><dt>Heap libre</dt><dd id="sys-heap">—</dd></div>
                </dl>
            </article>
        </section>

        <section class="card sensors-card">
            <div class="section-head">
                <h2>Capteurs live</h2>
                <span class="badge" id="sensor-badge">—</span>
            </div>
            <div class="sensor-grid">
                <div class="sensor-tile">
                    <p class="sensor-label">Température</p>
                    <p class="sensor-value"><span id="sensor-temp">—</span><small>°C</small></p>
                </div>
                <div class="sensor-tile">
                    <p class="sensor-label">Humidité</p>
                    <p class="sensor-value"><span id="sensor-hum">—</span><small>%</small></p>
                </div>
                <div class="sensor-tile">
                    <p class="sensor-label">Potentiomètre</p>
                    <p class="sensor-value"><span id="sensor-pot">—</span><small>%</small></p>
                </div>
                <div class="sensor-tile">
                    <p class="sensor-label">Bouton</p>
                    <p class="sensor-value"><span id="sensor-button">—</span></p>
                </div>
            </div>
        </section>

        <section class="card chart-card">
            <div class="section-head">
                <h2>Historique capteurs</h2>
                <span class="chart-legend">
                    <span class="legend-item legend-temp">Température</span>
                    <span class="legend-item legend-hum">Humidité</span>
                </span>
            </div>
            <p class="hint">Dernières mesures (rafraîchissement toutes les 2 s)</p>
            <div class="chart-wrap">
                <canvas id="sensor-chart" aria-label="Graphique température et humidité"></canvas>
            </div>
        </section>

        <section class="card actuators-card">
            <div class="section-head">
                <h2>LED</h2>
                <span class="badge" id="led-badge">—</span>
            </div>
            <p class="hint" id="threshold-mode">Mode auto : LED si température &gt; seuil</p>
            <label class="slider-label">
                <span class="slider-head">
                    <span>Seuil température</span>
                    <strong id="threshold-value">30,0 °C</strong>
                </span>
                <input type="range" id="temp-threshold-slider"
                       min="10" max="50" step="0.5" value="30">
            </label>
            <p class="hint">GPIO 5 — curseur = mode auto. Boutons = mode manuel.</p>
            <div class="actions">
                <button type="button" id="btn-led-on" data-action="led_on">Allumer</button>
                <button type="button" id="btn-led-off" data-action="led_off">Éteindre</button>
            </div>
            <p id="actuator-result" class="hint">—</p>
        </section>

        <section class="card config-card">
            <h2>Configuration MQTT</h2>
            <p class="hint" id="config-note">Chargement…</p>
            <form id="config-form" class="config-form">
                <label>
                    Broker
                    <input type="text" id="cfg-broker-input" name="broker" required>
                </label>
                <label>
                    Port
                    <input type="number" id="cfg-port-input" name="port" min="1" max="65535" required>
                </label>
                <label>
                    Utilisateur MQTT
                    <input type="text" id="cfg-user-input" name="user" autocomplete="off">
                </label>
                <label>
                    Mot de passe MQTT
                    <input type="password" id="cfg-password-input" name="password" autocomplete="off">
                </label>
                <div class="actions">
                    <button type="submit" id="btn-save-config">Enregistrer (NVS)</button>
                </div>
            </form>
            <p class="hint">Topic : <span id="cfg-topic">—</span></p>
            <pre id="config-result" class="result">—</pre>
        </section>
    </div>
    <script src="/app.js"></script>
</body>
</html>
)webembed0";

const char kAppJs[] PROGMEM = R"webembed0(﻿const REFRESH_MS = 2000;
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
)webembed0";

const char kStyleCss[] PROGMEM = R"webembed0(﻿:root {
    --bg: #0b1220;
    --panel: #111827;
    --panel-border: #1f2937;
    --text: #e5e7eb;
    --muted: #94a3b8;
    --accent: #38bdf8;
    --ok: #22c55e;
    --ko: #ef4444;
    --tile: #0f172a;
}

* {
    box-sizing: border-box;
    margin: 0;
    padding: 0;
}

body {
    font-family: "Segoe UI", system-ui, sans-serif;
    background: radial-gradient(circle at top, #172554 0%, var(--bg) 45%);
    color: var(--text);
    min-height: 100vh;
    line-height: 1.5;
}

.layout {
    max-width: 960px;
    margin: 0 auto;
    padding: 1.5rem;
    display: grid;
    gap: 1rem;
}

.header {
    display: flex;
    justify-content: space-between;
    align-items: flex-start;
    gap: 1rem;
    flex-wrap: wrap;
}

.eyebrow {
    color: var(--accent);
    font-size: 0.85rem;
    text-transform: uppercase;
    letter-spacing: 0.08em;
}

h1 {
    font-size: 1.8rem;
    margin-top: 0.25rem;
}

h2 {
    font-size: 1rem;
    margin-bottom: 0.75rem;
    color: var(--accent);
}

.header-meta {
    display: flex;
    flex-direction: column;
    align-items: flex-end;
    gap: 0.25rem;
    color: var(--muted);
    font-size: 0.9rem;
}

.card {
    background: rgba(17, 24, 39, 0.92);
    border: 1px solid var(--panel-border);
    border-radius: 14px;
    padding: 1rem 1.1rem;
    box-shadow: 0 10px 30px rgba(0, 0, 0, 0.25);
}

.status-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
    gap: 1rem;
}

.badge {
    display: inline-block;
    padding: 0.2rem 0.65rem;
    border-radius: 999px;
    font-size: 0.8rem;
    font-weight: 600;
    margin-bottom: 0.75rem;
    background: #334155;
    color: #cbd5e1;
}

.badge.ok {
    background: rgba(34, 197, 94, 0.15);
    color: var(--ok);
}

.badge.ko {
    background: rgba(239, 68, 68, 0.15);
    color: var(--ko);
}

dl {
    display: grid;
    gap: 0.45rem;
}

dl div {
    display: flex;
    justify-content: space-between;
    gap: 1rem;
}

dt {
    color: var(--muted);
    font-size: 0.9rem;
}

dd {
    font-weight: 600;
    text-align: right;
    word-break: break-all;
}

.section-head {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 0.75rem;
}

.sensor-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
    gap: 0.75rem;
}

.sensor-tile {
    background: var(--tile);
    border: 1px solid var(--panel-border);
    border-radius: 12px;
    padding: 1rem;
}

.sensor-label {
    color: var(--muted);
    font-size: 0.9rem;
}

.sensor-value {
    font-size: 2.2rem;
    font-weight: 700;
    margin-top: 0.35rem;
}

.sensor-value small {
    font-size: 1rem;
    color: var(--muted);
    margin-left: 0.25rem;
}

.chart-legend {
    display: flex;
    flex-wrap: wrap;
    gap: 0.75rem;
    font-size: 0.85rem;
}

.legend-item {
    display: inline-flex;
    align-items: center;
    gap: 0.35rem;
    color: var(--muted);
}

.legend-item::before {
    content: "";
    width: 1.25rem;
    height: 3px;
    border-radius: 999px;
}

.legend-temp::before {
    background: #f97316;
}

.legend-hum::before {
    background: #38bdf8;
}

.chart-wrap {
    position: relative;
    width: 100%;
    height: 220px;
    background: var(--tile);
    border: 1px solid var(--panel-border);
    border-radius: 12px;
    padding: 0.5rem;
}

#sensor-chart {
    display: block;
    width: 100%;
    height: 100%;
}

.hint {
    color: var(--muted);
    font-size: 0.9rem;
    margin-bottom: 0.75rem;
}

.config-list dd {
    max-width: 70%;
}

.config-form {
    display: grid;
    gap: 0.75rem;
    margin-bottom: 0.75rem;
}

.config-form label {
    display: grid;
    gap: 0.35rem;
    font-size: 0.9rem;
    color: var(--muted);
}

.config-form input {
    background: var(--tile);
    border: 1px solid var(--panel-border);
    border-radius: 10px;
    color: var(--text);
    padding: 0.55rem 0.75rem;
    font: inherit;
}

.config-form input:focus {
    outline: 2px solid rgba(56, 189, 248, 0.35);
    border-color: var(--accent);
}

.actions {
    display: flex;
    flex-wrap: wrap;
    gap: 0.5rem;
    margin-bottom: 0.75rem;
}

button {
    border: 1px solid #2563eb;
    background: #1d4ed8;
    color: white;
    border-radius: 10px;
    padding: 0.55rem 0.9rem;
    cursor: pointer;
    font-weight: 600;
}

button:hover {
    background: #2563eb;
}

.slider-label {
    display: grid;
    gap: 0.65rem;
    margin-bottom: 0.75rem;
}

.slider-head {
    display: flex;
    justify-content: space-between;
    align-items: baseline;
    color: var(--muted);
    font-size: 0.9rem;
}

.slider-head strong {
    color: var(--accent);
    font-size: 1.1rem;
}

input[type="range"] {
    width: 100%;
    accent-color: var(--accent);
    cursor: pointer;
}

.result {
    background: var(--tile);
    border: 1px solid var(--panel-border);
    border-radius: 10px;
    padding: 0.75rem;
    color: #cbd5e1;
    font-size: 0.85rem;
    overflow-x: auto;
    white-space: pre-wrap;
}

@media (max-width: 640px) {
    .header-meta {
        align-items: flex-start;
    }

    .sensor-value {
        font-size: 1.8rem;
    }
}
)webembed0";

constexpr AssetEntry kAssets[] = {
    {"/index.html", "text/html", kIndexHtml},
    {"/app.js", "application/javascript", kAppJs},
    {"/style.css", "text/css", kStyleCss},
};

}  // namespace

bool webAssetFind(const char* path, WebAsset& out) {
    for (const auto& asset : kAssets) {
        if (strcmp(path, asset.path) == 0) {
            out.contentType = asset.contentType;
            out.data = asset.data;
            out.length = strlen_P(asset.data);
            return true;
        }
    }
    return false;
}
