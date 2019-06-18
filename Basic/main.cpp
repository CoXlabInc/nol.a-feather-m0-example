#include <cox.h>

Timer timerHello;

static void taskHello(void *) {
  struct timeval t;
  gettimeofday(&t, nullptr);
  Serial2.printf("- [%lu.%06lu] Hello World!\n", (uint32_t) t.tv_sec, t.tv_usec);

  int32_t vcc = System.getSupplyVoltage();
  Serial2.printf("- Supply voltage: %ld mV\n", vcc);
  // digitalToggle(D13);

  Serial2.printf("- A0: %ld (%ld), VBAT=%ld mV\n",
   map(analogRead(A0), 0, 4095, 0, 2230), analogRead(A0), map(analogRead(D9), 0, 4095, 0, 2230 * 2));
}

void setup() {
  Serial2.begin(115200);
  Serial2.printf("\n*** [Feather-M0] Basic ***\n");

  uint32_t rcause = System.getResetReason();
  Serial2.printf("- Reset reason:0x%08lx\n", rcause);

  Serial2.onReceive([](SerialPort &p) {
    while (p.available() > 0) {
      char c = p.read();
      Serial2.write(c);
    }
  });
  Serial2.listen();

  timerHello.onFired(taskHello, NULL);
  timerHello.startPeriodic(500);

  pinMode(D13, OUTPUT);
  digitalWrite(D13, HIGH);

  Serial2.printf("- random(): %lu\n", random()); // This value must be changed on every boot.

  pinMode(A0, INPUT);
}
