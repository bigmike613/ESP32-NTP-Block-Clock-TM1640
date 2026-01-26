#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <NetworkUdp.h>
#include <ArduinoOTA.h>
#include <TM1640.h>
#include <WiFiUdp.h>
#include "NTP.h"
#include <math.h>

// -----------------------------------------------
// Hardware Configuration
// -----------------------------------------------
const int PIN_DIO = 12;
const int PIN_CLOCK = 13;
const int PIN_power = 14;
const int PIN_STB = 7;
const uint32_t updateinterval_Msec = 600000;
int display_brightness = 7;  // 0-7, configurable via web GUI

// -----------------------------------------------
// AP Mode Configuration
// -----------------------------------------------
const char* AP_SSID = "NTP-Clock-Setup";
const char* AP_PASS = "clocksetup";
const byte DNS_PORT = 53;

// -----------------------------------------------
// Timezone definitions
// -----------------------------------------------
struct TimezoneInfo {
  const char* name;
  const char* dstAbbrev;
  const char* stdAbbrev;
  int dstOffset;  // minutes from UTC
  int stdOffset;  // minutes from UTC
};

TimezoneInfo timezones[] = {
  {"Eastern",  "EDT", "EST", -240, -300},
  {"Central",  "CDT", "CST", -300, -360},
  {"Mountain", "MDT", "MST", -360, -420},
  {"Pacific",  "PDT", "PST", -420, -480},
  {"Alaska",   "AKDT", "AKST", -480, -540},
  {"Hawaii",   "HST", "HST", -600, -600},  // No DST
  {"Arizona",  "MST", "MST", -420, -420},  // No DST
  {"UTC",      "UTC", "UTC", 0, 0}
};
const int NUM_TIMEZONES = sizeof(timezones) / sizeof(timezones[0]);

// -----------------------------------------------
// Global Objects
// -----------------------------------------------
WiFiUDP wifiUdp;
NTP ntp(wifiUdp);
TM1640 module(PIN_DIO, PIN_CLOCK, PIN_STB);
WebServer server(80);
DNSServer dnsServer;
Preferences preferences;

// -----------------------------------------------
// State Variables
// -----------------------------------------------
String wifi_ssid = "";
String wifi_pass = "";
String ntp_server = "pool.ntp.org";
int timezone_index = 0;  // Default to Eastern
bool wifi_connected = false;
bool ap_mode = true;
bool config_saved = false;

unsigned long lastExecutedMillis_1 = 0;
unsigned long lastExecutedMillis_2 = 0;
unsigned long lastExecutedMillis_3 = 0;
bool last_update = false;
int digits[] = {0, 0, 0, 0};
bool blink_on = true;

// -----------------------------------------------
// HTML Templates
// -----------------------------------------------
void sendHTML(int code, const String& html) {
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.sendHeader("Connection", "close");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "*");
  server.send(code, "text/html; charset=utf-8", html);
}

void handleOptions() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "*");
  server.send(204);
}

String getHTMLHeader(const char* title) {
  return "<!DOCTYPE html><html><head>"
         "<meta charset='UTF-8'>"
         "<meta name='viewport' content='width=device-width, initial-scale=1, user-scalable=yes'>"
         "<title>" + String(title) + "</title>"
         "<style>"
         "body{font-family:Arial,sans-serif;margin:0;padding:20px;background:#1a1a2e;color:#eee;}"
         ".container{max-width:400px;margin:0 auto;}"
         "h1{color:#0f9;text-align:center;}"
         "h2{color:#0cf;border-bottom:1px solid #333;padding-bottom:10px;}"
         ".card{background:#16213e;border-radius:10px;padding:20px;margin:15px 0;box-shadow:0 4px 6px rgba(0,0,0,0.3);}"
         "label{display:block;margin:10px 0 5px;color:#aaa;}"
         "input[type=text],input[type=password],select{width:100%;padding:12px;margin:5px 0 15px;border:1px solid #333;border-radius:5px;background:#0d1b2a;color:#fff;box-sizing:border-box;}"
         "input[type=submit],button{width:100%;padding:14px;background:#0f9;color:#000;border:none;border-radius:5px;cursor:pointer;font-size:16px;font-weight:bold;margin:10px 0;}"
         "input[type=submit]:hover,button:hover{background:#0da;}"
         ".btn-secondary{background:#0cf;}"
         ".btn-danger{background:#f55;color:#fff;}"
         ".network{padding:12px;margin:8px 0;background:#0d1b2a;border-radius:5px;cursor:pointer;border:1px solid #333;display:flex;justify-content:space-between;align-items:center;}"
         ".network:hover{border-color:#0f9;}"
         ".ssid-name{flex:1;}"
         ".bars{display:flex;align-items:flex-end;gap:2px;height:20px;}"
         ".bar{width:4px;border-radius:1px;}"
         ".bar.on{background:#0f9;}"
         ".bar.off{background:#333;}"
         ".info{background:#1e3a5f;padding:15px;border-radius:5px;margin:10px 0;}"
         ".info p{margin:5px 0;}"
         ".success{color:#0f9;}"
         ".error{color:#f55;}"
         "a{color:#0cf;}"
         "</style></head><body><div class='container'>";
}

