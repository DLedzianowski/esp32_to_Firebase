// wysylanie -> dodac do FirebaseJson json
// odbieranie ->dodac w reciveByWiFi if else i zmienna do struct RreadData

// HA
// sensor - dodac w configuration.yaml -> sensor: json_attributes
// relay -  dodac configuration.yaml -> rest_command --> set_relay_01

#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include "PASSWORD.h"

// Sensors lib
#include <OneWire.h>
#include <DallasTemperature.h>
//#include <Wire.h>
#include <Adafruit_INA219.h>

// #define relay01PIN 14

#define DEBUG 0  // 1 = Serial ON, 0 = Serial OFF
#if DEBUG
  #define DEBUG_BEGIN(baud) Serial.begin(baud)
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_PRINT(y) Serial.print(y)
  #define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
  #define DEBUG_BEGIN(baud)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINT(y)
  #define DEBUG_PRINTF(...)
#endif

// Firebase
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
bool signupOK = false;
// DS18B20
#define DS18B20PIN 15
OneWire oneWire(DS18B20PIN);
DallasTemperature DS18B20sensors(&oneWire);
// INA219
Adafruit_INA219 ina219;


struct SentData {
    float DS18B20temp = 0.0;
    float voltage = 0.0;
    float current = 0.0;
    float power = 0.0;
}SentD;
struct RreadData {
    bool relay01 = false;
}ReadD;


void updateSensors();
void reciveByWiFi();
void sendByWiFi(FirebaseJson &json);
void connectWiFi();
void configFirebase();


void setup(){
    DEBUG_BEGIN(115200);
    pinMode(LED_BUILTIN, OUTPUT);  // LED_BUILTIN

    analogWrite(LED_BUILTIN, true);

    DS18B20sensors.begin();
    if (!ina219.begin()) {
        DEBUG_PRINTLN("Failed to find INA219 sensor.");
    }

    connectWiFi();
    configFirebase();
}


void loop(){
    updateSensors();

    // sending data
    FirebaseJson json;
    json.set("voltage_V", SentD.voltage);
    json.set("current_mA", SentD.current);
    json.set("power_mW", SentD.power);
    json.set("DS18B20temp", SentD.DS18B20temp);
    sendByWiFi(json);

    // setting relays
    reciveByWiFi();
    analogWrite(LED_BUILTIN, ReadD.relay01);
    
    analogWrite(LED_BUILTIN, false);

    DEBUG_PRINTLN("esp_sleep_enable_timer_wakeup.");
    esp_sleep_enable_timer_wakeup(1*60*1000000);  // 1 min
    esp_deep_sleep_start();
    delay(10000);
}

void updateSensors(){
    // DS18B20
    DS18B20sensors.requestTemperatures();
    SentD.DS18B20temp = DS18B20sensors.getTempCByIndex(0);
    SentD.DS18B20temp = round(SentD.DS18B20temp * 100) / 100.0;

    // ina219
    SentD.voltage = ina219.getBusVoltage_V();
    SentD.current = ina219.getCurrent_mA();
    SentD.power = ina219.getPower_mW();
}

void reciveByWiFi() {
    if (Firebase.ready() && signupOK) {
        if (Firebase.RTDB.getJSON(&fbdo, "HAtoESP/")) {
            FirebaseJson &json = fbdo.jsonObject();
            FirebaseJsonData jsonData;
            
            // saving data from json
            if (json.get(jsonData, "relay01") && jsonData.type == "boolean") {
                ReadD.relay01 = jsonData.boolValue; // Zapis do struktury
                DEBUG_PRINTLN("SUCCESS: 'relay01' loaded.");
            } else {
                DEBUG_PRINTLN("FAILED: 'relay01' not found or wrong type.");
            }

            //
            // DODAC IF ELSE z "nowa zmienna" z bazy danych
            //
        } else {
            DEBUG_PRINTF("FAILED READ: %s\n", fbdo.errorReason().c_str());
        }
    } else {
        DEBUG_PRINTLN("Firebase not ready or sign up failed");
    }
}

void sendByWiFi(FirebaseJson &json){
    if (Firebase.ready() && signupOK){
        //setJSONAsync() -> dla wyslania w tle
        if (!Firebase.RTDB.setJSON(&fbdo, "ESPtoHA", &json)) {
            DEBUG_PRINTF("FAILED send: %s\n", fbdo.errorReason().c_str());
        }
        else{
            DEBUG_PRINTLN("sendByWiFi SUCCESS");
        }
    }
    else{
        DEBUG_PRINTLN("Firebase not ready or sign up failed");
    }
}

void connectWiFi(){
    DEBUG_PRINT("Laczenie z WiFi: ");
    DEBUG_PRINTLN(WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 20){
        delay(500);
        DEBUG_PRINT(".");
        retry++;
    }

    if (WiFi.status() != WL_CONNECTED){
        DEBUG_PRINT("\nNie udalo sie polaczyc z WiFi.");
        ESP.restart();
    }
    else{
        DEBUG_PRINT("Polalaczono z WiFi! --- IP: ");
        DEBUG_PRINTLN(WiFi.localIP());
    }
}

void configFirebase(){
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;
    auth.user.email = USER_EMAIL;
    auth.user.password = USER_EMAIL_PASSWORD;

    DEBUG_PRINT("Firebase login");
    //config.token_status_callback = tokenStatusCallback;
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);

    int Firebase_try = 10;
    while (!Firebase.ready() && Firebase_try < 10) {
        DEBUG_PRINT(".");
        delay(500);
        Firebase_try++;
    }

    if (Firebase.ready()) {
        DEBUG_PRINTLN(" SUCCES");
        signupOK = true;
    } else {
        DEBUG_PRINTLN(" FAILED");
        signupOK = false;
    }
} 