/* Meepi - Porta remédios inteligente
   Feito por: Marina Dardenne
   Trabalho de conclusão de curso - IFSC
*/
/**********LIBRARIES**************/
#include <Adafruit_NeoPixel.h>
#include <PubSubClient.h>
#include <RotaryEncoder.h>
#include <WiFi.h>
#include <RTClib.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include "Esp32MQTTClient.h"
#include <EEPROM.h>

/*************LEDS*****************/
#define ledCount1 15
#define ledCount2 7
#define stripPin1 32
#define stripPin2 33

Adafruit_NeoPixel strip1(ledCount1, stripPin1, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip2(ledCount2, stripPin2, NEO_GRB + NEO_KHZ800);
// Argument 1 = Number of pixels in NeoPixel strip
// Argument 2 = Arduino pin number (most are valid)
// Argument 3 = Pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)

/***********ENCODER****************/
//#define DT 19
//#define CLK 18
//#define ROTARYSTEPS 1
//#define ROTARYMIN -8
//#define ROTARYMAX 8

// Setup a RotaryEncoder with 4 steps per latch for the 2 signal input pins:
// RotaryEncoder encoder(DT, CLK, RotaryEncoder::LatchMode::FOUR3);

// Setup a RotaryEncoder with 2 steps per latch for the 2 signal input pins:
//RotaryEncoder encoder(DT, CLK, RotaryEncoder::LatchMode::TWO03);

// Last known rotary position.
//int lastPos = -1;
/************EEPROM**************/
#define EEPROM_SIZE 30 // define the number of bytes you want to access

/**************RTC***************/
RTC_DS1307 rtc;

/****BUTTONS,BUZZER AND VM*****/
#define okmed 14
#define sync 12

volatile byte syncbutton = 0;
volatile int okbutton = 0;
String fulldate;

// Filtre anti-rebond (debouncer)
#define DEBOUNCE_TIME 250
volatile uint32_t DebounceTimer = 0;

#define motor 5
#define buzzer 4

const char* ssid = "WRITE-HERE-YOUR-NETWORK-NAME"; //Name
const char* password = "WRITE-HERE-YOUR-PASSWORD";      // Senha

/****************IBM Cloud Conexion - ESP32*********************/
#define ORG "" //ID de organização - Organization ID
#define DEVICE_TYPE "" //Insira o nome do componente
#define DEVICE_ID "" //Insira o ID
#define TOKEN "API TOKEN"// Insira o Token
/*Comunicação IOT – Não altere essa parte da programação*/
char server[] = ORG ".messaging.internetofthings.ibmcloud.com";
char authMethod[] = "INSERT HERE THE API KEY NAME";
char token[] = TOKEN;
char clientId[] = "a:WRITE ORGANIZATION NAME HERE:AND HERE THE DEVICE ID";
const char pubGetInfo[] = "iot-2/type/ESP32/id/meepi001/evt/getinfos/fmt/json"; // publishing in this topic
const char pubSendInfos[] = "iot-2/type/ESP32/id/meepi001/evt/sendinfos/fmt/json"; // publishing in this topic
const char lisMedInfo[] = "iot-2/type/ESP32/id/meepi001/evt/medinfo/fmt/json"; //listening in this topic
/**********************VARIABLES***************************/
int medFreq1 = 0;
int medFreq2 = 0;
int medFreq3 = 0;
int medFreq4 = 0;
int medFreq5 = 0;
int medFreq6 = 0;
int medFreq7 = 0;

int medPlace1[6];
int medPlace2[6];
int medPlace3[6];
int medPlace4[6];
int medPlace5[6];
int medPlace6[6];
int medPlace7[6];

int medTime1[2];
int medTime2[2];
int medTime3[2];
int medTime4[2];
int medTime5[2];
int medTime6[2];
int medTime7[2];

int medDate1[3];
int medDate2[3];
int medDate3[3];
int medDate4[3];
int medDate5[3];
int medDate6[3];
int medDate7[3];

bool med1 = false;
bool med2 = false;
bool med3 = false;
bool med4 = false;
bool med5 = false;
bool med6 = false;
bool med7 = false;

int medalert = 0;
int off = 0;
int o = 0;

unsigned long previousMillis = 0;
long interval = 300000;

bool publishStatus = true;


StaticJsonDocument<512> statusdoc;
char output2[512];
/*********************************************************/

void callback(char* topic, byte* payload, unsigned int length);
void wifiConnect();
void mqttConnect();
void mqttReconnect();
void colorWipe(uint32_t color, int wait);
void colorWipe2(uint32_t color, int wait);
void alertRoutine();
void okTimer();

WiFiClient wifiClient;
PubSubClient client(server, 1883, callback, wifiClient);


void IRAM_ATTR buttonpressed1() {
  if ( millis() - DEBOUNCE_TIME  >= DebounceTimer ) {
    DebounceTimer = millis();
    syncbutton = 1;
    Serial.println("Entrei na interrupcao - SYNC");
  }
}

void IRAM_ATTR buttonpressed2() {
  if ( millis() - DEBOUNCE_TIME  >= DebounceTimer ) {
    DebounceTimer = millis();
    okbutton = 1 + okbutton;
    Serial.println(okbutton);
    if (okbutton == 1) {
      okTimer();
    }
  }
}

void setup() {

  Serial.begin(115200);
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  if (! rtc.isrunning()) {
    Serial.println("RTC is NOT running!");
    // following line sets the RTC to the date & time this sketch was compiled
    //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    //rtc.adjust(DateTime(2021, 3, 31, 10, 02, 00));
  }

  //Buttons
  pinMode(okmed, INPUT);
  pinMode(sync, INPUT);
  attachInterrupt(sync, buttonpressed1, RISING);
  attachInterrupt(okmed, buttonpressed2, RISING);

  //Others
  pinMode(buzzer, OUTPUT);
  pinMode(motor, OUTPUT);

  //Strips
  strip1.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
  strip1.show();            // Turn OFF all pixels ASAP
  strip1.setBrightness(50); // Set BRIGHTNESS to about 1/5 (max = 255)

  strip2.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
  strip2.show();            // Turn OFF all pixels ASAP
  strip2.setBrightness(50); // Set BRIGHTNESS to about 1/5 (max = 255)

  //Internet and IBM conexion
  wifiConnect();                     // Create void wifiConnect
  mqttConnect();                     // Create o void mqttConnect

  client.subscribe(lisMedInfo, 1);

}

void loop() {
  if (!client.loop())
  {
    if (WiFi.status() != WL_CONNECTED) {
      colorWipe(strip1.Color(250, 0, 0), 50);
      WiFi.begin(ssid, password);
      for (int i = 0; i < 5; i++) {
        if (WiFi.status() != WL_CONNECTED)
        {
          delay(500);                      // Aguarda 500 milissegundos
          Serial.print(".");
        } else {
          break;
        }
      }
      if (WiFi.status() == WL_CONNECTED)
      {
        Serial.print("Wi-Fi conectado, Endereço de IP: ");
        Serial.println(WiFi.localIP());     // Indica o endereço de IP
        mqttReconnect();
        colorWipe(strip1.Color(0, 0, 0), 50);
      }
      else
      {
        Serial.println("Wi-Fi não conectado.");
      }
    }
    delay(500);
  }

  if (publishStatus == false) {
    publishStatus = client.publish(pubSendInfos, output2);
  }

  if (syncbutton == 1) {
    StaticJsonDocument<100> syncdoc;
    syncdoc["msg"] = "get";
    char output1[100];
    serializeJson(syncdoc, output1);
    Serial.println("Sending message to MQTT topic..");
    Serial.println(output1);
    client.publish(pubGetInfo, output1);
    syncbutton = 0;
  }

  yield();
  if (okbutton == 1 && (millis() - previousMillis >= interval)) {
    previousMillis = millis();
    medalert = 1;
    Serial.println("cronometro estorou + okbutton");
  }

  if (okbutton >= 2) {
    DateTime now = rtc.now();
    fulldate = String(now.day()) + "/" + String(now.month()) + "/" + String(now.year()) + " " + String(now.hour()) + ":" + String(now.minute());

    if (med1 == true) {
      statusdoc["okmed1"] = fulldate;
      Serial.println("to na criacao do okmed1");
      DateTime future (now + TimeSpan(0, medFreq1, 0, 0));
      medTime1[0] = future.hour();
      medDate1[2] = future.day();
      medDate1[1] = future.month();
      medDate1[0] = future.year();
      //[ano mes dia hora minuto]
      statusdoc["nextmed1"][0] = medDate1[0];
      statusdoc["nextmed1"][1] = medDate1[1];
      statusdoc["nextmed1"][2] = medDate1[2];
      statusdoc["nextmed1"][3] = medTime1[0];
      statusdoc["nextmed1"][4] = medTime1[1];
      med1 = false;
    }
    if (med2 == true) {
      statusdoc["okmed2"] = fulldate;
      Serial.println("to na criacao do okmed2");
      DateTime future (now + TimeSpan(0, medFreq2, 0, 0));
      medTime2[0] = future.hour();
      medDate2[2] = future.day();
      medDate2[1] = future.month();
      medDate2[0] = future.year();
      //[ano mes dia hora minuto]
      statusdoc["nextmed2"][0] = medDate2[0];
      statusdoc["nextmed2"][1] = medDate2[1];
      statusdoc["nextmed2"][2] = medDate2[2];
      statusdoc["nextmed2"][3] = medTime2[0];
      statusdoc["nextmed2"][4] = medTime2[1];
      med2 = false;
    }
    if (med3 == true) {
      statusdoc["okmed3"] = fulldate;
      Serial.println("to na criacao do okmed3");
      DateTime future (now + TimeSpan(0, medFreq3, 0, 0));
      medTime3[0] = future.hour();
      medDate3[2] = future.day();
      medDate3[1] = future.month();
      medDate3[0] = future.year();
      //[ano mes dia hora minuto]
      statusdoc["nextmed3"][0] = medDate3[0];
      statusdoc["nextmed3"][1] = medDate3[1];
      statusdoc["nextmed3"][2] = medDate3[2];
      statusdoc["nextmed3"][3] = medTime3[0];
      statusdoc["nextmed3"][4] = medTime3[1];
      med3 = false;
    }
    if (med4 == true) {
      statusdoc["okmed4"] = fulldate;
      Serial.println("to na criacao do okmed4");
      DateTime future (now + TimeSpan(0, medFreq4, 0, 0));
      medTime4[0] = future.hour();
      medDate4[2] = future.day();
      medDate4[1] = future.month();
      medDate4[0] = future.year();
      //[ano mes dia hora minuto]
      statusdoc["nextmed4"][0] = medDate4[0];
      statusdoc["nextmed4"][1] = medDate4[1];
      statusdoc["nextmed4"][2] = medDate4[2];
      statusdoc["nextmed4"][3] = medTime4[0];
      statusdoc["nextmed4"][4] = medTime4[1];
      med4 = false;
    }
    if (med5 == true) {
      statusdoc["okmed5"] = fulldate;
      Serial.println("to na criacao do okmed5");
      DateTime future (now + TimeSpan(0, medFreq5, 0, 0));
      medTime5[0] = future.hour();
      medDate5[2] = future.day();
      medDate5[1] = future.month();
      medDate5[0] = future.year();
      //[ano mes dia hora minuto]
      statusdoc["nextmed5"][0] = medDate5[0];
      statusdoc["nextmed5"][1] = medDate5[1];
      statusdoc["nextmed5"][2] = medDate5[2];
      statusdoc["nextmed5"][3] = medTime5[0];
      statusdoc["nextmed5"][4] = medTime5[1];
      med5 = false;
    }
    if (med6 == true) {
      statusdoc["okmed6"] = fulldate;
      Serial.println("to na criacao do okmed6");
      DateTime future (now + TimeSpan(0, medFreq6, 0, 0));
      medTime6[0] = future.hour();
      medDate6[2] = future.day();
      medDate6[1] = future.month();
      medDate6[0] = future.year();
      //[ano mes dia hora minuto]
      statusdoc["nextmed6"][0] = medDate6[0];
      statusdoc["nextmed6"][1] = medDate6[1];
      statusdoc["nextmed6"][2] = medDate6[2];
      statusdoc["nextmed6"][3] = medTime6[0];
      statusdoc["nextmed6"][4] = medTime6[1];
      med6 = false;
    }
    if (med7 == true) {
      statusdoc["okmed7"] = fulldate;
      Serial.println("to na criacao do okmed7");
      DateTime future (now + TimeSpan(0, medFreq7, 0, 0));
      medTime7[0] = future.hour();
      medDate7[2] = future.day();
      medDate7[1] = future.month();
      medDate7[0] = future.year();
      //[ano mes dia hora minuto]
      statusdoc["nextmed7"][0] = medDate7[0];
      statusdoc["nextmed7"][1] = medDate7[1];
      statusdoc["nextmed7"][2] = medDate7[2];
      statusdoc["nextmed7"][3] = medTime7[0];
      statusdoc["nextmed7"][4] = medTime7[1];
      med7 = false;
    }
    serializeJson(statusdoc, output2);
    Serial.println("Sending message to MQTT topic..");
    Serial.println(output2);
    publishStatus = client.publish(pubSendInfos, output2);
    medalert = 0;
    okbutton = 0;
    off = 1;
  }

  yield();
  DateTime now = rtc.now();
  int dia = now.day();
  int mes = now.month();
  int hora = now.hour();
  int minutos = now.minute();
  //int segundos = now.second();

  if (medDate1[2] == dia && medDate1[1] == mes && medTime1[0] == hora && medTime1[1] == minutos && med1 == false) {
    med1 = true;
    Serial.println("estou dentro do med1");
    medalert = 1;
    for (int j = 0; j < strip2.numPixels(); j++) {
      if (medPlace1[j] == 1) {
        //Serial.println("entrei no if - med1");
        strip2.setPixelColor(j, 148, 0, 211);
        strip2.show();
      }
    }
  }
  if (medDate2[2] == dia && medDate2[1] == mes && medTime2[0] == hora && medTime2[1] == minutos && med2 == false) {
    med2 = true;
    Serial.println("estou dentro do med2");
    medalert = 1;
    for (int j = 0; j < strip2.numPixels(); j++) {
      if (medPlace2[j] == 1) {
        strip2.setPixelColor(j, 148, 0, 211);
        strip2.show();
      }
    }
  }
  if (medDate3[2] == dia && medDate3[1] == mes && medTime3[0] == hora && medTime3[1] == minutos && med3 == false) {
    med3 = true;
    Serial.println("estou dentro do med3");
    medalert = 1;
    for (int j = 0; j < strip2.numPixels(); j++) {
      if (medPlace3[j] == 1) {
        strip2.setPixelColor(j, 148, 0, 211);
        strip2.show();
      }
    }
  }
  if (medDate4[2] == dia && medDate4[1] == mes && medTime4[0] == hora && medTime4[1] == minutos && med4 == false) {
    med4 = true;
    Serial.println("estou dentro do med4");
    medalert = 1;
    for (int j = 0; j < strip2.numPixels(); j++) {
      if (medPlace3[j] == 1) {
        strip2.setPixelColor(j, 148, 0, 211);
        strip2.show();
      }
    }
  }

  if (medDate5[2] == dia && medDate5[1] == mes && medTime5[0] == hora && medTime5[1] == minutos && med5 == false) {
    med5 = true;
    Serial.println("estou dentro do med5");
    medalert = 1;
    for (int j = 0; j < strip2.numPixels(); j++) {
      if (medPlace3[j] == 1) {
        strip2.setPixelColor(j, 148, 0, 211);
        strip2.show();
      }
    }
  }

  if (medDate6[2] == dia && medDate6[1] == mes && medTime6[0] == hora && medTime6[1] == minutos && med6 == false) {
    med6 = true;
    Serial.println("estou dentro do med6");
    medalert = 1;
    for (int j = 0; j < strip2.numPixels(); j++) {
      if (medPlace3[j] == 1) {
        strip2.setPixelColor(j, 148, 0, 211);
        strip2.show();
      }
    }
  }

  if (medDate7[2] == dia && medDate7[1] == mes && medTime7[0] == hora && medTime7[1] == minutos && med7 == false) {
    med7 = true;
    Serial.println("estou dentro do med7");
    medalert = 1;
    for (int j = 0; j < strip2.numPixels(); j++) {
      if (medPlace3[j] == 1) {
        strip2.setPixelColor(j, 148, 0, 211);
        strip2.show();
      }
    }
  }
  yield();
  if (medalert == 1) {
    Serial.println("to no if do medalert");
    alertRoutine();
  } else if (medalert == 0 && off == 1) {
    Serial.println("to no else do medalert");
    colorWipe(strip1.Color(0, 0, 0), 50);
    strip1.show();
    strip2.clear();
    strip2.show();
    off = 0;
  }
}

void callback(char* topic, byte* payload, unsigned int length)
{
  StaticJsonDocument<1024> doc;
  deserializeJson(doc, payload, length);

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  //FREQ
  medFreq1 = doc["medFreq1"];
  medFreq2 = doc["medFreq2"];
  medFreq3 = doc["medFreq3"];
  medFreq4 = doc["medFreq4"];
  medFreq5 = doc["medFreq5"];
  medFreq6 = doc["medFreq6"];
  medFreq7 = doc["medFreq7"];

  //TIME
  medTime1[0] = doc["medTime1"][0];
  medTime1[1] = doc["medTime1"][1];

  medTime2[0] = doc["medTime2"][0];
  medTime2[1] = doc["medTime2"][1];

  medTime3[0] = doc["medTime3"][0];
  medTime3[1] = doc["medTime3"][1];

  medTime4[0] = doc["medTime4"][0];
  medTime4[1] = doc["medTime4"][1];

  medTime5[0] = doc["medTime5"][0];
  medTime5[1] = doc["medTime5"][1];

  medTime6[0] = doc["medTime6"][0];
  medTime6[1] = doc["medTime6"][1];

  medTime7[0] = doc["medTime7"][0];
  medTime7[1] = doc["medTime7"][1];

  //DATE
  medDate1[0] = doc["medDate1"][0];
  medDate1[1] = doc["medDate1"][1];
  medDate1[2] = doc["medDate1"][2];
  medDate2[0] = doc["medDate2"][0];
  medDate2[1] = doc["medDate2"][1];
  medDate2[2] = doc["medDate2"][2];
  medDate3[0] = doc["medDate3"][0];
  medDate3[1] = doc["medDate3"][1];
  medDate3[2] = doc["medDate3"][2];
  medDate4[0] = doc["medDate4"][0];
  medDate4[1] = doc["medDate4"][1];
  medDate4[2] = doc["medDate4"][2];
  medDate5[0] = doc["medDate5"][0];
  medDate5[1] = doc["medDate5"][1];
  medDate5[2] = doc["medDate5"][2];
  medDate6[0] = doc["medDate6"][0];
  medDate6[1] = doc["medDate6"][1];
  medDate6[2] = doc["medDate6"][2];
  medDate7[0] = doc["medDate7"][0];
  medDate7[1] = doc["medDate7"][1];
  medDate7[2] = doc["medDate7"][2];

  //PLACE

  for (int i = 0; i < 7 ; i++) {
    medPlace1[i] = doc["medPlace1"][i];
  }

  for (int i = 0; i < 7 ; i++) {
    medPlace2[i] = doc["medPlace2"][i];
  }

  for (int i = 0; i < 7 ; i++) {
    medPlace3[i] = doc["medPlace3"][i];
  }

  for (int i = 0; i < 7 ; i++) {
    medPlace4[i] = doc["medPlace4"][i];
  }

  for (int i = 0; i < 7 ; i++) {
    medPlace5[i] = doc["medPlace5"][i];
  }

  for (int i = 0; i < 7 ; i++) {
    medPlace6[i] = doc["medPlace6"][i];
  }

  for (int i = 0; i < 7 ; i++) {
    medPlace7[i] = doc["medPlace7"][i];
  }

  // Show that the sync work it out
  colorWipe(strip1.Color(148, 0, 211), 50);
  colorWipe(strip1.Color(0, 0, 0), 50);
}

void wifiConnect()
{ // Função void wifiConnect
  Serial.print("Conectando a Rede ");
  Serial.print(ssid);                // Indica a Rede que o ESP32 irá se conectar
  WiFi.begin(ssid, password);        // Conecta ao ssid e o password configurado
  while (WiFi.status() != WL_CONNECTED)
  { // Enquanto estiver desconectado
    delay(500);                      // Aguarda 500 milissegundos
    Serial.print(".");
  }
  Serial.print("Wi-Fi conectado, Endereço de IP: ");
  Serial.println(WiFi.localIP());     // Indica o endereço de IP
}

void mqttConnect()
{ // Função void mqttConnect
  if (!!!client.connected())
  {
    Serial.print("Reconectando MQTT do cliente ");
    Serial.println(server);         // Indica o endereço do servidor

    while (!!!client.connect(clientId, authMethod, token))
    {
      Serial.print(".");
      delay(500);
    }
    client.setBufferSize(1024);

    if (client.subscribe(lisMedInfo))
    {
      Serial.println("Inscrito no tópico");      // IF SUBSCRIBED
    }
    else
    {
      Serial.println("Resposta FALHOU");         // IF NOT SUBSCRIBED
    }
  }
}

void mqttReconnect()
{ 
  if (!!!client.connected())
  {
    Serial.println("Connecting to MQTT...");
    if (client.connect(clientId, authMethod, token)) {
      Serial.println("Connected");
      client.subscribe(lisMedInfo, 1);
      client.setBufferSize(1024);
    } else {
      Serial.print("Failed with state: ");
      Serial.println(client.state());
    }
    if (client.subscribe(lisMedInfo))
    {
      Serial.println("Inscrito no tópico");      // IF SUBSCRIBED
    }
    else
    {
      Serial.println("Resposta FALHOU");         // IF NOT SUBSCRIBED
    }
  }
}

void colorWipe(uint32_t color, int wait) {
  for (int i = 0; i < strip1.numPixels(); i++) { // For each pixel in strip...
    strip1.setPixelColor(i, color);         //  Set pixel's color (in RAM)
    strip1.show();                          //  Update strip to match
    delay(wait);                            //  Pause for a moment
  }
}

void colorWipe2(uint32_t color, int wait) {
  for (int i = 0; i < strip2.numPixels(); i++) { // For each pixel in strip...
    strip2.setPixelColor(i, color);         //  Set pixel's color (in RAM)
    strip2.show();                          //  Update strip to match
    delay(wait);                            //  Pause for a moment
  }
}

void okTimer() {
  medalert = 0;
  previousMillis = millis();
  Serial.println("comecando a contagem");
}

void alertRoutine() {
  colorWipe(strip1.Color(148, 0, 211), 50);
  colorWipe(strip1.Color(0, 0, 0), 50);
  colorWipe(strip1.Color(148, 0, 211), 50);
  colorWipe(strip1.Color(0, 0, 0), 50);
  colorWipe(strip1.Color(148, 0, 211), 50);
  digitalWrite(buzzer, HIGH);
  digitalWrite(motor, HIGH);
  delay(500);
  digitalWrite(buzzer, LOW);
  digitalWrite(motor, LOW);
  delay(500);
  digitalWrite(buzzer, HIGH);
  digitalWrite(motor, HIGH);
  delay(500);
  digitalWrite(buzzer, LOW);
  digitalWrite(motor, LOW);
  delay(5000);
}
