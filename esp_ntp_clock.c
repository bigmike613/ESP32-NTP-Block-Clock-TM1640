#include <ESPmDNS.h>
#include <NetworkUdp.h>
#include <ArduinoOTA.h>
#include <TM1640.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "NTP.h"
#include <math.h>
#include "secrets.h"


WiFiUDP wifiUdp;
NTP ntp(wifiUdp);



// -----------------------------------------------
// Edit Below
// -----------------------------------------------
// WI-FI
const char* ssid     = secret_wifi_ssid;
const char* password = secret_wifi_pass;

// Pins connected to display
const int PIN_DIO = 18;
const int PIN_CLOCK = 19;
// optional power pin to reset display on boot
const int PIN_power = 4;
// usually not needed for display to work
const int PIN_STB = 7;

const char* ntpServer = "pool.ntp.org";
const uint32_t   updateinterval_Msec = 600000; // 10 minutes
const int brightness = 6; // 0-7

// -----------------------------------------------
// Edit Above
// -----------------------------------------------



TM1640 module(PIN_DIO, PIN_CLOCK , PIN_STB); 

unsigned long lastExecutedMillis_1 = 0; 
unsigned long lastExecutedMillis_2 = 0;
unsigned long lastExecutedMillis_3 = 0;
bool last_update = false;
bool ntpUpdateReturnSuccess()
{
  unsigned long currentMillis = millis();
  if (currentMillis - lastExecutedMillis_2 >= updateinterval_Msec) {
    lastExecutedMillis_2 = currentMillis;
    last_update = ntp.update();
    return last_update;
  }
  else
  {
    if (currentMillis - lastExecutedMillis_3 >= 30000 && ! last_update) {
    lastExecutedMillis_3 = currentMillis;
    last_update = ntp.update();
    return last_update;
    }
    return last_update;
  }
}
void setup() {
  Serial.begin(115200);
  pinMode(PIN_power, OUTPUT);
  digitalWrite(PIN_power, LOW);
  delay(300);
  digitalWrite(PIN_power, HIGH);
  module.begin(true, brightness);   // on=true, birghtness (range 0-7)
  module.clearDisplay();
  module.setDisplayToString("HI");
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

 // timezone settings
  ntp.ruleDST("EDT", Second, Sun, Mar, 2, -240);
  ntp.ruleSTD("EST", First, Sun, Nov, 2, -300);
  ntp.updateInterval(10); // this is set low so everytime ntp.update is called, it actually runs update (which only happens every updateinterval_Msec).
  ntp.begin();
  ntp.update();

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

void loop() {
  ArduinoOTA.handle();
  
  int hour24 = ntp.hours();  // 0–23
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

  digits[2] =  floor(ntp.minutes()/ 10);
  digits[3] = ntp.minutes() % 10;


  unsigned long currentMillis = millis();
  if (currentMillis - lastExecutedMillis_1 >= 500) {
    lastExecutedMillis_1 = currentMillis; 

    if (! ntpUpdateReturnSuccess())
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
