#include <cox.h>

#define PRINT_TO_OLED_WING

#ifdef PRINT_TO_OLED_WING
#include <dev/Adafruit_GFX.hpp>
#include <dev/Adafruit_SSD1306.hpp>
Adafruit_SSD1306 display(128, 32, &Wire);
#endif

Timer timerHello;
uint16_t distance = 0; //distance
uint16_t strength = 0; // signal strength
uint8_t rangeType = 0; //range scale
/*Value range:
 00 (short distance)
 03 (intermediate distance)
 07 (long distance) */

const byte sensor1 = 0x10; //TFMini I2C Address

//Write two bytes to a spot
boolean readDistance(uint8_t deviceAddress) {
  Wire.beginTransmission(deviceAddress);
  Wire.write(0x01); //MSB
  Wire.write(0x02); //LSB
  Wire.write(7); //Data length: 7 bytes for distance data
  if (Wire.endTransmission(false) != 0) {
    return false; //Sensor did not ACK
  }
  Wire.requestFrom(deviceAddress, (uint8_t)7); //Ask for 7 bytes

  if (Wire.available() == 7) {
    for (uint8_t x = 0 ; x < 7 ; x++) {
      uint8_t incoming = Wire.read();

      if (x == 2) {
        distance = incoming; //LSB of the distance value "Dist_L"
      } else if (x == 3) {
        distance |= incoming << 8; //MSB of the distance value "Dist_H"
      } else if (x == 4) {
        strength = incoming; //LSB of signal strength value
      } else if (x == 5) {
        strength |= incoming << 8; //MSB of signal strength value
      } else if (x == 6) {
        rangeType = incoming; //range scale
      }
    }
  } else {
    Serial2.println("No wire data avail");
    return false;
  }

  return true;
}

static void taskHello(void *) {
  digitalWrite(D13, HIGH);
  bool readSuccess = readDistance(sensor1);
  digitalWrite(D13, LOW);
  if (readSuccess) {
    Serial2.print("\tDist[");
    Serial2.print(distance);
    Serial2.print("]\tstrength[");
    Serial2.print(strength);
    Serial2.print("]\tmode[");
    Serial2.print(rangeType);
    Serial2.print("]");
    Serial2.println();

#ifdef PRINT_TO_OLED_WING
    display.clearDisplay();
    display.setCursor(0, 0);
    if (distance < 65535) {
      display.print(distance);
      display.println(" cm");
    } else {
      display.println("Too far");
    }
    display.display();
#endif
  } else {
    Serial2.println("Read fail");
#ifdef PRINT_TO_OLED_WING
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Read error");
    display.display();
#endif
  }

  timerHello.startOneShot(20);
}

void setup() {
  Serial2.begin(115200);
  Serial2.printf("\n*** [Feather-M0] TFMini via I2C Test ***\n");

  pinMode(D13, OUTPUT);
  digitalWrite(D13, LOW);

  Wire.begin();

#ifdef PRINT_TO_OLED_WING
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Address 0x3C for 128x32
  display.display(); //splash
  delay(1000);
  display.setTextSize(3);
  display.setTextColor(WHITE);
#endif

  timerHello.onFired(taskHello, nullptr);
  timerHello.startOneShot(20);
}