String getHTMLFooter() {
  return "</div></body></html>";
}

// -----------------------------------------------
// WiFi Scanning
// -----------------------------------------------
String scanNetworks() {
  String html = "";
  int n = WiFi.scanNetworks();
  if (n == 0) {
    html += "<p>No networks found. <a href='/scan'>Scan again</a></p>";
  } else {
    // Collect unique SSIDs with best signal strength
    String uniqueSSIDs[20];
    int uniqueRSSI[20];
    int uniqueCount = 0;

    for (int i = 0; i < n && uniqueCount < 20; i++) {
      String ssid = WiFi.SSID(i);
      if (ssid.length() == 0) continue;  // Skip hidden networks

      // Check if SSID already exists
      bool found = false;
      for (int j = 0; j < uniqueCount; j++) {
        if (uniqueSSIDs[j] == ssid) {
          // Keep the stronger signal
          if (WiFi.RSSI(i) > uniqueRSSI[j]) {
            uniqueRSSI[j] = WiFi.RSSI(i);
          }
          found = true;
          break;
        }
      }
      if (!found) {
        uniqueSSIDs[uniqueCount] = ssid;
        uniqueRSSI[uniqueCount] = WiFi.RSSI(i);
        uniqueCount++;
      }
    }

    html += "<form action='/connect' method='post'>";
    for (int i = 0; i < uniqueCount; i++) {
      int rssi = uniqueRSSI[i];
      int bars = 1;
      if (rssi > -50) bars = 4;
      else if (rssi > -60) bars = 3;
      else if (rssi > -70) bars = 2;

      String signalBars = "<span class='bars'>";
      for (int b = 1; b <= 4; b++) {
        if (b <= bars) {
          signalBars += "<span class='bar on' style='height:" + String(b * 4 + 4) + "px'></span>";
        } else {
          signalBars += "<span class='bar off' style='height:" + String(b * 4 + 4) + "px'></span>";
        }
      }
      signalBars += "</span>";

      html += "<div class='network' onclick=\"document.getElementById('ssid').value='" + uniqueSSIDs[i] + "'\">";
      html += "<span class='ssid-name'>" + uniqueSSIDs[i] + "</span>";
      html += signalBars;
      html += "</div>";
    }
    html += "<label>Selected Network:</label>";
    html += "<input type='text' id='ssid' name='ssid' placeholder='Click a network above or type SSID'>";
    html += "<label>Password:</label>";
    html += "<input type='password' name='password' placeholder='WiFi Password'>";
    html += "<input type='submit' value='Connect to WiFi'>";
    html += "</form>";
  }
  WiFi.scanDelete();
  return html;
}

// -----------------------------------------------
// Web Handlers - AP Mode
// -----------------------------------------------
void handleRoot() {
  if (ap_mode) {
    String html = getHTMLHeader("NTP Clock Setup");
    html += "<h1>NTP Clock Setup</h1>";
    html += "<div class='card'>";
    html += "<h2>WiFi Networks</h2>";
    html += scanNetworks();
    html += "</div>";
    html += getHTMLFooter();
    sendHTML(200, html);
  } else {
    handleStatus();
  }
}

