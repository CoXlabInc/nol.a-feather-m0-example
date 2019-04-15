#include <cox.h>
#include <dev/Adafruit_GFX.hpp>
#include <dev/Adafruit_SSD1306.hpp>

Adafruit_SSD1306 display = Adafruit_SSD1306(128, 32, &Wire);
// Adafruit_SSD1306 display(-1, Wire, 0x3C);

Timer timerHello;

static void taskHello(void *) {
  digitalToggle(D13);
}

void setup() {
  Serial2.begin(115200);
  Serial2.printf("\n*** [Feather-M0] OLED FeatherWing Test ***\n");

  timerHello.onFired(taskHello, NULL);
  timerHello.startPeriodic(500);

  pinMode(D13, OUTPUT);
  digitalWrite(D13, HIGH);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Address 0x3C for 128x32

  // display.begin(SSD1306_SWITCHCAPVCC);
  // display.setTextColor(WHITE);
  // display.setTextSize(2);
  // display.clearDisplay();
  // display.print("* Feather-M0 *");
  display.display();

  pinMode(D5, INPUT_PULLUP);
  attachInterrupt(D5, []() {
    Serial2.printf("- 'C' is pressed!\n");
  }, FALLING);

  pinMode(D6, INPUT_PULLUP);
  attachInterrupt(D6, []() {
    Serial2.printf("- 'B' is pressed!\n");
  }, FALLING);

  pinMode(D9, INPUT_PULLUP);
  attachInterrupt(D9, []() {
    Serial2.printf("- 'A' is pressed!\n");
  }, FALLING);
}
