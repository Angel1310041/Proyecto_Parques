#include "interfaz.h"
#include "hardware.h"
#include "comunicacion.h"
#include "config.h"
#include "eeprom.h"
#include "pantalla.h"
#include <WiFi.h>
#include <WebServer.h>
#include <FS.h>
#include <SPIFFS.h>
#include <WiFiType.h>
#include <Preferences.h>

WebServer server(80);

void Interfaz::entrarModoProgramacion() {
  modoProgramacion = true;
  esp_task_wdt_reset();
  digitalWrite(LED_STATUS, HIGH);
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  xTaskCreatePinnedToCore(
    endpointsMProg,
    "Endpoints",
    8192,
    NULL,
    2,
    NULL,
    0
  );
  imprimirSerial("---# Modo Programacion Activado #---", 'g');
}

void endpointsMProg(void *pvParameters) {
  imprimirSerial("Modo Programacion Falso", 'b');
  servidorModoProgramacion();  // Inicia el AP y el servidor web
  vTaskDelete(NULL);
}

// Modifica el modo de programacion por boton fisico
  void modoprogporbotonfisico() {
  static unsigned long tiempoInicio = 0;
  static bool botonAnterior = false;

  bool botonPresionado = digitalRead(BOTON_MODO_PROG) == LOW;

  if (!modoProgramacion) {
  if (botonPresionado) {
  if (!botonAnterior) {
  tiempoInicio = millis();
  } else {
  if (millis() - tiempoInicio >= 3000) {
  Interfaz::entrarModoProgramacion();
  }
  }
  } else if (botonAnterior) {
  tiempoInicio = 0;
  }
  }
  botonAnterior = botonPresionado;
  }

  void Interfaz::salirModoProgramacion() {
    modoProgramacion = false;
    digitalWrite(LED_STATUS, LOW);  // Apagar el LED 35
    server.stop();                  // Detener el servidor web
    WiFi.softAPdisconnect(true);    // Desconectar el AP WiFi
    imprimirSerial("---# Modo Programacion Desactivado #---", 'g');
    delay(1000);                    // Pequeña pausa antes de reiniciar
    ESP.restart();                  // Reiniciar la tarjeta
}
  // preferencias (o configuraciones guardadas en memoria no volátil) para redes WiFi, pero solo la primera vez que se ejecuta
  void inicializarWiFiPrefs() {
  Preferences prefs;
  prefs.begin("WIFI_NETS", false); // Nombre fijo del namespace

  bool initialized = prefs.getBool("initialized", false);
  if (!initialized) {
    // Limpiar todas las redes posibles (3)
    for (int i = 0; i < 3; i++) {
      String keySSID = "ssid_" + String(i);
      String keyPASS = "pass_" + String(i);

      // Solo borrar si existe la clave
      if (prefs.isKey(keySSID.c_str())) {
        prefs.remove(keySSID.c_str());
      }
      if (prefs.isKey(keyPASS.c_str())) {
        prefs.remove(keyPASS.c_str());
      }

      // También limpiamos la estructura en RAM
      memset(&redes[i], 0, sizeof(Network));
    }

    prefs.putBool("initialized", true);
    imprimirSerial("Preferencias WiFi inicializadas", 'g');

    // Aseguramos coherencia RAM-EEPROM
    ManejoEEPROM::guardarRedesEEPROM();
  }

  prefs.end();
}

  
  //implementacion de dos cosas al mismo tiempo página HTML servida desde el ESP32
  //Una API RESTful (de intercambio de datos)
  void servidorModoProgramacion() {
  if (!SPIFFS.begin(true)) {
  imprimirSerial("Error al montar SPIFFS", 'r');
  return;
  }

  WiFi.softAP("Interfazlor", "12345678");
  IPAddress myIP = WiFi.softAPIP();
  imprimirSerial(String("Servidor web iniciado en: http://") + myIP.toString(), 'y');

  // Configurar endpoints
  server.on("/", HTTP_GET, []() {
  if (SPIFFS.exists("/interfaz.html.gz")) {
  File file = SPIFFS.open("/interfaz.html.gz", FILE_READ);
  server.streamFile(file, "text/html");
  file.close();
  } else {
  server.send(404, "text/plain", "Archivo no encontrado");
  }
  });

   // Nuevo endpoint para manejar favoritos
  server.on("/api/wifi/favorite", HTTP_POST, []() {
    if (!server.hasArg("plain")) {
        server.send(400, "text/plain", "Bad Request");
        return;
    }

    String body = server.arg("plain");
    DynamicJsonDocument doc(128);
    deserializeJson(doc, body);

    String ssid = doc["ssid"].as<String>();
    
    if(ManejoEEPROM::guardarRedFavorita(ssid.c_str())) {
        DynamicJsonDocument response(128);
        response["success"] = true;
        response["message"] = "Red marcada como favorita";
        String jsonResponse;
        serializeJson(response, jsonResponse);
        server.send(200, "application/json", jsonResponse);
    } else {
        server.send(404, "text/plain", "Red no encontrada");
    }
});

  // Configuración GET
  server.on("/api/config", HTTP_GET, []() {
  DynamicJsonDocument doc(256);
  doc["id"] = configLora.IDLora;
  doc["channel"] = configLora.Canal;
  doc["displayOn"] = configLora.Pantalla;
  doc["UART"] = configLora.UART;
  doc["I2C"] = configLora.I2C;
  doc["WiFi"] = configLora.WiFi;
  doc["DEBUG"] = configLora.DEBUG;
  String respuesta;
  serializeJson(doc, respuesta);
  server.send(200, "application/json", respuesta);
  });

  // Configuración POST
  server.on("/api/config", HTTP_POST, []() {
  if (!server.hasArg("plain")) {
  server.send(400, "text/plain", "Bad Request");
  return;
  }

  String body = server.arg("plain");
  DynamicJsonDocument doc(256);
  deserializeJson(doc, body);

  String newId = doc["id"].as<String>();
  int newChannel = doc["channel"];

  if (newId.length() > 0 && newId.length() <= 3 && newChannel >= 0 && newChannel <= 8) {
  strncpy(configLora.IDLora, newId.c_str(), sizeof(configLora.IDLora));
  configLora.Canal = newChannel;

  if (doc.containsKey("WiFi")) configLora.WiFi = doc["WiFi"];
  if (doc.containsKey("DEBUG")) configLora.DEBUG = doc["DEBUG"];

  ManejoEEPROM::guardarTarjetaConfigEEPROM();
  lora.begin(canales[configLora.Canal]);
  lora.startReceive();

  DynamicJsonDocument responseDoc(128);
  responseDoc["success"] = true;
  String response;
  serializeJson(responseDoc, response);
  server.send(200, "application/json", response);

  mostrarEstadoLoRa(String(configLora.IDLora), String(configLora.Canal), "3.1.1.1");
  } else {
  server.send(400, "text/plain", "Parámetros inválidos");
  }
  });

  // Control WiFi
  server.on("/api/wifi", HTTP_POST, []() {
  if (!server.hasArg("plain")) {
  server.send(400, "text/plain", "Bad Request");
  return;
  }

  String body = server.arg("plain");
  DynamicJsonDocument doc(128);
  deserializeJson(doc, body);

  String state = doc["state"].as<String>();
  configLora.WiFi = (state == "1");

  ManejoEEPROM::guardarTarjetaConfigEEPROM();

  DynamicJsonDocument responseDoc(128);
  responseDoc["success"] = true;
  responseDoc["message"] = "WiFi " + String(configLora.WiFi ? "activado" : "desactivado");
  String response;
  serializeJson(responseDoc, response);
  server.send(200, "application/json", response);
  });

  // Escaneo WiFi
  server.on("/api/wifi/scan", HTTP_GET, []() {
  if (!configLora.WiFi) {
  server.send(200, "application/json", "{\"error\":\"WiFi desactivado\"}");
  return;
  }

  DynamicJsonDocument doc(1024);
  JsonArray redes = doc.createNestedArray("redes");

  int n = WiFi.scanNetworks(false, true);
  imprimirSerial(String("Escaneando redes WiFi... Encontradas: ") + String(n), 'y');

  for (int i = 0; i < n; ++i) {
  JsonObject red = redes.createNestedObject();
  red["ssid"] = WiFi.SSID(i);
  red["rssi"] = WiFi.RSSI(i);
  red["channel"] = WiFi.channel(i);
  red["encryption"] = getEncryptionType(WiFi.encryptionType(i));
  red["encryptionType"] = (int)WiFi.encryptionType(i);
  }

  String respuesta;
  serializeJson(doc, respuesta);
  server.send(200, "application/json", respuesta);
  WiFi.scanDelete();
  });

  // Conectar a WiFi (modificado para limitar a 3 redes guardadas)
  server.on("/api/wifi/connect", HTTP_POST, []() {
  if (!configLora.WiFi) {
  server.send(200, "application/json", "{\"error\":\"WiFi desactivado\"}");
  return;
  }

  if (!server.hasArg("plain")) {
  server.send(400, "text/plain", "Bad Request");
  return;
  }

  String body = server.arg("plain");
  DynamicJsonDocument doc(256);
  deserializeJson(doc, body);

  String ssid = doc["ssid"].as<String>();
  String password = doc["password"].as<String>();
  bool saveNetwork = doc.containsKey("save") ? doc["save"].as<bool>() : true;

  if (ssid.isEmpty()) {
  server.send(400, "text/plain", "SSID no puede estar vacío");
  return;
  }

  // Guardar la red si se especificó
  if (saveNetwork) {
  // Contar redes guardadas
  int count = 0;
  for (int i = 0; i < 3; i++) {
  if (strlen(redes[i].ssid) > 0) count++;
  // Si la red ya existe, permitir actualizar la contraseña
  if (strcmp(redes[i].ssid, ssid.c_str()) == 0) {
      break; // Permitimos actualizar
  }
  }
  // Si ya hay 3 y la red no existe, rechazar
  bool exists = false;
  for (int i = 0; i < 3; i++) {
  if (strcmp(redes[i].ssid, ssid.c_str()) == 0) {
      exists = true;
      break;
  }
  }
  if (count >= 3 && !exists) {
  DynamicJsonDocument responseDoc(128);
  responseDoc["success"] = false;
  responseDoc["message"] = "No se pueden guardar más de 3 redes WiFi";
  String response;
  serializeJson(responseDoc, response);
  server.send(200, "application/json", response);
  return;
  }
  // Si hay espacio o la red ya existe, guardar normalmente
  if (ManejoEEPROM::guardarWiFiCredenciales(ssid.c_str(), password.c_str())) {
  imprimirSerial(String("Red guardada: ") + ssid, 'g');
  } else {
  imprimirSerial("No se pudo guardar la red", 'y');
  }
  }

  // Intentar conectar
  WiFi.begin(ssid.c_str(), password.c_str());
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
  delay(500);
  imprimirSerial(".", 'y');
  }

  DynamicJsonDocument responseDoc(256);
  if (WiFi.status() == WL_CONNECTED) {
  responseDoc["success"] = true;
  responseDoc["message"] = "Conectado a " + ssid;
  responseDoc["ip"] = WiFi.localIP().toString();

  // Actualizar lista de redes guardadas en la respuesta
  JsonArray savedNetworks = responseDoc.createNestedArray("savedNetworks");
  for (int i = 0; i < 3; i++) {
  if (strlen(redes[i].ssid) > 0) {
      savedNetworks.add(redes[i].ssid);
  }
  }
  } else {
  responseDoc["success"] = false;
  responseDoc["message"] = "Error al conectar a " + ssid;
  }

  String response;
  serializeJson(responseDoc, response);
  server.send(200, "application/json", response);
  });

  // Conectar a red guardada
  server.on("/api/wifi/connect/saved", HTTP_POST, []() {
  if (!configLora.WiFi) {
  server.send(200, "application/json", "{\"error\":\"WiFi desactivado\"}");
  return;
  }

  if (!server.hasArg("plain")) {
  server.send(400, "text/plain", "Bad Request");
  return;
  }

  String body = server.arg("plain");
  DynamicJsonDocument doc(256);
  deserializeJson(doc, body);

  String ssid = doc["ssid"].as<String>();

  if (ssid.length() == 0) {
  server.send(400, "text/plain", "SSID no puede estar vacío");
  return;
  }

  // Buscar la red guardada
  const char* password = nullptr;
  bool found = false;

  for (int i = 0; i < 3; ++i) {
  if (strcmp(redes[i].ssid, ssid.c_str()) == 0) {
  password = redes[i].password;
  found = true;
  break;
  }
  }

  if (!found || password == nullptr) {
  server.send(404, "application/json", "{\"error\":\"Red no encontrada o sin contraseña\"}");
  return;
  }

  // Intentar conectar
  WiFi.begin(ssid.c_str(), password);
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
  delay(500);
  imprimirSerial(".", 'y');
  }

  DynamicJsonDocument responseDoc(256);
  if (WiFi.status() == WL_CONNECTED) {
  responseDoc["success"] = true;
  responseDoc["message"] = "Conectado a " + ssid;
  responseDoc["ip"] = WiFi.localIP().toString();
  } else {
  responseDoc["success"] = false;
  responseDoc["message"] = "Error al conectar a " + ssid;
  }

  String response;
  serializeJson(responseDoc, response);
  server.send(200, "application/json", response);
  });

  // Redes guardadas
   server.on("/api/wifi/saved", HTTP_GET, []() {
    DynamicJsonDocument doc(512);
    JsonArray redesJson = doc.createNestedArray("redes");

    bool hasNetworks = false;
    for (int i = 0; i < 3; i++) {
      if (strlen(redes[i].ssid) > 0) {
        JsonObject red = redesJson.createNestedObject();
        red["ssid"] = redes[i].ssid;
        red["id"] = i;
        red["isFavorite"] = redes[i].isFavorite;
        hasNetworks = true;
      }
    }

    if (!hasNetworks) {
      doc["message"] = "No hay redes guardadas";
    }

    String respuesta;
    serializeJson(doc, respuesta);
    server.send(200, "application/json", respuesta);
  });

  

  // Eliminar red guardada
  server.on("/api/wifi/remove", HTTP_POST, []() {
  if (!server.hasArg("plain")) {
  server.send(400, "text/plain", "Bad Request");
  return;
  }

  String body = server.arg("plain");
  DynamicJsonDocument doc(256);
  deserializeJson(doc, body);

  String ssid = doc["ssid"].as<String>();

  if (ssid.length() == 0) {
  server.send(400, "text/plain", "SSID no puede estar vacío");
  return;
  }

  bool removed = false;
  for (int i = 0; i < 3; ++i) {
  if (strcmp(redes[i].ssid, ssid.c_str()) == 0) {
  memset(&redes[i], 0, sizeof(Network)); // Limpiar la estructura
  removed = true;
  ManejoEEPROM::guardarRedesEEPROM(); // Nueva función para guardar en EEPROM
  break;
  }
  }

  if (removed) {
  DynamicJsonDocument responseDoc(128);
  responseDoc["success"] = true;
  responseDoc["message"] = "Red eliminada correctamente";
  String response;
  serializeJson(responseDoc, response);
  server.send(200, "application/json", response);
  } else {
  server.send(404, "text/plain", "Red no encontrada");
  }
  });

  // Resetear credenciales WiFi
