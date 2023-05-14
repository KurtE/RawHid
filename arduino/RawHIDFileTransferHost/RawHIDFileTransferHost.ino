//=============================================================================
// Simple test to see about doing file operations between two Teensy boards
// running RawHID.  This is the Host one... i.e. one that plugs into your
// main host.
// It also forwards any Serial Emulation data it receives as well to Serial
// This sketch is very much like the main teensy example: USBtoSerial.ino
//
// This sketch can be built with most of the USB types
//
// This sketch can be configured to use any File system.  Will setup to
// use eitehr SD or LittleFS
//
// This example is in the public domain
//=============================================================================

#include "USBHost_t36.h"
#include <LittleFS.h>
#include <SD.h>

// First pass LittleFS with Program
// Also allow this sketch to work regardless if we decide to use MTP or not
#if defined(USB_MTPDISK) || defined(USB_MTPDISK_SERIAL)
// If user selected a USB type that includes MTP, then set it up
#include <MTP_Teensy.h>
#endif

#ifdef MTP_TEENSY_H
#define mtp_loop MTP.loop()

#else
#pragma message "Note Built without MTP support"
#define MTP_MAX_FILENAME_LEN 256
#define mtp_loop
#endif

//=============================================================================
// Options
//=============================================================================
// If SD pin is defined will use it else LitleFS
#define SD_CS_PIN BUILTIN_SDCARD

// uncomment the line below to output debug information
#define DEBUG_OUTPUT

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

DMAMEM uint8_t buffer[512];  // most of the time will be 64 bytes, but if we support 512...
uint16_t rx_size = 64;       // later will change ... hopefully

#define FILE_IO_SIZE 4096
#define FILE_BUFFER_SIZE (FILE_IO_SIZE * 8)
uint8_t g_transfer_buffer[FILE_BUFFER_SIZE];
uint16_t g_transfer_buffer_head = 0;
uint16_t g_transfer_buffer_tail = 0;

typedef struct {
  uint16_t type;    // type of data in buffer
  uint16_t size;    // size of data in buffer
  uint8_t data[0];  // packet data.
} RawHID_packet_t;

typedef struct {
  uint16_t type;  // type of data in buffer
  uint16_t size;  // size of data in buffer
} RawHID_packet_header_t;

typedef struct {
  int32_t status;           // status information
  uint32_t size;            // optional size
  uint32_t modifyDateTime;  // optional date/time fields
} RawHID_status_packet_data_t;

typedef struct {
  uint32_t count;  // progress information
} RawHID_progress_packet_data_t;

typedef struct __attribute__((packed)) {
  unit32_t        size;
  DateTimeFields  dtfCreate;
  DateTimeFields  dtfModify;
  uint8_t         isDirectory;
  char            name[1];
} RawHID_file_info_t;

//=============================================================================
// File system objects.
//=============================================================================
#ifdef SD_CS_PIN
SDClass fs;
#else
#define LFS_SIZE (1024 * 512)
LittleFS_Program fs;
#endif

uint32_t LFSRAM_SIZE = 65536;  // probably more than enough...
LittleFS_RAM lfsram;

File current_directory;  // keep a directory

enum { CMD_NONE = -1,
       CMD_DATA = 0,
       CMD_DIR = 1,
       CMD_CD,
       CMD_DOWNLOAD,
       CMD_UPLOAD,
       CMD_LOCAL_DIR,
       CMD_LOCAL_CD,
       CMD_RESPONSE,
       CMD_PROGRESS,
       CMD_RESET };

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
extern int send_status_packet(int32_t status, uint32_t size, uint32_t modifyDateTime = 0,
                              uint32_t timeout = 1000);
extern bool OnReceiveHIDData(uint32_t usage, const uint8_t *data, uint32_t len);