void handleScan() {
  handleRoot();
}

void handleConnect() {
  if (server.hasArg("ssid") && server.hasArg("password")) {
    wifi_ssid = server.arg("ssid");
    wifi_pass = server.arg("password");

    String html = getHTMLHeader("WiFi Setup");
    html += "<h1>NTP Clock Setup</h1>";
    html += "<div class='card'>";
    html += "<h2>WiFi Selected</h2>";
    html += "<p>Network: <strong>" + wifi_ssid + "</strong></p>";
    html += "<p>Now configure your timezone and NTP server.</p>";
    html += "</div>";

    html += "<div class='card'>";
    html += "<h2>Timezone & NTP</h2>";
    html += "<form action='/save' method='post'>";
    html += "<input type='hidden' name='ssid' value='" + wifi_ssid + "'>";
    html += "<input type='hidden' name='password' value='" + wifi_pass + "'>";

    html += "<label>Timezone:</label>";
    html += "<select name='timezone'>";
    for (int i = 0; i < NUM_TIMEZONES; i++) {
      String selected = (i == 0) ? " selected" : "";
      html += "<option value='" + String(i) + "'" + selected + ">" + String(timezones[i].name) + "</option>";
    }
    html += "</select>";

    html += "<label>NTP Server:</label>";
    html += "<input type='text' name='ntpserver' value='pool.ntp.org' placeholder='NTP Server'>";

    html += "<label>Display Brightness: <span id='brightval'>7</span></label>";
    html += "<input type='range' name='brightness' min='0' max='7' value='7' oninput=\"document.getElementById('brightval').textContent=this.value\" style='width:100%'>";

    html += "<input type='submit' value='Save & Connect'>";
    html += "</form>";
    html += "</div>";
    html += getHTMLFooter();
    sendHTML(200, html);
  } else {
    server.sendHeader("Location", "/");
    server.send(302);
  }
}

void handleSave() {
  if (server.hasArg("ssid") && server.hasArg("password") &&
      server.hasArg("timezone") && server.hasArg("ntpserver")) {

    wifi_ssid = server.arg("ssid");
    wifi_pass = server.arg("password");
    timezone_index = server.arg("timezone").toInt();
    ntp_server = server.arg("ntpserver");
    if (server.hasArg("brightness")) {
      display_brightness = server.arg("brightness").toInt();
      display_brightness = constrain(display_brightness, 0, 7);
    }

    // Save to preferences
    preferences.begin("ntpclock", false);
    preferences.putString("ssid", wifi_ssid);
    preferences.putString("pass", wifi_pass);
    preferences.putInt("tz", timezone_index);
    preferences.putString("ntp", ntp_server);
    preferences.putInt("bright", display_brightness);
    preferences.end();

    config_saved = true;

    String html = getHTMLHeader("Connecting");
    html += "<h1>NTP Clock Setup</h1>";
    html += "<div class='card'>";
    html += "<h2 class='success'>Configuration Saved!</h2>";
    html += "<div class='info'>";
    html += "<p><strong>WiFi:</strong> " + wifi_ssid + "</p>";
    html += "<p><strong>Timezone:</strong> " + String(timezones[timezone_index].name) + "</p>";
    html += "<p><strong>NTP Server:</strong> " + ntp_server + "</p>";
    html += "</div>";
    html += "<p>The clock will now restart and connect to your WiFi network.</p>";
    html += "<p>Once connected, you can access this configuration page at the clock's IP address.</p>";
    html += "</div>";
    html += getHTMLFooter();
    sendHTML(200, html);

    delay(2000);
    ESP.restart();
  } else {
    server.sendHeader("Location", "/");
    server.send(302);
  }
}

