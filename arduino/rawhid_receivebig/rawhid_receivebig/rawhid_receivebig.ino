/* RawHID receive big 
  This example code is in the public domain.
*/

// RawHID packets are always 64 bytes
byte buffer[512];

int rx_size;

unsigned int packetCount = 0;
uint32_t last_packet_number = (uint32_t) - 1;
uint32_t packet_count = 0;
elapsedMillis em;
elapsedMillis emTotal;
bool run_active = false;

void setup() {
  while (!Serial);
  pinMode(13, OUTPUT);
  Serial.begin(9600);
  Serial.println(F("RawHID lots of input test"));
  rx_size = RawHID.rxSize();
//  rx_size = 64;
  Serial.printf("RawHid RX Size: %d\n", rx_size);
  em = 0;
  Serial.println(F("Waiting for packets"));
}


void loop() {
  int n;
  n = RawHID.recv(buffer, 0); // 0 timeout = do not wait
  if (n > 0) {
    packet_count++;
    digitalToggleFast(13);
    // check Serial numbers
    uint32_t packet_number = 0;
    for (int i = 0; buffer[i] >= '0' && buffer[i] <= '9'; i++) {
      packet_number = packet_number * 10 + buffer[i] - '0';
    }
    if (packet_number == 0) {
      Serial.println("Looks like new run started");
      last_packet_number = 0;
      packet_count = 1;
      emTotal = 0;
    } else if (packet_number != (last_packet_number + 1)) {
      Serial.printf("Missing? cnt: %u, Last: %u cur:%u\n", packet_count, packet_number, last_packet_number);
    } else if ((buffer[8] != buffer[rx_size-2]) || (buffer[rx_size-1] != '\n')) {
      Serial.printf("msg format error: %u\n", packet_count);
    }
    if (buffer[7] == '$') {
      Serial.printf("Received end marker: %u %u Time:%u\n", packet_count, packet_number,
        (uint32_t)emTotal);
    }
    last_packet_number = packet_number;
    run_active = true;
    em = 0;
    if ((packet_count & 0x3) == 0) delay(3);
    if ((packet_count & 0x3ff) == 0) Serial.print(".");
    if ((packet_count & 0xffff) == 0) Serial.println();
  } else if (run_active && (em > 1000)) {
    Serial.printf("\nTimeout: %u %u\n", packet_count, last_packet_number);
    run_active = false;
    
  }
}
