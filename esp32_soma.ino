/**
   A BLE client example that is rich in capabilities.
*/

#include "BLEDevice.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include "cy_wifi_cfg.h"
#include "cy_mqtt_cfg.h"

#define SOMA_ADDR "E5:46:D7:AE:93:20"
#define SOMA_ADDR1 "D8:77:D5:4E:A9:6B"
//  # battery
// The remote service we wish to connect to.
static BLEUUID servUUID_batt("0000180f-0000-1000-8000-00805f9b34fb");
// The characteristic of the remote service we are interested in.
static BLEUUID charUUID_batt("00002a19-0000-1000-8000-00805f9b34fb");

//  # motor control
// The remote service we wish to connect to.
static BLEUUID servUUID_mot("00001861-B87F-490C-92CB-11BA5EA5167C");
// The characteristic of the remote service we are interested in.
static BLEUUID charUUID_mot_stat("00001525-B87F-490C-92CB-11BA5EA5167C");
static BLEUUID charUUID_mot_ctrl("00001530-B87F-490C-92CB-11BA5EA5167C");
static BLEUUID charUUID_mot_targ("00001526-B87F-490C-92CB-11BA5EA5167C");
//  MOTOR_MOVE_UP = 69
//  MOTOR_MOVE_DOWN = 96

static BLEAddress *pServerAddress;
static BLEAddress *pServerAddress1;
static BLEAddress somaAddress(SOMA_ADDR);
static BLEAddress somaAddress1(SOMA_ADDR1);
static boolean doConnect = false;
static boolean connected = false;
static boolean doConnect1 = false;
static boolean scan1 = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;

#define MQTT_BASE "soma/"
#define MQTT_DEV_BASE MQTT_BASE SOMA_ADDR "/"
#define MQTT_LWT MQTT_DEV_BASE "LWT"
#define MQTT_BATTERY MQTT_DEV_BASE "battery"
#define MQTT_STATE MQTT_DEV_BASE "state"
//#define MQTT_LIGHT MQTT_DEV_BASE "light"
//#define MQTT_CONDUCTIVITY MQTT_DEV_BASE "conductivity"

WiFiClient espClient;
PubSubClient client(espClient);

