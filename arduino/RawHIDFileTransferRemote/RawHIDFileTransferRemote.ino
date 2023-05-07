#include <MemoryHexDump.h>

//=============================================================================
// Simple test to see about doing file operations between two Teensy boards
// running RawHID.  This is the Host one... i.e. one that plugs into your
// main host.
// It also forwards any Serial Emulation data it receives as well to Serial
// This sketch is very much like the main teensy example: USBtoSerial.ino
// 
// This sketch should be built with USB Type of RawHID
//
// This sketch can be configured to use any File system.  Will setup to
// use eitehr SD or LittleFS
//
// This example is in the public domain
//=============================================================================

#include "USBHost_t36.h"
#include <LittleFS.h>
#include <SD.h>

//=============================================================================
// Options
//=============================================================================
// If SD pin is defined will use it else LitleFS
#define SD_CS_PIN  BUILTIN_SDCARD

// uncomment the line below to output debug information
#define DEBUG_OUTPUT

// uncomment this one if you wish to output debug to something other than Serial.
#define DEBUG_PORT Serial1
#define DEBUG_BAUD 115200

uint16_t rx_size = 64;  // later will change ... hopefully
byte buffer[512];


#define FILE_IO_SIZE 4096
#define FILE_BUFFER_SIZE (FILE_IO_SIZE * 8)
uint8_t g_transfer_buffer[FILE_BUFFER_SIZE];
uint16_t g_transfer_buffer_head = 0;
uint16_t g_transfer_buffer_tail = 0;

typedef struct {
  uint16_t type;       // type of data in buffer
  uint16_t size;      // size of data in buffer
  uint8_t data[0];    // packet data.
} RawHID_packet_t;

typedef struct {
  uint16_t type;       // type of data in buffer
  uint16_t size;      // size of data in buffer
} RawHID_packet_header_t;

typedef struct {
  int32_t status;    // status information
  uint32_t size;      // optional size 
  uint32_t modifyDateTime; // optional date/time fields
} RawHID_status_packet_data_t;

typedef struct {
  uint32_t count;    // progress information
} RawHID_progress_packet_data_t;


//=============================================================================
// File system objects.
//=============================================================================

#ifdef SD_CS_PIN
SDClass fs;
#else
#define LFS_SIZE    (1024 * 512)
LittleFS_Program    fs;
#endif

File                current_directory; // keep a directory 

enum {CMD_NONE = -1, CMD_DATA=0, CMD_DIR=1, CMD_CD, CMD_DOWNLOAD, CMD_UPLOAD, CMD_LOCAL_DIR, CMD_LOCAL_CD, 
      CMD_RESPONSE, CMD_PROGRESS, CMD_RESET};

//=============================================================================
// optional debug stuff
//=============================================================================
#ifdef DEBUG_OUTPUT
#ifdef DEBUG_PORT
#define DBGPrintf DEBUG_PORT.printf
#else
#define DBGPrintf Serial.printf
#define DEBUG_PORT Serial
#endif
#else
// not debug have it do nothing
inline void DBGPrintf(...) {
}
#endif

//=============================================================================
// forward references
//=============================================================================
extern int send_status_packet(int32_t status, uint32_t size, uint32_t modifyDateTime=0, 
    uint32_t timeout = 1000);



//=============================================================================
// Setup
//=============================================================================
void setup() {
  while (!Serial && millis() < 5000) ; //wait up to 5 seconds
#if defined(DEBUG_OUTPUT) && defined (DEBUG_PORT)
  DEBUG_PORT.begin(DEBUG_BAUD);
  DEBUG_PORT.println("RawHIDFileTransferRemote stated - debug port");
#endif    
#ifdef SD_CS_PIN
  if (!fs.begin(SD_CS_PIN)) {
      Serial.printf("Failed to initialize the SD disk");
  }
#else
  if (!fs.begin(LFS_SIZE)) {
    Serial.printf("Failed to initialize the LittleFS Program disk");
  }
#endif

#ifdef __IMXRT1062__
  if (CrashReport) {
    Serial.print(CrashReport);
  }
 #endif

  Serial.printf("\n\nUSB Host RawHid File Transfers Remote(slave) side\n");

  // debug pins
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
}

