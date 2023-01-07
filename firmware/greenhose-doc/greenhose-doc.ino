#include <Firebase_ESP_Client.h>
#include <DNSServer.h>

// Provide the token generation process info.
#include "addons/TokenHelper.h"
// Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <WebSerial.h>
#include <Preferences.h>

#include <OneWire.h>
#include <DallasTemperature.h>
#include "webpage.h"

#define RELAY_ON 4
#define RELAY_OFF 5

#define LED_PIN 13

// GPIO where the DS18B20 is connected to
#define ONE_WIRE_PIN 33

#define BATTERY_PIN 36

// #define DEBUG

#define uS_TO_S_FACTOR 1000000 /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP 300      /* Time ESP32 will go to sleep (in seconds) */

#define API_KEY "AIzaSyC7Sk72AEjvyrU85-g8EpQJe3b9CFn-rvA"

// Insert RTDB URLefine the RTDB URL
#define DATABASE_URL "https://greenhouse-doc-default-rtdb.europe-west1.firebasedatabase.app"

String appVersion = "0";

Preferences conf;
AsyncWebServer server(80);
DNSServer dnsServer;

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Variable to save USER UID
String uid;

// Database main path (to be updated in setup with the user UID)
// Parent Node (to be updated in every loop)
String databasePath;
String configPath;
String dataPath;

// Database child nodes
String tempPath = "/temperature";
String timePath = "/timestamp";
String batteryPath = "/battery";


int timestamp;
FirebaseJson json;

const char* ntpServer = "pool.ntp.org";

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_PIN);

// Pass our oneWire reference to Dallas Temperature sensor
DallasTemperature sensors(&oneWire);


bool isAP = false;
bool ok = false;
bool connected = false;
String ssid;
String password;

void addServerPaths() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/html", index_html);
  });
  AsyncElegantOTA.begin(&server);  // Start AsyncElegantOTA
  WebSerial.begin(&server);
}

class CaptiveRequestHandler : public AsyncWebHandler {
public:
  CaptiveRequestHandler() {
    addServerPaths();
  }
  virtual ~CaptiveRequestHandler() {
  }

  bool canHandle(AsyncWebServerRequest* request) {
    //request->addInterestingHeader("ANY");
    return true;
  }

  void handleRequest(AsyncWebServerRequest* request) {
    AsyncResponseStream* response = request->beginResponseStream("text/html");
    response->print("<!DOCTYPE html><html><head><title>Captive Portal</title></head><body>");
    response->print("<p>This is out captive portal front page.</p>");
    response->printf("<p>You were trying to reach: http://%s%s</p>", request->host().c_str(), request->url().c_str());
    response->printf("<p>Try opening <a href='http://%s'>this link</a> instead</p>", WiFi.softAPIP().toString().c_str());
    response->print("</body></html>");
    request->send(response);
  }
};

unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    //WebSerial.println("Failed to obtain time");
    return (0);
  }
  time(&now);
  return now;
}

void recvMsg(uint8_t* data, size_t len) {
  WebSerial.println("Received Data...");
  String d = "";
  for (int i = 0; i < len; i++) {
    d += char(data[i]);
  }
  conf.putString("ssid", d.substring(0, d.indexOf(" ")));
  conf.putString("pass", d.substring(d.indexOf(" ") + 1));
  WebSerial.println(conf.getString("ssid", "Cezar"));
  WebSerial.println(conf.getString("pass", "Beggingyou"));
  ESP.restart();
  // WebSerial.print("Echo :");
  // WebSerial.println(d);
  // connected = !connected;
}