bool connectToServer(BLEAddress pAddress) {
  Serial.println("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
  Serial.print("Forming a connection to ");
  Serial.println(pAddress.toString().c_str());

  BLEClient*  pClient  = BLEDevice::createClient();
  Serial.println(" - Created client");

  // Connect to the remove BLE Server.
  pClient->connect(pAddress);

  if ( pClient->isConnected() ) {
    Serial.println(" - Connected to server");

    //~~~~~~~~ Battery ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService = pClient->getService(servUUID_batt);
    if (pRemoteService == nullptr) {
      Serial.print("Failed to find our service UUID: ");
      Serial.println(servUUID_batt.toString().c_str());
      return false;
    }
    Serial.println(" - Found our service for battery");

    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID_batt);
    if (pRemoteCharacteristic == nullptr) {
      Serial.print("Failed to find our characteristic UUID: ");
      Serial.println(charUUID_batt.toString().c_str());
      return false;
    }
    Serial.println(" - Found our characteristic for battery");

    // Read the value of the characteristic.
    std::string value = pRemoteCharacteristic->readValue();
    Serial.print("The characteristic value was: ");
    const char *val = value.c_str();

    Serial.print("Hex: ");
    for (int i = 0; i < 1; i++) {
      Serial.print((int)val[i], HEX);
      Serial.print(" ");
    }
    Serial.println(" ");

    char buffer[64];
    float batt = (val[0] );
    Serial.print("Battery: ");
    Serial.println(batt);
    snprintf(buffer, 64, "%f", batt);
    client.publish(MQTT_BATTERY, buffer);

    delay(20);

    //~~~~~~~~ Motor ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // Obtain a reference to the service we are after in the remote BLE server.
    pRemoteService = pClient->getService(servUUID_mot);
    if (pRemoteService == nullptr) {
      Serial.print("Failed to find our service UUID: ");
      Serial.println(servUUID_mot.toString().c_str());
      return false;
    }
    Serial.println(" - Found our service for motor");


    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID_mot_stat);
    if (pRemoteCharacteristic == nullptr) {
      Serial.print("Failed to find our characteristic UUID: ");
      Serial.println(charUUID_mot_stat.toString().c_str());
      return false;
    }
    Serial.println(" - Found our characteristic for motor state");

    // Read the value of the characteristic.
    std::string value1 = pRemoteCharacteristic->readValue();
    Serial.print("The characteristic value was: ");
    const char *val1 = value1.c_str();

    Serial.print("Hex: ");
    for (int i = 0; i < 1; i++) {
      Serial.print((int)val1[i], HEX);
      Serial.print(" ");
    }
    Serial.println(" ");

    float mot_stat = (val1[0] );
    Serial.print("Motor state: ");
    Serial.println(mot_stat);
    snprintf(buffer, 64, "%f", mot_stat);
    client.publish(MQTT_STATE, buffer);

    // Obtain a reference to the characteristic in the service of the remote BLE server.
    //pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID_mot_targ);
    //if (pRemoteCharacteristic == nullptr) {
    //  Serial.print("Failed to find our characteristic UUID: ");
    //  Serial.println(charUUID_mot_targ.toString().c_str());
    //  return false;
    //}
    //Serial.println(" - Found our characteristic for motor target");

    //String newValue = String("50");
    //Serial.println("Setting new characteristic value to \"" + newValue + "\"");
    // Set the characteristic's value to be the array of bytes that is actually a string.
    // pRemoteCharacteristic->writeValue(newValue.c_str(), newValue.length());

    pClient->disconnect();

    delay(500);
    if ( !pClient->isConnected() ) {
      Serial.println(" - Disconnected from server");
      Serial.println("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
    }
  }
  else {
    Serial.println(" - Connection failed");
  }
}
/**
   Scan for BLE servers and find the first one that advertises the service we are looking for.
*/
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    /**
        Called for each advertising BLE server.
    */
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      Serial.print("BLE Advertised Device found: ");
      Serial.println(advertisedDevice.toString().c_str());

      if ( advertisedDevice.getAddress().toString() == somaAddress.toString() ) {
        pServerAddress = new BLEAddress(advertisedDevice.getAddress());
        Serial.println(pServerAddress->toString().c_str());


        // We have found a device, let us now see if it contains the service we are looking for.
        Serial.println("Found our device 0!");

        doConnect = true;

      } // Found our server


      if ( advertisedDevice.getAddress().toString() == somaAddress1.toString() ) {
        pServerAddress1 = new BLEAddress(advertisedDevice.getAddress());
        Serial.println(pServerAddress->toString().c_str());

        // We have found a device, let us now see if it contains the service we are looking for.
        Serial.println("Found our device 1!");

        doConnect1 = true;

      } // Found our server

      if ( doConnect && doConnect1 ) {
        advertisedDevice.getScan()->stop();
      }

    }

}; // MyAdvertisedDeviceCallbacks


void setup() {
  Serial.begin(115200);

  Serial.println("Starting Arduino BLE Client application...");
  BLEDevice::init("");

  Serial.println("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning and start the
  // scan to run for 30 seconds.
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(30);

  Serial.println("Connecting WiFi...");
  WiFi.begin(wifi_ssid, wifi_pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  client.setServer(mqtt_server, 1883);
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("somaClient", mqtt_user, mqtt_pass, MQTT_LWT, 0, true, "offline")) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }


} // End of setup.


// This is the Arduino main loop function.
void loop() {

  client.loop();

  // If the flag "doConnect" is true then we have scanned for and found the desired
  // BLE Server with which we wish to connect.  Now we connect to it.  Once we are
  // connected we set the connected flag to be true.
  if ( doConnect && doConnect1 ) {

    if (doConnect == true) {
      connectToServer(*pServerAddress);
      //doConnect = false;
    }

    if (doConnect1 == true) {
      connectToServer(*pServerAddress1);
      //doConnect1 = false;
    }

  }

  client.loop();

  delay(1800000); // Delay a second between loops.
  // connectToServer(*pServerAddress);
} // End of loop
