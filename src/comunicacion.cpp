#include <Arduino.h>
#include <RCSwitch.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include "config.h"
#include "pantalla.h"
#include "heltec.h"
#include "RadioLib.h"
#include "comunicacion.h"
#include "eeprom.h"
#include "interfaz.h"

extern SX1262 lora;
//extern bool tieneInternet;
LoRaConfig configLora;
String ultimoComandoRecibido = "";
TwoWire I2CGral = TwoWire(1);

volatile bool rfSignalReceived = false;
unsigned long lastRFSignalTime = 0;
static RCSwitch mySwitch = RCSwitch();

unsigned long io1TimerStart = 0;
unsigned long io2TimerStart = 0;
bool io1TimerActive = false;
bool io2TimerActive = false;
const unsigned long IO_TIMEOUT_MS = 5UL * 60UL * 1000UL; // 5 minutos

// Definición de los objetos IRsend para cada reflector
IRsend irsend1(PIN_R1);
IRsend irsend2(PIN_R2);
IRsend irsend3(PIN_R3);
IRsend irsend4(PIN_R4);

// Array de punteros a los objetos IRsend para facilitar el acceso por ID
IRsend* irsenders[] = {&irsend1, &irsend2, &irsend3, &irsend4};
const int NUM_REFLECTORS = sizeof(irsenders) / sizeof(irsenders[0]);

// Códigos IR para las funciones de los reflectores
const uint64_t reflector_codes[] = {
    0xffa05f, 0xff20df, 0xff609f, 0xffe01f,
    0xff906f, 0xff10ef, 0xff50af, 0xffd02f,
    0xffb04f, 0xff30cf, 0xff708f, 0xfff00f,
    0xffa857, 0xff28d7, 0xff6897, 0xffe817,
    0xff9867, 0xff18e7, 0xff58a7, 0xffd827,
    0xff8877, 0xff08f7, 0xff48b7, 0xffc837
};
const int NUM_REFLECTOR_FUNCTIONS = sizeof(reflector_codes) / sizeof(reflector_codes[0]);

void habilitarPantalla() {
    configLora.Pantalla = true;
    configurarDisplay(true);
    imprimirSerial("Pantalla habilitada.", 'g');
    ManejoEEPROM::guardarTarjetaConfigEEPROM();
}

void deshabilitarPantalla() {
    configLora.Pantalla = false;
    configurarDisplay(false); 
    imprimirSerial("Pantalla deshabilitada.", 'y');
    ManejoEEPROM::guardarTarjetaConfigEEPROM();
}

void parpadearLEDStatus(int veces, int periodo_ms) {
    for (int i = 0; i < veces; ++i) {
        digitalWrite(LED_STATUS, HIGH);
        delay(periodo_ms / 2);
        digitalWrite(LED_STATUS, LOW);
        delay(periodo_ms / 2);
    }
}

void manejarCodigoRF(unsigned long value) {
    parpadearLEDStatus(3, 200);

    if (value == 7969128||value == 8046824|| value == 4495000|| value == 12984||value == 10209048) {
        digitalWrite(PIN_IO1, HIGH);
        digitalWrite(PIN_IO2, LOW);
        io1TimerStart = millis();
        io1TimerActive = true;
        io2TimerActive = false;
        io2TimerStart = 0;
        imprimirSerial("PIN_IO1 habilitado por RF", 'g');
    } else if (value == 7969124 || value == 10209044|| value == 12980 || value == 8046820 || value == 4494996) {
        digitalWrite(PIN_IO2, HIGH);
        digitalWrite(PIN_IO1, LOW);
        io2TimerStart = millis();
        io2TimerActive = true;
        io1TimerActive = false;
        io1TimerStart = 0;
        imprimirSerial("PIN_IO2 habilitado por RF", 'g');
    } else if (value == 7969122 || value == 8046818 || value == 10209042 || value == 12978 || value == 4494994) {
        digitalWrite(PIN_IO1, HIGH);
        digitalWrite(PIN_IO2, LOW);
        io1TimerStart = millis();
        io1TimerActive = true;
        io2TimerActive = false;
        io2TimerStart = 0;
        imprimirSerial("PIN_IO1 habilitado por RF", 'g');
    } else if (value == 7969121 || value == 4494993 || value == 12977 || value == 10209041 || value == 8046817) {
        digitalWrite(PIN_IO1, LOW);
        digitalWrite(PIN_IO2, LOW);
        io1TimerActive = false;
        io2TimerActive = false;
        io1TimerStart = 0;
        io2TimerStart = 0;
        imprimirSerial("PIN_IO1 y PIN_IO2 deshabilitados por RF", 'y');
    }
}

