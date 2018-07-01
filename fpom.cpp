/* Flower-Pot-O-Meter v2 
 *
 * this program is a bridge between an nrf24l01 mesh network and
 * an mqtt broker.
 * a node red flow reads the mqtt messages and sends commands back
 * over mqtt which in turn are relayed to the nrf24l01 mesh
 */
 
#ifndef __AVR__

#include <RF24Mesh/RF24Mesh.h>  
#include <RF24/RF24.h>
#include <RF24Network/RF24Network.h>
#include <stdlib.h> 
#include <stdint.h>
#include <mosquitto.h>
#include "fpom.h"


// RF24 config
RF24 radio(22,0, BCM2835_SPI_SPEED_1MHZ);
RF24Network network(radio);
RF24Mesh mesh(radio,network);

// mqtt config
#define MQTT_HOST "localhost"
#define MQTT_PORT 1883
struct mosquitto *mosq;

// callback on mosquitto message
void mosq_message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message) {
  uint8_t* payload = (uint8_t*)message->payload;
  uint8_t command = payload[0];
  
  printf("\tMQTT -> message '%.*s' for topic '%s'\n", message->payloadlen, payload, message->topic);
  // message: COMMAND.VALUE
  // e.g.: D.1000 -> sleep for 1000 ms
  // or:   P.3 -> water for 3 sec
  // TODO: this part has to be reworked, i am bad at C
  uint32_t value = 0;
  if (message->payloadlen > 2) {
    uint8_t value_chars[message->payloadlen-1];
    //printf("message length: %d", message->payloadlen);
    for (int i = 2; i < message->payloadlen; i++) {
      value_chars[i-2] = payload[i];
      //printf("value char: %c", payload[i]);
    }
    value_chars[sizeof(value_chars)/sizeof(uint8_t)-1] = 0;
    value = strtoul((char*)value_chars, NULL, 10);
  }
  char **topics;
  int topic_count;
  mosquitto_sub_topic_tokenise(message->topic, &topics, &topic_count);
  uint8_t nodeID = atoi(topics[topic_count-1]);
  printf("interpretation: command -> %c, value: %d, nodeID: %d\n", command, value, nodeID);
  payload_t p;
  p.command = command;
  p.value = value;
  for (int retryCount = 0; retryCount < 10; retryCount++) {
    if (retryCount > 0) {
      printf("Retrying to send data. (%d)\n", retryCount+1);
    }
    if (!mesh.write(&p, MESSAGE_TYPE, sizeof(struct payload_t), nodeID)) {
      printf("nrf24: could not send mesh data\n");
    }
    else {
      printf("\tmessage -> NRF relayed to node %d %c %d (size: %d)\n", nodeID, p.command, p.value, sizeof(struct payload_t));
      break;
    }
  }
}

void mosq_connect_callback(struct mosquitto *_mosq, void *obj, int rc) {
  switch (rc) {
  case 0: {
    printf("successfully connected to mosquitto!\n");
    int retcode = mosquitto_subscribe(mosq, NULL, "/flowers/commands/+", 0);
    if (retcode == 0) {
      printf("subscribed to %s\n", "/flowers/commands/+\n");
      return;
    }
    printf("ERROR, could not subscribe to /flowers/commands/+!!\n");
    break;
  }
  default: {
    printf("error! could not connect to mosquitto, rc: %d\n", rc);
  }
  }
}


uint32_t loop_counter = 0;

void initMesh() {
  printf("Init mesh...\n");

  // setting the data rate crashes the nrf module (on pi as well as on arduino)
  // radio.setDataRate(RF24_250KBPS); 

  // Set the nodeID to 0 for the master node
  mesh.setNodeID(0);

  // start the mesh
  mesh.begin();

  // print radio details
  radio.printDetails();

  printf("Mesh initialized\n");
}

void initMqtt() {
  printf("Init MQTT...\n");
  mosquitto_lib_init();
  mosq = mosquitto_new("raspi-nrf-bridge", true, 0);
  mosquitto_message_callback_set(mosq, mosq_message_callback);
  mosquitto_connect_callback_set(mosq, mosq_connect_callback);
  int rc = mosquitto_connect_async(mosq, MQTT_HOST, MQTT_PORT, 60);
  if (rc != MOSQ_ERR_SUCCESS) {
    printf("mqtt: could not connect mqtt, error code %d\n", rc);
    exit(1);
  }
  mosquitto_loop_start(mosq);
  printf("Init MQTT done!\n");
}

void rf24_mesh() {

  // Call network.update as usual to keep the network updated
  mesh.update();

  // In addition, keep the 'DHCP service' running on the master node so addresses will
  // be assigned to the sensor nodes
  mesh.DHCP();
  
  
  // Check for incoming data from the sensors
  while(network.available()) {
    RF24NetworkHeader header;
    network.peek(header);
    
    uint32_t res=0;
    uint16_t nodeID = 0;
    payload_t payload;
    if (header.type == MESSAGE_TYPE) {
      network.read(header,&payload,sizeof(payload)); 
      nodeID = mesh.getNodeID(header.from_node);
      printf("\tNRF24 -> message: Received %c:%d from %d\n", payload.command, payload.value, nodeID);
      char topic_template[] = "/flowers/sensors/%d";
      char topic[sizeof(topic_template)+2];
      sprintf(topic, topic_template, nodeID);
      ssize_t bufsz = snprintf(NULL, 0, "%c.%d", payload.command, payload.value);
      char res_str[bufsz];
      sprintf(res_str, "%c.%d", payload.command, payload.value);
      if (int publish_result = mosquitto_publish(mosq, 0, topic, sizeof(res_str), &res_str, 0, false) != MOSQ_ERR_SUCCESS) {
        printf("mqtt: could not publish sensor data to mosquitto for node id %d, value %d, error %d\n", nodeID, res, publish_result);
      }
      else {
        printf("\tmessage -> MQTT sent\n");
      }
    }
    else {
      network.read(header,0,0); 
      printf("nrf24: Rcv bad type %d from 0%o\n",header.type,header.from_node);     
    }
  }
  if (++loop_counter % 1000 == 0) {
    printf("loop counter: %d\n", loop_counter);
  }
}

int main(int argc, char** argv) {
  
  printf("Starting Flower-Pot-O-Meter Bridge v2\n");

  initMesh();
  initMqtt();

  while(1) {
    rf24_mesh();
    delay(2);
  }
  return 0;
}


#endif