void setup(void) {
  conf.begin("greenhouse-doc", false);

  pinMode(ONE_WIRE_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  ssid = conf.getString("ssid", "Cezar");
  password = conf.getString("pass", "Beggingyou");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  int i = 30;
  // Wait for connection
  while ((WiFi.status() != WL_CONNECTED) && i) {
    delay(500);
    i--;
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }

  if (!i) {
    WiFi.disconnect();
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("GreenHouse-Doc");
    dnsServer.start(53, "*", WiFi.softAPIP());
    server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);  //only when requested from APF
    server.begin();
    isAP = true;
    digitalWrite(LED_PIN, HIGH);
  } else {
#ifdef DEBUG
    server.begin();
#endif
    addServerPaths();
    digitalWrite(LED_PIN, LOW);
  }

  WebSerial.msgCallback(recvMsg);
  sensors.begin();

  // WebSerial.println("Waiting to start ...");
  // while (connected == false) {
  //   delay(100);
  // }

  if (!isAP) {
    configTime(0, 0, ntpServer);

    config.api_key = API_KEY;

    // Assign the user sign in credentials
    auth.user.email = conf.getString("user", "tanasie.raul@gmail.com");
    auth.user.password = conf.getString("usr-pw", "beggingyou");

    // Assign the RTDB URL (required)
    config.database_url = DATABASE_URL;

    Firebase.reconnectWiFi(true);
    fbdo.setResponseSize(4096);

    // Assign the callback function for the long running token generation task */
    config.token_status_callback = tokenStatusCallback;  //see addons/TokenHelper.h

    // Assign the maximum retry of token generation
    config.max_token_generation_retry = 5;

    // Initialize the library with the Firebase authen and config
    Firebase.begin(&config, &auth);

    // Getting the user UID might take a few seconds
    while ((auth.token.uid) == "") {
      delay(1000);
    }
    // Print user UID
    uid = auth.token.uid.c_str();

    // Update database path
    databasePath = "/devices/" + String(ESP.getEfuseMac());
  }
}



// The Firebase download callback function
void rtdbDownloadCallback(RTDB_DownloadStatusInfo info) {
  if (info.status == fb_esp_rtdb_download_status_init) {
    Serial.printf("Downloading firmware file %s (%d)\n", info.remotePath.c_str(), info.size);
  } else if (info.status == fb_esp_rtdb_download_status_download) {
    Serial.printf("Downloaded %d%s, Elapsed time %d ms\n", (int)info.progress, "%", info.elapsedTime);
  } else if (info.status == fb_esp_rtdb_download_status_complete) {
    Serial.println("Update firmware completed.");
    Serial.println();
    Serial.println("Restarting...\n\n");
    delay(2000);
    ESP.restart();
  } else if (info.status == fb_esp_rtdb_download_status_error) {
    Serial.printf("Download failed, %s\n", info.errorMsg.c_str());
  }
}

void loop() {
  if (isAP) {
    dnsServer.processNextRequest();

    // if (connected) {
    int n = WiFi.scanNetworks(false, false, true, 100U);
    // WebSerial.println("scan done");
    delay(3000);
    if (n == 0) {
      // WebSerial.println("no networks found");
    } else {
      // WebSerial.print(n);
      // WebSerial.println("networks found");
      WebSerial.print("0");
      for (int i = 0; i < n; ++i) {
        // Print SSID and RSSI for each network found
        WebSerial.print(WiFi.SSID(i));
        delay(100);
      }
    }
    delay(10000);
    // }
  } else {
    if (Firebase.ready()) {

      //Get current timestamp
      timestamp = getTime();

      Firebase.RTDB.getString(&fbdo, "devices/firmwareVersion");
      String version = fbdo.stringData();
      if (version == appVersion) {
        if (!Firebase.RTDB.downloadOTA(&fbdo, F("firmware/temp_relay"), rtdbDownloadCallback))
          Serial.println(fbdo.errorReason());
      }
      sensors.requestTemperatures();
      float temperatureC = sensors.getTempCByIndex(0);
      // WebSerial.println(temperatureC);

      dataPath = "/devices/" + String(ESP.getEfuseMac()) + "/data/" + String(timestamp);


      int battReading = analogReadMilliVolts(BATTERY_PIN);
      uint8_t batteryPercentage = (battReading - 1500) / 600.0 * 100;
      json.set(batteryPath.c_str(), String(batteryPercentage) + "%");
      json.set(tempPath.c_str(), String(temperatureC));
      json.set(timePath.c_str(), String(timestamp));
      char output[100];
      Firebase.RTDB.setJSON(&fbdo, dataPath.c_str(), &json);

#ifdef DEBUG
      delay(5000);
#else
      esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
      esp_deep_sleep_start();
#endif
    } else {
      WebSerial.println("auth_fail");
    }
  }
}
