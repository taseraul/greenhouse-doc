#include <Firebase_ESP_Client.h>
#include <DNSServer.h>

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <WebSerial.h>
#include <Preferences.h>
#include "SPIFFS.h"

#include <OneWire.h>
#include <DallasTemperature.h>

#define RELAY_ON 4
#define RELAY_OFF 16
#define CONF_PIN 17
#define LED_PIN 13
#define ONE_WIRE_PIN 33

#define BATTERY_PIN 36

#define uS_TO_MIN_FACTOR 60000000
#define TIME_TO_SLEEP 300

#define API_KEY "AIzaSyC7Sk72AEjvyrU85-g8EpQJe3b9CFn-rvA"
#define DATABASE_URL "https://greenhouse-doc-default-rtdb.europe-west1.firebasedatabase.app"

OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature sensors(&oneWire);

const char* ntpServer = "pool.ntp.org";

String appVersion = "0.3";

typedef struct {
  uint16_t timeout;
  uint32_t nextRelayChange;
  bool nextState;
} DeviceInfo;

DeviceInfo devInfo;

Preferences conf;
AsyncWebServer server(80);
DNSServer dnsServer;

String uid;
String configPath;
String dataPath;
FirebaseJson json;
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

int timestamp;
bool isAP = false;
bool ok = false;
bool connected = false;
String ssid;
String password;

void addServerPaths() {
  server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(SPIFFS, "/index.html", String(), false);
  });
  AsyncElegantOTA.begin(&server);
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
    request->send(SPIFFS, "/index.html", String(), false);
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
  String d = "";
  for (int i = 0; i < len; i++) {
    d += char(data[i]);
  }
  if (isAP) {
    conf.putString("ssid", d.substring(0, d.indexOf(" ")));
    conf.putString("pass", d.substring(d.indexOf(" ") + 1));
    WebSerial.println(conf.getString("ssid", "Cezar"));
    WebSerial.println(conf.getString("pass", "Beggingyou"));
    ESP.restart();
  }
}