//=============================================================================
// Setup
//=============================================================================
void setup() {
// mandatory to begin the MTP session.
#ifdef MTP_TEENSY_H
  MTP.begin();

  if (lfsram.begin(LFSRAM_SIZE)) {
    Serial.printf("Ram Drive of size: %u initialized\n", LFSRAM_SIZE);
    uint32_t istore = MTP.addFilesystem(lfsram, "RAM");
    Serial.printf("Set Storage Index drive to %u\n", istore);
  }

#endif

  // Initialize the FS
#ifdef SD_CS_PIN
  if (!fs.begin(SD_CS_PIN)) {
    Serial.printf("Failed to initialize the SD disk");
  }
#ifdef MTP_TEENSY_H
  MTP.addFilesystem(fs, "SD");
#endif
#else

  if (!fs.begin(LFS_SIZE)) {
    Serial.printf("Failed to initialize the LittleFS Program disk");
  }
#ifdef MTP_TEENSY_H
  MTP.addFilesystem(fs, "Program");
#endif
#endif


  while (!Serial && millis() < 5000)
    ;  //wait up to 5 seconds
#ifdef __IMXRT1062__
  if (CrashReport) {
    Serial.print(CrashReport);
  }
#endif

  Serial.printf("\n\nUSB Host RawHid File Transfers Host(master) side\n");

  myusb.begin();

  rawhid1.attachReceive(OnReceiveHIDData);

  show_command_help();
}

//=============================================================================
// loop
//=============================================================================
void loop() {
  mtp_loop;
  myusb.Task();

  uint8_t filename[256];

  int cmd = check_Serial_for_command(filename);
  if (cmd != CMD_NONE) {
    Serial.printf("CMD:%d Filename:%s\n", cmd, filename);
    switch (cmd) {
      case CMD_DIR: remote_dir(filename); break;
      case CMD_CD: remote_cd(filename); break;
      case CMD_DOWNLOAD: download(filename); break;
      case CMD_UPLOAD: upload(filename); break;
      case CMD_LOCAL_DIR: print_local_dir(filename); break;
      case CMD_LOCAL_CD: local_cd(filename); break;
      case CMD_RESET: remote_reset(); break;
    }
    rawhid1.attachReceive(OnReceiveHIDData);
  }

  // check if any data has arrived on the USBHost serial port
  forward_remote_serial();

  // Optional.
  UpdateActiveDeviceInfo();
}


//=============================================================================
// OnReceiveHIDData - called when we receive RawHID packet from the USBHost object
//=============================================================================
bool OnReceiveHIDData(uint32_t usage, const uint8_t *data, uint32_t len) {
  DBGPrintf("OnReceiveHidDta(%x %p %u)\n", usage, data, len);

  // lets try to copy the data out.


  g_transfer_buffer_head = 0;
  g_transfer_buffer_tail = 0;

  return true;
}

//=============================================================================
// Show the user some information about commands
//=============================================================================
void show_command_help() {
  Serial.println("Command list");
  Serial.println("\td [optional pattern] - show directory files on remote FS");
  Serial.println("\tc <file pattern> - Change directory on remote FS");
  Serial.println("\tr <file spec> - download(Receive) file from From remote");
  Serial.println("\tu <file spec> - Upload file to remote");
  Serial.println("\tld [optional pattern] - show directory files on Local FS");
  Serial.println("\tlc <file pattern> - Change directory on local FS");
  Serial.println("\t$ - Reboot remote");
}

//=============================================================================
// Check Serial for any pending commands waiting for us.
//=============================================================================
int check_Serial_for_command(uint8_t *filename) {
  *filename = 0;                             // make sure null terminated.
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
    case 'd': cmd = CMD_DIR; break;
    case 'c': cmd = CMD_CD; break;
    case 'r': cmd = CMD_DOWNLOAD; break;
    case 'u': cmd = CMD_UPLOAD; break;
    case 'l':
      {
        ch = Serial.read();
        Serial.write((ch >= ' ') ? ch : '\n');
        if (ch == 'd') cmd = CMD_LOCAL_DIR;
        else if (ch == 'c') cmd = CMD_LOCAL_CD;
      }
      break;
    case '$': cmd = CMD_RESET; break;
  }
  // now lets see if there is a file name...
  ch = Serial.read();
  Serial.write((ch >= ' ') ? ch : '\n');
  // TODO: maybe better parsing.
  while (ch == ' ') {
    ch = Serial.read();
    Serial.write((ch >= ' ') ? ch : '\n');
  }

  while (ch > ' ') {
    *filename++ = ch;
    ch = Serial.read();
    Serial.write((ch >= ' ') ? ch : '\n');
  }
  *filename = '\0';
  while (ch != -1) {
    ch = Serial.read();  // make sure we removed everything.
    Serial.write((ch >= ' ') ? ch : '\n');
  }
  if (cmd == CMD_NONE) show_command_help();
  return cmd;
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

