#include <WiFi.h>
#include <EEPROM.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Firebase_ESP_Client.h>
#include <WebServer.h>


// Firebase credentials
#define API_KEY "AIzaSyC8HgkU-JHZ-LtSosqt2uwkklafwVWk7zQ"
#define DATABASE_URL "https://iot-display-project-default-rtdb.asia-southeast1.firebasedatabase.app"
#define USER_EMAIL "shifaq2002@gmail.com"
#define USER_PASSWORD "917603Sm#"


// EEPROM settings
#define EEPROM_SIZE 101
#define SSID_ADDR 0
#define PASS_ADDR 32
#define DEVICE_ID_ADDR 64
#define CONFIG_FLAG_ADDR 100


// OLED settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);


// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;


// Web server
WebServer server(80);


// Global vars
String ssid, password, deviceID;


// Pins
#define CONFIG_BUTTON 0
#define LED_INDICATOR 2


void showStatus(String line1, String line2 = "", String line3 = "", String line4 = "") {
  display.clearDisplay();
  display.setCursor(0, 0);
  if (line1 != "") display.println(line1);
  if (line2 != "") display.println(line2);
  if (line3 != "") display.println(line3);
  if (line4 != "") display.println(line4);
  display.display();
}


// EEPROM Read
void readEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  char s[33], p[33], d[33];
  for (int i = 0; i < 32; i++) {
    s[i] = EEPROM.read(SSID_ADDR + i);
    p[i] = EEPROM.read(PASS_ADDR + i);
    d[i] = EEPROM.read(DEVICE_ID_ADDR + i);
  }
  s[32] = p[32] = d[32] = '\0';
  ssid = String(s);
  password = String(p);
  deviceID = String(d);
  EEPROM.end();
}


// EEPROM Write
void writeEEPROM(String s, String p, String d) {
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < 32; i++) {
    EEPROM.write(SSID_ADDR + i, i < s.length() ? s[i] : 0);
    EEPROM.write(PASS_ADDR + i, i < p.length() ? p[i] : 0);
    EEPROM.write(DEVICE_ID_ADDR + i, i < d.length() ? d[i] : 0);
  }
  EEPROM.write(CONFIG_FLAG_ADDR, 1);
  EEPROM.commit();
  EEPROM.end();
}


// Wi-Fi Connect
bool connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(500);
    retry++;
  }
  return WiFi.status() == WL_CONNECTED;
}


// Web config page
void setupWebConfigRoutes() {
  String scanned = "<ul><li>Scanning...</li></ul>";
  int n = WiFi.scanNetworks();
  if (n > 0) {
    scanned = "<ul>";
    for (int i = 0; i < n; i++) {
      scanned += "<li>" + WiFi.SSID(i) + " (" + String(WiFi.RSSI(i)) + " dBm)</li>";
    }
    scanned += "</ul>";
  }


  server.on("/", HTTP_GET, [=]() {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>"
                  "<title>ESP32 Wi-Fi Config</title>"
                  "<style>body { font-family: Arial; background: #f4f4f4; padding: 20px; }"
                  "form { background: #fff; padding: 20px; border-radius: 10px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }"
                  "input { margin: 5px 0; padding: 10px; width: 100%; border: 1px solid #ccc; border-radius: 5px; }"
                  "input[type=submit], button { background: #2196F3; color: white; border: none; cursor: pointer; padding: 10px; border-radius: 5px; margin-top: 10px; }"
                  "ul { list-style-type: none; padding: 0; } li { padding: 5px 0; }"
                  "</style></head><body>"
                  "<h2>ESP32 WiFi Configuration</h2>"
                  "<form action='/save'>"
                  "SSID: <input name='ssid' placeholder='WiFi Name'><br>"
                  "Password: <input name='pass' type='password' placeholder='WiFi Password'><br>"
                  "Device ID: <input name='id' placeholder='Device ID'><br>"
                  "<input type='submit' value='Save Settings'>"
                  "</form><br>"
                  "<button onclick='location.reload()'>🔄 Refresh</button>"
                  "<h3>Available Networks:</h3>" + scanned +
                  "</body></html>";
    server.send(200, "text/html", html);
  });


  server.on("/save", HTTP_GET, []() {
    String newSSID = server.arg("ssid");
    String newPASS = server.arg("pass");
    String newID = server.arg("id");

    ssid = newSSID;
    password = newPASS;
    deviceID = newID;


    writeEEPROM(ssid, password, deviceID);


    server.send(200, "text/html",
      "<!DOCTYPE html><html><head>"
      "<meta http-equiv='refresh' content='5;url=http://192.168.4.1/'>"
      "<script>setTimeout(() => window.location.href = 'http://192.168.4.1/', 5000);</script>"
      "<style>body { font-family: Arial; text-align: center; padding: 50px; background: #f0f0f0; }"
      "h2 { color: green; }</style></head><body>"
      "<h2>✅ Settings Saved!</h2><p>Rebooting into AP mode...</p>"
      "</body></html>");


    delay(5000);
    ESP.restart();
  });
}


