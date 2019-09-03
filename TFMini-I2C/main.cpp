#include <cox.h>
#include <algorithm>

#define PRINT_TO_OLED_WING
#define SEND_TO_LORAWAN

uint16_t distance = 0; //distance
uint16_t strength = 0; // signal strength
uint8_t rangeType = 0; //range scale
/*Value range:
 00 (short distance)
 03 (intermediate distance)
 07 (long distance) */

const byte sensor1 = 0x10; //TFMini I2C Address

#ifdef PRINT_TO_OLED_WING
#include <dev/Adafruit_GFX.hpp>
#include <dev/Adafruit_SSD1306.hpp>
Adafruit_SSD1306 display(128, 32, &Wire);
#endif //PRINT_TO_OLED_WING

#ifdef SEND_TO_LORAWAN
#include <LoRaMacKR920.hpp>
SystemFeatherM0::RFM9x RFM95(A4, D10, D11);
LoRaMacKR920 LoRaWAN(RFM95, 3);

static uint8_t devEui[] = "\x98\x76\xB6\x00\x00\x10\xED\xF9";
static const uint8_t appEui[] = "\x00\x00\x00\x00\x00\x00\x00\x00";
static uint8_t appKey[] = "\x8b\xb5\x2a\x8d\x80\xbc\xb0\x49\xbf\xfa\x8d\xb3\x47\x10\xae\xcb";

LoRaMacFrame *sendingFrame = nullptr;
bool joinedToLoRaWAN = false;
uint16_t distanceLastReported = 0;
struct timeval timeLastReport;
#endif //SEND_TO_LORAWAN

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

static void taskSense(void *) {
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

#ifdef SEND_TO_LORAWAN
  struct timeval now, diff;
  gettimeofday(&now, nullptr);
  timersub(&now, &timeLastReport, &diff);
  if (joinedToLoRaWAN && !sendingFrame && (distance != distanceLastReported || diff.tv_sec > 30)) {
    sendingFrame = new LoRaMacFrame(255);
    if (!sendingFrame) {
      Serial2.printf("* Out of memory\n");
      return;
    }

    sendingFrame->port = 1;
    sendingFrame->type = LoRaMacFrame::CONFIRMED;
    sendingFrame->len = sprintf((char *) sendingFrame->buf, "\"distance\":%u", distance);

    error_t err = LoRaWAN.send(sendingFrame);
    Serial2.printf("* Sending a report (%s (%u byte)): %d\n", sendingFrame->buf, sendingFrame->len, err);
    if (err != ERROR_SUCCESS) {
      delete sendingFrame;
      sendingFrame = nullptr;
      return;
    } else {
      distanceLastReported = distance;
      gettimeofday(&timeLastReport, nullptr);
    }
  }
#endif

  postTask(taskSense, nullptr);
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
#endif //PRINT_TO_OLED_WING

#ifdef SEND_TO_LORAWAN
  System.setTimeDiff(9 * 60);  // KST

  LoRaWAN.begin([]() -> uint8_t {
    return 0; // means that the device is connected to an external power source.
  });
  LoRaWAN.onSendDone([](LoRaMac &lw, LoRaMacFrame *frame) {
    // digitalWrite(D13, LOW);
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
    if (frame == sendingFrame) {
      sendingFrame = nullptr;
    }
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
      joinedToLoRaWAN = true;
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

  gettimeofday(&timeLastReport, nullptr);
#endif //SEND_TO_LORAWAN

  postTask(taskSense, nullptr);
}