int send_status_packet(int32_t status, uint32_t size, uint32_t modifyDateTime, uint32_t timeout) {
  RawHID_status_packet_data_t status_data = { status, size, modifyDateTime };
  return send_rawhid_packet(CMD_RESPONSE, &status_data, sizeof(status_data), timeout);
}


void remote_dir(uint8_t *filename) {
  if (!send_rawhid_packet(CMD_DIR, (uint8_t *)filename, strlen((char *)filename) + 1)) {
    Serial.println("Remote directory failed");
  }
}

void remote_cd(uint8_t *filename) {
}

void remote_reset() {
  Serial.println("Send remote reset command");
  send_rawhid_packet(CMD_RESET, nullptr, 0, 5000);
}


//=============================================================================
// Download a file code.
//=============================================================================
File g_transfer_file;
volatile bool g_transfer_complete = false;
volatile uint16_t g_cb_file_buffer_used = 0;
volatile int32_t g_transfer_status = 0;
uint32_t g_transfer_size = 0;
uint32_t g_transfer_modify_date_time = 0;


bool onReceiveDownload(uint32_t usage, const uint8_t *data, uint32_t len) {
  //DBGPrintf("ORDLonReceiveDownload(%x %p %u)", usage, data, len);

  // lets check the data
  RawHID_packet_t *packet = (RawHID_packet_t *)data;
  uint16_t cb_data = packet->size;

  // Quick check to see if an error happened.
  if (packet->type == CMD_RESPONSE) {
    RawHID_status_packet_data_t *status_data = (RawHID_status_packet_data_t *)packet->data;
    DBGPrintf("ORDL - Status: %d %d\n", status_data->status, status_data->size);
    g_transfer_status = status_data->status;
    g_transfer_size = status_data->size;
    g_transfer_modify_date_time = status_data->modifyDateTime;
    return true;
  }

  else if (packet->type == CMD_DATA) {
    //DBGPrintf(" - Data cb:%u", cb_data);
    // we have limits on packets going back and forth to make sure we don't run out of room.
    // so going to write this to be quick and dirty.
    // how much can
    uint32_t head = g_transfer_buffer_head;
    uint32_t cb = cb_data;
    if (cb > (sizeof(g_transfer_buffer) - head)) cb = sizeof(g_transfer_buffer) - head;
    memcpy(g_transfer_buffer + head, packet->data, cb);
    head += cb;
    if (head >= sizeof(g_transfer_buffer)) head = 0;
    cb_data -= cb;
    if (cb_data) {
      memcpy(g_transfer_buffer, packet->data + cb, cb_data);
      head += cb_data;
    }
    g_transfer_buffer_head = head;
    g_cb_file_buffer_used += packet->size;
    DBGPrintf("ORDL DATA : %u\n", g_cb_file_buffer_used);
    if (packet->size < (rx_size - sizeof(RawHID_packet_header_t))) g_transfer_complete = true;
  } else {
    DBGPrintf(" - ???\n");
  }
  return true;
}