// -----------------------------------------------
// Web Handlers - Station Mode
// -----------------------------------------------
void handleStatus() {
  String html = getHTMLHeader("NTP Clock Status");
  html += "<h1>NTP Clock</h1>";

  html += "<div class='card'>";
  html += "<h2>Status</h2>";
  html += "<div class='info'>";
  html += "<p><strong>WiFi Network:</strong> " + wifi_ssid + "</p>";
  html += "<p><strong>IP Address:</strong> " + WiFi.localIP().toString() + "</p>";
  html += "<p><strong>Signal Strength:</strong> " + String(WiFi.RSSI()) + " dBm</p>";
  html += "<p><strong>Timezone:</strong> " + String(timezones[timezone_index].name) + "</p>";
  html += "<p><strong>NTP Server:</strong> " + ntp_server + "</p>";
  html += "<p><strong>Brightness:</strong> " + String(display_brightness) + "/7</p>";
  html += "<p><strong>Current Time:</strong> " + String(ntp.hours()) + ":" +
          (ntp.minutes() < 10 ? "0" : "") + String(ntp.minutes()) + ":" +
          (ntp.seconds() < 10 ? "0" : "") + String(ntp.seconds()) + "</p>";
  html += "<p><strong>NTP Sync:</strong> " + String(last_update ? "<span class='success'>OK</span>" : "<span class='error'>Failed</span>") + "</p>";
  html += "</div>";
  html += "</div>";

  html += "<div class='card'>";
  html += "<h2>Settings</h2>";
  html += "<a href='/settings'><button>Change Settings</button></a>";
  html += "<a href='/reset'><button class='btn-danger'>Reset & Reconfigure</button></a>";
  html += "</div>";

  html += getHTMLFooter();
  sendHTML(200, html);
}

void handleSettings() {
  String html = getHTMLHeader("Clock Settings");
  html += "<h1>Clock Settings</h1>";

  html += "<div class='card'>";
  html += "<h2>Timezone & NTP</h2>";
  html += "<form action='/updatesettings' method='post'>";

  html += "<label>Timezone:</label>";
  html += "<select name='timezone'>";
  for (int i = 0; i < NUM_TIMEZONES; i++) {
    String selected = (i == timezone_index) ? " selected" : "";
    html += "<option value='" + String(i) + "'" + selected + ">" + String(timezones[i].name) + "</option>";
  }
  html += "</select>";

  html += "<label>NTP Server:</label>";
  html += "<input type='text' name='ntpserver' value='" + ntp_server + "'>";

  html += "<label>Display Brightness: <span id='brightval'>" + String(display_brightness) + "</span></label>";
  html += "<input type='range' name='brightness' min='0' max='7' value='" + String(display_brightness) + "' oninput=\"document.getElementById('brightval').textContent=this.value\" style='width:100%'>";

  html += "<input type='submit' value='Save Settings'>";
  html += "</form>";
  html += "<a href='/'><button class='btn-secondary'>Back</button></a>";
  html += "</div>";

  html += getHTMLFooter();
  sendHTML(200, html);
}

void handleUpdateSettings() {
  if (server.hasArg("timezone") && server.hasArg("ntpserver")) {
    timezone_index = server.arg("timezone").toInt();
    ntp_server = server.arg("ntpserver");
    if (server.hasArg("brightness")) {
      display_brightness = server.arg("brightness").toInt();
      display_brightness = constrain(display_brightness, 0, 7);
    }

    preferences.begin("ntpclock", false);
    preferences.putInt("tz", timezone_index);
    preferences.putString("ntp", ntp_server);
    preferences.putInt("bright", display_brightness);
    preferences.end();

    // Apply new timezone and force NTP resync
    applyTimezone();
    ntp.stop();
    ntp.begin();
    last_update = ntp.update();

    // Apply new brightness
    module.setupDisplay(true, display_brightness);

    String html = getHTMLHeader("Settings Saved");
    html += "<h1>Settings Saved</h1>";
    html += "<div class='card'>";
    html += "<h2 class='success'>Settings Updated!</h2>";
    html += "<p>Timezone: " + String(timezones[timezone_index].name) + "</p>";
    html += "<p>NTP Server: " + ntp_server + "</p>";
    html += "<p>Brightness: " + String(display_brightness) + "/7</p>";
    html += "<a href='/'><button>Back to Status</button></a>";
    html += "</div>";
    html += getHTMLFooter();
    sendHTML(200, html);
  } else {
    server.sendHeader("Location", "/settings");
    server.send(302);
  }
}

