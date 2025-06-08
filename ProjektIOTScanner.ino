#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <PubSubClient.h>

const char* ssid     = "";
const char* password = "";

IPAddress local_IP(192, 168, 55, 125);   // Ustaw swój statyczny adres IP
IPAddress gateway(192, 168, 55, 1);      // Ustaw bramę (gateway)
IPAddress subnet(255, 255, 255, 0);     // Ustaw maskę podsieci
const bool statyczne = false; //false - off , true - on
const bool tryb = false; // true - REST , false - Mqtt
const bool debug = true; //false - off , true - on
const char* mqtt_server = "test.mosquitto.org";
const int mqtt_port = 1883;
const char* mqtt_topic = "esp32test/ble/devices";
const long interval = 1000;
unsigned int lastMsg = 0;

WiFiClient espClient;
PubSubClient client(espClient);
BLEScan* pBLEScan;
WiFiServer server(80);
struct BLEDeviceInfo {
  String name;
  int rssi;
  float distance;
};

std::vector<BLEDeviceInfo> foundDevices;

float calculateDistance(int rssi, int txPower = -59, float n = 2.8) {
    // n ≈ 2 dla otwartej przestrzeni (idealne warunki)
    // n ≈ 2.7-3.5 dla typowego biura/pomieszczenia
    // n ≈ 3.0-5.0 dla środowisk z wieloma przeszkodami
  return pow(10.0, ((float)(txPower - rssi)) / (10.0 * n));
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    String devName = advertisedDevice.getName().c_str();
    if (devName.length() == 0) return;

    int rssi = advertisedDevice.getRSSI();
    float dist = calculateDistance(rssi);

    BLEDeviceInfo info;
    info.name = devName;
    info.rssi = rssi;
    info.distance = dist;
    foundDevices.push_back(info);
    if(debug){
        Serial.printf("Detected: %s (RSSI: %d, Distance: %.2f m)\n", devName.c_str(), rssi, dist);
    }
  }
};

void callback(char *topic, byte *payload, unsigned int length) {
    if(debug){
        Serial.print("Message arrived in topic: ");
        Serial.println(topic);
        Serial.print("Message:");
        for (int i = 0; i < length; i++) {
            Serial.print((char) payload[i]);
        }
        Serial.println();
        Serial.println("-----------------------");
    }
}
void reconnect() {
    while (!client.connected()) {
        String client_id = "esp32-client-";
        client_id += String(WiFi.macAddress());
        Serial.printf("The client %s connects to the public MQTT broker\n", client_id.c_str());
        if (client.connect(client_id.c_str())) {
            if (debug) {
            Serial.println("MQTT broker connected");
            }
        } else {
            if (debug) {
            Serial.print("failed with state ");
            Serial.print(client.state());
            }
            delay(100);
        }
    }
}
void setup() {
    if(debug){
        Serial.begin(115200);
    }
    if (statyczne){
        if (!WiFi.config(local_IP, gateway, subnet)) {
            if(debug){
            Serial.println("Konfiguracja statycznego IP nie powiodła się");
            }
        }
    }
    if(debug){
        Serial.begin(115200);
        Serial.println();
        Serial.println();
        Serial.print("Connecting to ");
        Serial.println(ssid);
    }

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        if(debug){
        Serial.print(".");
        }
    }
    if(debug){
        Serial.println("");
        Serial.println("WiFi connected.");
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());
    }

    BLEDevice::init("Beacon Scanner");
    BLEDevice::setPower(ESP_PWR_LVL_P7, ESP_BLE_PWR_TYPE_SCAN);
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true); 
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
    if(tryb==false){
        client.setServer(mqtt_server, mqtt_port);
        client.setCallback(callback);   
        
    }else{
        server.begin();
    }
}

void loop() {
    if(tryb==true){
        WiFiClient client = server.available(); 
        if (client) {
            if(debug){
                Serial.println("New Client.");
            }
            String currentLine = "";
            while (client.connected()) {
                if (client.available()) {
                    char c = client.read();
                    if (c == '\n') {
                        if (currentLine.length() == 0) {
                            foundDevices.clear();
                            pBLEScan->start(1, false);  // 1 sekunda skanowania
                            client.println("HTTP/1.1 200 OK");
                            client.println("Content-type: application/json");
                            client.println();
                            String json = "{ \"devices\": [";
                            for (size_t i = 0; i < foundDevices.size(); ++i) {
                                json += "{";
                                json += "\"Device Number\": " + String(i + 1) + ",";
                                json += "\"Name\": \"" + foundDevices[i].name + "\",";
                                json += "\"RSSI\": " + String(foundDevices[i].rssi) + ",";
                                json += "\"Distance_m\": " + String(foundDevices[i].distance, 2);
                                json += "}";
                                if (i < foundDevices.size() - 1) {
                                    json += ",";
                                }
                            }
                            json += "]}";

                            client.print(json);
                            client.println();
                            break;
                        } else {
                            currentLine = "";
                        }
                    } else if (c != '\r') {
                        currentLine += c;
                    }
                }
            }
            client.stop();
            if(debug){
                Serial.println("Client Disconnected.");
            }
        }
    }
    else
    {
        if (!client.connected()) {
            reconnect();  
        }
        client.loop();
        unsigned long now = millis();
        if (now - lastMsg > interval) {
            lastMsg = now;
            foundDevices.clear();
            pBLEScan->start(1, false); // skanuj przez 1s
            pBLEScan->clearResults();  
            String json = "{ \"devices\": [";
            for (size_t i = 0; i < foundDevices.size(); ++i) {
            json += "{";
            json += "\"Device Number\": " + String(i + 1) + ",";
            json += "\"Name\": \"" + foundDevices[i].name + "\",";
            json += "\"RSSI\": " + String(foundDevices[i].rssi) + ",";
            json += "\"Distance_m\": " + String(foundDevices[i].distance, 2);
            json += "}";
            if (i < foundDevices.size() - 1) {
                json += ",";
            }
            }
            json += "]}";
            client.publish(mqtt_topic, json.c_str());
        }
    }
}