//-----------------------------------------------------------------------------
// download
//-----------------------------------------------------------------------------
void download(uint8_t *filename) {
  // todo: right now file names can not be longer than our packets
  // clear out the buffer
  Serial.printf("Downloading %s\n", filename);
  g_transfer_buffer_head = 0;
  g_transfer_buffer_tail = 0;
  g_cb_file_buffer_used = 0;  // could compute.
  g_transfer_status = -1;     // status of the operation 0 is OK
  g_transfer_complete = false;
  uint32_t total_bytes_transfered = 0;
  rawhid1.attachReceive(onReceiveDownload);

  if (!send_rawhid_packet(CMD_DOWNLOAD, filename, strlen((char *)filename))) {
    Serial.println("download failed = failed to send command");
    return;
  }

  // first lets wait for status message from other side.
  elapsedMillis em;
  while ((g_transfer_status == -1) && (em < 1000))
    ;

  if (g_transfer_status != 0) {
    // wwe failed one way or another
    Serial.printf("download failed - transfer status:%u\n", g_transfer_status);
  }

  // need to add some error checking.
  elapsedMillis emTransferTime = 0;
  g_transfer_file = fs.open((char *)filename, FILE_WRITE_BEGIN);
  g_transfer_file.truncate();

  // Lets try to write out 4K chunks.
  // now lets receive the data or complete or the other side sent us a status.
  RawHID_progress_packet_data_t progress_data = { FILE_IO_SIZE };
  DBGPrintf("\n#################### Enter Download main Loop ##########################\n");
  do {
    elapsedMillis em = 0;
    DBGPrintf("\n>>>>>> Enter data loop (%u %u %d) <<<<<<<<\n",
              g_cb_file_buffer_used, g_transfer_complete, g_transfer_status);
    while ((g_cb_file_buffer_used < FILE_IO_SIZE) && !g_transfer_complete && (g_transfer_status == 0)) {
      // if we have mtp... do it now
      //mtp_loop;
      forward_remote_serial();
      yield();
      if (em > 5000) {
        // something went wrong.
        Serial.printf("Download failed timeout waiting for data(%u)\n", total_bytes_transfered);
        send_status_packet(2, 0);
        g_transfer_file.close();
        return;
      }
    }
    DBGPrintf("\n>>>>>> Exit data loop (%u %u %d) <<<<<<<<\n",
              g_cb_file_buffer_used, g_transfer_complete, g_transfer_status);

    if (g_cb_file_buffer_used < FILE_IO_SIZE) progress_data.count = g_cb_file_buffer_used;

    elapsedMillis emWrite;
    uint32_t cbWritten = g_transfer_file.write(&g_transfer_buffer[g_transfer_buffer_tail], progress_data.count);

    DBGPrintf("download file write: %u %u dt:%u\n", cbWritten, progress_data.count, (uint32_t)emWrite);

    if (cbWritten != progress_data.count) {
      Serial.printf("\n$$Download error: only wrote %d bytes expected %d - retry\n", cbWritten, progress_data.count);

      // lets retry once?
      cbWritten += g_transfer_file.write(&g_transfer_buffer[g_transfer_buffer_tail + cbWritten], progress_data.count - cbWritten);
      if (cbWritten != progress_data.count) {
        Serial.printf("\t$$Retry failed: only wrote %d bytes expected %d\n", cbWritten, progress_data.count);
      }
    }
    Serial.write('.');
    __disable_irq();
    g_transfer_buffer_tail += progress_data.count;
    if (g_transfer_buffer_tail >= sizeof(g_transfer_buffer)) g_transfer_buffer_tail = 0;
    g_cb_file_buffer_used -= progress_data.count;
    total_bytes_transfered += progress_data.count;
    __enable_irq();
    // let other side know we wrote out some stuff
    if (!send_rawhid_packet(CMD_PROGRESS, (uint8_t *)&progress_data, sizeof(progress_data), 1000)) {
      Serial.printf("Download failed timeout - Sending progress(%u)", total_bytes_transfered);
      send_status_packet(2, 0);
      g_transfer_file.close();
      return;
    }
  } while ((g_cb_file_buffer_used > FILE_IO_SIZE) || !g_transfer_complete);
  DBGPrintf("\n#################### Exit Download main Loop ##########################\n");

  // At end we may have a partial buffer to write.
  if (g_cb_file_buffer_used) {
    uint32_t cbWrite = g_cb_file_buffer_used;
    if ((g_transfer_buffer_tail + cbWrite) > sizeof(g_transfer_buffer))
      cbWrite = sizeof(g_transfer_buffer) - g_transfer_buffer_tail;
    g_transfer_file.write(&g_transfer_buffer[g_transfer_buffer_tail], cbWrite);
    progress_data.count = cbWrite;
    g_cb_file_buffer_used -= cbWrite;
    if (g_cb_file_buffer_used) {
      // Wrapped around to start of buffer
      g_transfer_file.write(&g_transfer_buffer, g_cb_file_buffer_used);
      progress_data.count = g_cb_file_buffer_used;
    }
    send_rawhid_packet(CMD_PROGRESS, (uint8_t *)&progress_data, sizeof(progress_data), 1000);
    total_bytes_transfered += progress_data.count;
    Serial.write('.');
    DBGPrintf("(%u)\n", total_bytes_transfered);
    forward_remote_serial();
  }

  DateTimeFields tm;
  breakTime(g_transfer_modify_date_time, tm);
  g_transfer_file.setModifyTime(tm);

  g_transfer_file.close();
  Serial.printf("\nCompleted, total byte: %u elapsed millis: %u\n", total_bytes_transfered, (uint32_t)emTransferTime);

#ifdef MTP_TEENSY_H
  // currently using sledghammer
  MTP.send_DeviceResetEvent();
#endif
}