void setup(void) {
  conf.begin("greenhouse-doc", false);

  pinMode(ONE_WIRE_PIN, INPUT_PULLUP);
  pinMode(CONF_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  pinMode(RELAY_ON, OUTPUT);
  pinMode(RELAY_OFF, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  if (digitalRead(CONF_PIN)) {
    ssid = conf.getString("ssid", "Cezar");
    password = conf.getString("pass", "Beggingyou");

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    while ((WiFi.status() != WL_CONNECTED)) {
      delay(500);
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }
    digitalWrite(LED_PIN, LOW);
#if 1
    SPIFFS.begin(true);
    WebSerial.msgCallback(recvMsg);
    addServerPaths();
    server.begin();
#endif
  }

  else {
    SPIFFS.begin(true);
    WiFi.disconnect();
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("GreenHouse-Doc");
    WebSerial.msgCallback(recvMsg);
    dnsServer.start(53, "*", WiFi.softAPIP());
    server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);  //only when requested from APF
    server.begin();
    isAP = true;
    digitalWrite(LED_PIN, HIGH);
  }

  sensors.begin();

  if (!isAP) {
    configTime(0, 0, ntpServer);
    config.api_key = API_KEY;
    auth.user.email = conf.getString("user", "tanasie.raul@gmail.com");
    auth.user.password = conf.getString("usr-pw", "beggingyou");
    config.database_url = DATABASE_URL;
    Firebase.reconnectWiFi(true);
    fbdo.setResponseSize(4096);
    config.token_status_callback = tokenStatusCallback;
    config.max_token_generation_retry = 5;
    Firebase.begin(&config, &auth);
    while ((auth.token.uid) == "") {
      delay(1000);
    }
    uid = auth.token.uid.c_str();
  }
}

void sendDiscoverdAp() {
  dnsServer.processNextRequest();
  int n = WiFi.scanNetworks(false, false, true, 100U);
  if (n == 0) {
  } else {
    WebSerial.print("0");
    for (int i = 0; i < n; ++i) {
      WebSerial.print(WiFi.SSID(i));
      delay(10);
    }
  }
}

void fcsDownloadCallback(FCS_DownloadStatusInfo info) {
  if (info.status == fb_esp_fcs_download_status_complete) {
    digitalWrite(LED_PIN, LOW);
    delay(50);
    digitalWrite(LED_PIN, HIGH);
    delay(50);
    digitalWrite(LED_PIN, LOW);
    delay(50);
    digitalWrite(LED_PIN, HIGH);
    delay(50);
    digitalWrite(LED_PIN, LOW);
    delay(50);
    digitalWrite(LED_PIN, HIGH);
    delay(50);
    digitalWrite(LED_PIN, LOW);
    delay(50);
    digitalWrite(LED_PIN, HIGH);
    delay(2000);
    ESP.restart();
  }
}

void checkFw() {
  Firebase.RTDB.getString(&fbdo, "devices/firmwareVersion/temp_relay");
  String version = fbdo.stringData();
  if (version != appVersion) {
    Firebase.Storage.downloadOTA(&fbdo, F("greenhouse-doc.appspot.com"), "firmware/temp_relay/firmware.bin", fcsDownloadCallback);
  }
}

void getDeviceConf() {
  String devicePath = "/devices/" + String(ESP.getEfuseMac());
  String userDevicesPath = "/users/" + uid + "/devices";
  FirebaseJson deviceJson;
  FirebaseJsonData jsonData;
  FirebaseJsonArray ownedDevices;
  int i;

  Firebase.RTDB.getString(&fbdo, devicePath + "/properties");
  if (fbdo.stringData() == String("")) {
    deviceJson.set("/properties/name", String("Generic temperature-relay"));
    deviceJson.set("/properties/owner", uid);
    deviceJson.set("/properties/type/[0]", "temperature");
    deviceJson.set("/properties/type/[1]", "relay");
    deviceJson.set("/properties/updateInterval", 5);
    deviceJson.set("/properties/relay/nextTrigger", 0);
    deviceJson.set("/properties/relay/nextState", false);
    deviceJson.set("/properties/relay/state", false);

    Firebase.RTDB.setJSON(&fbdo, devicePath.c_str(), &deviceJson);

    Firebase.RTDB.getArray(&fbdo, userDevicesPath, &ownedDevices);
    for (i = 0; i < ownedDevices.size(); i++) {
      ownedDevices.get(jsonData, i);
      if (jsonData.to<String>() == String(ESP.getEfuseMac()))
        break;
    }
    if (i == ownedDevices.size()) {
      ownedDevices.add(String(ESP.getEfuseMac()));
      Firebase.RTDB.setArray(&fbdo, userDevicesPath.c_str(), &ownedDevices);
    }
    devInfo.nextState = false;
    devInfo.nextRelayChange = 0;
    devInfo.timeout = 5;
  } else {
    fbdo.jsonObject().get(jsonData, "/relay/nextState");
    devInfo.nextState = jsonData.to<bool>();
    fbdo.jsonObject().get(jsonData, "/relay/nextTrigger");
    devInfo.nextRelayChange = jsonData.to<uint32_t>();
    fbdo.jsonObject().get(jsonData, "/updateInterval");
    devInfo.timeout = jsonData.to<uint16_t>();
  }
}

void loop() {
  if (isAP) {
    sendDiscoverdAp();
    delay(10000);
  } else {
    if (Firebase.ready()) {
      checkFw();
      getDeviceConf();
      timestamp = getTime();
      sensors.requestTemperatures();
      float temperatureC = sensors.getTempCByIndex(0);

      dataPath = "/devices/" + String(ESP.getEfuseMac()) + "/data/" + String(timestamp);

      int battReading = analogReadMilliVolts(BATTERY_PIN);
      uint8_t batteryPercentage = (battReading - 1500) / 600.0 * 100;
      json.set("/battery", String(batteryPercentage) + "%");
      json.set("/temperature", String(temperatureC));
      json.set("/timestamp", String(timestamp));

      Firebase.RTDB.setJSON(&fbdo, dataPath.c_str(), &json);

      if (timestamp > devInfo.nextRelayChange) {
        if (devInfo.nextState) {
          digitalWrite(RELAY_ON, HIGH);
          delay(200);
          digitalWrite(RELAY_ON, LOW);
        } else {
          digitalWrite(RELAY_OFF, HIGH);
          delay(200);
          digitalWrite(RELAY_OFF, LOW);
        }
        Firebase.RTDB.setBool(&fbdo, "/devices/" + String(ESP.getEfuseMac()) + "/properties/relay/state", devInfo.nextState);
      }

      esp_sleep_enable_timer_wakeup(devInfo.timeout * uS_TO_MIN_FACTOR);
      esp_deep_sleep_start();
    }
  }
}
