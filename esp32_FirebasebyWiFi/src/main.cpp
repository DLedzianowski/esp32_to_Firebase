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


// wysylanie -> dodac do FirebaseJson json
// odbieranie ->dodac w reciveByWiFi if else i zmienna do struct RreadData

// HA
// sensor - dodac w configuration.yaml -> sensor: json_attributes
// relay -  dodac configuration.yaml -> rest_command --> set_relay_01
  

// z 05 trzeba to co jest na 30 dni - acces
// moze czytanie zminnych bez opozninia

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
    pinMode(LED_BUILTIN, OUTPUT);  // LED_BUILTIN
    DS18B20sensors.begin();
    if (!ina219.begin()) {
        Serial.println("Failed to find INA219 sensor.");
    }

    Serial.begin(115200);
    connectWiFi();
    configFirebase();
}


void loop(){
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi off! restart...");
        ESP.restart();
    }

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
    analogWrite(BUILTIN_LED, ReadD.relay01);

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
            } else {
                Serial.println("FAILED: 'relay01' not found or wrong type.");
            }

            //
            // DODAC IF ELSE z "nowa zmienna" z bazy danych
            //
        } else {
            Serial.printf("FAILED READ: %s\n", fbdo.errorReason().c_str());
        }
    } else {
        Serial.println("Firebase not ready or sign up failed");
    }
}

void sendByWiFi(FirebaseJson &json){
    if (Firebase.ready() && signupOK){
        //setJSONAsync() -> dla wyslania w tle
        if (!Firebase.RTDB.setJSON(&fbdo, "ESPtoHA", &json)) {
            Serial.printf("FAILED send: %s\n", fbdo.errorReason().c_str());
        }
    }
    else{
        Serial.println("Firebase not ready or sign up failed");
    }
}

void connectWiFi(){
    Serial.print("Laczenie z WiFi: ");
    Serial.println(WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 20){
        delay(500);
        Serial.print(".");
        retry++;
    }

    if (WiFi.status() != WL_CONNECTED){
        Serial.print("\nNie udalo sie polaczyc z WiFi.");
        ESP.restart();
    }
    else{
        Serial.print("Polalaczono z WiFi! --- IP: ");
        Serial.println(WiFi.localIP());
    }
}

void configFirebase(){
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;
    if(Firebase.signUp(&config, &auth, "", "")){
        Serial.println("signUp OK");
        signupOK = true;
        Firebase.begin(&config, &auth);
        Firebase.reconnectWiFi(true);
        config.token_status_callback = tokenStatusCallback;
    }
    else{
        Serial.printf("signUp failed: %s\n", config.signer.signupError.message.c_str());
    }
}