server.on("/wifi/remove-all", HTTP_POST, []() {
for (int i = 0; i < 3; ++i) {
  memset(&redes[i], 0, sizeof(Network));
}
ManejoEEPROM::guardarRedesEEPROM();
server.send(200, "application/json", "{\"success\":true,\"message\":\"Todas las redes WiFi eliminadas\"}");
}); 
  // Control de pantalla
server.on("/api/display", HTTP_POST, []() {
    if (!server.hasArg("plain")) {
        server.send(400, "text/plain", "Bad Request");
        return;
    }

    String body = server.arg("plain");
    DynamicJsonDocument doc(128);
    deserializeJson(doc, body);

    bool nuevoEstado = doc["state"] == "1";
    configurarDisplay(nuevoEstado); // Esta función ahora muestra el estado en serial

    DynamicJsonDocument responseDoc(128);
    responseDoc["success"] = true;
    responseDoc["display"] = nuevoEstado ? 1 : 0;
    
    String response;
    serializeJson(responseDoc, response);
    server.send(200, "application/json", response);
});

  // Control UART
// Control UART
server.on("/api/uart", HTTP_POST, []() {
    if (!server.hasArg("plain")) {
        server.send(400, "text/plain", "Bad Request");
        return;
    }

    String body = server.arg("plain");
    DynamicJsonDocument doc(128);
    deserializeJson(doc, body);

    bool nuevoEstado = doc["state"] == "1";
    configLora.UART = nuevoEstado;
    ManejoEEPROM::guardarTarjetaConfigEEPROM();

    // Solo mostrar un mensaje cuando se desactiva la UART
    if (!nuevoEstado) {
        imprimirSerial("UART inhabilitado", 'y');
    }

    ManejoComunicacion::initUART();

    DynamicJsonDocument responseDoc(128);
    responseDoc["success"] = true;
    responseDoc["uart"] = configLora.UART;
    
    String response;
    serializeJson(responseDoc, response);
    server.send(200, "application/json", response);
});
  // Control I2C
 // Endpoint para control I2C
