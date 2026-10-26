#include <Wire.h>

const int MPU_ADDR = 0x68;
int16_t AcX, AcY, AcZ;

void setup() {
  Wire.begin();
  Serial.begin(115200);
  while (!Serial);

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);
  Serial.println("FocusBox Orientation Detector Ready");
}

void loop() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 6, true);

  if (Wire.available() >= 6) {
    AcX = Wire.read() << 8 | Wire.read();
    AcY = Wire.read() << 8 | Wire.read();
    AcZ = Wire.read() << 8 | Wire.read();

    const int THRESHOLD = 10000; // ค่าขีดเริ่มตรวจจับทิศทาง

    if (AcZ > THRESHOLD) {
      Serial.println("Face UP (ด้านหน้าหงายขึ้น)");
    } else if (AcZ < -THRESHOLD) {
      Serial.println("Face DOWN (คว่ำหน้าลง)");
    } else if (AcX > THRESHOLD) {
      Serial.println("Tilt RIGHT (เอียงขวา)");
    } else if (AcX < -THRESHOLD) {
      Serial.println("Tilt LEFT (เอียงซ้าย)");
    } else if (AcY > THRESHOLD) {
      Serial.println("Tilt FORWARD (เอียงไปข้างหน้า)");
    } else if (AcY < -THRESHOLD) {
      Serial.println("Tilt BACKWARD (เอียงมาข้างหลัง)");
    }
  }

  delay(250);
}