//=============================================================================
// upload a file code.
//=============================================================================
bool onReceiveUpload(uint32_t usage, const uint8_t *data, uint32_t len) {
  DBGPrintf("ORUp(%x %p %u)", usage, data, len);

  // lets check the data
  RawHID_packet_t *packet = (RawHID_packet_t *)data;

  // Quick check to see if an error happened.
  if (packet->type == CMD_RESPONSE) {
    RawHID_status_packet_data_t *status_data = (RawHID_status_packet_data_t *)packet->data;
    g_transfer_status = status_data->status;
    //g_transfer_size = status_data->size;
    DBGPrintf(" - RSP: %u\n", g_transfer_status);
    return true;
  } else if (packet->type == CMD_PROGRESS) {
    RawHID_progress_packet_data_t *progress_data = (RawHID_progress_packet_data_t *)packet->data;
    if (progress_data->count <= g_cb_file_buffer_used) g_cb_file_buffer_used -= progress_data->count;
    else g_cb_file_buffer_used = 0;
    DBGPrintf(" - PROG: %u %u\n", progress_data->count, g_cb_file_buffer_used);
  } else {
    DBGPrintf("\n");
  }
  return true;
}

//-----------------------------------------------------------------------------
// upload
//-----------------------------------------------------------------------------
void upload(uint8_t *filename) {
  // todo: right now file names can not be longer than our packets
  // clear out the buffer
  Serial.printf("Uploading %s\n", filename);

  g_transfer_buffer_head = 0;
  g_transfer_buffer_tail = 0;
  g_cb_file_buffer_used = 0;  // could compute.
  g_transfer_status = -1;     // status of the operation 0 is OK
  g_transfer_complete = false;

  // need to add some error checking.
  g_transfer_file = fs.open((char *)filename, FILE_READ);
  if (!g_transfer_file) {
    Serial.println("Upload failed, file not found");
  }

  rawhid1.attachReceive(onReceiveUpload);

  if (!send_rawhid_packet(CMD_UPLOAD, filename, strlen((char *)filename))) {
    Serial.println("upload failed");
    return;
  }

  // first lets wait for status message from other side.
  elapsedMillis em;
  while ((g_transfer_status == -1) && (em < 1000))
    ;

  if (g_transfer_status != 0) {
    // wwe failed one way or another
    Serial.println("upload failed");
  }

  // Send them a status packet that has size and modify time
  uint32_t file_size = g_transfer_file.size();
  uint32_t count_bytes_sent = 0;
  DateTimeFields dtf;
  g_transfer_file.getModifyTime(dtf);
  DBGPrintf("File Size:%u Modify: %02u/%02u/%04u %02u:%02u\n", file_size,
            dtf.mon + 1, dtf.mday, dtf.year + 1900, dtf.hour, dtf.min);
  send_status_packet(0, file_size, makeTime(dtf));  // send a failure code


  // in this case we are only going to use about 4K of the buffer.
  uint16_t cb_buffer = g_transfer_file.read(g_transfer_buffer, FILE_IO_SIZE);
  uint16_t cb_read = cb_buffer;
  uint16_t buffer_index = 0;
  uint16_t cb_transfer = rx_size - sizeof(RawHID_packet_header_t);
  bool eof = cb_read == 0;

  uint32_t send_packet_num = 0;

  while (!eof && cb_transfer) {
    //todo: don't hard code buffer sizes...
    myusb.Task();
    if (!eof && (cb_buffer < cb_transfer)) {
      memmove(g_transfer_buffer, &g_transfer_buffer[buffer_index], cb_buffer);
      buffer_index = 0;
      cb_read = g_transfer_file.read(&g_transfer_buffer[cb_buffer], FILE_IO_SIZE);
      cb_buffer += cb_read;
      DBGPrintf("$$upload read: cb read:%u buf:%u\n", cb_read, cb_buffer);
      eof = cb_read == 0;
    }

    // don't output more than other side can handle
    while (g_cb_file_buffer_used > (sizeof(g_transfer_buffer) - cb_transfer)) {
      myusb.Task();
      //      mtp_loop; // should put in timeout.
    }
    if (cb_transfer > cb_buffer) cb_transfer = cb_buffer;
    count_bytes_sent += cb_transfer;
    DBGPrintf("<%u>", count_bytes_sent);

    if (!send_rawhid_packet(CMD_DATA, &g_transfer_buffer[buffer_index], cb_transfer, 5000)) {
      Serial.println("\n*** Upload Failed - Timeout ***\n");
      g_transfer_file.close();
      return;
    }
    delay(1);

    __disable_irq();
    g_cb_file_buffer_used += cb_transfer;
    __enable_irq();
    cb_buffer -= cb_transfer;
    buffer_index += cb_transfer;

    // bugbug: see if I put in a delay every so often if that will hep or not.
    send_packet_num++;
    if ((send_packet_num & 0xf) == 0) delay(10);
  }
  if (cb_transfer != (rx_size - sizeof(RawHID_packet_header_t))) {
    // send a zero length to let them know we are done
    send_rawhid_packet(CMD_DATA, &g_transfer_buffer[buffer_index], cb_transfer, 5000);
  }
  g_transfer_file.close();
  Serial.println("\nCompleted");
}




