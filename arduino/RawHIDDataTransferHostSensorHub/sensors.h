struct lidar_packt_t {
  uint8_t id;
  long timestamp;
  float x;
  float y;
  float z;
  float distance;
};
lidar_packt_t lidarPacket;

struct imu_packet_t {
  uint8_t id;
  long timestamp;
  float roll;
  float pitch;
  float yaw;
} ;
imu_packet_t imuPacket;


struct press_packt_t {
  uint8_t id = 20;
  long timestamp;
  float altitude;
  float pressure;
  float temperature;
};
press_packt_t pressPacket;
