// uncomment the line below to output debug information
//#define DEBUG_OUTPUT

// Uncomment the line below to print out information about the USB devices that attach.
#define PRINT_DEVICE_INFO


//=============================================================================
// USB Objects
//=============================================================================
USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
USBHIDParser hid1(myusb);
USBHIDParser hid2(myusb);

DMAMEM uint8_t rawhid_big_buffer[8 * 512] __attribute__((aligned(32)));
RawHIDController rawhid1(myusb, 0, rawhid_big_buffer, sizeof(rawhid_big_buffer));
USBSerialEmu seremu(myusb);

#ifdef PRINT_DEVICE_INFO
USBDriver *drivers[] = { &hub1, &hub2, &hid1, &hid2 };
#define CNT_DEVICES (sizeof(drivers) / sizeof(drivers[0]))
const char *driver_names[CNT_DEVICES] = { "Hub1", "Hub2", "HID1", "HID2" };
bool driver_active[CNT_DEVICES] = { false, false, false, false };

// Lets also look at HID Input devices
USBHIDInput *hiddrivers[] = { &rawhid1, &seremu };
#define CNT_HIDDEVICES (sizeof(hiddrivers) / sizeof(hiddrivers[0]))
const char *hid_driver_names[CNT_DEVICES] = { "RawHid1", "SerEmu" };
bool hid_driver_active[CNT_DEVICES] = { false, false };
#endif

//=============================================================================
// forward references
//=============================================================================
extern bool OnReceiveHIDData(uint32_t usage, const uint8_t *data, uint32_t len);

byte buffer[512];
byte rxBuffer[512];
uint16_t rx_size = 64;       // later will change ... hopefully

enum {CMD_NONE = -1, CMD_DATA_IMU=0, CMD_DATA_LIDAR=1, CMD_DATA_PRESS, CMD_DATA_ALL, CMD_STOP};

typedef struct {
  uint8_t type;    // type of data in buffer
  uint8_t size;
  uint8_t data[0];  // packet data.
} RawHID_packet_t;

typedef struct {
  uint8_t type;    // type of data in buffer
  uint8_t data[0];  // packet data.
} RawHID_rcv_packet_t;

//=============================================================================
// optional debug stuff
//=============================================================================
#ifdef DEBUG_OUTPUT
#define DBGPrintf Serial.printf
#else
// not debug have it do nothing
inline void DBGPrintf(...) {
}
#endif

//=============================================================================
// Send packet to USBHost Raw hid
//=============================================================================
bool send_rawhid_packet(int cmd, void *packet_data, uint16_t data_size, uint32_t timeout = 250) {
  if (!rawhid1) {
    Serial.println("No RAWHID device connected");
    return false;
  }
  DBGPrintf("lsrhid: %d %p %u %u\n", cmd, packet_data, data_size, timeout);
  memset(buffer, 0, rx_size);
  RawHID_packet_t *packet = (RawHID_packet_t *)buffer;
  packet->type = cmd;
  if (packet_data) memcpy(packet->data, packet_data, data_size);
  packet->size = data_size;

  elapsedMillis emTimeout = 0;
  for (;;) {
    if (rawhid1.sendPacket(buffer)) return true;
    if (emTimeout > timeout) {
      DBGPrintf("\t *** Timeout(%u) ***\n", (uint32_t)emTimeout);
      return false;
    }
  }
}