void handleReset() {
  String html = getHTMLHeader("Reset Clock");
  html += "<h1>Reset Clock</h1>";
  html += "<div class='card'>";
  html += "<h2 class='error'>Confirm Reset</h2>";
  html += "<p>This will erase all settings and restart the clock in setup mode.</p>";
  html += "<form action='/doreset' method='post'>";
  html += "<input type='submit' value='Yes, Reset Everything' class='btn-danger'>";
  html += "</form>";
  html += "<a href='/'><button class='btn-secondary'>Cancel</button></a>";
  html += "</div>";
  html += getHTMLFooter();
  sendHTML(200, html);
}

void handleDoReset() {
  preferences.begin("ntpclock", false);
  preferences.clear();
  preferences.end();

  String html = getHTMLHeader("Resetting");
  html += "<h1>Resetting...</h1>";
  html += "<div class='card'>";
  html += "<h2>Settings Cleared</h2>";
  html += "<p>The clock will restart in setup mode.</p>";
  html += "<p>Connect to WiFi network: <strong>" + String(AP_SSID) + "</strong></p>";
  html += "<p>Password: <strong>" + String(AP_PASS) + "</strong></p>";
  html += "</div>";
  html += getHTMLFooter();
  sendHTML(200, html);

  delay(2000);
  ESP.restart();
}

void handleNotFound() {
  if (ap_mode) {
    // Captive portal - redirect all requests to root
    server.sendHeader("Location", "http://192.168.4.1/");
    server.send(302);
  } else {
    server.send(404, "text/plain", "Not Found");
  }
}

// -----------------------------------------------
// Helper Functions
// -----------------------------------------------
void applyTimezone() {
  TimezoneInfo& tz = timezones[timezone_index];
  if (tz.dstOffset == tz.stdOffset) {
    // No DST
    ntp.ruleDST(tz.dstAbbrev, Second, Sun, Mar, 2, tz.dstOffset);
    ntp.ruleSTD(tz.stdAbbrev, First, Sun, Nov, 2, tz.stdOffset);
  } else {
    // Has DST
    ntp.ruleDST(tz.dstAbbrev, Second, Sun, Mar, 2, tz.dstOffset);
    ntp.ruleSTD(tz.stdAbbrev, First, Sun, Nov, 2, tz.stdOffset);
  }
}

bool ntpUpdateReturnSuccess() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastExecutedMillis_2 >= updateinterval_Msec) {
    lastExecutedMillis_2 = currentMillis;
    last_update = ntp.update();
    return last_update;
  } else {
    if (currentMillis - lastExecutedMillis_3 >= 30000 && !last_update) {
      lastExecutedMillis_3 = currentMillis;
      last_update = ntp.update();
      return last_update;
    }
    return last_update;
  }
}

void blink_colon(int digit1, int digit2) {
  if (blink_on) {
    module.setDisplayDigit(digit1, 1, false);
    module.setDisplayDigit(digit2, 2, false);
    blink_on = false;
  } else {
    module.setDisplayDigit(digit1, 1, true);
    module.setDisplayDigit(digit2, 2, true);
    blink_on = true;
  }
}

