#ifndef COMUNICACION_H
  #define COMUNICACION_H
  #include "config.h"

  struct Activacion {
    int pin;                   // Pin a controlar (IO2 o IO3)
    int tiempoDesactivacion;   // Tiempo en milisegundos
    bool activo;               // Estado actual
    TimerHandle_t timer;       // Timer asociado
};

static Activacion activaciones[2] = {
    {PIN_IO1, 0, false, NULL},
    {PIN_IO2, 0, false, NULL}
};

  class ManejoComunicacion {
    public:
      static void initRFReceiver();
      static void leerRFReceiver();
      static void inicializar();
      static String leerSerial();
      static String leerVecinal();
      static void initI2C();
      static void initUART();
      static int scannerI2C();
      static void escribirVecinal(String envioVecinal);
      static void enviarRespuestaRemota(const String& idDestino, char red, const String& mensaje, const String& valorAleatorio);
      static void procesarComando(const String &comandoRecibido, String &respuesta);
      static void envioMsjLoRa(String comandoLoRa);
  };

extern unsigned long io1TimerStart;
extern unsigned long io2TimerStart;
extern bool io1TimerActive;
extern bool io2TimerActive;
extern const unsigned long IO_TIMEOUT_MS;
#endif