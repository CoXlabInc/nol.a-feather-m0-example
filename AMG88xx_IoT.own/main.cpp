#include <cox.h>
#include <dev/Adafruit_AMG88xx.hpp>
#include <dev/Adafruit_SSD1306.hpp>
#include <LoRaMacKR920.hpp>
#include <algorithm>

Adafruit_AMG88xx amg(Wire);
Adafruit_SSD1306 display(128, 32, &Wire);
SystemFeatherM0::RFM9x RFM95(A4, D10, D11);
LoRaMacKR920 LoRaWAN(RFM95, 3);

static uint8_t devEui[] = "\x98\x76\xB6\x00\x00\x10\xED\x81";
static const uint8_t appEui[] = "\x00\x00\x00\x00\x00\x00\x00\x00";
static uint8_t appKey[] = "\x44\x81\x3b\xab\x68\xfc\x24\x44\xb0\x9d\xf4\x68\x29\xf6\xb4\xf8";

static void taskSense(void *) {
  float pixels[AMG88xx_PIXEL_ARRAY_SIZE];

  amg.readPixels(pixels);

  LoRaMacFrame *f = new LoRaMacFrame(255);
  if (!f) {
    Serial2.printf("* Out of memory\n");
    return;
  }

  f->port = 1;
  f->type = LoRaMacFrame::CONFIRMED;
  uint8_t i = sprintf((char *) f->buf, "UAMG88xx");
  for (int y = 0; y < AMG88xx_PIXEL_ARRAY_SIZE; y++) {
    uint16_t temp = (uint16_t) round(pixels[y] * 100);
    f->buf[i++] = lowByte(temp);
    f->buf[i++] = highByte(temp);
    Serial2.print('['); Serial2.print(y); Serial2.print(']'); Serial2.print(' ');
    Serial2.print(pixels[y]);
    Serial2.print(" => ");
    Serial2.println(temp);
  }
  f->len = i;
  LoRaWAN.useADR = false;
  f->modulation = Radio::MOD_LORA;
  f->meta.LoRa.bw = Radio::BW_125kHz;
  f->meta.LoRa.sf = Radio::SF7;

  error_t err = LoRaWAN.send(f);
  Serial2.printf("* Sending a report (%u byte): %d\n", f->len, err);
  if (err != ERROR_SUCCESS) {
    delete f;
    postTask(taskSense, nullptr);
    return;
  }
  digitalWrite(D13, HIGH);

  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Uplink ");
  display.print(f->len);
  display.print(' ');
  display.println("byte");
  display.print(" - Result:");
  display.println(err);
  display.println();
  display.print("Temp:");
  display.println(amg.readThermistor());
  display.display();
}

void setup() {
  Serial2.begin(115200);

  pinMode(D13, OUTPUT);
  digitalWrite(D13, LOW);

  pinMode(D5, INPUT_PULLUP);
  attachInterrupt(D5, []() {
    taskSense(nullptr);
    display.setCursor(0, 32);
    display.println("Sense immediately!");
    display.display();
  }, FALLING);

  pinMode(D9, INPUT);

  Wire.begin();
  amg.begin();

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Address 0x3C for 128x32
  display.display();
  delay(1000);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("*** [Feather-M0] ***");
  display.println("AMG8833 Test");
  display.display();
  delay(1000);

  static Timer timerSense;
  timerSense.onFired(taskSense, nullptr);

  System.setTimeDiff(9 * 60);  // KST

  LoRaWAN.begin([]() -> uint8_t {
    int32_t vBat = map(analogRead(D9), 0, 4095, 0, 2 * 3300 * 100 / 148);
    if (vBat < 0) {
      return 255; // not measured
    } else {
      return map(vBat, 3300, 4200, 1, 254); // measured battery level
    }
  });
  LoRaWAN.onSendDone([](LoRaMac &lw, LoRaMacFrame *frame) {
    digitalWrite(D13, LOW);

    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("Uplink (");
    display.print(frame->fCnt);
    display.println(") done");
    display.print(" - Result:");
    display.print(frame->result);
    display.display();

    Serial2.printf(
      "* Send done(%d): destined for port:%u, fCnt:0x%lX, Freq:%lu Hz, "
      "Power:%d dBm, # of Tx:%u, ",
      frame->result,
      frame->port,
      frame->fCnt,
      frame->freq,
      frame->power,
      frame->numTrials
    );

    if (frame->modulation == Radio::MOD_LORA) {
      const char *strBW[] = {
        "Unknown", "125kHz", "250kHz", "500kHz", "Unexpected value"
      };
      if (frame->meta.LoRa.bw > 3) {
        frame->meta.LoRa.bw = (Radio::LoRaBW_t) 4;
      }
      Serial2.printf(
        "LoRa, SF:%u, BW:%s, ", frame->meta.LoRa.sf, strBW[frame->meta.LoRa.bw]
      );
    } else if (frame->modulation == Radio::MOD_FSK) {
      Serial2.printf("FSK, ");
    } else {
      Serial2.printf("Unkndown modulation, ");
    }
    if (frame->type == LoRaMacFrame::UNCONFIRMED) {
      Serial2.printf("UNCONFIRMED");
    } else if (frame->type == LoRaMacFrame::CONFIRMED) {
      Serial2.printf("CONFIRMED");
    } else if (frame->type == LoRaMacFrame::MULTICAST) {
      Serial2.printf("MULTICAST (error)");
    } else if (frame->type == LoRaMacFrame::PROPRIETARY) {
      Serial2.printf("PROPRIETARY");
    } else {
      Serial2.printf("unknown type");
    }
    Serial2.printf(" frame\n");

    for (uint8_t t = 0; t < frame->numTrials; t++) {
      const char *strTxResult[] = {
        "not started",
        "success",
        "no ack",
        "air busy",
        "Tx timeout",
      };
      Serial2.printf("- [%u] %s\n", t, strTxResult[std::min((int) frame->txResult[t], 4)]);
    }
    delete frame;
    timerSense.startOneShot(10000);
  });
  LoRaWAN.onJoin([](
    LoRaMac &lw,
    bool joined,
    const uint8_t *joinedDevEui,
    const uint8_t *joinedAppEui,
    const uint8_t *joinedAppKey,
    const uint8_t *joinedNwkSKey,
    const uint8_t *joinedAppSKey,
    uint32_t joinedDevAddr,
    const RadioPacket &,
    uint32_t airTime
  ) {
    Serial2.printf("* Tx time of JoinRequest: %lu usec.\n", airTime);
    if (joined) {
      Serial2.println("* Joining done!");
      postTask(taskSense, nullptr);
    } else {
      Serial2.println("* Joining failed. Retry to join.");
      lw.beginJoining(devEui, appEui, appKey);
    }
  });

  LoRaWAN.setPublicNetwork(false);
  Serial2.printf(
    "* DevEUI: %02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X\n",
    devEui[0], devEui[1], devEui[2], devEui[3],
    devEui[4], devEui[5], devEui[6], devEui[7]
  );
  Serial2.printf(
    "* AppKey: %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
    appKey[0], appKey[1], appKey[2], appKey[3],
    appKey[4], appKey[5], appKey[6], appKey[7],
    appKey[8], appKey[9], appKey[10], appKey[11],
    appKey[12], appKey[13], appKey[14], appKey[15]
  );
  Serial2.println("* Let's start join!");
  LoRaWAN.beginJoining(devEui, appEui, appKey);
}