server.on("/api/i2c", HTTP_POST, []() {
    if (!server.hasArg("plain")) {
        server.send(400, "text/plain", "Bad Request");
        return;
    }

    String body = server.arg("plain");
    DynamicJsonDocument doc(128);
    deserializeJson(doc, body);

    bool enable = doc["state"] == "1";
    configLora.I2C = enable;
    ManejoEEPROM::guardarTarjetaConfigEEPROM();

    if (enable) {
        Wire.begin(SDA, SCL); // Reemplaza con tus pines
        Wire.setClock(100000); // 100 kHz
        imprimirSerial("I2C activado", 'g');
    } else {
        Wire.end();
        imprimirSerial("I2C desactivado", 'y');
    }

    DynamicJsonDocument response(128);
    response["success"] = true;
    response["i2c_enabled"] = configLora.I2C;
    
    String jsonResponse;
    serializeJson(response, jsonResponse);
    server.send(200, "application/json", jsonResponse);
});


// Escaneo I2C mejorado con mensaje de resultado
    server.on("/api/i2c/scan", HTTP_GET, []() {
        if (!configLora.I2C) {
            server.send(200, "application/json", "{\"error\":\"I2C desactivado\"}");
            return;
        }

        DynamicJsonDocument doc(1024);
        JsonArray dispositivos = doc.createNestedArray("dispositivos");
        JsonArray direcciones = doc.createNestedArray("direcciones");

        // Escanear bus I2C
        byte error, address;
        int nDevices = 0;

        for (address = 1; address < 127; address++) {
            Wire.beginTransmission(address);
            error = Wire.endTransmission();

            if (error == 0) {
                JsonObject dispositivo = dispositivos.createNestedObject();
                dispositivo["direccion"] = "0x" + String(address, HEX);
                dispositivo["decimal"] = address;

                // Identificar dispositivos comunes
                if (address == 0x3C || address == 0x3D) {
                    dispositivo["tipo"] = "Pantalla OLED";
                } else if (address == 0x68) {
                    dispositivo["tipo"] = "Reloj RTC (DS3231)";
                } else if (address == 0x76 || address == 0x77) {
                    dispositivo["tipo"] = "Sensor BME280/BMP280";
                } else if (address >= 0x20 && address <= 0x27) {
                    dispositivo["tipo"] = "Expansor GPIO (MCP23017)";
                } else if (address == 0x40) {
                    dispositivo["tipo"] = "Sensor HTU21D/PCA9685";
                } else {
                    dispositivo["tipo"] = "Desconocido";
                }
                direcciones.add("0x" + String(address, HEX));
                nDevices++;
            }
            delay(10);
        }

        // Mensaje según resultado
        if (nDevices == 0) {
            doc["mensaje"] = "No se encontró ningún dispositivo I2C en el bus.";
        } else {
            doc["mensaje"] = String("Se encontraron ") + nDevices + " dispositivo(s) I2C: ";
            // Agrega las direcciones encontradas al mensaje
            String lista = "";
            for (JsonVariant dir : direcciones) {
                lista += dir.as<String>() + " ";
            }
            doc["mensaje"] = doc["mensaje"].as<String>() + lista;
        }

        // Agregar información de red
        JsonObject red = doc.createNestedObject("red");
        if (WiFi.status() == WL_CONNECTED) {
            red["conectado"] = true;
            red["ssid"] = WiFi.SSID();
            red["ip_local"] = WiFi.localIP().toString();
            red["mac"] = WiFi.macAddress();
            red["rssi"] = WiFi.RSSI();
            red["estado"] = "Conectado";
        } else {
            red["conectado"] = false;
            red["estado"] = "Desconectado";
        }

        // Agregar información del AP
        JsonObject ap = doc.createNestedObject("access_point");
        ap["activo"] = WiFi.softAPgetStationNum() > 0;
        ap["ip"] = WiFi.softAPIP().toString();
        ap["estaciones_conectadas"] = WiFi.softAPgetStationNum();

        doc["dispositivos_encontrados"] = nDevices;
        doc["status"] = "Escaneo completado";

        String respuesta;
        serializeJson(doc, respuesta);
        server.send(200, "application/json", respuesta);
    });
  // Reinicio del dispositivo
  server.on("/api/reboot", HTTP_POST, []() {
  server.send(200, "application/json", "{\"success\":true,\"message\":\"Reiniciando dispositivo...\"}");
  delay(500);
  Interfaz::salirModoProgramacion();
  ESP.restart();
  });

