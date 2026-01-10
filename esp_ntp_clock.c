#include <ESPmDNS.h>
#include <NetworkUdp.h>
#include <ArduinoOTA.h>
#include <TM1640.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "NTPClient.h"
#include <math.h>


WiFiUDP ntpUDP;

// -----------------------------------------------
// Edit Below
// -----------------------------------------------
const char* ssid     = "<WI-FI SSID HERE>";
const char* password = "<WI-FI Password here>";

// Pins connected to display
const int PIN_DIO = 18;
const int PIN_CLOCK = 19;
// optional power pin to reset display on boot
const int PIN_power = 4;
// usually not needed for display to work
const int PIN_STB = 7;

const char* ntpServer = "pool.ntp.org";
const int  gmtOffset_sec = -18000; // currently set to Eastern US
const long   updateinterval_Msec = 600000; // 10 minutes
const int brightness = 3; // 0-7

// -----------------------------------------------
// Edit Above
// -----------------------------------------------

NTPClient timeClient(ntpUDP, ntpServer, gmtOffset_sec, updateinterval_Msec);
TM1640 module(PIN_DIO, PIN_CLOCK , PIN_STB); 


void setup() {
  Serial.begin(115200);
  pinMode(PIN_power, OUTPUT);
  digitalWrite(PIN_power, LOW);
  delay(300);
  digitalWrite(PIN_power, HIGH);
  module.begin(true, brightness);   // on=true, birghtness (range 0-7)
  module.clearDisplay();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    module.setSegments(0x80, 2);
    delay(500);
    module.setSegments(0x00, 2);
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");

  ArduinoOTA.setPassword("admin");
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
      } else {  // U_SPIFFS
        type = "filesystem";
      }

      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) {
        Serial.println("Auth Failed");
      } else if (error == OTA_BEGIN_ERROR) {
        Serial.println("Begin Failed");
      } else if (error == OTA_CONNECT_ERROR) {
        Serial.println("Connect Failed");
      } else if (error == OTA_RECEIVE_ERROR) {
        Serial.println("Receive Failed");
      } else if (error == OTA_END_ERROR) {
        Serial.println("End Failed");
      }
    });

  ArduinoOTA.begin();

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  timeClient.begin();
  timeClient.update();

  // 2 = AM, 6 = AM + Alarm1
  // module.setSegments(0, 4);
  // 2 = PM, 6 = PM + Alarm2
  // module.setSegments(2, 5);
}

int digits[] = {0, 0, 0, 0};
bool blink_on = true;
void blink_colon(int digit1, int digit2)
{
  if (blink_on)
  {
    module.setDisplayDigit(digit1, 1, false);
    module.setDisplayDigit(digit2, 2, false);
    blink_on = false;
  }
  else
  {
    module.setDisplayDigit(digit1, 1, true);
    module.setDisplayDigit(digit2, 2, true);
    blink_on = true;
  }
}
unsigned long lastExecutedMillis_1 = 0; 
void loop() {
  ArduinoOTA.handle();
  

  int hour24 = timeClient.getHours();  // 0–23
  int hour12 = hour24 % 12;
  if (hour12 == 0) {
    hour12 = 12;  // midnight or noon
  }

  bool isPM = (hour24 >= 12);

  if (hour12 < 10)
  {
    digits[0]=0;
    digits[1]= hour12;
  }
  else
  {
    digits[0]= 1;
    digits[1]= hour12 - 10;
  }

  digits[2] =  floor(timeClient.getMinutes()/ 10);
  digits[3] = timeClient.getMinutes() % 10;
  unsigned long currentMillis = millis();
  if (currentMillis - lastExecutedMillis_1 >= 500) {
    lastExecutedMillis_1 = currentMillis; 

    if (! timeClient.update())
    {
      Serial.println("Failed to obtain time.");
      if (isPM)
      {
        module.setSegments(0, 4);
        module.setSegments(6, 5);
      }
      else
      {
        module.setSegments(6, 4);
        module.setSegments(0, 5);
      }
    }
    else
    {
      if (isPM)
      {
        module.setSegments(0, 4);
        module.setSegments(2, 5);
      }
      else
      {
        module.setSegments(2, 4);
        module.setSegments(0, 5);
      }
    }
    if (digits[0] == 0)
    {
      module.setSegments(0x00, 0);
    }
    else
    {
      module.setDisplayDigit(digits[0], 0, false);
    }
    module.setDisplayDigit(digits[1], 1, true);
    module.setDisplayDigit(digits[2], 2, true);
    module.setDisplayDigit(digits[3], 3, false);
    blink_colon(digits[1], digits[2]);
  }

  
}