//=============================================================================
// loop
//=============================================================================
void loop() {
  int n;
  digitalWriteFast(LED_BUILTIN, HIGH);
  n = RawHID.recv(buffer, 0); // 0 timeout = do not wait
  if (n > 0) {
    RawHID_packet_t *packet = (RawHID_packet_t*)buffer;
    // Lets see if this is a top level command.
    DBGPrintf("RLOOP CMD:%d %u\n", packet->type, packet->size);
    switch (packet->type) {
      case CMD_DIR: 
        print_dir((char*)packet->data);
        break;
      case CMD_CD: 
        // todo:
        send_status_packet(2, 0);
        break;
      case CMD_DOWNLOAD:
        sendFile((char*)packet->data);
        break;  
      case CMD_UPLOAD:
        receiveFile((char*)packet->data);
        break;  
      case CMD_RESET:
        _reboot_Teensyduino_();
        break;  
    }
  }
  digitalWriteFast(LED_BUILTIN, LOW);
}

//=============================================================================
// Send a packet
//=============================================================================
int send_rawhid_packet(int cmd, void *packet_data, uint8_t data_size, uint32_t timeout=1000) {
//  DBGPrintf("RSHID CMD:%d %p %u %u\n", cmd, packet_data, data_size, timeout);
  memset(buffer, 0, rx_size);
  RawHID_packet_t *packet = (RawHID_packet_t*)buffer;
  packet->type = cmd;
  packet->size = data_size;
  if (packet_data) memcpy(packet->data, packet_data, data_size);
  #ifdef DEBUG_OUTPUT
  //MemoryHexDump(Serial, buffer, 64, false, "RMT:\n");
  #endif
  return RawHID.send(buffer, timeout);
}

int send_status_packet(int32_t status, uint32_t size, uint32_t modifyDateTime, uint32_t timeout) {
  RawHID_status_packet_data_t status_data = {status, size, modifyDateTime};
  return send_rawhid_packet(CMD_RESPONSE, &status_data, sizeof(status_data), timeout);
}


//=============================================================================
//  Recove a file code. 
//=============================================================================
File g_transfer_file;
volatile bool g_transfer_complete = false;
volatile uint16_t g_cb_file_buffer_used = 0;
volatile uint8_t g_transfer_status = 0;
uint32_t          g_transfer_size = 0;
uint32_t          g_transfer_modify_date_time = 0;

void checkForRawHIDReceive_ReceiveFile() {
  // see if we have any messages pending for us.
  digitalWriteFast(4, HIGH);
  int n = RawHID.recv(buffer, 0); // 0 timeout = do not wait
  if (n > 0) {

    RawHID_packet_t *packet = (RawHID_packet_t*)buffer;

    //DBGPrintf("CRHReceive type:%d %u ", packet->type, packet->size);

    // lets check the data 
    uint8_t cb_data = packet->size;

    // Quick check to see if an error happened. 
    if (packet->type == CMD_RESPONSE) {
      RawHID_status_packet_data_t *status_data = (RawHID_status_packet_data_t*)packet->data;
      DBGPrintf("ORRF - Status: %d %d\n", status_data->status, status_data->size);
      g_transfer_status = status_data->status;
      g_transfer_size = status_data->size;
      g_transfer_modify_date_time = status_data->modifyDateTime;
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
      DBGPrintf("ORRF DATA : %u\n", g_cb_file_buffer_used);
      if (packet->size < (rx_size - sizeof(RawHID_packet_header_t))) g_transfer_complete = true;
    } else {
      DBGPrintf("ORRF ???\n");
    }
  }
  digitalWriteFast(4, LOW);
}

