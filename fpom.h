// this part is included in the arduino code too, so 
// both programs on arduino and raspberry "talk the same language"
/* START: common definitions for arduino and pi */
const uint8_t ALERT  =  'A'; // note: currently not used
const uint8_t DREAM  =  'D'; // rPI -> arduino: Sleep for "D.XXXX" milliseconds
const uint8_t PUMP   =  'P'; // rPI -> arduino: Pump for "P.XX" seconds
const uint8_t SENSOR =  'S'; // rPI -> arduino: please send sensor data ("S.0")
                            // arduino -> rPI: sensor result ("S.XXX") 
                            // (humidity from 0: completely wet to 1023: completely dry)
const uint8_t PING   =  'K'; // node (raspi) is sending this regularily if alive and not sleeping or pumping or sensing...
const uint8_t BATTERY = 'B'; // battery level

const uint8_t MESSAGE_TYPE = 65; // nRF24Mesh message type (65 and up need to be ACKed)

struct payload_t {
  uint8_t  command;
  uint32_t value;
} __attribute__((packed)); // packed attribute needed, else length will be not correct (power of two)
/* END: common definitions for arduino and pi */
