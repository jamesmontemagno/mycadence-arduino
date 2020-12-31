#include "Arduino.h"
#include "heltec.h"
#include "BLEDevice.h"
#include "icons.h"
#include "device.h"
#include "power.h"

static boolean connected = false;

static BLERemoteCharacteristic* sensorCharacteristic;
static BLEAdvertisedDevice* device;
static BLEClient* client;
static BLEScan* scanner;

static int cadence = 0;
static unsigned long runtime = 0;
static unsigned long last_millis = 0;

static int prevCumulativeCrankRev = 0;
static int prevCrankTime = 0;
static double rpm = 0;
static double prevRPM = 0;
static int prevCrankStaleness = 0;
static int stalenessLimit = 4;

#define debug 0
#define maxCadence 120

// Called when device sends update notification
static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* data, size_t length, bool isNotify) {
  
  int cumulativeCrankRev = int((data[2] << 8) + data[1]);
  int lastCrankTime = int((data[4] << 8) + data[3]);

  if(debug)    
  {
    Serial.println("Notify callback for characteristic");
    Serial.print("cumulativeCrankRev: ");
    Serial.println(cumulativeCrankRev);
    Serial.print("lastCrankTime: ");
    Serial.println(lastCrankTime);
  }
    
  int deltaRotations = cumulativeCrankRev - prevCumulativeCrankRev;
  if (deltaRotations < 0) 
  { 
    deltaRotations += 65535; 
  }

  int timeDelta = lastCrankTime - prevCrankTime;
  if (timeDelta < 0) 
  { 
    timeDelta += 65535; 
  }

  if(debug)    
  {
    Serial.print("deltaRotations: ");
    Serial.println(deltaRotations);
    Serial.print("timeDelta: ");
    Serial.println(timeDelta);
  }
  
  // In Case Cad Drops, we use PrevRPM 
  // to substitute (up to 4 seconds before reporting 0)
  if (timeDelta != 0)
  {
      prevCrankStaleness = 0;
      double timeMins = ((double)timeDelta) / 1024.0 / 60.0;
      rpm = ((double)deltaRotations) / timeMins;
      prevRPM = rpm;
      
    if(debug)    
    {
      Serial.print("timeMins: ");
      Serial.println(timeMins);
      Serial.print("timeDelta != 0: rpm - ");
      Serial.println(rpm);
    }
  }
  else if (timeDelta == 0 && prevCrankStaleness < stalenessLimit)
  {
      rpm = prevRPM;
      prevCrankStaleness += 1;
      if(debug)    
      {
        Serial.print("timeDelta == 0 and not stale yet, rpm -");
        Serial.println(rpm);
      }
  }
  else if (prevCrankStaleness >= stalenessLimit)
  {
      rpm = 0.0;
      if(debug)    
      {
        Serial.print("stale");
        Serial.println(rpm);
      }      
  }

  prevCumulativeCrankRev = cumulativeCrankRev;
  prevCrankTime = lastCrankTime;
  
  if(debug)    
  {
    Serial.print("prevCumulativeCrankRev: ");
    Serial.println(prevCumulativeCrankRev);
    Serial.print("prevCrankTime: ");
    Serial.println(prevCrankTime);
  }
  
  cadence = (int)rpm;
  //Test
  //cadence = cadence + 2;
  
  
  if(debug) {
    Serial.print("CALLBACK(");
    Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
    Serial.print(":");
    Serial.print(length);
    Serial.print("):");
    for(int x = 0; x < length; x++) {
      if(data[x] < 16) {
        Serial.print("0");
      }
      Serial.print(data[x], HEX);
    }
    Serial.println();
  }
}

// Called on connect or disconnect
class ClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    digitalWrite(LED,HIGH);
    Serial.println("Connected!");
  }
  void onDisconnect(BLEClient* pclient) {
    connected = false;
    delete(client);
    client = nullptr;
    digitalWrite(LED,LOW);
    Serial.println("Disconnected!");
  }
};

class AdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("BLE Advertised Device found: ");
    Serial.println(advertisedDevice.toString().c_str());
    if(advertisedDevice.getName().size() > 0) {
      BLEAdvertisedDevice * d = new BLEAdvertisedDevice;
      *d = advertisedDevice;
      addDevice(d);
    }
  }
};
    
