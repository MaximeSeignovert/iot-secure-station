const REFRESH_MS = 2000;
const API_TOKEN_KEY = 'iot-api-token';

const el = (id) => document.getElementById(id);

function getApiToken() {
    return sessionStorage.getItem(API_TOKEN_KEY) || '';
}

function setApiToken(value) {
    sessionStorage.setItem(API_TOKEN_KEY, value);
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

async function fetchJson(url, options = {}) {
    const headers = {
        Accept: 'application/json',
        ...(options.headers || {}),
    };

    const token = getApiToken();
    if (token) {
        headers['X-Api-Token'] = token;
    }

    const response = await fetch(url, {
        ...options,
        headers,
    });

    if (!response.ok) {
        throw new Error(`${url} → HTTP ${response.status}`);
    }

    return response.json();
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
    el('mqtt-user').textContent = data.mqtt?.user || '—';

    el('sys-uptime').textContent = formatUptime(data.uptime ?? 0);
    el('sys-heap').textContent =
        typeof data.heapFree === 'number'
            ? `${Math.round(data.heapFree / 1024)} Ko`
            : '—';
}

async function refreshSensors() {
    const data = await fetchJson('/api/sensors');

    setBadge(el('sensor-badge'), data.valid, 'Valide', 'Erreur');
    el('sensor-temp').textContent =
        data.valid ? Number(data.temp).toFixed(1) : '—';
    el('sensor-hum').textContent =
        data.valid ? Number(data.humidity).toFixed(1) : '—';
}

async function refreshConfig() {
    const data = await fetchJson('/api/config');

    el('cfg-broker-input').value = data.broker || '';
    el('cfg-port-input').value = data.port ?? 1883;
    el('cfg-user-input').value = data.user || '';
    el('cfg-topic').textContent = data.topic || '—';
    el('config-note').textContent = data.note || '';

    const tokenInput = el('api-token-input');
    if (!tokenInput.value) {
        tokenInput.value = getApiToken();
    }
}

async function saveConfig(event) {
    event.preventDefault();

    const result = el('config-result');
    result.textContent = 'Enregistrement…';

    const token = el('api-token-input').value.trim();
    setApiToken(token);

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
        result.textContent = JSON.stringify(data, null, 2);
    } catch (error) {
        result.textContent = error.message;
    }
}

async function refreshAll() {
    try {
        await Promise.all([refreshStatus(), refreshSensors(), refreshConfig()]);
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
    refreshAll();
    setInterval(refreshAll, REFRESH_MS);
});
