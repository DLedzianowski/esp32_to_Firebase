#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

#include "PASSWORD.h"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

// Zmienne do wysyłania danych
int sent_nr_1 = 0;
String URL_withPacket = "";

// Zmienne do odbierania danych
int received_nr_1 = 0;
String received_text = "";


void connectWiFi();
void configFirebase();

void setup(){
    pinMode(LED_BUILTIN, OUTPUT);

    Serial.begin(9600);//default baudrate for ESP
    delay(4000);
    connectWiFi();
    configFirebase();
}

void loop(){
    if (WiFi.status() != WL_CONNECTED) {  // Jeśli straciliśmy połączenie WiFi, ponów próbę
        Serial.println("WiFi off! restart...");
        ESP.restart();
    }
    
    if (Firebase.ready() && signupOK){
        if (Firebase.RTDB.setInt(&fbdo, "Sensor/PV", sent_nr_1)){
            digitalWrite(LED_BUILTIN, HIGH);
            Serial.printf("\n%d - successfully saved to: %s\n", sent_nr_1, fbdo.dataPath());
        }
        else{
            Serial.printf("FALIED: %s\n", fbdo.errorReason());
        }
    }
    else{
    Serial.println("Firebase not ready or sign up failed");
    }
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
    delay(4900);

    sent_nr_1 += 1;
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

    if (WiFi.status() == WL_CONNECTED){
        Serial.print("Polalaczono z WiFi! --- IP: ");
        Serial.println(WiFi.localIP());
    }
    else{
        Serial.print("\nNie udalo sie polaczyc z WiFi.");
        ESP.restart();
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