void manejarComandoRGB(const String& color, String& respuesta) {
    // Configura los pines como OUTPUT por robustez
    pinMode(PIN_IO4, OUTPUT); // ROJO
    pinMode(PIN_IO5, OUTPUT); // VERDE
    pinMode(PIN_IO6, OUTPUT); // AZUL

    // Valores PWM para cada canal (0-255)
    int valR = 0, valG = 0, valB = 0;

    String colorMayus = color;
    colorMayus.toUpperCase();

    if (colorMayus == "ROJO") {
        valR = 255;
        respuesta = "RGB: ROJO encendido";
    } else if (colorMayus == "VERDE") {
        valG = 255;
        respuesta = "RGB: VERDE encendido";
    } else if (colorMayus == "AZUL") {
        valB = 255;
        respuesta = "RGB: AZUL encendido";
    } else if (colorMayus == "BLANCO") {
        valR = 255; valG = 255; valB = 255;
        respuesta = "RGB: BLANCO encendido";
    } else if (colorMayus == "CIAN") {
        valG = 255; valB = 255;
        respuesta = "RGB: CIAN (VERDE+AZUL) encendido";
    } else if (colorMayus == "MORADO" || colorMayus == "VIOLETA" || colorMayus == "PURPURA") {
        valR = 128; valB = 255;
        respuesta = "RGB: MORADO (ROJO+AZUL) encendido";
    } else if (colorMayus == "NARANJA") {
        valR = 255; valG = 100;
        respuesta = "RGB: NARANJA (ROJO+VERDE) encendido";
    } else if (colorMayus == "ROSA") {
        valR = 255; valG = 50; valB = 150;
        respuesta = "RGB: ROSA (ROJO+AZUL+un poco de VERDE) encendido";
    } else if (colorMayus == "AMARILLO") {
        valR = 255; valG = 150;
        respuesta = "RGB: AMARILLO (ROJO+VERDE) encendido";
    } else if (colorMayus == "OCEAN") {
        valG = 100 ;valB = 255;
        respuesta = "RGB: OCEAN (VERDE+AZUL) encendido";
    } else if (colorMayus == "OFF" || colorMayus == "APAGADO") {
        respuesta = "RGB: Todos los colores apagados";
    } else {
        respuesta = "RGB: Color no reconocido: " + color;
    }

    // Aplica los valores PWM
    analogWrite(PIN_IO4, valR); // ROJO
    analogWrite(PIN_IO5, valG); // VERDE
    analogWrite(PIN_IO6, valB); // AZUL

    imprimirSerial(respuesta, 'c');
}


void ManejoComunicacion::initRFReceiver() {
    mySwitch.enableReceive(RECEPTOR_RF); 
    imprimirSerial("Receptor RF RX500 inicializado con rc-switch en pin " + String(RECEPTOR_RF), 'g');
}

void ManejoComunicacion::leerRFReceiver() {
    static unsigned long ultimoValor = 0; 
    static unsigned long ultimoTiempo = 0; 
    const unsigned long intervalo = 200;   

    if (mySwitch.available()) {
        unsigned long value = mySwitch.getReceivedValue();
        unsigned long ahora = millis();

        if (value == 0) {
            Serial.println("Codigo desconocido recibido (demasiado corto o ruido)");
        } else {
            if (value != ultimoValor || (ahora - ultimoTiempo) >= intervalo) {
                Serial.println("Codigo RF recibido: " + String(value));
                manejarCodigoRF(value);
                ultimoValor = value;
                ultimoTiempo = ahora;
            }
        }
        mySwitch.resetAvailable();
    }
}

