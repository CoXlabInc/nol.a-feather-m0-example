#include <cox.h>
#include <dev/Adafruit_GFX.hpp>
#include <dev/Adafruit_SSD1306.hpp>

Adafruit_SSD1306 display(128, 32, &Wire);

Timer timerHello;

static void taskHello(void *) {
  digitalToggle(D13);
}

void setup() {
  timerHello.onFired(taskHello, nullptr);
  timerHello.startPeriodic(500);

  pinMode(D13, OUTPUT);
  digitalWrite(D13, HIGH);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Address 0x3C for 128x32
  display.display();
  delay(1000);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("*** [Feather-M0] ***");
  display.println("OLED FeatherWing Test");
  display.println();
  display.println("Press any button!");
  display.display();

  pinMode(D5, INPUT_PULLUP);
  attachInterrupt(D5, []() {
    display.fillRect(0, 24, 128, 8, BLACK);
    display.setCursor(0, 24);
    display.print("'C' is pressed!");
    display.display();
  }, FALLING);

  pinMode(D6, INPUT_PULLUP);
  attachInterrupt(D6, []() {
    display.fillRect(0, 24, 128, 8, BLACK);
    display.setCursor(0, 24);
    display.print("'B' is pressed!");
    display.display();
  }, FALLING);

  pinMode(D9, INPUT_PULLUP);
  attachInterrupt(D9, []() {
    display.fillRect(0, 24, 128, 8, BLACK);
    display.setCursor(0, 24);
    display.print("'A' is pressed!");
    display.display();
  }, FALLING);
}
