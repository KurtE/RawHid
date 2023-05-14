//setup to get LIDAR Data
#include <Wire.h>
#include <LIDARLite.h>

LIDARLite myLidarLite;

#include "SparkFun_BNO080_Arduino_Library.h"
BNO080 myIMU;

//Set up pressure/temp sensor
#include "MPL3115A2.h"

//Create an instance of the object
MPL3115A2 myPressure;

// setup your sensor data structure

struct sensor_packet_t{
  //put your variable definitions here for the data you want to send
  //THIS MUST BE EXACTLY THE SAME ON THE OTHER ARDUINO
  long timestamp;
  uint8_t id;
  int16_t distance;    // packet data.
  float roll;
  float pitch;
  float yaw;
  float altitude;
  float pressure;
  float temperature;
};
sensor_packet_t sensor;

struct lidar_packt_t {
  uint8_t id = 30;
  long timestamp;
  float x;
  float y;
  float z;
  float distance;
};
lidar_packt_t lidarPacket;

struct imu_packt_t {
  uint8_t id = 10;
  long timestamp;
  float roll;
  float pitch;
  float yaw;
};
imu_packt_t imuPacket;

struct press_packt_t {
  uint8_t id = 20;
  long timestamp;
  float altitude;
  float pressure;
  float temperature;
};
press_packt_t pressPacket;


//give a name to the group of data
