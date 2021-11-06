#include <SD.h>
#include <SPI.h>

File myFile;

const int chipSelect = BUILTIN_SDCARD;
#define BUFFER_SIZE 128
uint8_t write_buffer[BUFFER_SIZE];

void setup()
{
  while (!Serial && millis() < 5000);
  Serial.begin(9600);

  if (CrashReport) {
    Serial.print(CrashReport);
  }
  Serial.println("\n" __FILE__ " " __DATE__ " " __TIME__);
  delay(3000);
  Serial.print("Initializing SD card...");

  if (!SD.begin(chipSelect)) {
    Serial.println("initialization failed!");
    return;
  }
  Serial.println("initialization done.");

  // open the file.
  Serial.println("Write Large Index File");
  myFile = SD.open("LargeIndexedTestfile.txt", FILE_WRITE_BEGIN);
  if (myFile) {
    myFile.truncate(); // Make sure we wipe out whatever was written earlier
    for (uint32_t i = 0; i < 43000*4; i++) {
      memset(write_buffer, 'A'+ (i & 0xf), sizeof(write_buffer));
      myFile.printf("%06u ", i >> 2);  // 4 per physical buffer
      myFile.write(write_buffer, i? 120 : 120-12); // first buffer has other data...
      myFile.printf("\n");
    }
    myFile.close();
    Serial.println("\ndone.");
  }
}

void loop()
{
  // nothing happens after setup
}
