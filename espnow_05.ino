/*
  Nope
*/

#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_wifi.h>
#include <EEPROM.h>

const int dataLength = 6;//must great than 6 for transformer pair mac address
const int pairPin = 4;//Pull down when boot into pair mode
const int ledPin = 18;//led pin for data transformer display
const int masterPin = 3;//Pull down to make this device work on reciver mode(half of dataLength)

// Default Address, will replaced by eeprom
uint8_t targetAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t eepAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Define variables
uint8_t SendData[dataLength];
uint8_t ReceiveData[dataLength];
uint8_t SendDataFilter[3][dataLength];

// Define status
bool pairMode;//0=pair, 1=normal
bool isMaster;//0=master(Sender), 1=slave(Receiver)
bool ledStatus;
bool isSyncOk;
bool isEEPWrite;
uint8_t lastPackageFailCount;
uint8_t keyCount = 0;
unsigned long previousMillis = 0;

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if (status != ESP_NOW_SEND_SUCCESS) {
    Serial.println("Last Packet Delivery Fail :(");
    digitalWrite(ledPin, false);
    lastPackageFailCount += 1;
  } else {
    lastPackageFailCount = 0;
  }
}

// Callback when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  if (pairMode) { // normal receive
    bool macCompare = true;
    for (int i = 0; i < 6; i++) {
      if (mac[i] != targetAddress[i]) {
        macCompare = false; //This data is not for me
        break;
      }
    }
    if (macCompare) {// if sender is paired then accept data
      memcpy(&ReceiveData, incomingData, sizeof(ReceiveData));
      Serial.print("ReceiveData[0]: "); Serial.println(ReceiveData[0]);
      ledStatus = !ledStatus;
      digitalWrite(ledPin, ledStatus);   // toggle LED

      // Check sync status for save power
      for (int i = 0; i < dataLength; i++) {
        if (SendData[i] != ReceiveData[i]) {
          isSyncOk = !pairMode;
          Serial.print("some data wrong with:"); Serial.println(i);
          break;
        } else {
          isSyncOk = true;
        }
      }
    }
  } else { // pair mode
    memcpy(&ReceiveData, incomingData, sizeof(ReceiveData));
    //Serial.print("ReceiveData[0]: "); Serial.println(ReceiveData[0]);
    //Serial.print("Mac[0]: "); Serial.println(mac[0]);
    bool macPair = true;
    for (int i = 0; i < 6; i++) {
      if (mac[i] != ReceiveData[i]) {
        macPair = false;
        break;
      }
    }
    if (macPair) {
      if (!isEEPWrite) {
        for (int i = 0; i < 6; i++) {
          EEPROM.writeUChar(i, ReceiveData[i]);
        }
        EEPROM.commit();
        isEEPWrite = true;
        Serial.println("Pair Success!");
      }
      digitalWrite(ledPin, true);
    }
  }
}

void initMaster() {
  pinMode(35, INPUT_PULLDOWN);
}

void initSlave() {
  pinMode(5, OUTPUT);
}

void loopMaster() {
  for (int i = 0; i < 3; i++) {
    SendDataFilter[2][i] = SendDataFilter[1][i];
    SendDataFilter[1][i] = SendDataFilter[0][i];
  }
  
  bitWrite(SendDataFilter[0][0], 0, digitalRead(35));
  
  for (int i = 0; i < 3; i++) {
    if ((SendDataFilter[0][i] == SendDataFilter[1][i]) && (SendDataFilter[1][i] == SendDataFilter[2][i])) {
      if (SendData[i] != SendDataFilter[0][i]) isSyncOk = false;// if data changed then it need sync immediately
      SendData[i] = SendDataFilter[0][i];
    }
  }
}

void loopSlave() {
  if (lastPackageFailCount > 1) {// lost communication, pull all out down
    for (int i = 0; i < 3; i++) {
      ReceiveData[i] = 0;
    }
  }
  digitalWrite(5, bitRead(ReceiveData[0], 0) == 1);
}

void setup() {
  Serial.begin(115200); // Init Serial Monitor
  EEPROM.begin(6); //Init EEPROM

  // Init Public Pin
  pinMode(ledPin, OUTPUT);
  pinMode(pairPin, INPUT_PULLUP);
  pinMode(masterPin, INPUT_PULLUP);

  // Restore Target MAC From EEPROM
  for (int i = 0; i < 6; i++) {
    targetAddress[i] = EEPROM.readUChar(i * sizeof(targetAddress[0]));
    Serial.print("eep:"); Serial.print(i); Serial.print("="); Serial.println(targetAddress[i]);
  }

  isMaster = digitalRead(masterPin); //slave toggle this pin to low when start.

  if (isMaster) {
    initMaster();// Init master pin
  } else {
    initSlave();// Init slave pin
  }

  // Pair mode check when start
  if (digitalRead(pairPin) == LOW) {
    // pair, go broadcast
    for (int i = 0; i < 6; i++) {
      targetAddress[i] = 255;
      Serial.print("eep:"); Serial.print(i); Serial.print("="); Serial.println(targetAddress[i]);
    }
    pairMode = false;
    Serial.println("Working on Broadcast mode");
  } else {
    pairMode = true;
    Serial.println("Working on Point 2 Point mode");
  }

  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
  // Config LR mode
  esp_wifi_set_protocol( WIFI_IF_STA, WIFI_PROTOCOL_LR );

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Register for Send CB to get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);

  // Register peer
  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, targetAddress, 6);
  peerInfo.ifidx = WIFI_IF_STA;
  peerInfo.channel = 0;
  peerInfo.encrypt = pairMode;

  // Add peer
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  // Register for a callback function that will be called when data is received
  esp_now_register_recv_cb(OnDataRecv);
}

void loop() {
  
  if (isMaster) {
    loopMaster();
    for (int i = dataLength / 2; i < dataLength; i++) SendData[i] = ReceiveData[i];
  } else {
    loopSlave();
    for (int i = 0; i < dataLength / 2; i++) SendData[i] = ReceiveData[i];
    //digitalWrite(testLed, SendData[0] == 1);
  }

  // If in pair mode, only send self mac address.
  if (!pairMode) WiFi.macAddress(SendData);

  // Sender
  unsigned long currentMillis = millis();
  unsigned long delayTime = isSyncOk ? 1000 : 50;
  if (currentMillis - previousMillis >= delayTime) {
    previousMillis = currentMillis;
    esp_now_send(targetAddress, (uint8_t *) &SendData, sizeof(SendData));
    Serial.print("send data[0]:"); Serial.println(SendData[0]);
    if (!isMaster) isSyncOk = true;// not on sender do this cuz sender need do sync check
    //Serial.print("send data[1]:"); Serial.println(SendData[1]);
    //Serial.print("send data[2]:"); Serial.println(SendData[2]);
  }
}