void inicializarPinesIR() {
    pinMode(PIN_R1, OUTPUT);
    pinMode(PIN_R2, OUTPUT);
    pinMode(PIN_R3, OUTPUT);
    pinMode(PIN_R4, OUTPUT);
}

void enviarComandoReflector(int reflectorID, int functionIndex, String &respuesta) {
    // Validación de rango
    if (reflectorID < 0 || reflectorID > NUM_REFLECTORS) {
        respuesta = "ERR: ID de reflector fuera de rango. Usar 0 (todos) o 1-" + String(NUM_REFLECTORS);
        imprimirSerial(respuesta, 'r');
        return;
    }
    if (functionIndex < 0 || functionIndex >= NUM_REFLECTOR_FUNCTIONS) {
        respuesta = "ERR: Indice de funcion de reflector fuera de rango (0-" + String(NUM_REFLECTOR_FUNCTIONS - 1) + ")";
        imprimirSerial(respuesta, 'r');
        return;
    }

    String destino = String(configLora.IDLora);
    char red = 'L';
    String comando = "R" + String(reflectorID) + (functionIndex + 1 < 10 ? "0" : "") + String(functionIndex + 1);
    String valorAleatorio = String(random(10, 99));
    String comandoCompleto = destino + "@" + red + "@" + comando + "@" + valorAleatorio;

    imprimirSerial("Enviando comando reflector a procesarComando: " + comandoCompleto, 'c');
    ManejoComunicacion::procesarComando(comandoCompleto, respuesta);
}

void ManejoComunicacion::inicializar() {
    Serial.begin(9600);
    delay(1000);
    initI2C();
    initUART();
    initRFReceiver();
    // Inicializar los pines IR como OUTPUT
    inicializarPinesIR();

    // Inicializar los emisores IR
    for (int i = 0; i < NUM_REFLECTORS; ++i) {
        irsenders[i]->begin();
    }
    imprimirSerial("Emisores IR inicializados.", 'g');

    if (lora.begin() != RADIOLIB_ERR_NONE) {
        Serial.println("LoRa init failed!");
        while (true);
    }
}

void ManejoComunicacion::initUART() {
    if (configLora.UART) {
        Serial2.begin(9600, SERIAL_8N1, UART_RX, UART_TX); 
        imprimirSerial("UART inicializado correctamente.", 'g');
    } else {
        imprimirSerial("UART inhabilitado", 'y');
    }
}

String ManejoComunicacion::leerVecinal() {
    if (configLora.UART) {
        String comandoVecinal = "";
        if (Serial2.available()) {
            comandoVecinal = Serial2.readStringUntil('\n');
            if (comandoVecinal != "") {
                imprimirSerial("Comando recibido en Serial2: " + comandoVecinal, 'y');
                return comandoVecinal;
            } else {
                return "";
            }
        }
        return "";
    } else {
        imprimirSerial("Comunicacion UART deshabilitada", 'y');
        return "";
    }
}

void ManejoComunicacion::escribirVecinal(String envioVecinal) {
    if (configLora.UART) {
        Serial2.println(envioVecinal);
        imprimirSerial("Comando enviado a la Alarma Vecinal:\n   -> " + envioVecinal, 'g');
    }
}

void ManejoComunicacion::initI2C() {
    if (configLora.I2C) {
        Wire.begin(I2C_SDA, I2C_SCL);
        imprimirSerial("I2C inicializado correctamente.", 'g');
    } else {
        imprimirSerial("I2C inhabilitado", 'y');
    }
}

int ManejoComunicacion::scannerI2C() {
    byte error, address;
    int nDevices = 0;
    imprimirSerial("Escaneando puerto I2C...\n", 'y');

    for (address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();

        if (error == 0) {
            String mensaje = "I2C encontrado en la direccion 0x";
            if (address < 16)
                mensaje += "0";
            mensaje += String(address, HEX);
            mensaje += " !";
            imprimirSerial(mensaje, 'c');
            nDevices++;
        } else if (error == 4) {
            String mensaje = "Error desconocido en la direccion 0x";
            if (address < 16)
                mensaje += "0";
            mensaje += String(address, HEX);
            imprimirSerial(mensaje, 'r');
        }
        delay(5);
    }

    if (nDevices == 0) {
        imprimirSerial("Ningun dispositivo I2C encontrado", 'r');
    } else {
        imprimirSerial("\nEscaneo completado!", 'g');
    }
    return nDevices;
}

