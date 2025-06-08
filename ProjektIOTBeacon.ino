#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
const String Nazwa = "TestBeacon13"; // <<< Tutaj ustawiasz nazwę beaconu
void setup() {
  Serial.begin(115200);

  // Inicjalizacja BLE
  BLEDevice::init(Nazwa); 

  BLEServer *pServer = BLEDevice::createServer();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  BLEAdvertisementData advertisementData;
  
  // Ustawiamy nazwę w pakiecie reklamowym
  advertisementData.setName(Nazwa);
  pAdvertising->setAdvertisementData(advertisementData);
  pAdvertising->setScanResponse(false); // bez dodatkowych danych
  pAdvertising->start();

  Serial.println("Beacon BLE nadawany jako 'TestBeacon'");
}

void loop() {
}