//=============================================================================
// Print the local directory
//=============================================================================
void print_local_dir(uint8_t *filename) {
  if (!current_directory) {
    current_directory = fs.open("/");
  }
  Serial.println("Local File system directory");
  current_directory.rewindDirectory();
  printDirectory(current_directory, 0);
}

void printDirectory(File dir, int numSpaces) {
  DateTimeFields dtf;
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) {
      // Serial.println("** no more files **");
      break;
    }
    printSpaces(numSpaces);
    Serial.print(entry.name());
    printSpaces(36 - numSpaces - strlen(entry.name()));

    if (entry.getCreateTime(dtf)) {
      Serial.printf(" C: %02u/%02u/%04u %02u:%02u", dtf.mon + 1, dtf.mday, dtf.year + 1900, dtf.hour, dtf.min);
    }

    if (entry.getModifyTime(dtf)) {
      Serial.printf(" M: %02u/%02u/%04u %02u:%02u", dtf.mon + 1, dtf.mday, dtf.year + 1900, dtf.hour, dtf.min);
    }
    if (entry.isDirectory()) {
      Serial.println("  /");
      printDirectory(entry, numSpaces + 2);
    } else {
      // files have sizes, directories do not
      Serial.print("  ");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}

void printSpaces(int num) {
  for (int i = 0; i < num; i++) {
    Serial.print(" ");
  }
}

void local_cd(uint8_t *filename) {
  if (!current_directory) {
    current_directory = fs.open(*filename ? (char *)filename : "/");
  } else {
    // todo:
    //current_directory.
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
        if (hiddrivers[i] == &rawhid1) {
          rx_size = rawhid1.rxSize();
          Serial.printf("   RX Size:%u TX Size:%u\n", rx_size, rawhid1.txSize());
        }
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