void startAPMode() {
  Serial.println("Starting AP Mode...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  delay(100);

  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  // Setup DNS server for captive portal
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/scan", handleScan);
  server.on("/connect", HTTP_POST, handleConnect);
  server.on("/save", HTTP_POST, handleSave);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started in AP mode");

  ap_mode = true;
  module.setDisplayToString("CON");
}

void startStationMode() {
  Serial.println("Starting Station Mode...");
  Serial.print("Connecting to: ");
  Serial.println(wifi_ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());

  module.setDisplayToString("CON");

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    wifi_connected = true;
    ap_mode = false;

    // Setup OTA
    ArduinoOTA.setPassword("admin");
    ArduinoOTA
      .onStart([]() {
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
        Serial.println("Start updating " + type);
      })
      .onEnd([]() { Serial.println("\nEnd"); })
      .onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
      })
      .onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");
      });
    ArduinoOTA.begin();

    // Setup mDNS
    if (MDNS.begin("ntpclock2")) {
      Serial.println("mDNS responder started: ntpclock2.local");
    }

    // Setup NTP
    applyTimezone();
    ntp.updateInterval(10);
    ntp.begin();
    last_update = ntp.update();

    // Setup web server for station mode
    server.on("/", HTTP_GET, handleStatus);
    server.on("/", HTTP_OPTIONS, handleOptions);
    server.on("/settings", HTTP_GET, handleSettings);
    server.on("/settings", HTTP_OPTIONS, handleOptions);
    server.on("/updatesettings", HTTP_POST, handleUpdateSettings);
    server.on("/updatesettings", HTTP_OPTIONS, handleOptions);
    server.on("/reset", HTTP_GET, handleReset);
    server.on("/reset", HTTP_OPTIONS, handleOptions);
    server.on("/doreset", HTTP_POST, handleDoReset);
    server.on("/doreset", HTTP_OPTIONS, handleOptions);
    server.onNotFound(handleNotFound);

    server.begin();
    Serial.println("HTTP server started in Station mode");

    module.clearDisplay();
  } else {
    Serial.println("");
    Serial.println("WiFi connection failed! Starting AP mode...");
    startAPMode();
  }
}

// -----------------------------------------------
// Setup
// -----------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(100);

  // Initialize display
  pinMode(PIN_power, OUTPUT);
  digitalWrite(PIN_power, LOW);
  delay(300);
  digitalWrite(PIN_power, HIGH);
  module.begin(true, display_brightness);
  module.clearDisplay();
  module.setDisplayToString("CON");

  // Load saved preferences
  preferences.begin("ntpclock", true);
  wifi_ssid = preferences.getString("ssid", "");
  wifi_pass = preferences.getString("pass", "");
  timezone_index = preferences.getInt("tz", 0);  // Default to Eastern
  ntp_server = preferences.getString("ntp", "pool.ntp.org");
  display_brightness = preferences.getInt("bright", 7);  // Default to max
  preferences.end();

  Serial.println("Loaded settings:");
  Serial.println("SSID: " + wifi_ssid);
  Serial.println("TZ: " + String(timezones[timezone_index].name));
  Serial.println("NTP: " + ntp_server);

  // Decide mode based on saved credentials
  if (wifi_ssid.length() > 0) {
    startStationMode();
  } else {
    startAPMode();
  }
}

// -----------------------------------------------
// Main Loop
// -----------------------------------------------
void loop() {
  if (ap_mode) {
    dnsServer.processNextRequest();
    server.handleClient();
    return;
  }

  // Station mode
  server.handleClient();
  ArduinoOTA.handle();

  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    module.setDisplayToString("CON");
    delay(5000);
    ESP.restart();
    return;
  }

  // Update time display
  int hour24 = ntp.hours();
  int hour12 = hour24 % 12;
  if (hour12 == 0) hour12 = 12;
  bool isPM = (hour24 >= 12);

  if (hour12 < 10) {
    digits[0] = 0;
    digits[1] = hour12;
  } else {
    digits[0] = 1;
    digits[1] = hour12 - 10;
  }

  digits[2] = floor(ntp.minutes() / 10);
  digits[3] = ntp.minutes() % 10;

  unsigned long currentMillis = millis();
  if (currentMillis - lastExecutedMillis_1 >= 500) {
    lastExecutedMillis_1 = currentMillis;

    if (!ntpUpdateReturnSuccess()) {
      Serial.println("Failed to obtain time.");
      if (isPM) {
        module.setSegments(0, 4);
        module.setSegments(6, 5);
      } else {
        module.setSegments(6, 4);
        module.setSegments(0, 5);
      }
    } else {
      if (isPM) {
        module.setSegments(0, 4);
        module.setSegments(2, 5);
      } else {
        module.setSegments(2, 4);
        module.setSegments(0, 5);
      }
    }

    if (digits[0] == 0) {
      module.setSegments(0x00, 0);
    } else {
      module.setDisplayDigit(digits[0], 0, false);
    }
    module.setDisplayDigit(digits[1], 1, true);
    module.setDisplayDigit(digits[2], 2, true);
    module.setDisplayDigit(digits[3], 3, false);
    blink_colon(digits[1], digits[2]);
  }
}
