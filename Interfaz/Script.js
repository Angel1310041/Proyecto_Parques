    document.addEventListener('DOMContentLoaded', function() {
    // Variables globales
    const apiUrl = 'http://' + window.location.hostname + '/api';
    let currentConfig = {};

    // Elementos del DOM
    const menuBtn = document.getElementById('menu-btn');
    const sidebar = document.getElementById('sidebar');
    const mainContent = document.getElementById('main-content');
    const menuLinks = document.querySelectorAll('.menu-list a');
    const sections = document.querySelectorAll('.content-section');
    const i2cDevicesList = document.getElementById('i2c-devices');
    const wifiEnabledCheckbox = document.getElementById('wifi-enabled');
    const loraIdInput = document.getElementById('lora-id');
    const loraChannelInput = document.getElementById('lora-channel');

    // --- Overlay para cerrar sidebar ---
    const overlay = document.createElement('div');
    overlay.id = 'sidebar-overlay';
    overlay.style.display = 'none';
    overlay.style.position = 'fixed';
    overlay.style.zIndex = '99';
    overlay.style.top = '0';
    overlay.style.left = '0';
    overlay.style.right = '0';
    overlay.style.bottom = '0';
    overlay.style.background = 'rgba(0,0,0,0.2)';
    document.body.appendChild(overlay);

    // Inicialización
    initMenu();
    loadCurrentConfig();
    setupEventListeners();
    initWiFiSection();
    setupInputRestrictions();
    setupSidebarOverlay();

    function initMenu() {
    // Botón de menú hamburguesa
    menuBtn.addEventListener('click', function() {
    sidebar.classList.toggle('active');
    mainContent.classList.toggle('shifted');
    toggleMenuAnimation();
    if (sidebar.classList.contains('active')) {
    overlay.style.display = 'block';
    } else {
    overlay.style.display = 'none';
    }
    });

    // Submenú desplegable
    const submenuToggle = document.querySelector('.submenu-toggle');
    if (submenuToggle) {
    submenuToggle.addEventListener('click', function(e) {
    e.preventDefault();
    this.parentElement.classList.toggle('active');
    document.querySelector('.submenu').classList.toggle('active');
    });
    }

    // Navegación entre secciones
    menuLinks.forEach(link => {
    link.addEventListener('click', function(e) {
    if (!this.classList.contains('submenu-toggle')) {
    e.preventDefault();
    showSection(this.getAttribute('data-section'));
    closeMenuOnMobile();
    }
    });
    });
    }

    function setupSidebarOverlay() {
    // Cierra el sidebar al hacer clic fuera de él
    overlay.addEventListener('click', function() {
    sidebar.classList.remove('active');
    mainContent.classList.remove('shifted');
    overlay.style.display = 'none';
    toggleMenuAnimation();
    });
    }

    function toggleMenuAnimation() {
    const spans = menuBtn.querySelectorAll('span');
    if (sidebar.classList.contains('active')) {
    spans[0].style.transform = 'rotate(45deg) translate(5px, 5px)';
    spans[1].style.opacity = '0';
    spans[2].style.transform = 'rotate(-45deg) translate(7px, -6px)';
    } else {
    spans[0].style.transform = 'none';
    spans[1].style.opacity = '1';
    spans[2].style.transform = 'none';
    }
    }

    function closeMenuOnMobile() {
    if (window.innerWidth <= 768) {
    sidebar.classList.remove('active');
    mainContent.classList.remove('shifted');
    overlay.style.display = 'none';
    toggleMenuAnimation();
    }
    }

    function showSection(sectionId) {
    sections.forEach(section => section.classList.remove('active'));
    const targetSection = sectionId === 'salir'
    ? document.getElementById('salir-section')
    : document.getElementById(`${sectionId}-section`);
    if (targetSection) targetSection.classList.add('active');

    // Cargar redes guardadas cuando se muestra la sección WiFi
    if (sectionId === 'wifi') {
    showSavedNetworks();
    }
    }

    async function loadCurrentConfig() {
    try {
    const response = await fetch(`${apiUrl}/config`);
    currentConfig = await response.json();

    document.getElementById('current-id').textContent = currentConfig.id || 'HELTEC01';
    document.getElementById('current-channel').textContent = currentConfig.channel || '0';
    document.getElementById('current-wifi').textContent = currentConfig.WiFi ? 'Activado' : 'Desactivado';
    loraIdInput.value = currentConfig.id || 'HELTEC01';
    loraChannelInput.value = currentConfig.channel || '0';
    wifiEnabledCheckbox.checked = currentConfig.WiFi || false;

    updateButtonStates();
    } catch (error) {
    console.error('Error al cargar configuración:', error);
    setDefaultValues();
    }
    }

    function setDefaultValues() {
    document.getElementById('current-id').textContent = 'HELTEC01';
    document.getElementById('current-channel').textContent = '0';
    document.getElementById('current-wifi').textContent = 'Desactivado';
    loraIdInput.value = 'HELTEC01';
    loraChannelInput.value = '0';
    wifiEnabledCheckbox.checked = false;
    }

    function updateButtonStates() {
    const updateButton = (elementId, isActive) => {
    const btn = document.getElementById(elementId);
    if (btn) {
    btn.classList.toggle('active', isActive);
    }
    };

    updateButton('activar-pantalla', currentConfig.displayOn);
    updateButton('apagar-pantalla', !currentConfig.displayOn);
    updateButton('activar-uart', currentConfig.UART);
    updateButton('apagar-uart', !currentConfig.UART);
    updateButton('activar-wifi', currentConfig.WiFi);
    updateButton('apagar-wifi', !currentConfig.WiFi);
    updateButton('escaner-i2c', currentConfig.I2C);
    updateButton('desactivar-i2c', !currentConfig.I2C);
    }

    function setupEventListeners() {
    // Formulario de configuración LoRa
    const configForm = document.getElementById('config-form');
    if (configForm) {
    configForm.addEventListener('submit', async function(e) {
    e.preventDefault();
    const id = loraIdInput.value;
    const channel = loraChannelInput.value;
    const wifiEnabled = wifiEnabledCheckbox.checked;

    if (!validateConfigInput(id, channel)) return;

    try {
    const response = await fetch(`${apiUrl}/config`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ id, channel, WiFi: wifiEnabled })
    });
    const data = await response.json();

    if (data.success) {
    showAlert('success', 'Configuración guardada correctamente');
    updateUIWithNewConfig(id, channel, wifiEnabled);
    } else {
    throw new Error(data.message || 'Error desconocido');
    }
    } catch (error) {
    showError('Error al guardar configuración:', error);
    }
    });
    }

    // Asignar eventos a los botones
    setupButton('activar-pantalla', () => sendDisplayCommand('1'));
    setupButton('apagar-pantalla', () => sendDisplayCommand('0'));
    setupButton('prueba-pantalla', () => {
    const currentId = document.getElementById('current-id').textContent;
    sendDisplayTestCommand(currentId);
    });

    setupButton('activar-uart', () => sendUartCommand('1'));
    setupButton('apagar-uart', () => sendUartCommand('0'));

    setupButton('escaner-i2c', scanI2CDevices);
    setupButton('desactivar-i2c', () => sendI2CCommand('0'));

    // --- BOTÓN SALIR/REINICIAR ---
    setupButton('salir-reiniciar', async () => {
    showConfirmDialog(
    '¿Estás seguro que deseas salir del modo de programación y reiniciar la tarjeta? Esto apagará el LED y reiniciará el dispositivo.',
    async () => {
    try {
    // Llama al endpoint de reboot (debe apagar LED y reiniciar en el backend)
    const response = await fetch(`${apiUrl}/reboot`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' }
    });

    if (response.ok) {
    showRebootMessage();
    } else {
    throw new Error('Error al reiniciar la tarjeta');
    }
    } catch (error) {
    showError('Error al reiniciar la tarjeta:', error);
    }
    }
    );
    });

    setupButton('cancelar-salir', () => showSection('inicio'));

    // Estilo para botón cancelar
    const cancelarBtn = document.getElementById('cancelar-salir');
    if (cancelarBtn) cancelarBtn.classList.add('btn-wifi');
    }

    function setupButton(id, handler) {
    const btn = document.getElementById(id);
    if (btn) btn.addEventListener('click', handler);
    }

    function validateConfigInput(id, channel) {
    // Solo letras y números, máximo 3 caracteres, sin ñ, sin caracteres especiales, no '000'
    if (!/^[A-Za-z0-9]{3}$/.test(id) || /ñ|Ñ/.test(id) || id.toUpperCase() === '000') {
    showAlert('error', 'ID inválido. Solo letras y números (sin ñ), exactamente 3 caracteres y distinto de "000".');
    return false;
    }

    // Canal solo número entre 0 y 8
    if (!/^[0-8]$/.test(channel)) {
    showAlert('error', 'Canal inválido. Debe ser un número entre 0 y 8.');
    return false;
    }

    return true;
    }

    function updateUIWithNewConfig(id, channel, wifiEnabled) {
    document.getElementById('current-id').textContent = id;
    document.getElementById('current-channel').textContent = channel;
    document.getElementById('current-wifi').textContent = wifiEnabled ? 'Activado' : 'Desactivado';
    currentConfig.id = id;
    currentConfig.channel = channel;
    currentConfig.WiFi = wifiEnabled;
    }

    function showRebootMessage() {
    const salirSection = document.getElementById('salir-section');
    if (salirSection) {
    salirSection.innerHTML = `
    <div class="funcion-container">
    <h2>Reiniciando tarjeta Heltec...</h2>
    <div class="reboot-message">
    <p>La tarjeta se está reiniciando. Por favor, espera unos segundos.</p>
    <div class="loading-spinner"></div>
    <p>La página se recargará automáticamente.</p>
    </div>
    </div>
    `;
    }

    // Intentar recargar después de 10 segundos
    setTimeout(() => {
    window.location.reload();
    }, 10000);
    }

    function showExitProgrammingMessage() {
    const salirSection = document.getElementById('salir-section');
    if (salirSection) {
    salirSection.innerHTML = `
    <div class="funcion-container">
    <h2>Saliendo del modo programación...</h2>
    <div class="reboot-message">
    <p>El dispositivo se está reiniciando en modo normal.</p>
    <div class="loading-spinner"></div>
    <p>La conexión se cerrará automáticamente.</p>
    </div>
    </div>
    `;
    }

    // Intentar recargar después de 10 segundos
    setTimeout(() => {
    window.location.href = 'http://' + window.location.hostname;
    }, 10000);
    }

    // WiFi Management Functions
    function initWiFiSection() {
    // Control WiFi
    setupButton('activar-wifi', () => sendWiFiCommand('1'));
    setupButton('apagar-wifi', () => sendWiFiCommand('0'));

    // Añadir clases de estilo a los botones WiFi
    const activarWifiBtn = document.getElementById('activar-wifi');
    const apagarWifiBtn = document.getElementById('apagar-wifi');
    if (activarWifiBtn) activarWifiBtn.classList.add('btn-wifi');
    if (apagarWifiBtn) apagarWifiBtn.classList.add('btn-wifi');

    // Escaneo WiFi
    setupButton('scan-wifi', scanWiFiNetworks);
    setupButton('refresh-wifi', scanWiFiNetworks);

    // Redes guardadas
    setupButton('show-saved', showSavedNetworks);
    setupButton('remove-all-wifi', removeAllNetworks);

    // Botón salir
    const salirBtn = document.getElementById('salir-reiniciar');
    if (salirBtn) salirBtn.classList.add('btn-salir');
    }

    async function sendWiFiCommand(state) {
    try {
    const response = await fetch(`${apiUrl}/wifi`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ state })
    });
    const data = await response.json();

    if (data.success) {
    showAlert('success', `WiFi ${state === '1' ? 'activado' : 'desactivado'} correctamente`);
    currentConfig.WiFi = (state === '1');
    document.getElementById('current-wifi').textContent = currentConfig.WiFi ? 'Activado' : 'Desactivado';
    updateButtonStates();
    } else {
    throw new Error(data.message || 'Error desconocido');
    }
    } catch (error) {
    showError('Error al controlar WiFi:', error);
    }
    }

    function scanWiFiNetworks() {
    const wifiTable = document.getElementById('wifi-table');
    wifiTable.innerHTML = '<tr><td colspan="4" class="loading">Escaneando redes WiFi...</td></tr>';

    fetch(`${apiUrl}/wifi/scan`)
    .then(response => {
    if (!response.ok) throw new Error('Error en la respuesta del servidor');
    return response.json();
    })
    .then(data => {
    if (data.error) {
    wifiTable.innerHTML = `<tr><td colspan="4" class="error">${data.error}</td></tr>`;
    } else if (data.redes && data.redes.length > 0) {
    renderWiFiTable(data.redes);
    } else {
    wifiTable.innerHTML = '<tr><td colspan="4">No se encontraron redes WiFi</td></tr>';
    }
    })
    .catch(error => {
    wifiTable.innerHTML = `<tr><td colspan="4" class="error">Error: ${error.message}</td></tr>`;
    });
    }

    function renderWiFiTable(networks) {
    const wifiTable = document.getElementById('wifi-table');
    wifiTable.innerHTML = `
    <tr>
    <th>SSID</th>
    <th>Seguridad</th>
    <th>Señal</th>
    <th>Acción</th>
    </tr>
    `;

    networks.forEach(network => {
    const row = document.createElement('tr');
    const strength = getStrengthLevel(network.rssi);
    const securityColor = getSecurityColor(network.encryptionType);

    row.innerHTML = `
    <td>${network.ssid || 'Oculta'}</td>
    <td style="color: ${securityColor}">${network.encryption}</td>
    <td>
    <div class="signal-strength">
    <div class="signal-bar ${strength > 0 ? 'active' : ''}"></div>
    <div class="signal-bar ${strength > 1 ? 'active' : ''}"></div>
    <div class="signal-bar ${strength > 2 ? 'active' : ''}"></div>
    <div class="signal-bar ${strength > 3 ? 'active' : ''}"></div>
    </div>
    </td>
    <td>
    <button class="connect-btn" data-ssid="${network.ssid || ''}" data-encryption="${network.encryptionType}">
    Conectar
    </button>
    </td>
    `;
    wifiTable.appendChild(row);
    });

    // Agregar event listeners a los botones de conexión
    document.querySelectorAll('.connect-btn').forEach(btn => {
    btn.addEventListener('click', function() {
    const ssid = this.getAttribute('data-ssid');
    const encryption = this.getAttribute('data-encryption');
    showPasswordDialog(ssid, encryption);
    });
    });
    }

    function getStrengthLevel(rssi) {
    if (rssi >= -50) return 4; // Excelente
    if (rssi >= -60) return 3; // Bueno
    if (rssi >= -70) return 2; // Regular
    if (rssi >= -80) return 1; // Débil
    return 0; // Muy débil
    }

    function getSecurityColor(encryptionType) {
    if (encryptionType === 0) return '#2ecc71'; // Abierta (verde)
    if (encryptionType === 1) return '#f39c12'; // WEP (naranja)
    return '#e74c3c'; // WPA/WPA2 (rojo)
    }

    function showPasswordDialog(ssid, encryptionType) {
    const dialog = document.createElement('div');
    dialog.className = 'password-dialog';
    dialog.innerHTML = `
    <div class="dialog-content">
    <h3>Conectar a "${ssid}"</h3>
    ${encryptionType > 0 ? `
    <input type="password" id="wifi-password" placeholder="Contraseña WiFi" required>
    <label class="save-network">
    <input type="checkbox" id="save-network" checked>
    Guardar esta red
    </label>
    ` : '<p>Red abierta (sin contraseña)</p>'}
    <div class="dialog-buttons">
    <button id="cancel-wifi">Cancelar</button>
    <button id="confirm-wifi">Conectar</button>
    </div>
    </div>
    `;
    document.body.appendChild(dialog);

    document.getElementById('cancel-wifi').addEventListener('click', () => {
    document.body.removeChild(dialog);
    });

    document.getElementById('confirm-wifi').addEventListener('click', () => {
    const password = encryptionType > 0 ?
    document.getElementById('wifi-password').value : '';
    const saveNetwork = encryptionType > 0 ?
    document.getElementById('save-network').checked : true;

    if (encryptionType > 0 && !password) {
    showAlert('error', 'Por favor ingresa la contraseña');
    return;
    }

    connectToWiFi(ssid, password, encryptionType, saveNetwork);
    document.body.removeChild(dialog);
    });
    }

    async function connectToWiFi(ssid, password, encryptionType, saveNetwork = true) {
    try {
    showAlert('info', `Conectando a ${ssid}...`);

    const response = await fetch(`${apiUrl}/wifi/connect`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ ssid, password, encryptionType, save: saveNetwork })
    });

    const data = await response.json();

    if (data.success) {
    showAlert('success', `Conectado a ${ssid} correctamente`);
    // Actualizar la lista de redes guardadas
    showSavedNetworks();
    // Actualizar estado de conexión
    updateConnectionStatus(ssid, data.ip);
    } else {
    throw new Error(data.message || 'Error al conectar');
    }
    } catch (error) {
    showError('Error al conectar a WiFi:', error);
    }
    }

    async function showSavedNetworks() {
    const savedNetworksContainer = document.getElementById('saved-networks');
    savedNetworksContainer.innerHTML = '<p class="loading">Cargando redes guardadas...</p>';

    try {
    const response = await fetch(`${apiUrl}/wifi/saved`);
    const data = await response.json();

    if (data.message) {
    savedNetworksContainer.innerHTML = `<p>${data.message}</p>`;
    } else if (data.redes && data.redes.length > 0) {
    renderSavedNetworks(data.redes);
    } else {
    savedNetworksContainer.innerHTML = '<p>No hay redes guardadas</p>';
    }
    } catch (error) {
    console.error('Error al cargar redes guardadas:', error);
    savedNetworksContainer.innerHTML = `<p class="error">Error al cargar redes guardadas</p>`;
    }
    }

    // --- REDES GUARDADAS CON FAVORITA ---
    function renderSavedNetworks(networks) {
    const savedNetworksContainer = document.getElementById('saved-networks');
    savedNetworksContainer.innerHTML = ''; // Limpiar contenedor

    if (networks.length === 0) {
    savedNetworksContainer.innerHTML = '<p>No hay redes guardadas</p>';
    return;
    }

    const table = document.createElement('table');
    table.className = 'saved-networks-table';
    table.innerHTML = `
    <thead>
    <tr>
    <th>Favorita</th>
    <th>Nombre de Red</th>
    <th>Acciones</th>
    </tr>
    </thead>
    <tbody>
    </tbody>
    `;

    const tbody = table.querySelector('tbody');

    networks.forEach(network => {
    const row = document.createElement('tr');
    row.innerHTML = `
    <td>
    <button class="favorite-btn${network.isFavorite ? ' active' : ''}" 
    data-ssid="${network.ssid}" 
    title="Marcar como favorita">
    <span class="star-icon">${network.isFavorite ? '★' : '☆'}</span>
    </button>
    </td>
    <td>${network.ssid}</td>
    <td>
    <button class="btn-wifi connect-saved" data-ssid="${network.ssid}">Conectar</button>
    <button class="btn-salir delete-saved" data-ssid="${network.ssid}">Eliminar</button>
    </td>
    `;

    // Evento para el botón de favorito
    row.querySelector('.favorite-btn').addEventListener('click', () => {
    setFavoriteNetwork(network.ssid);
    });

    tbody.appendChild(row);
    });

    savedNetworksContainer.appendChild(table);

    // Eventos para marcar favorita
    document.querySelectorAll('.favorite-btn').forEach(btn => {
    btn.addEventListener('click', async () => {
    const ssid = btn.getAttribute('data-ssid');
    await setFavoriteNetwork(ssid);
    showSavedNetworks(); // Refresca la lista
    });
    });

    // Eventos para conectar/eliminar
    document.querySelectorAll('.connect-saved').forEach(btn => {
    btn.addEventListener('click', () => {
    const ssid = btn.getAttribute('data-ssid');
    connectToSavedNetwork(ssid);
    });
    });

    document.querySelectorAll('.delete-saved').forEach(btn => {
    btn.addEventListener('click', () => {
    const ssid = btn.getAttribute('data-ssid');
    removeNetwork(ssid);
    });
    });
    }

    async function setFavoriteNetwork(ssid) {
    try {
    const response = await fetch(`${apiUrl}/wifi/favorite`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ ssid })
    });
    const data = await response.json();
    if (!data.success) throw new Error(data.message || 'Error al marcar favorita');
    showAlert('success', `Red "${ssid}" marcada como favorita`);
    showSavedNetworks(); // Refrescar la lista
    } catch (error) {
    showAlert('error', `Error al marcar favorita: ${error.message}`);
    }
    }

    async function connectToSavedNetwork(ssid) {
    try {
    showAlert('info', `Conectando a ${ssid}...`);

    const response = await fetch(`${apiUrl}/wifi/connect/saved`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ ssid })
    });

    const data = await response.json();

    if (data.success) {
    showAlert('success', `Conectado a ${ssid} correctamente`);
    // Actualizar estado de conexión en la interfaz
    updateConnectionStatus(ssid, data.ip);
    } else {
    throw new Error(data.message || 'Error al conectar');
    }
    } catch (error) {
    showAlert('error', `Error al conectar: ${error.message}`);
    }
    }

    async function removeNetwork(ssid) {
    showConfirmDialog(`¿Eliminar la red "${ssid}"?`, async () => {
    try {
    const response = await fetch(`${apiUrl}/wifi/remove`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ ssid })
    });

    const data = await response.json();

    if (data.success) {
    showAlert('success', `Red "${ssid}" eliminada`);
    showSavedNetworks(); // Actualizar la lista
    } else {
    throw new Error(data.message || 'Error al eliminar');
    }
    } catch (error) {
    showAlert('error', `Error al eliminar: ${error.message}`);
    }
    });
    }

    async function removeAllNetworks() {
    showConfirmDialog('¿Eliminar TODAS las redes guardadas?', async () => {
    try {
    const response = await fetch(`${apiUrl}/wifi/remove-all`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' }
    });

    const data = await response.json();

    if (data.success) {
    showAlert('success', 'Todas las redes eliminadas correctamente');
    showSavedNetworks();
    } else {
    throw new Error(data.message || 'Error al eliminar');
    }
    } catch (error) {
    showError('Error al eliminar redes:', error);
    }
    });
    }

    function updateConnectionStatus(ssid, ip) {
    const statusElement = document.getElementById('wifi-status');
    if (statusElement) {
    statusElement.innerHTML = `
    <p><strong>Conectado a:</strong> ${ssid}</p>
    <p><strong>Dirección IP:</strong> ${ip}</p>
    `;
    }

    // Actualizar botones de WiFi
    updateButtonStates();
    }

    // Funciones para otras secciones
    async function sendDisplayCommand(state) {
    try {
    const response = await fetch(`${apiUrl}/display`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ state })
    });

    const data = await response.json();

    if (data.success) {
    showAlert('success', `Pantalla ${state === '1' ? 'activada' : 'apagada'}`);
    currentConfig.displayOn = (state === '1');
    updateButtonStates();
    } else {
    throw new Error(data.message || 'Error desconocido');
    }
    } catch (error) {
    showError('Error al controlar pantalla:', error);
    }
    }