// AP mode
void startAPMode() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(CONFIG_FLAG_ADDR, 0);  // Clear config flag
  EEPROM.commit();
  EEPROM.end();


  digitalWrite(LED_INDICATOR, HIGH);
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32_Config", "12345678");
  IPAddress ip = WiFi.softAPIP();


  showStatus("AP Mode: ESP32_Config", "IP: " + ip.toString());


  setupWebConfigRoutes();
  server.begin();


  unsigned long startTime = millis();
  while (millis() - startTime < 180000) {
    server.handleClient();
    delay(10);
  }


  ESP.restart();
}


// Setup
void setup() {
  pinMode(CONFIG_BUTTON, INPUT_PULLUP);
  pinMode(LED_INDICATOR, OUTPUT);
  digitalWrite(LED_INDICATOR, LOW);
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  showStatus("Booting...");


  readEEPROM();
  delay(1000);


  if (EEPROM.read(CONFIG_FLAG_ADDR) == 1 || digitalRead(CONFIG_BUTTON) == LOW || ssid.length() == 0) {
    startAPMode();
  }


  if (!connectWiFi()) {
    startAPMode();
  }


  digitalWrite(LED_INDICATOR, LOW);
  showStatus("WiFi: Connected", "IP: " + WiFi.localIP().toString());


  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;


  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);


  delay(1000);
  if (Firebase.ready()) {
    Serial.println("Firebase connected successfully.");
    showStatus("Firebase: Connected");
    Firebase.RTDB.setString(&fbdo, "/deviceID", deviceID);
    Firebase.RTDB.setString(&fbdo, "/lastAction", "System restarted");
  } else {
    Serial.println("Failed to connect to Firebase.");
    showStatus("Firebase: Disconnected");
  }


  server.on("/reconfigure", HTTP_GET, []() {
    Firebase.RTDB.setString(&fbdo, "/lastAction", "WiFi reconfiguration started");
    server.send(200, "text/plain", "Reconfiguring...");
    delay(1000);
    startAPMode();
  });


  server.begin();
}


// Loop
void loop() {
  server.handleClient();


  if (WiFi.status() != WL_CONNECTED) {
    showStatus("WiFi: Disconnected", "Reconnecting...");
    connectWiFi();
    return;
  }


  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 5000) {
    lastUpdate = millis();


    String msg = "";
    String action = "";


    if (Firebase.ready()) {
      if (Firebase.RTDB.getString(&fbdo, "/deviceText") && fbdo.dataType() == "string") {
        msg = fbdo.stringData();
      }


      if (Firebase.RTDB.getString(&fbdo, "/lastAction") && fbdo.dataType() == "string") {
        action = fbdo.stringData();
      }
    }


    showStatus("WiFi: Connected", "ID: " + deviceID, msg, action);
  }
}