//-----------------------------------------------------------------------------
// receive file
//-----------------------------------------------------------------------------
void receiveFile(char * filename) {
  // todo: right now file names can not be longer than our packets
  // clear out the buffer
  Serial.printf("Receive File %s\n", filename);
  digitalWriteFast(2, HIGH);
  g_transfer_buffer_head = 0;
  g_transfer_buffer_tail = 0;
  g_cb_file_buffer_used = 0;  // could compute.
  g_transfer_status = 0xff;   // status of the operation 0 is OK 
  g_transfer_complete = false;
  uint32_t total_bytes_transfered = 0;

  // need to add some error checking. 
  g_transfer_file = fs.open(filename, FILE_WRITE_BEGIN);
  if (!g_transfer_file) {
    send_status_packet(1, 0);
    return;
  }
  // we opened the file lets truncate it now
  send_status_packet(0, 0); // let them know we received the data
  g_transfer_file.truncate();
  
  // Lets try to write out 4K chunks.
  // now lets receive the data
  RawHID_progress_packet_data_t progress_data = {FILE_IO_SIZE};
  do { 
    checkForRawHIDReceive_ReceiveFile();
    DBGPrintf("\n>>>>>> Enter data loop (%u %u %d) <<<<<<<<\n", 
          g_cb_file_buffer_used, g_transfer_complete, g_transfer_status);
    digitalWriteFast(3, HIGH);
    while ((g_cb_file_buffer_used < FILE_IO_SIZE) && !g_transfer_complete) {
      // if we have mtp... do it now
      checkForRawHIDReceive_ReceiveFile();
      yield();
    }
    digitalWriteFast(3, LOW);
    DBGPrintf("\n>>>>>> Exit data loop (%u %u %d) <<<<<<<<\n", 
          g_cb_file_buffer_used, g_transfer_complete, g_transfer_status);
    if (g_cb_file_buffer_used < FILE_IO_SIZE) progress_data.count = g_cb_file_buffer_used;

    elapsedMillis emWrite;
    uint32_t cbWritten = g_transfer_file.write(&g_transfer_buffer[g_transfer_buffer_tail], progress_data.count);

    DBGPrintf("Receive file write: %u %u dt:%u\n", cbWritten,  progress_data.count, (uint32_t)emWrite);

    if (cbWritten != progress_data.count) {
      Serial.printf("\n$$Receive error: only wrote %d bytes expected %d - retry\n", cbWritten,  progress_data.count);

      // lets retry once?  
      cbWritten += g_transfer_file.write(&g_transfer_buffer[g_transfer_buffer_tail+cbWritten], progress_data.count-cbWritten);
      if (cbWritten != progress_data.count) {
        Serial.printf("\t$$Retry failed: only wrote %d bytes expected %d\n", cbWritten,  progress_data.count);
      }
    }
    uint16_t cb_write = (g_cb_file_buffer_used < FILE_IO_SIZE)? g_cb_file_buffer_used : FILE_IO_SIZE;
    g_transfer_file.write(&g_transfer_buffer[g_transfer_buffer_tail], cb_write );
    //Serial.write('.');

    __disable_irq();
    g_transfer_buffer_tail += progress_data.count;
    if (g_transfer_buffer_tail >= sizeof(g_transfer_buffer)) g_transfer_buffer_tail = 0;
    g_cb_file_buffer_used -= progress_data.count;
    total_bytes_transfered += progress_data.count;
    __enable_irq();
    // let other side know we wrote out some stuff

    if (!send_rawhid_packet(CMD_PROGRESS, (uint8_t*)&progress_data, sizeof(progress_data), 1000)) {
        Serial.printf("Download failed timeout - Sending progress(%u)", total_bytes_transfered);
        send_status_packet(2, 0);
        g_transfer_file.close();
        return;
    }

  } while ((g_cb_file_buffer_used > FILE_IO_SIZE) || !g_transfer_complete);

  // At end we may have a partial buffer to write.
  if (g_cb_file_buffer_used) {
    uint32_t cbWrite = g_cb_file_buffer_used;
    if ((g_transfer_buffer_tail + cbWrite) > sizeof(g_transfer_buffer)) 
      cbWrite = sizeof(g_transfer_buffer) - g_transfer_buffer_tail;
    g_transfer_file.write(&g_transfer_buffer[g_transfer_buffer_tail], cbWrite );
    progress_data.count = cbWrite;
    g_cb_file_buffer_used -= cbWrite;
    if (g_cb_file_buffer_used) {
      // Wrapped around to start of buffer
      g_transfer_file.write(&g_transfer_buffer, g_cb_file_buffer_used );
      progress_data.count = g_cb_file_buffer_used;
    }
    send_rawhid_packet(CMD_PROGRESS, (uint8_t*)&progress_data, sizeof(progress_data), 1000);
    total_bytes_transfered += progress_data.count;
    Serial.write('.');
    DBGPrintf("(%u)\n", total_bytes_transfered);
  }
  g_transfer_file.close();
  Serial.println("\nremote Completed");
  digitalWriteFast(2, LOW);

}


//=============================================================================
// Send a file code. 
//=============================================================================
void checkForRawHIDReceive_sendFile() {
  int n = RawHID.recv(buffer, 0); // 0 timeout = do not wait
  if (n > 0) {
    RawHID_packet_t *packet = (RawHID_packet_t*)buffer;

    DBGPrintf("CRHSend type:%d %u ", packet->type, packet->size);
    if (packet->type == CMD_PROGRESS) {
      RawHID_progress_packet_data_t *progress_data = (RawHID_progress_packet_data_t*)packet->data;
      DBGPrintf(" - progress: %u\n", progress_data->count);
      if (progress_data->count <= g_cb_file_buffer_used) g_cb_file_buffer_used -= progress_data->count;
      else g_cb_file_buffer_used = 0;
    } else {
      DBGPrintf("\n");
    }
  }
}

