#include "RF24.h"
#include "RF24Network.h"
#include "RF24Mesh.h"
#include <SPI.h>
#include <EEPROM.h>
#include <LowPower.h>
#include "fpom.h"

#define PIN_PUMP 3
#define PIN_NRF_CE 7
#define PIN_NRF_CS 8

#define PDOWN // power down with LowPower lib
//#define READVCC // read vcc voltage
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

void setup() {
  pinMode(PIN_PUMP, OUTPUT);
  digitalWrite(PIN_PUMP, LOW);
  Serial.begin(9600);
  mesh.setNodeID(nodeID);
  // Connect to the mesh
  #ifdef NRF24
  Serial.println(F("Connecting to the mesh..."));
  mesh.begin();
  Serial.println(F("Connected!"));
  #endif
}

void readHumidity() {
  delay(200);
  humidity  = analogRead(0);
  delay(10);
  humidity += analogRead(0);
  delay(10);
  humidity += analogRead(0);
  delay(10);
  humidity += analogRead(0);
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
    Serial.println("Renewing Address");
    long mstart = millis();
    mesh.renewAddress();
    if (millis() - mstart > 10000) {
      if(radio.failureDetected){ 
        radio.begin();                       // Attempt to re-configure the radio with defaults
        radio.failureDetected = 0; 
      }
    }
    Serial.println("Address renewed!");
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
        #ifdef READVCC
          Serial.print("mV: ");
          Serial.println(readVcc());
        #endif
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
  dream(8000);
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

void loop() {
  #ifdef NRF24
  updateMesh();
  readNetwork();
  if (millis() > last_ping + 5000) {
    last_ping = millis();
    send(PING, last_ping);
  }
  #endif
}

#ifdef READVCC
long readVcc() {
  // Read 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference
  #if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
    ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
    ADMUX = _BV(MUX5) | _BV(MUX0);
  #elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
    ADMUX = _BV(MUX3) | _BV(MUX2);
  #else
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #endif  

  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA,ADSC)); // measuring

  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH  
  uint8_t high = ADCH; // unlocks both

  long result = (high<<8) | low;

  result = 1125300L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
  return result; // Vcc in millivolts
}
#endif