String ManejoComunicacion::leerSerial() {
    String comandoVecinal = "";
    if (Serial.available()) {
        comandoVecinal = Serial.readStringUntil('\n');
        if (comandoVecinal != "") {
            imprimirSerial("Comando recibido en Serial: " + comandoVecinal, 'y');
            return comandoVecinal;
        }
    }
    return "";
}

void ManejoComunicacion::envioMsjLoRa(String comandoLoRa) {
    // comandoLoRa debe tener el formato: DEST@L@mensaje@valorAleatorio@
    // Extraer destino
    String destino = comandoLoRa.substring(0, 3);
    String msgID = String(configLora.IDLora) + "-" + String(millis());
    char paquete[256];
    snprintf(paquete, sizeof(paquete), "ORIG:%s|DEST:%s|MSG:%s|HOP:%s|CANAL:%d|ID:%s",
        configLora.IDLora, destino.c_str(), comandoLoRa.c_str(), destino.c_str(), configLora.Canal, msgID.c_str());
    imprimirSerial("Enviando por LoRa: " + String(paquete), 'c');
    int resultado = lora.transmit(paquete);
    if (resultado == RADIOLIB_ERR_NONE) {
        imprimirSerial("Respuesta enviada correctamente por LoRa.", 'g');
    } else {
        imprimirSerial("Error al enviar respuesta por LoRa: " + String(resultado), 'r');
    }
    lora.startReceive();
}

void ManejoComunicacion::enviarRespuestaRemota(const String& idDestino, char red, const String& mensaje, const String& valorAleatorio) {
    if (red == 'L') {
        // Estructura: ID@L@mensaje@valorAleatorio
        String respuestaCmd = idDestino + "@L@" + mensaje + "@" + valorAleatorio + "@";
        ManejoComunicacion::envioMsjLoRa(respuestaCmd);
        imprimirSerial("Respuesta enviada por LoRa a " + idDestino + ": " + mensaje, 'g');
    } else if (red == 'V') {
        // Estructura: mensaje>valorAleatorio
        String respuestaCmd = mensaje + ">" + valorAleatorio;
        ManejoComunicacion::escribirVecinal(respuestaCmd);
        imprimirSerial("Respuesta enviada por Vecinal a " + idDestino + ": " + mensaje, 'g');
    } else {
        imprimirSerial("Red desconocida para enviar respuesta: " + String(red), 'r');
    }
}

