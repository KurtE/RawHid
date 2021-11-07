// Simple test of USB Host Mouse/Keyboard
//
// This example is in the public domain

#include "USBHost_t36.h"


USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
USBHIDParser hid1(myusb);
USBHIDParser hid2(myusb);

DMAMEM uint8_t rawhid_big_buffer[2048] __attribute__ ((aligned(32)));
RawHIDController rawhid1(myusb, 0, rawhid_big_buffer, sizeof(rawhid_big_buffer));
USBSerialEmu seremu(myusb);

USBDriver *drivers[] = {&hub1, &hub2, &hid1, &hid2};
#define CNT_DEVICES (sizeof(drivers)/sizeof(drivers[0]))
const char * driver_names[CNT_DEVICES] = {"Hub1", "Hub2", "HID1" , "HID2"};
bool driver_active[CNT_DEVICES] = {false, false, false, false};

// Lets also look at HID Input devices
USBHIDInput *hiddrivers[] = {&rawhid1, &seremu};
#define CNT_HIDDEVICES (sizeof(hiddrivers)/sizeof(hiddrivers[0]))
const char * hid_driver_names[CNT_DEVICES] = {"RawHid1", "SerEmu"};
bool hid_driver_active[CNT_DEVICES] = {false, false};

#define FILE_SIZE 22400000ul
int packet_size;
int count_packets;

int packet_num = -1; // say we are not active

Stream* SerialEmuEcho;

void setup()
{
  while (!Serial) ; // wait for Arduino Serial Monitor

  if (CrashReport) {
    Serial.print(CrashReport);
  }
  Serial.println("\n\nUSB Host Testing");
  Serial.println(sizeof(USBHub), DEC);
#if defined(USB_DUAL_SERIAL) || defined(USB_TRIPLE_SERIAL)
  SerialUSB1.begin(115200);
  SerialEmuEcho = &SerialUSB1;  
#else
  SerialEmuEcho = &Serial;  
#endif  


  myusb.begin();

  rawhid1.attachReceive(OnReceiveHidData);
}

uint8_t buf[512];

void loop()
{
  myusb.Task();
  UpdateActiveDeviceInfo();
  if (Serial.available()) {
    while (Serial.read() != -1) ;
    if (rawhid1 && (packet_num == -1)) {
      packet_num = 0;
      packet_size = rawhid1.txSize();
      count_packets = FILE_SIZE / packet_size;

      Serial.println("Start sending rawhid data");
    } else {
      Serial.println("End sending Rawhid data");
    }
  }

  if (seremu) {
    uint8_t emu_buffer[64];
    uint16_t cb;
    if ((cb = seremu.available())) {
      if (cb > sizeof(emu_buffer)) cb = sizeof(emu_buffer);
      int rd = seremu.readBytes(emu_buffer, cb);
      SerialEmuEcho->write(emu_buffer, rd);
    }
  }

  // See if we have some RAW data
  if (rawhid1 && (packet_num >= 0) ) {
    memset(buf, 'A' + (packet_num & 0xf), packet_size) ;
    snprintf((char *)buf, sizeof(buf), "%07u", packet_num);
    buf[7] = (packet_num == (count_packets - 1)) ? '$' : ' ';
    buf[packet_size - 1] = '\n';
    if (rawhid1.sendPacket(buf, packet_size)) {
      if ((packet_num & 0x1ff) == 0) Serial.print("+");
      if ((packet_num & 0xffff) == 0) Serial.println();
      packet_num++;
      if (packet_num == count_packets) {
        Serial.println("\nDone...");
        packet_num = -1;
      }
    }
  }
}

// check to see if the device list has changed:
void UpdateActiveDeviceInfo() {
  for (uint8_t i = 0; i < CNT_DEVICES; i++) {
    if (*drivers[i] != driver_active[i]) {
      if (driver_active[i]) {
        Serial.printf("*** Device %s - disconnected ***\n", driver_names[i]);
        driver_active[i] = false;
      } else {
        Serial.printf("*** Device %s %x:%x - connected ***\n", driver_names[i], drivers[i]->idVendor(), drivers[i]->idProduct());
        driver_active[i] = true;

        const uint8_t *psz = drivers[i]->manufacturer();
        if (psz && *psz) Serial.printf("  manufacturer: %s\n", psz);
        psz = drivers[i]->product();
        if (psz && *psz) Serial.printf("  product: %s\n", psz);
        psz = drivers[i]->serialNumber();
        if (psz && *psz) Serial.printf("  Serial: %s\n", psz);

        // Note: with some keyboards there is an issue that they don't output in boot protocol mode
        // and may not work.  The above code can try to force the keyboard into boot mode, but there
        // are issues with doing this blindly with combo devices like wireless keyboard/mouse, which
        // may cause the mouse to not work.  Note: the above id is in the builtin list of
        // vendor IDs that are already forced
      }
    }
  }

  for (uint8_t i = 0; i < CNT_HIDDEVICES; i++) {
    if (*hiddrivers[i] != hid_driver_active[i]) {
      if (hid_driver_active[i]) {
        Serial.printf("*** HID Device %s - disconnected ***\n", hid_driver_names[i]);
        hid_driver_active[i] = false;
      } else {
        Serial.printf("*** HID Device %s %x:%x - connected ***\n", hid_driver_names[i], hiddrivers[i]->idVendor(), hiddrivers[i]->idProduct());
        hid_driver_active[i] = true;

        const uint8_t *psz = hiddrivers[i]->manufacturer();
        if (psz && *psz) Serial.printf("  manufacturer: %s\n", psz);
        psz = hiddrivers[i]->product();
        if (psz && *psz) Serial.printf("  product: %s\n", psz);
        psz = hiddrivers[i]->serialNumber();
        if (psz && *psz) Serial.printf("  Serial: %s\n", psz);
        if (hiddrivers[i] == &seremu) {
          Serial.printf("   RX Size:%u TX Size:%u\n", seremu.rxSize(), seremu.txSize());
        }
        if (hiddrivers[i] == &rawhid1) {
          Serial.printf("   RX Size:%u TX Size:%u\n", rawhid1.rxSize(), rawhid1.txSize());
        }
      }
    }
  }

}
bool OnReceiveHidData(uint32_t usage, const uint8_t *data, uint32_t len) {
  Serial.print("RawHID data: ");
  Serial.println(usage, HEX);
  while (len) {
    uint8_t cb = (len > 16) ? 16 : len;
    const uint8_t *p = data;
    uint8_t i;
    for (i = 0; i < cb; i++) {
      Serial.printf("%02x ", *p++);
    }
    Serial.print(": ");
    for (i = 0; i < cb; i++) {
      Serial.write(((*data >= ' ') && (*data <= '~')) ? *data : '.');
      data++;
    }
    len -= cb;
    Serial.println();
  }
  return true;
}