async function sendDisplayTestCommand() {
    try {
        const response = await fetch(`${apiUrl}/display/test`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' }
        });
        
        const data = await response.json();
        
        if (data.success) {
            showAlert('success', `ID: ${data.id}, Versión: ${data.version}`);
        } else {
            throw new Error(data.message || 'Error al mostrar información');
        }
    } catch (error) {
        showAlert('error', `Error: ${error.message}`);
    }
}

// Y actualiza el event listener del botón para que llame a la función sin parámetros
setupButton('prueba-pantalla', sendDisplayTestCommand);

    async function sendUartCommand(state) {
    try {
    const response = await fetch(`${apiUrl}/uart`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ state })
    });

    const data = await response.json();

    if (data.success) {
    showAlert('success', `UART ${state === '1' ? 'activado' : 'desactivado'}`);
    currentConfig.UART = (state === '1');
    updateButtonStates();
    } else {
    throw new Error(data.message || 'Error desconocido');
    }
    } catch (error) {
    showError('Error al controlar UART:', error);
    }
    }

    async function scanI2CDevices() {
    i2cDevicesList.innerHTML = '<li class="loading">Buscando dispositivos I2C...</li>';

    try {
    const response = await fetch(`${apiUrl}/i2c/scan`);
    const data = await response.json();

    if (data.error) {
    i2cDevicesList.innerHTML = `<li class="error">${data.error}</li>`;
    } else if (data.devices && data.devices.length > 0) {
    renderI2CDevices(data.devices);
    } else {
    i2cDevicesList.innerHTML = '<li class="no-devices">No se encontraron dispositivos I2C</li>';
    }

    currentConfig.I2C = true;
    updateButtonStates();
    } catch (error) {
    i2cDevicesList.innerHTML = `<li class="error">Error: ${error.message}</li>`;
    }
    }

    function renderI2CDevices(devices) {
    i2cDevicesList.innerHTML = '';

    devices.forEach(device => {
    const item = document.createElement('li');
    item.className = 'device-item';
    item.innerHTML = `
    <span class="device-address">0x${device.address.toString(16)}</span>
    <span class="device-type">${device.type || 'Desconocido'}</span>
    `;
    i2cDevicesList.appendChild(item);
    });
    }

    async function sendI2CCommand(state) {
    try {
    const response = await fetch(`${apiUrl}/i2c`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ state })
    });

    const data = await response.json();

    if (data.success) {
    showAlert('success', `I2C ${state === '1' ? 'activado' : 'desactivado'}`);
    currentConfig.I2C = (state === '1');
    updateButtonStates();

    if (state === '0') {
    i2cDevicesList.innerHTML = '<li class="no-devices">I2C desactivado</li>';
    }
    } else {
    throw new Error(data.message || 'Error desconocido');
    }
    } catch (error) {
    showError('Error al controlar I2C:', error);
    }
    }

    // Restricciones en tiempo real para los campos de ID y Canal
    function setupInputRestrictions() {
    // ID: Solo letras y números, sin ñ, máximo 3 caracteres, no '000'
    loraIdInput.addEventListener('input', function(e) {
    let value = this.value
    .replace(/[^A-Za-z0-9]/g, '') // Solo letras y números
    .replace(/ñ|Ñ/g, '');         // Elimina ñ/Ñ si se cuela
    value = value.substring(0, 3);     // Máximo 3 caracteres

    // No permitir '000'
    if (value.toUpperCase() === '000') {
    value = '';
    }

    this.value = value;
    });

    loraIdInput.addEventListener('paste', function(e) {
    let paste = (e.clipboardData || window.clipboardData).getData('text');
    paste = paste.replace(/[^A-Za-z0-9]/g, '').replace(/ñ|Ñ/g, '').substring(0, 3);
    if (paste.toUpperCase() === '000') {
    e.preventDefault();
    this.value = '';
    }
    });

    // Canal: Solo números del 0 al 8
    loraChannelInput.addEventListener('input', function(e) {
    let value = this.value.replace(/[^0-9]/g, '');
    if (value.length > 1) value = value[0]; // Solo un dígito
    if (value !== '' && (parseInt(value) < 0 || parseInt(value) > 8)) {
    value = '';
    }
    this.value = value;
    });

    loraChannelInput.addEventListener('paste', function(e) {
    let paste = (e.clipboardData || window.clipboardData).getData('text');
    if (!/^[0-8]$/.test(paste)) {
    e.preventDefault();
    this.value = '';
    }
    });
    }

    // Funciones de utilidad
    function showAlert(type, message) {
    const alert = document.createElement('div');
    alert.className = `alert ${type}`;
    alert.textContent = message;
    document.body.appendChild(alert);

    setTimeout(() => {
    alert.style.opacity = '0';
    setTimeout(() => document.body.removeChild(alert), 300);
    }, 3000);
    }

    function showError(context, error) {
    console.error(context, error);
    showAlert('error', `${context} ${error.message}`);
    }

    function showConfirmDialog(message, confirmCallback) {
    const dialog = document.createElement('div');
    dialog.className = 'confirm-dialog';
    dialog.innerHTML = `
    <div class="dialog-content">
    <h3>Confirmar</h3>
    <p>${message}</p>
    <div class="dialog-buttons">
    <button id="cancel-action">Cancelar</button>
    <button id="confirm-action">Confirmar</button>
    </div>
    </div>
    `;
    document.body.appendChild(dialog);

    document.getElementById('cancel-action').addEventListener('click', () => {
    document.body.removeChild(dialog);
    });

    document.getElementById('confirm-action').addEventListener('click', () => {
    confirmCallback();
    document.body.removeChild(dialog);
    });
    }
    });