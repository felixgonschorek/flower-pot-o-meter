#include <Battery.h>
#include <RF24.h>
#include <RF24Network.h>
#include "RF24Mesh.h"
#include <SPI.h>
#include <EEPROM.h>
#include <LowPower.h>
#include "fpom.h"

#define PIN_PUMP 3
#define PIN_NRF_CE 7
#define PIN_NRF_CS 8

#define PIN_SENSOR_POWER 4
#define PIN_BATTERY_AIN A1
#define PIN_BATTERY_SENSE_SWITCH 2

#define BATTERY_MIN 5000
#define BATTERY_MAX 8200


#define PDOWN // power down with LowPower lib
#define NRF24


/**
 * User Configuration: nodeID - A unique identifier for each radio. Allows addressing
 * to change dynamically with physical changes to the mesh.
 *
 * In this example, configuration takes place below, prior to uploading the sketch to the device
 * A unique value from 1-255 must be configured for each node.
 * This will be stored in EEPROM on AVR devices, so remains persistent between further uploads, loss of power, etc.
 *
 **/
#define nodeID 1

/**** Configure the nrf24l01 CE and CS pins ****/
RF24 radio(PIN_NRF_CE, PIN_NRF_CS);
RF24Network network(radio);
RF24Mesh mesh(radio, network);

uint32_t displayTimer = 0;
uint32_t humidity = 0;
Battery battery(BATTERY_MIN, BATTERY_MAX, A1);

void setup() {  
  pinMode(PIN_PUMP, OUTPUT);
  pinMode(PIN_SENSOR_POWER, OUTPUT);
  pinMode(PIN_BATTERY_SENSE_SWITCH, OUTPUT);
  digitalWrite(PIN_BATTERY_SENSE_SWITCH, LOW);
  digitalWrite(PIN_PUMP, LOW);
  digitalWrite(PIN_SENSOR_POWER, LOW);
  
  Serial.begin(9600);
  battery.begin(3300, 2.38, &sigmoidal);
  battery.onDemand(PIN_BATTERY_SENSE_SWITCH, HIGH);
  
  mesh.setNodeID(nodeID);
  // Connect to the mesh
  #ifdef NRF24
  Serial.println(F("Connecting to the mesh..."));
  if (mesh.begin()) {
    Serial.println(F("Connected!"));
  }
  else {
    Serial.println(F("Could not connect to the mesh on setup()"));
  }
  #endif
}

void readHumidity() {
  digitalWrite(PIN_SENSOR_POWER, HIGH);
  delay(200);
  humidity  = analogRead(0);
  delay(10);
  humidity += analogRead(0);
  delay(10);
  humidity += analogRead(0);
  delay(10);
  humidity += analogRead(0);
  digitalWrite(PIN_SENSOR_POWER, LOW);
  humidity /= 4;
}

void dream(uint32_t ms) {

  Serial.print("Sleeping for ");
  Serial.print(ms);
  Serial.println("ms");

  uint8_t remainder = ms % 8000;
  int x = ms / 8000;
  #ifdef NRF24
  if (!mesh.releaseAddress()) {
    Serial.println("Could not release mesh address before going to sleep!");
  }
  Serial.println("Powering Radio down");
  radio.powerDown();
  #endif
  Serial.end();
  for (int i = 0; i < x; i++) {
    #ifdef PDOWN
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF); 
    #else
    delay(8000);
    #endif
  }
  delay(remainder);
  Serial.println("Waking up!");
  Serial.begin(9600);
  Serial.println("Powering Radio up");
  #ifdef NRF24
  radio.powerUp();
  updateMesh();
  #endif
  Serial.println("Wake UP Done");
  
}

#ifdef NRF24
void updateMesh() {
  //Serial.println("Update Mesh...");
  mesh.update();
  if (!mesh.checkConnection()) {
    //refresh the network address
    Serial.println("Connection check failed, renewing address");
    long mstart = millis();
    mesh.renewAddress();
    if (millis() - mstart > 10000) {
      Serial.println("Renew took > 10 secs... failure?");
      Serial.println("Restarting mesh...");
      // Attempt to re-configure the radio with defaults
      if (mesh.begin() == false) {
        Serial.println("Could not restart mesh :-/");
      }
    }
    else {
      Serial.println("Address renewed!");
    }
  }
}

void readNetwork() {
  while (network.available()) {
    Serial.println("available!");
    RF24NetworkHeader header;
    network.peek(header);
    switch(header.type) {
      case MESSAGE_TYPE:
        payload_t payload;
        payload.value = 0;
        network.read(header, &payload, sizeof(payload));
        Serial.print("Received command ");
        switch (payload.command) {
          case ALERT:
            Serial.print("ALERT");
            break;
          case DREAM:
            Serial.print("DREAMING for ");
            Serial.print(payload.value);
            Serial.println("ms");
            dream(payload.value);
            break;
          case PUMP:
            Serial.println("PUMP");
            pump(payload.value);
            break;
          case SENSOR:
            Serial.println("SENSOR");
            readHumidity();
            send(SENSOR, humidity);
            break;
          default:
            Serial.print("UNKNOWN: ");
            Serial.println(payload.command);
        }
        break;
      default:
        Serial.print("Unknown message type: ");
        Serial.println(header.type);
        network.read(header, 0, 0);
        break;
    }
  }
}
#endif

void pump(int seconds) {
  Serial.print("Pumping for ");
  Serial.print(seconds);
  Serial.println(" seconds...");
  digitalWrite(PIN_PUMP, HIGH);
  delay(seconds * 1000);
  digitalWrite(PIN_PUMP, LOW);
  delay(5);
  dream(8000*4);
}


void send(uint8_t command, uint32_t value) {
  payload_t payload;
  payload.value = value;
  payload.command = command;
  
  if (!mesh.write(&payload, MESSAGE_TYPE, sizeof(payload))) {
    // If a write fails, check connectivity to the mesh network
    if ( ! mesh.checkConnection() ) {
      //refresh the network address
      Serial.println("Renewing Address");
      mesh.renewAddress();
    } else {
      Serial.println("Re-Sending command");
      send(command, value); // 
    }
  } else {
    Serial.print("Send OK: ");
    Serial.print(command);
    Serial.print(", ");
    Serial.println(value);
  }
}

uint32_t last_ping = 0;

void readBattery() {
  Serial.print("Battery voltage is ");
  Serial.print(battery.voltage());
  Serial.print(" (");
  Serial.print(battery.level(battery.voltage()));
  Serial.println("%)");
}

void loop() {
  #ifdef NRF24
  updateMesh();
  readNetwork();
  if (millis() > last_ping + 60000) {
    last_ping = millis();
    send(BATTERY, battery.voltage());
    send(PING, last_ping);
    readBattery();
  }
  #endif
  
}



