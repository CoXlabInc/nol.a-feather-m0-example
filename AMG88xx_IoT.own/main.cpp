#include <cox.h>
#include <dev/Adafruit_AMG88xx.hpp>

Adafruit_AMG88xx amg(Wire);

void setup() {
  Serial2.begin(115200);

  Wire.begin();
  amg.begin();

  static Timer timerSense;
  timerSense.onFired([](void *) {
    float pixels[AMG88xx_PIXEL_ARRAY_SIZE];

    amg.readPixels(pixels);

    Serial2.print("[");
    for(int i = 1; i <= AMG88xx_PIXEL_ARRAY_SIZE; i++){
      Serial2.print(pixels[i-1]);
      Serial2.print(", ");
      if( i%8 == 0 ) Serial2.println();
    }
    Serial2.println("]");
    Serial2.println();

  }, nullptr);
  timerSense.startPeriodic(100);
}
