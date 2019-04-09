#include <cox.h>

Timer timerHello;

static void taskHello(void *) {
  Serial2.printf("Hello World!\n");
  digitalToggle(D13);
}

void setup() {
  Serial2.begin(115200);
  Serial2.printf("\n*** [Feather-M0] Basic ***\n");
  timerHello.onFired(taskHello, NULL);
  timerHello.startPeriodic(100);
  pinMode(D13, OUTPUT);
  digitalWrite(D13, HIGH);
}
