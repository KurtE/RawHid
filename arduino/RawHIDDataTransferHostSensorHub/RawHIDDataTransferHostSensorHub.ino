#include "USBHost_t36.h"
#include "sensors.h"
#include "defines.h"

void setup() {
  while (!Serial && millis() < 5000) ; //wait up to 5 seconds

#ifdef __IMXRT1062__
  if (CrashReport) {
    Serial.print(CrashReport);
  }
#endif

  Serial.printf("\n\nUSB Host RawHid File Transfers Host(master) side\n");

  myusb.begin();

  rawhid1.attachReceive(OnReceiveHIDData);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWriteFast(LED_BUILTIN, LOW);

  // Optional.
  //UpdateActiveDeviceInfo();
  
  show_command_help();
}

void loop() {

  int cmd = check_Serial_for_command();
  if (cmd != CMD_NONE) {
    switch (cmd) {
      case CMD_DATA_IMU: 
        send_rawhid_packet(CMD_DATA_IMU, nullptr, 0, 1000); 
        break;
      case CMD_DATA_LIDAR: send_rawhid_packet(CMD_DATA_LIDAR, nullptr, 0, 1000); break;
      case CMD_DATA_PRESS: send_rawhid_packet(CMD_DATA_PRESS, nullptr, 0, 1000); break;
      case CMD_DATA_ALL: send_rawhid_packet(CMD_DATA_ALL, nullptr, 0, 1000); break;
      case CMD_STOP:
      {
        send_rawhid_packet(CMD_STOP, nullptr, 0, 1000); break;
      }
    }
    rawhid1.attachReceive(OnReceiveHIDData);
  }

  // check if any data has arrived on the USBHost serial port
  forward_remote_serial();

  // Optional.
  //UpdateActiveDeviceInfo();
}



//=============================================================================
// OnReceiveHIDData - called when we receive RawHID packet from the USBHost object
//=============================================================================
bool OnReceiveHIDData(uint32_t usage, const uint8_t *data, uint32_t len) {
  DBGPrintf("OnReceiveHidDta(%x %p %u)\n", usage, data, len);
  RawHID_rcv_packet_t *packet = (RawHID_rcv_packet_t*)data;

  if(packet->type == 10) {
    //imu_packet_t *imuPacket = (imu_packet_t*)data;
    memcpy(&imuPacket, data, sizeof(imuPacket));
    Serial.printf("%d, %d, %f, %f, %f\n", imuPacket.id, imuPacket.timestamp, imuPacket.roll, imuPacket.pitch, imuPacket.yaw);
  }
  
  if(packet->type == 30) {
    //imu_packet_t *imuPacket = (imu_packet_t*)data;
    memcpy(&lidarPacket, data, sizeof(lidarPacket));
    Serial.printf("%d, %d, %f\n", lidarPacket.id, lidarPacket.timestamp, lidarPacket.distance);
  }
  //memcpy(&mydata, data, sizeof(mydata));
  //Serial.printf("%d, %d, %d, %f, %f, %f, %f, %f, %f\n", mydata.id, mydata.timestamp,
  //             mydata.distance, mydata.roll, mydata.pitch, 
  //              mydata.yaw, mydata.altitude, mydata.pressure, mydata.temperature);
  

  return true;
}


//=============================================================================
//=============================================================================
void forward_remote_serial() {
  uint16_t rd, wr, n;
  uint8_t emu_buffer[512];

  // check if any data has arrived on the USBHost serial port
  rd = seremu.available();
  if (rd > 0) {
    // check if the USB virtual serial port is ready to transmit
    wr = Serial.availableForWrite();
    if (wr > 0) {
      // compute how much data to move, the smallest
      // of rd, wr and the buffer size
      if (rd > wr) rd = wr;
      if (rd > sizeof(emu_buffer)) rd = sizeof(emu_buffer);
      // read data from the USB host serial port
      n = seremu.readBytes((char *)emu_buffer, rd);
      // write it to the USB port
      //DBGPrintf("U-S(%u %u):", rd, n);
      Serial.write(emu_buffer, n);
    }
  }
}


//=============================================================================
// updateActiveDeviceInfo
//=============================================================================
// check to see if the device list has changed:
void UpdateActiveDeviceInfo() {
#ifdef PRINT_DEVICE_INFO
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
        //if (hiddrivers[i] == &seremu) {
        //  Serial.printf("   RX Size:%u TX Size:%u\n", seremu.rxSize(), seremu.txSize());
        //}
        //if (hiddrivers[i] == &rawhid1) {
        //  rx_size = rawhid1.rxSize();
        //  Serial.printf("   RX Size:%u TX Size:%u\n", rx_size, rawhid1.txSize());
        //}
      }
    }
  }
#else
  // we still want to find out if rawhid1 connected and if so what size is it.
  static bool last_rawhid1 = false;
  if (rawhid1 && !last_rawhid1) {
    rx_size = rawhid1.rxSize();
    DBGPrintf("rawhid1 connected rx:%u tx:%u\n", rx_size, rawhid1.txSize());
  }
  last_rawhid1 = rawhid1;

#endif
}


//=============================================================================
// Show the user some information about commands
//=============================================================================
void show_command_help() {
  Serial.println("Command list");
  Serial.println("\tI - Get IMU Data");
  Serial.println("\tL - Get LIDAR Data");
  Serial.println("\tP - Get Press/Temp Data");
  Serial.println("\tA - Get All sensor Data");
  Serial.println("\tS - Stop getting data");
}

//=============================================================================
// Check Serial for any pending commands waiting for us.
//=============================================================================
int check_Serial_for_command() {
  if (!Serial.available()) return CMD_NONE;  // no commands pending.
  Serial.print("$$");
  int ch = Serial.read();
  Serial.write((ch >= ' ') ? ch : '\n');
  int cmd = CMD_NONE;
  while (ch == ' ') {
    ch = Serial.read();
    Serial.write((ch >= ' ') ? ch : '\n');
  }
  switch (ch) {
    case 'I': cmd = CMD_DATA_IMU; break;
    case 'L': cmd = CMD_DATA_LIDAR; break;
    case 'P': cmd = CMD_DATA_PRESS; break;
    case 'A': cmd = CMD_DATA_ALL; break;
    case 'S': cmd = CMD_STOP; break;
  }

  if (cmd == CMD_NONE) show_command_help();
  return cmd;
}