// Control de prueba de pantalla - Muestra SOLO ID y Versión
server.on("/api/display/test", HTTP_POST, []() {
    // Encender la pantalla si está apagada
    if (!configLora.Pantalla) {
        configurarDisplay(true);
    }

    // Mostrar ID y Versión en la pantalla OLED por 5 segundos
    mostrarMensaje("ID: " + String(configLora.IDLora), 
                  "Ver: " + Version, 
                  5000);  // Mostrar por 5 segundos

    // No necesitamos apagar la pantalla aquí, mostrarMensaje() ya maneja el tiempo
    
    // Respuesta JSON
    DynamicJsonDocument response(128);
    response["success"] = true;
    response["id"] = configLora.IDLora;
    response["version"] = Version;
    
    String jsonResponse;
    serializeJson(response, jsonResponse);
    server.send(200, "application/json", jsonResponse);
});

// Manejador para rutas no encontradas
server.onNotFound([]() {
    server.send(404, "text/plain", "Ruta no encontrada");
});

server.begin();

while (modoProgramacion) {
    server.handleClient();
    delay(10);
}
  }

  //escanea las redes WiFi, detecta si están protegidas y con qué tipo de seguridad,
  String getEncryptionType(wifi_auth_mode_t encryptionType) {
  switch (encryptionType) {
  case WIFI_AUTH_OPEN: return "Abierta";
  case WIFI_AUTH_WEP: return "WEP";
  case WIFI_AUTH_WPA_PSK: return "WPA-PSK";
  case WIFI_AUTH_WPA2_PSK: return "WPA2-PSK";
  case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2-PSK";
  case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-Enterprise";
  default: return "Desconocido";
  }
  }