//-----------------------------------------------------------------------------
// This is the download code. 
//-----------------------------------------------------------------------------
void sendFile(char * filename) {
  // todo: right now file names can not be longer than our packets
  // clear out the buffer
  Serial.printf("Sending %s\n", filename);
  g_transfer_buffer_head = 0;
  g_transfer_buffer_tail = 0;
  g_cb_file_buffer_used = 0;  // could compute.
  g_transfer_status = 0xff;   // status of the operation 0 is OK 
  g_transfer_complete = false;

  // need to add some error checking. 
  g_transfer_file = fs.open(filename, FILE_READ);
  if(!g_transfer_file) {
    Serial.println("SendFile failed, file not found");
    send_status_packet(1, 0);  // send a failure code
    return;
  }

  uint32_t file_size = g_transfer_file.size();
  DateTimeFields dtf;
  g_transfer_file.getModifyTime(dtf);

  send_status_packet(0, file_size, makeTime(dtf));  // send a failure code

  // in this case we are only going to use about 4K of the buffer. 
  uint16_t cb_buffer = g_transfer_file.read(g_transfer_buffer, FILE_IO_SIZE);
  uint16_t cb_read = cb_buffer;
  uint16_t buffer_index = 0;
  uint16_t cb_transfer = rx_size - sizeof(RawHID_packet_header_t);
  bool eof = cb_read == 0;
  
  while(!eof && cb_transfer) {
    // see if anything from the otherside. 
    checkForRawHIDReceive_sendFile();
    //todo: don't hard code buffer sizes...
    if (!eof && (cb_buffer < cb_transfer)) {
      memmove(g_transfer_buffer, &g_transfer_buffer[buffer_index], cb_buffer);
      buffer_index = 0;
      cb_read = g_transfer_file.read(&g_transfer_buffer[cb_buffer], FILE_IO_SIZE); 
      cb_buffer += cb_read;
      DBGPrintf("$$rsf read: cb read:%u buf:%u\n", cb_read, cb_buffer);
      eof = cb_read == 0;
    }

    // don't output more than other side can handle
    elapsedMillis emBusy = 0;
    while (g_cb_file_buffer_used > (sizeof(g_transfer_buffer) - cb_transfer)) {
      // see if anything from the otherside. 
      checkForRawHIDReceive_sendFile();

      //TODO: maybe better define timeout
      if (emBusy > 5000) {
        Serial.printf("SendFile failed - timeout waiting for progress update");
        send_status_packet(2, 0);  // say timeout
        g_transfer_file.close();  
        return;
      }

    }
    if (cb_transfer > cb_buffer) cb_transfer = cb_buffer;
    uint8_t retry_count = 3;

    if (retry_count && !send_rawhid_packet(CMD_DATA, &g_transfer_buffer[buffer_index],cb_transfer, 5000)) {
      Serial.printf("sendfail send packet failed - retry count %u", retry_count);
      retry_count--;
    }

    __disable_irq();
    g_cb_file_buffer_used += cb_transfer;
    __enable_irq();
    cb_buffer -= cb_transfer;
    buffer_index += cb_transfer;
  }
  if (cb_transfer != (rx_size - sizeof(RawHID_packet_header_t))) {
    // send a zero length to let them know we are done
    if (send_rawhid_packet(CMD_DATA, nullptr, 0, 5000) <= 0) {
        Serial.printf("SendFile failed - timeout sending packet");
        g_transfer_file.close();  
        return;
    }
  }
  g_transfer_file.close();  
  Serial.println("\nRemote Send File Completed");
}




//=============================================================================
// Print the local directory 
//=============================================================================
bool print_dir(char * filename) {
  if (!current_directory) {
    current_directory = fs.open("/");
  }
  Serial.println("Remote File system directory");
  if (!current_directory) {
    send_status_packet(1, 0);
    return false;
  }
  printDirectory(current_directory, 0);
  send_status_packet(0, 0);

  return true;
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
      Serial.printf(" C: %02u/%02u/%04u %02u:%02u", dtf.mon + 1, dtf.mday, dtf.year + 1900, dtf.hour, dtf.min );
    }

    if (entry.getModifyTime(dtf)) {
      Serial.printf(" M: %02u/%02u/%04u %02u:%02u", dtf.mon + 1, dtf.mday, dtf.year + 1900, dtf.hour, dtf.min );
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

void local_cd(char * filename) {
  if (!current_directory) {
    current_directory = fs.open(*filename? filename : "/");
  } else {
    // todo:
    //current_directory. 
  }

}
