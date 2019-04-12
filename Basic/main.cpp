#include <cox.h>

Timer timerHello;

static void taskHello(void *) {
  struct timeval t;
  gettimeofday(&t, nullptr);
  Serial2.printf("[%lu.%06lu] Hello World!\n", (uint32_t) t.tv_sec, t.tv_usec);
  digitalToggle(D13);
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
}
