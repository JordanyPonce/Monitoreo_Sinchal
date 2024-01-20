#include <Arduino.h>
#include <WiFi.h>
#include <UniversalTelegramBot.h>
#include <WiFiClientSecure.h>
#include <OneWire.h>
#include <DallasTemperature.h>

/* Definición credenciales de la red WiFi */
const char* ssid = "NETLIFE-JOREMY";
const char* pass = "095508023925";
const char* token = "6529364790:AAHdc-YTV6TILXihjFMEQexZRxw5xqv3GqM"; 

// Restablecer el estado de los mensajes
bool mensajeEnviadoOptimo = false;
bool mensajeEnviado = false;

WiFiClientSecure client;
UniversalTelegramBot bot(token, client);

const int TDS_PIN = 39;
const int TEMPERATURE_PIN=2;
const int NUM_LECTURAS = 50;
bool precaucionVisible = false;
const int PH_PIN = 36;            //pH meter Analog output to Arduino Analog Input 0
#define Offset 0.00            //deviation compensate
#define samplingInterval 20
#define printInterval 800
#define ArrayLenth  40    //times of collection
int pHArray[ArrayLenth];   //Store the average value of the sensor feedback
int pHArrayIndex=0;
unsigned long lastMessageTime = 0;
const unsigned long messageInterval = 3600000;  // Intervalo de tiempo para enviar mensajes en milisegundos (1 hora en este caso)
OneWire oneWire(TEMPERATURE_PIN);
DallasTemperature sensors(&oneWire);

float obtenerPromedioTDS() {
  float promedio = 0;
  for (int i = 0; i < NUM_LECTURAS; i++) {
    promedio += analogRead(TDS_PIN);
    delay(10);
  }
  promedio /= NUM_LECTURAS;
  return promedio;
}

double obtenerPromedioPH(int* arr, int number){
  int i;
  int max,min;
  double ValorPH;
  long amount=0;
  if(number<=0){
    Serial.println("Error number for the array to avraging!/n");
    return 0;
  }
  if(number<5){   //less than 5, calculated directly statistics
    for(i=0;i<number;i++){
      amount+=arr[i];
    }
    ValorPH = amount/number;
    return ValorPH;
  }else{
    if(arr[0]<arr[1]){
      min = arr[0];max=arr[1];
    }
    else{
      min=arr[1];max=arr[0];
    }
    for(i=2;i<number;i++){
      if(arr[i]<min){
        amount+=min;        //arr<min
        min=arr[i];
      }else {
        if(arr[i]>max){
          amount+=max;    //arr>max
          max=arr[i];
        }else{
          amount+=arr[i]; //min<=arr<=max
        }
      }//if
    }//for
    ValorPH = (double)amount/(number-2);
  }//if
  return ValorPH;
}
void enviarMensajeAlerta(const String &mensaje) {
    String chatId = "1375625648";
    if (bot.sendMessage(chatId, mensaje, "")) {
        Serial.println("Mensaje de alerta enviado con éxito.");
    } else {
        Serial.println("Error al enviar el mensaje de alerta.");
    }
}
void setup() {
  Serial.begin(115200);
  Serial.println();
  sensors.begin();
  delay(30);
  Serial.println("pH meter experiment!");    //Test the serial monitor
  // Conecta a la red Wi-Fi
  Serial.print("Conectar a la red WiFi");
  Serial.print(ssid);
  WiFi.begin(ssid, pass);
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("Conexión Wi-Fi exitosa!. Dirección IP: ");
  Serial.println(WiFi.localIP());

  // Envia un mensaje al bot de Telegram indicando que el sistema inció
  String chatId = "1375625648"; 
  String message = "El módulo ESP32 se encuentra en línea  y en constante monitoreo 👷‍♂️✅.\n📌 Puede conocer los valores de TDS y PH mediante los siguientes comandos:\n/TDS y /PH"; 

  if (bot.sendMessage(chatId, message, "")) {
    Serial.println("Mensaje enviado con éxito.");
  } else {
    Serial.println("Error al enviar el mensaje.");
  }
}

void loop() {
  //TDS
  float messageTDS;
  messageTDS = obtenerPromedioTDS();
  //TEMPERATURE
  float temperature = sensors.getTempCByIndex(0);
  sensors.requestTemperatures();
  //PH
  static unsigned long samplingTime = millis();
  static unsigned long printTime = millis();
  static float pHValue,voltage;
  //TELEGRAM
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  String chatId = "1375625648";
  if (WiFi.status() != WL_CONNECTED) {
        enviarMensajeAlerta("¡ALERTA! La conexión WiFi está perdida.");
    }
  if (millis() - lastMessageTime > messageInterval) {
        enviarMensajeAlerta("¡ALERTA! El dispositivo ESP32 está en línea, pero no ha enviado mensajes recientemente.");
        lastMessageTime = millis();  // Actualiza el tiempo del último mensaje enviado
    }
  Serial.print("Temperatura del agua: ");
  Serial.print(temperature);
  Serial.println(" °C");

  if(millis() - samplingTime > samplingInterval)
{
    pHArray[pHArrayIndex++] = analogRead(PH_PIN);
    if(pHArrayIndex == ArrayLenth) pHArrayIndex = 0;
    
    voltage = obtenerPromedioPH(pHArray, ArrayLenth) * 5.0 / 1024 / 6 + 0.15;
    // Nueva ecuación para calcular el pHValue con la regresión lineal
    pHValue = double(32.567*(voltage)*(voltage) - 124.81*(voltage) + 123.57);
;

    samplingTime = millis();
}
  if(millis() - printTime > printInterval)   //Every 800 milliseconds, print a numerical, convert the state of the LED indicator
  {
    Serial.print("Voltage:");
        Serial.print(voltage,2);
        Serial.print("    pH value: ");
    Serial.println(pHValue,2);
        printTime=millis();
  } 
if (messageTDS>=1200){
  if (!mensajeEnviado) {
    mensajeEnviadoOptimo = false;
    String message = "⚠️⚠️⚠️ ¡ALERTA! ⚠️⚠️⚠️\nLos niveles de TDS se encuentran fuera del rango configurado, se debe cortar el suministro de agua.\n";
    if (bot.sendMessage(chatId, message, "")) {
      Serial.println("Mensaje enviado con éxito.");
    } 
    else {
      Serial.println("Error al enviar el mensaje.");
    }
  mensajeEnviado = true; // Marcar el mensaje como enviado
  }
}
else { // Cuando tdsValue es menor a 1200
  // Restablecer el estado de los mensajes
  mensajeEnviado = false;
  // Verificar si el mensaje de niveles óptimos no ha sido enviado
  if (!mensajeEnviadoOptimo) {
    String message = "Los niveles de TDS son óptimos para la potabilización. ✅✅✅\n";
    if (bot.sendMessage(chatId, message, "")) {
      Serial.println("Mensaje de niveles óptimos enviado con éxito.");
    } 
    else {
      Serial.println("Error al enviar el mensaje de niveles óptimos.");
    }
      mensajeEnviadoOptimo = true; // Marcar el mensaje de niveles óptimos como enviado
  }
}


for (int i = 0; i < numNewMessages; i++) {
  String chatId = bot.messages[i].chat_id;
  String text = bot.messages[i].text;
  // Procesa el mensaje
  if (text == "/TDS") {
    String message = "📌 Valor de TDS: " + String(messageTDS);
    bot.sendMessage(chatId, message, "");
  } 
  else if (text == "/PH") {
    String message = "📌 Valor de PH: " + String(pHValue);
    bot.sendMessage(chatId, message, "");
  }
}

delay(1000);

}
