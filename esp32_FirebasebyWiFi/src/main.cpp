// wysylanie -> dodac do FirebaseJson json
// odbieranie ->dodac w reciveByWiFi if else i zmienna do struct RreadData

// HA
// sensor - dodac w configuration.yaml -> sensor: json_attributes
// relay -	dodac configuration.yaml -> rest_command --> set_relay_01

#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include "PASSWORD.h"

// Sensors lib
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <Adafruit_BME280.h>

#define relay01PIN GPIO_NUM_4
#define relay02PIN GPIO_NUM_32
#define ADCvoltagePIN GPIO_NUM_36

#define DEBUG 0	// 1 = Serial ON, 0 = Serial OFF
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
// BME280
Adafruit_BME280 bme280;


struct SentData {
	float DS18B20temp = 0.0;
	float bme280Temperature = 0.0;
	float bme280Pressure = 0.0;
	float bme280Humidity = 0.0;
	float voltagePercent = 0.0;
} SentD;
struct RreadData {
	bool relay01 = false;
	bool relay02 = false;
} ReadD;

void updateSensors();
void reciveByWiFi();
void sendByWiFi(FirebaseJson &json);
void connectWiFi();
void configFirebase();

void setup(){
	DEBUG_BEGIN(115200);
	pinMode(relay01PIN, OUTPUT);
	pinMode(relay02PIN, OUTPUT);

	DS18B20sensors.begin();

	if (!bme280.begin(BME280_ADDRESS_ALTERNATE)){
		DEBUG_PRINTLN("Failed to find bme280 sensor.");
	}

	connectWiFi();
	configFirebase();
}


void loop(){
	updateSensors();

	// sending data
	FirebaseJson json;
	json.set("DS18B20temp", SentD.DS18B20temp);
	json.set("bme280Temperature", SentD.bme280Temperature);
	json.set("bme280Pressure", SentD.bme280Pressure);
	json.set("bme280Humidity", SentD.bme280Humidity);
	json.set("voltagePercent", SentD.voltagePercent);
	sendByWiFi(json);

	// setting relays
	reciveByWiFi();
	digitalWrite(relay01PIN, !ReadD.relay01);
	digitalWrite(relay02PIN, !ReadD.relay02);

	// RTC_GPIO SET
	gpio_hold_dis(relay01PIN);
	gpio_hold_dis(relay02PIN);
	gpio_deep_sleep_hold_en();
	gpio_hold_en(relay01PIN);
	gpio_hold_en(relay02PIN);
	delay(100);

	DEBUG_PRINTLN("esp_sleep_enable_timer_wakeup.");
	esp_sleep_enable_timer_wakeup(1*60*1000000);	// 1 min
	esp_deep_sleep_start();
	delay(10000);
}

void updateSensors(){
	// DS18B20
	DS18B20sensors.requestTemperatures();
	SentD.DS18B20temp = DS18B20sensors.getTempCByIndex(0);
	SentD.DS18B20temp = round(SentD.DS18B20temp * 100) / 100.0;
	DEBUG_PRINT("DS18B20temp"); DEBUG_PRINTLN(SentD.DS18B20temp);

	// BME280
	SentD.bme280Temperature = bme280.readTemperature();
	DEBUG_PRINT("Temperature = "); DEBUG_PRINTLN(SentD.bme280Temperature);
	SentD.bme280Pressure = bme280.readPressure() / 100.0F;
	DEBUG_PRINT("Pressure = "); DEBUG_PRINTLN(SentD.bme280Pressure);
	SentD.bme280Humidity = bme280.readHumidity();
	DEBUG_PRINT("Humidity = "); DEBUG_PRINTLN(SentD.bme280Humidity);

	// voltagePercent
	SentD.voltagePercent = 0.0;
	for (int i = 0; i < 10; i++) {
		SentD.voltagePercent += analogRead(ADCvoltagePIN) / 4096.0 * 100.0;
		delay(10);
	}
	SentD.voltagePercent = SentD.voltagePercent / 10.0;
	DEBUG_PRINT("ADCvoltage = "); DEBUG_PRINTLN(SentD.voltagePercent);
}

void reciveByWiFi() {
	if (Firebase.ready() && signupOK) {
		uint8_t retry = 0;
		while (retry<3) {
			if (Firebase.RTDB.getJSON(&fbdo, "HAtoESP/")) {
				FirebaseJson &json = fbdo.jsonObject();
				FirebaseJsonData jsonData;
				
				// saving data from json
				if (json.get(jsonData, "relay01") && jsonData.type == "boolean") {
					ReadD.relay01 = jsonData.boolValue; // Zapis do struktury
					DEBUG_PRINTLN("SUCCESS: 'relay01' loaded.");
				}
				else {
					DEBUG_PRINTLN("FAILED: 'relay01' not found or wrong type.");
				}

				if (json.get(jsonData, "relay02") && jsonData.type == "boolean") {
					ReadD.relay02 = jsonData.boolValue; // Zapis do struktury
					DEBUG_PRINTLN("SUCCESS: 'relay02' loaded.");
				}
				else {
					DEBUG_PRINTLN("FAILED: 'relay02' not found or wrong type.");
				}

				//
				// DODAC IF ELSE z "nowa zmienna" z bazy danych
				//
				retry = 3; // saving data from json -> OK
			} else {
				DEBUG_PRINTF("FAILED READ: %s\n", fbdo.errorReason().c_str());
				DEBUG_PRINTLN("Retrying...");
				delay(500);
				retry++;
			}
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
	//config.signer.test_mode = true; //

	DEBUG_PRINT("Firebase login");
	config.token_status_callback = tokenStatusCallback;
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
