#include <Arduino.h>
#include "config.h"
#include "eeprom.h"

extern LoRaConfig configLora;
Preferences preferences;

// Frecuencias para la banda de 915 MHz
float canales[9] = {
  915.0, 915.2, 915.4, 915.6, 915.8, 916.0, 916.2, 916.4, 916.6
};