void updateDisplay() {
  Heltec.display->clear();
  
  // Runtime
  Heltec.display->setFont(ArialMT_Plain_24);
  char buf[5];
  const int minutes = int(runtime / 60000);
  itoa(minutes, buf, 10);
  Heltec.display->setTextAlignment(TEXT_ALIGN_RIGHT);
  Heltec.display->drawString(48, 0, buf);
  Heltec.display->setTextAlignment(TEXT_ALIGN_LEFT);
  Heltec.display->drawXbm(0, 4, clock_icon_width, clock_icon_height, clock_icon);
  Heltec.display->drawString(48, 0, ":");
  const int seconds = int(runtime % 60000)/1000;
  if(seconds < 10) {
    buf[0] = '0';
    itoa(seconds, &buf[1], 10);  
  } else {
    itoa(seconds, buf, 10);  
  }
  Heltec.display->drawString(54, 0, buf);

  // Cadence
  Heltec.display->drawXbm(0, 26, cadence_icon_width, cadence_icon_height, cadence_icon);
  itoa(cadence, buf, 10);
  Heltec.display->drawString(22, 22, buf);

  uint8_t progress = 0;
  if(cadence >= maxCadence)
    progress = 100;
  else
    progress = uint8_t((100 * cadence) / maxCadence);
  Heltec.display->drawProgressBar(0, 49, 126, 14, progress);

  Heltec.display->display();
}

bool connectToServer() {
  Serial.print("Connecting to ");
  Serial.println(device->getName().c_str());

  Heltec.display->clear();
  Heltec.display->setLogBuffer(10, 50);
  Heltec.display->setFont(ArialMT_Plain_10);
  Heltec.display->println("Connecting to:");
  Heltec.display->println(device->getName().c_str());
  Heltec.display->drawLogBuffer(0, 0);
  Heltec.display->display();
    
  client = BLEDevice::createClient();
  client->setClientCallbacks(new ClientCallback());
  client->connect(device);

  // Sometimes it immediately disconnects - client will be null if so
  delay(200);
  if(client == nullptr) {
    return false;
  }
  
  BLERemoteService* remoteService = client->getService(serviceUUID);
  if (remoteService == nullptr) {
    Serial.print("Failed to find service UUID: ");
    Serial.println(serviceUUID.toString().c_str());
    client->disconnect();
    return false;
  }
  Serial.println("Found device.");

  // Look for the sensor
  sensorCharacteristic = remoteService->getCharacteristic(notifyUUID);
  if (sensorCharacteristic == nullptr) {
    Serial.print("Failed to find sensor characteristic UUID: ");
    Serial.println(notifyUUID.toString().c_str());
    client->disconnect();
    return false;
  }
  sensorCharacteristic->registerForNotify(notifyCallback);
  Serial.println("Enabled sensor notifications.");

  
  Serial.println("Activated status callbacks.");

  return true;
}

void setup() {
  Serial.begin(115200);
  Serial.flush();
  delay(50);
    
  Heltec.display->init();
  Heltec.display->flipScreenVertically();
  
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  pinMode(KEY_BUILTIN, OUTPUT);
  digitalWrite(KEY_BUILTIN, HIGH);

  BLEDevice::init("");
  scanner = BLEDevice::getScan();
  scanner->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
  scanner->setInterval(1349);
  scanner->setWindow(449);
  scanner->setActiveScan(true);
}

void loop() {
  // Start scan
  if(!connected){
    Serial.println("Start Scan!");
    Heltec.display->clear();
    Heltec.display->drawXbm(64, 0, mountain_icon_width, mountain_icon_height, mountain_icon);
    Heltec.display->setLogBuffer(10, 50);
    Heltec.display->setFont(ArialMT_Plain_10);
    Heltec.display->println("Starting Scan..");
    Heltec.display->drawLogBuffer(0, 0);
    Heltec.display->display();
    scanner->start(11, false); // Scan for 10 seconds
    BLEDevice::getScan()->stop();

    device = selectDevice(); // Pick a device
    if(device != nullptr) {
      connected = connectToServer();
      if(!connected) {
        Serial.println("Failed to connect...");
        Heltec.display->println("Failed to");
        Heltec.display->println("connect...");
        Heltec.display->drawLogBuffer(0, 0);
        Heltec.display->display();
        delay(3100);
        return;
      }
    } else {
      Serial.println("No device found...");
      Heltec.display->println("No device"); 
      Heltec.display->println("found...");
      Heltec.display->drawLogBuffer(0, 0);
      Heltec.display->display();
      delay(3100);
      return;
    }
  }

  // Update timer if cadence is rolling
  if(cadence > 0) {
    unsigned long now = millis();
    runtime += now - last_millis;
    last_millis = now; 
  } else {
    last_millis = millis();  
  }
  
  delay(200); // Delay 200ms between loops.
  updateDisplay();
}
