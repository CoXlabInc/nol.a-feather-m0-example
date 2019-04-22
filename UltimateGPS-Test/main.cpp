#include <cox.h>

#define GPS_EN  A5

void setup() {
  Serial0.begin(115200);

#ifdef GPS_EN
  pinMode(GPS_EN, OUTPUT);
  digitalWrite(GPS_EN, LOW);
#endif

  Serial2.begin(9600);
  Serial2.onReceive([](SerialPort &p) {
    while (p.available() > 0) {
      char c = p.read();
      Serial0.write(c);
    }
  });
  Serial2.listen();
}
