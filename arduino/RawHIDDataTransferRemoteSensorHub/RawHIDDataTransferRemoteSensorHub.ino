//sensor header include
#include "sensors.h"


//=============================================================================
// Globals used in transfers and the like. 
//=============================================================================
#define FILE_IO_SIZE 4096
#define FILE_BUFFER_SIZE (FILE_IO_SIZE * 8)

enum {CMD_NONE = -1, CMD_DATA_IMU=0, CMD_DATA_LIDAR=1, CMD_DATA_PRESS, CMD_DATA_ALL, CMD_STOP};

typedef struct {
  uint16_t type;       // type of data in buffer
  uint16_t size;      // size of data in buffer
  uint8_t data[0];    // packet data.
} RawHID_packet_t;

uint16_t rx_size = 64;  // later will change ... hopefully
byte buffer[512];
byte rxBuffer[512];
uint8_t cmd = CMD_NONE;

void setup(){
  while (!Serial && millis() < 5000) ; //wait up to 5 seconds

#ifdef __IMXRT1062__
  if (CrashReport) {
    Serial.print(CrashReport);
  }
 #endif

#ifdef USB_RAWHID512
rx_size = RawHID.rxSize();
#endif

  Serial.printf("\n\nUSB Host RawHid File Transfers Remote(slave) side\n");
  Serial.printf("Transfer size: %u\n", rx_size);
  
  // Set configuration to default and I2C to 400 kHz
  myLidarLite.begin(0, true); 
  Wire.setClock(400000);
  // Change this number to try out alternate configurations
  myLidarLite.configure(0); 
  
  Wire1.begin();
  Wire1.setClock(400000);
  while (myIMU.begin(0x4b, Wire1, 255) == false)
  {
    Serial.println(F("BNO080 not detected at default I2C address. Check your jumpers and the hookup guide. Freezing..."));
    delay(10);
  }
  
  myIMU.enableRotationVector(10); //Send data update every 50ms

  Serial.println(F("Rotation vector enabled"));
  Serial.println(F("Output in form roll, pitch, yaw"));
  
  myPressure.begin(); // Get sensor online

  //Configure the sensor
  //myPressure.setModeAltimeter(); // Measure altitude above sea level in meters
  myPressure.setModeBarometer(); // Measure pressure in Pascals from 20 to 110 kPa
  myPressure.setOversampleRate(4); // Set Oversample to the recommended 128
  myPressure.enableEventFlags(); // Enable all three pressure and temp event flags 

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWriteFast(LED_BUILTIN, LOW);
  
}

void loop(){
  int n;
  int cmd_status = 0;
  n = RawHID.recv(rxBuffer, 0); // 0 timeout = do not wait
  if (n > 0) {
    digitalWriteFast(LED_BUILTIN, HIGH);
    RawHID_packet_t *packet = (RawHID_packet_t*)rxBuffer;
    cmd = packet->type;
  }
    // Lets see if this is a top level command.
    // hack if one of the commands fails and receives a new command
    // request try to process it.
    //do {
      //DBGPrintf("RLOOP CMD:%d %u\n", packet->type, packet->size);
      switch (cmd) {
        case CMD_DATA_IMU: 
          getIMU();
          cmd_status = 1;
          break;
        case CMD_DATA_LIDAR: 
          getLidar();
          //memcpy(buffer, &lidarPacket, sizeof(lidarPacket));
          //RawHID.send(buffer, 1000);
          cmd_status = 1;
          break;
        case CMD_DATA_PRESS:
          //getPres();
          //memcpy(buffer, &pressPacket, sizeof(pressPacket));
          //RawHID.send(buffer, 1000);
          cmd_status = 1;
          break;
        case CMD_DATA_ALL:  
        {  
          getIMU();
          getLidar();
          //getPres();
          //memcpy(buffer, &pressPacket, sizeof(pressPacket));
          //RawHID.send(buffer, 1000)
          //cmd_status = 1;
        }
          break;
        case CMD_STOP:
          cmd_status = 0;
          cmd = CMD_NONE;
          break;  
      }
    //} while (cmd_status == 1 );
  //}
  
  digitalWriteFast(LED_BUILTIN, LOW);

  //Serial.printf("%d, %d, %f, %f, %f, %f, %f, %f\n", millis(), mypacket.distance, mypacket.roll, mypacket.pitch, 
  //              mypacket.yaw, mypacket.altitude, mypacket.pressure/100.0f, mypacket.temperature);

  //delay(25);
}


void getIMU() {
  float roll, pitch, yaw;
  
  //Look for reports from the IMU
  if (myIMU.dataAvailable() == true)
  {
    roll = (myIMU.getRoll()) * 180.0 / PI; // Convert roll to degrees
    pitch = (myIMU.getPitch()) * 180.0 / PI; // Convert pitch to degrees
    yaw = (myIMU.getYaw()) * 180.0 / PI; // Convert yaw / heading to degrees

    imuPacket.roll = roll;
    imuPacket.pitch = pitch;
    imuPacket.yaw = yaw;
    
    imuPacket.timestamp = millis();
    memcpy(buffer, &imuPacket, sizeof(imuPacket));
    RawHID.send(buffer, 1000);
  }
}


void getLidar() {
  // Take a measurement with receiver bias correction and print to serial terminal (remove false
  //sensor.distance = myLidarLite.distance(true));
  //memcpy(buffer, &msensor, sizeof(sensor));
  //RawHID.send(buffer, 1000);
  
  // Take 99 measurements without receiver bias correction and print to serial terminal
  //for(int i = 0; i < 99; i++)
  //{
  lidarPacket.distance = myLidarLite.distance(false);  
  lidarPacket.timestamp = millis();
  memcpy(buffer, &lidarPacket, sizeof(lidarPacket));
  RawHID.send(buffer, 1000);

}

void getPress() {
  //pressPacket.altitude = myPressure.readAltitudeFt();
  pressPacket.pressure = myPressure.readPressure();
  pressPacket.temperature = myPressure.readTempF();
  pressPacket.timestamp = millis();
  memcpy(buffer, &lidarPacket, sizeof(lidarPacket));
  RawHID.send(buffer, 1000);
}