void ManejoComunicacion::procesarComando(const String &comandoRecibido, String &respuesta) {
    esp_task_wdt_reset();

    if (comandoRecibido.isEmpty()) { 
        respuesta = "Comando vacio";
        imprimirSerial(respuesta, 'r');
        return;
    } else if (comandoRecibido == ultimoComandoRecibido) {
        imprimirSerial("Comando repetido, no se ejecutara una nueva accion\n", 'c');
        respuesta = "Comando repetido";
        return;
    }
    ultimoComandoRecibido = comandoRecibido;
    
    int primerSep = comandoRecibido.indexOf('@');
    int segundoSep = comandoRecibido.indexOf('@', primerSep + 1);
    int tercerSep = comandoRecibido.indexOf('@', segundoSep + 1);

    if (primerSep == -1 || segundoSep == -1 || tercerSep == -1 ||
        primerSep == 0 || segundoSep == primerSep + 1 || tercerSep == segundoSep + 1) {
        respuesta = "ERR: Formato de comando invalido. Usa: ID@R@CMD@##";
        imprimirSerial(respuesta, 'r');
        return;
    }

    String comandoDestinoID = comandoRecibido.substring(0, primerSep); 
    char red = comandoRecibido.charAt(primerSep + 1);
    String comandoProcesar = comandoRecibido.substring(segundoSep + 1, tercerSep); 
    String valorAleatorio = comandoRecibido.substring(tercerSep + 1); 

    String prefix1 = "";
    String accion = "";

    imprimirSerial("\nComando recibido en procesado de comando: " + comandoRecibido);
    imprimirSerial("ID de Destino del Comando -> " + comandoDestinoID);
    imprimirSerial("Red -> " + String(red));
    imprimirSerial("Comando a Procesar -> " + comandoProcesar);
    imprimirSerial("Valor Aleatorio -> " + valorAleatorio);

    if (strcmp(comandoDestinoID.c_str(), configLora.IDLora) == 0 || strcmp(comandoDestinoID.c_str(), "000") == 0) {
        imprimirSerial("Comando dirigido a este nodo (" + String(configLora.IDLora) + ") o es broadcast (000).", 'g');

        if (red == 'L') { 
            if (comandoProcesar.startsWith("ID")) {
                accion = comandoProcesar.substring(2, 3);
                if (accion == "L") {
                    respuesta = "ID del nodo: " + String(configLora.IDLora);
                    imprimirSerial("El ID del nodo es -> " + String(configLora.IDLora), 'y');
                } else if (accion == "C") {
                    if (comandoProcesar.length() >= 7) { 
                        prefix1 = comandoProcesar.substring(4, 7); 
                        imprimirSerial("Cambiando el ID del nodo a -> " + prefix1, 'c');
                        strncpy(configLora.IDLora, prefix1.c_str(), sizeof(configLora.IDLora) - 1);
                        configLora.IDLora[sizeof(configLora.IDLora) - 1] = '\0';
                        ManejoEEPROM::guardarTarjetaConfigEEPROM();
                        respuesta = "ID cambiado a: " + prefix1;
                    } else {
                        imprimirSerial("Formato de comando ID inválido para cambio. Esperado: IDC:XXX", 'r');
                        respuesta = "Formato ID invalido";
                    }
                }
                ManejoComunicacion::enviarRespuestaRemota("000", red, respuesta, valorAleatorio);
                return;
            } else if (comandoProcesar.startsWith("CH")) {
                accion = comandoProcesar.substring(2, 3);
                if (accion == "L") {
                    respuesta = "Canal actual: " + String(configLora.Canal);
                    imprimirSerial("El canal del nodo es -> " + String(configLora.Canal), 'y');
                } else if (accion == "C") {
                    int idxMayor = comandoProcesar.indexOf('>');
                    if (idxMayor != -1 && idxMayor + 1 < comandoProcesar.length()) {
                        prefix1 = comandoProcesar.substring(idxMayor + 1);
                        imprimirSerial("Cambiando el canal del nodo a -> " + prefix1, 'c');
                        configLora.Canal = prefix1.toInt();
                        if (configLora.Canal >= 0 && configLora.Canal < (sizeof(canales) / sizeof(canales[0]))) { 
                            ManejoEEPROM::guardarTarjetaConfigEEPROM();
                            lora.begin(canales[configLora.Canal]); 
                            respuesta = "Canal cambiado a: " + prefix1;
                            ManejoComunicacion::enviarRespuestaRemota("000", red, respuesta, valorAleatorio);
                            esp_restart(); 
                        } else {
                            respuesta = "Canal fuera de rango";
                            imprimirSerial("Canal especificado fuera de rango: " + prefix1, 'r');
                            ManejoComunicacion::enviarRespuestaRemota("000", red, respuesta, valorAleatorio);
                        }
                    } else {
                        imprimirSerial("Formato de comando de canal inválido", 'r');
                        respuesta = "Formato de comando de canal inválido";
                        ManejoComunicacion::enviarRespuestaRemota("000", red, respuesta, valorAleatorio);
                    }
                }
                ManejoComunicacion::enviarRespuestaRemota("000", red, respuesta, valorAleatorio);
                return;
            } else if(comandoProcesar.startsWith("RGB>")){
                String color = comandoProcesar.substring(4); // Extrae el color después de '>'
                manejarComandoRGB(color, respuesta);
                ManejoComunicacion::enviarRespuestaRemota("000", red, respuesta, valorAleatorio);
                return;
            } else if (comandoProcesar.startsWith("SCR")) {
                accion = comandoProcesar.substring(4, 5);
                if (accion == "L") {
                    respuesta = "Pantalla " + String(configLora.Pantalla ? "activada" : "desactivada");
                    String mensaje = "La pantalla de la LoRa se encuentra ";
                    mensaje += configLora.Pantalla ? "Activada" : "Desactivada";
                    imprimirSerial(mensaje);
                } else if (accion == "0") {
                    imprimirSerial("Desactivando la pantalla");
                    configurarDisplay(false); 
                    respuesta = "Pantalla deshabilitada";
                } else if (accion == "1") {
                    imprimirSerial("Activando la pantalla");
                    configurarDisplay(true);
                    respuesta = "Pantalla habilitada";
                }
                ManejoComunicacion::enviarRespuestaRemota("000", red, respuesta, valorAleatorio);
                return;
            } else if(comandoProcesar.startsWith("R") && comandoProcesar.length() == 4) {
                int reflectorID = comandoProcesar.substring(1, 2).toInt(); // 0=Todos, 1-4
                int funcion = comandoProcesar.substring(2, 4).toInt();     // 01-24

                imprimirSerial("Comando de Reflector recibido. ID: " + String(reflectorID) + ", Funcion: " + String(funcion), 'g');
                
                // Validar ID del reflector
                if (reflectorID < 0 || reflectorID > NUM_REFLECTORS) {
                    respuesta = "ERR: ID de reflector fuera de rango. Usar 0 (todos) o 1-" + String(NUM_REFLECTORS);
                    imprimirSerial(respuesta, 'r');
                    ManejoComunicacion::enviarRespuestaRemota("000", red, respuesta, valorAleatorio);
                    return;
                }

                // Validar índice de función
                if (funcion < 1 || funcion > NUM_REFLECTOR_FUNCTIONS) {
                    respuesta = "ERR: Indice de funcion de reflector fuera de rango (01-" + String(NUM_REFLECTOR_FUNCTIONS) + ").";
                    imprimirSerial(respuesta, 'r');
                    ManejoComunicacion::enviarRespuestaRemota("000", red, respuesta, valorAleatorio);
                    return;
                }

                uint64_t irCode = reflector_codes[funcion - 1];

                if (reflectorID == 0) { // Todos los reflectores
                    for (int i = 0; i < NUM_REFLECTORS; ++i) {
                        irsenders[i]->sendNEC(irCode, 32);
                        delay(50);
                        imprimirSerial("Enviando codigo IR " + String(irCode, HEX) + " a reflector " + String(i + 1), 'g');
                    }
                    respuesta = "Comando IR " + String(irCode, HEX) + " enviado a TODOS los reflectores.";
                } else {
                    irsenders[reflectorID - 1]->sendNEC(irCode, 32);
                    respuesta = "Comando IR " + String(irCode, HEX) + " enviado a reflector " + String(reflectorID);
                    imprimirSerial(respuesta, 'g');
                }
                ManejoComunicacion::enviarRespuestaRemota("000", red, respuesta, valorAleatorio);
                return;
            } else if (comandoProcesar.startsWith("OUT>")) {
                int primerMayor = comandoProcesar.indexOf('>', 4); // Busca el segundo '>'
                if (primerMayor == -1) {
                    respuesta = "ERR: Formato OUT invalido. Usa: OUT>{1/2}>{0/1/L}";
                    imprimirSerial(respuesta, 'r');
                    ManejoComunicacion::enviarRespuestaRemota("000", red, respuesta, valorAleatorio);
                    return;
                }
                String salidaStr = comandoProcesar.substring(4, primerMayor); // '1' o '2'
                String accionStr = comandoProcesar.substring(primerMayor + 1); // '0', '1' o 'L'

                int pin = -1;
                bool isIO1 = false, isIO2 = false;

                if (salidaStr == "1") { pin = PIN_IO1; isIO1 = true; }
                else if (salidaStr == "2") { pin = PIN_IO2; isIO2 = true; }
                else {
                    respuesta = "ERR: Salida no reconocida: " + salidaStr + ". Usa 1 o 2.";
                    imprimirSerial(respuesta, 'r');
                    ManejoComunicacion::enviarRespuestaRemota("000", red, respuesta, valorAleatorio);
                    return;
                }

                if (accionStr == "1") {
                    pinMode(pin, OUTPUT);
                    digitalWrite(pin, HIGH);
                    respuesta = "Pin IO" + salidaStr + " habilitado (HIGH)";
                    imprimirSerial(respuesta, 'g');
                    if (isIO1) {
                        io1TimerStart = millis();
                        io1TimerActive = true;
                    } else if (isIO2) {
                        io2TimerStart = millis();
                        io2TimerActive = true;
                    }
                } else if (accionStr == "0") {
                    pinMode(pin, OUTPUT);
                    digitalWrite(pin, LOW);
                    respuesta = "Pin IO" + salidaStr + " deshabilitado (LOW)";
                    imprimirSerial(respuesta, 'y');
                    if (isIO1) {
                        io1TimerActive = false;
                        io1TimerStart = 0;
                    } else if (isIO2) {
                        io2TimerActive = false;
                        io2TimerStart = 0;
                    }
                } else if (accionStr == "L") {
                    pinMode(pin, INPUT);
                    int estado = digitalRead(pin);
                    respuesta = "Pin IO" + salidaStr + " estado: " + String(estado ? "HABILITADO (HIGH)" : "DESHABILITADO (LOW)");
                    imprimirSerial(respuesta, 'c');
                } else {
                    respuesta = "ERR: Acción no reconocida: " + accionStr + ". Usa 0, 1 o L.";
                    imprimirSerial(respuesta, 'r');
                }
                ManejoComunicacion::enviarRespuestaRemota("000", red, respuesta, valorAleatorio);
                return;
            } else if(comandoProcesar == "INFO") {
                String estado = "ID:" + String(configLora.IDLora) +
                                ",CANAL:" + String(configLora.Canal) + 
                                ",UART:" + String(configLora.UART ? "ON" : "OFF") +
                                ",I2C:" + String(configLora.I2C ? "ON" : "OFF") +
                                ",DEBUG:" + String(configLora.DEBUG ? "ON" : "OFF")+
                                ",PANTALLA:" + String(configLora.Pantalla ? "ON" : "OFF") +
                                ",VERSION:" + Version;

                imprimirSerial("Enviando estado de la tarjeta: " + estado, 'c');
                ManejoComunicacion::enviarRespuestaRemota("000", red, estado, valorAleatorio);
                respuesta = "Estado enviado a TODOS (broadcast): " + estado;
                return;
            } else if (comandoProcesar.startsWith("I2C")) {
                accion = comandoProcesar.substring(4, 5);
                if (accion == "L") {
                    respuesta = "I2C " + String(configLora.I2C ? "activada" : "desactivada");
                    String mensaje = "La comunicacion I2C se encuentra ";
                    mensaje += configLora.I2C ? "Activada" : "Desactivada";
                    imprimirSerial(mensaje);
                } else if (accion == "0") {
                    imprimirSerial("Desactivando la comunicacion I2C", 'y');
                    configLora.I2C = false;
                    ManejoEEPROM::guardarTarjetaConfigEEPROM();
                    respuesta = "I2C desactivada";
                } else if (accion == "1") {
                    imprimirSerial("Activando la comunicacion I2C");
                    configLora.I2C = true;
                    ManejoEEPROM::guardarTarjetaConfigEEPROM();
                    ManejoComunicacion::initI2C(); 
                    respuesta = "I2C activada";
                }
                ManejoComunicacion::enviarRespuestaRemota("000", red, respuesta, valorAleatorio);
                return;
            } else if (comandoProcesar.startsWith("SCANI2C")) {
                if (configLora.I2C) {
                    imprimirSerial("Escaneando el puerto I2C en busca de dispositivos...", 'y');
                    int encontrados = ManejoComunicacion::scannerI2C();
                    imprimirSerial("Se encontraron " + String(encontrados) + " direcciones I2C");
                    respuesta = "Se encontraron " + String(encontrados) + " direcciones I2C";
                } else {
                    imprimirSerial("La comunicacion I2C esta desactivada, no se puede escanear", 'r');
                    respuesta = "I2C desactivada, no se puede escanear";
                }
                ManejoComunicacion::enviarRespuestaRemota("000", red, respuesta, valorAleatorio);
                return;
            } else if (comandoProcesar.startsWith("UART")) {
                accion = comandoProcesar.substring(5, 6);
                if (accion == "L") {
                    respuesta = "UART " + String(configLora.UART ? "activada" : "desactivada");
                    String mensaje = "La comunicacion UART se encuentra ";
                    mensaje += configLora.UART ? "Activada" : "Desactivada";
                    imprimirSerial(mensaje);
                } else if (accion == "0") {
                    imprimirSerial("Desactivando la comunicacion UART");
                    configLora.UART = false;
                    ManejoEEPROM::guardarTarjetaConfigEEPROM();
                    respuesta = "UART desactivada";
                } else if (accion == "1") {
                    imprimirSerial("Activando la comunicacion UART");
                    configLora.UART = true;
                    ManejoEEPROM::guardarTarjetaConfigEEPROM();
                    ManejoComunicacion::initUART(); 
                    respuesta = "UART activada";
                }
                ManejoComunicacion::enviarRespuestaRemota("000", red, respuesta, valorAleatorio);
                return;
            }else if (comandoProcesar.startsWith("FORMAT")) {
    imprimirSerial("Reiniciando la tarjeta LoRa...");
    respuesta = "Reiniciando la tarjeta LoRa...";
    ManejoComunicacion::enviarRespuestaRemota("000", red, respuesta, valorAleatorio);

    // Espera 1 segundo pero reseteando el watchdog
    for (int i = 0; i < 10; ++i) {
        delay(100);
        esp_task_wdt_reset();
    }
    ManejoEEPROM::borrarTarjetaConfigEEPROM(); 
    esp_restart();
    return;
} else if(comandoProcesar.startsWith("RESET")){
    imprimirSerial("Reiniciando el sistema...");
    respuesta = "Reiniciando el sistema...";
    ManejoComunicacion::enviarRespuestaRemota("000", red, respuesta, valorAleatorio);

    for (int i = 0; i < 10; ++i) {
        delay(100);
        esp_task_wdt_reset();
    }
    esp_restart();
    return;
} else if (comandoProcesar.startsWith("MPROG")) {
                imprimirSerial("Entrando a modo programacion a traves de comando...");
                if (!modoProgramacion) { 
                    Interfaz::entrarModoProgramacion(); 
                }
                respuesta = "Entrando a modo programación";
                ManejoComunicacion::enviarRespuestaRemota("000", red, respuesta, valorAleatorio);
                return;
            } else {
                respuesta = "ERR: Comando LoRa desconocido";
                imprimirSerial("Comando LoRa desconocido: " + comandoProcesar, 'r');
                ManejoComunicacion::enviarRespuestaRemota("000", red, respuesta, valorAleatorio);
                return;
            }
        } else if (red == 'V') {
    imprimirSerial("Comando para red Vecinal (UART).", 'g');
    String comandoVecinal = comandoProcesar + ">" + valorAleatorio;
    ManejoComunicacion::escribirVecinal(comandoVecinal); 
    respuesta = "Comando enviado a vecinal: " + comandoVecinal;
    // Envía la respuesta por UART a todos (broadcast)
    ManejoComunicacion::enviarRespuestaRemota("000", red, respuesta, valorAleatorio);
    return;
}else {
            respuesta = "ERR: Red desconocida";
            imprimirSerial("Red desconocida: " + String(red), 'r');
        }
    } else {
        imprimirSerial("Comando ignorado, no es para este ID (" + String(configLora.IDLora) + ") ni para 000. Destino recibido: " + comandoDestinoID, 'y');
        respuesta = "IGNORADO: " + comandoDestinoID;
    }
}