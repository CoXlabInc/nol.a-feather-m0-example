#include <cox.h>
#include <algorithm>

#define INTERVAL_SEND 20000
#define INIT_CLASS_C 0

#include <LoRaMacKR920.hpp>

SystemFeatherM0::RFM9x RFM95(A5, D10, D11);
LoRaMacKR920 LoRaWAN(RFM95);

Timer timerSend;

static uint8_t devEui[] = "\x98\x76\xB6\x00\x00\x10\xED\xF9";
static const uint8_t appEui[] = "\x00\x00\x00\x00\x00\x00\x00\x00";
static uint8_t appKey[16];

static void taskPeriodicSend(void *) {
  LoRaMacFrame *f = new LoRaMacFrame(255);
  if (!f) {
    printf("* Out of memory\n");
    return;
  }

  f->port = 1;
  f->type = LoRaMacFrame::CONFIRMED;
  f->len = sprintf(
    (char *) f->buf,
    "\"Now\":%lu",
    (uint32_t) System.getDateTime()
  );

  /* Uncomment below line to specify frequency. */
  // f->freq = 922500000;

  /* Uncomment below lines to specify parameters manually. */
  // LoRaWAN.useADR = false;
  // f->modulation = Radio::MOD_LORA;
  // f->meta.LoRa.bw = Radio::BW_125kHz;
  // f->meta.LoRa.sf = Radio::SF7;
  // f->power = 1; /* Index 1 => MaxEIRP - 2 dBm */

  /* Uncomment below line to specify number of trials. */
  // f->numTrials = 1;

  error_t err = LoRaWAN.send(f);
  Serial2.printf("* Sending periodic report (%s (%u byte)): %d\n", f->buf, f->len, err);
  if (err != ERROR_SUCCESS) {
    delete f;
    timerSend.startOneShot(INTERVAL_SEND);
    return;
  }

  digitalWrite(D13, HIGH);
}

static void eventLoRaWANJoin(
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
    digitalWrite(D13, LOW);
    postTask(taskPeriodicSend, NULL);
  } else {
    Serial2.println("* Joining failed. Retry to join.");
    lw.beginJoining(devEui, appEui, appKey);
  }
}

static void eventLoRaWANSendDone(LoRaMac &lw, LoRaMacFrame *frame) {
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

  timerSend.startOneShot(INTERVAL_SEND);
}

static void eventLoRaWANReceive(LoRaMac &lw, const LoRaMacFrame *frame) {
  static uint32_t fCntDownPrev = 0;

  Serial2.printf(
    "* Received a frame. Destined for port:%u, fCnt:0x%X",
    frame->port, frame->fCnt
  );

  if (fCntDownPrev != 0 && fCntDownPrev == frame->fCnt) {
    Serial2.print(" (duplicate)");
  }
  fCntDownPrev = frame->fCnt;
  Serial2.printf(", Freq:%lu Hz, RSSI:%d dB", frame->freq, frame->power);

  if (frame->modulation == Radio::MOD_LORA) {
    const char *strBW[] = {
      "Unknown", "125kHz", "250kHz", "500kHz", "Unexpected value"
    };
    Serial2.printf(
      ", LoRa, SF:%u, BW:%s",
      frame->meta.LoRa.sf, strBW[std::min((int) frame->meta.LoRa.bw, 4)]
    );
  } else if (frame->modulation == Radio::MOD_FSK) {
    Serial2.printf(", FSK");
  } else {
    Serial2.printf(", Unkndown modulation");
  }
  if (frame->type == LoRaMacFrame::UNCONFIRMED) {
    Serial2.printf(", Type:UNCONFIRMED,");
  } else if (frame->type == LoRaMacFrame::CONFIRMED) {
    Serial2.printf(", Type:CONFIRMED,");
  } else if (frame->type == LoRaMacFrame::MULTICAST) {
    Serial2.printf(", Type:MULTICAST,");
  } else if (frame->type == LoRaMacFrame::PROPRIETARY) {
    Serial2.printf(", Type:PROPRIETARY,");
  } else {
    Serial2.printf(", unknown type,");
  }

  for (uint8_t i = 0; i < frame->len; i++) {
    Serial2.printf(" %02X", frame->buf[i]);
  }
  Serial2.printf(" (%u byte)\n", frame->len);

  if (
    (frame->type == LoRaMacFrame::CONFIRMED || lw.framePending) &&
    lw.getNumPendingSendFrames() == 0
  ) {
    // If there is no pending send frames, send an empty frame to ack or pull more frames.
    LoRaMacFrame *emptyFrame = new LoRaMacFrame(0);
    if (emptyFrame) {
      error_t err = LoRaWAN.send(emptyFrame);
      if (err != ERROR_SUCCESS) {
        delete emptyFrame;
      }
    }
  }
}

void setup() {
  Serial2.begin(115200);
  Serial2.printf("\n*** [Feather-M0] LoRaWAN  Example ***\n");

  System.setTimeDiff(9 * 60);  // KST

  timerSend.onFired(taskPeriodicSend, NULL);

  LoRaWAN.begin();
  LoRaWAN.onSendDone(eventLoRaWANSendDone);
  LoRaWAN.onReceive(eventLoRaWANReceive);
  LoRaWAN.onJoin(eventLoRaWANJoin);

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

  pinMode(D13, OUTPUT);
  digitalWrite(D13, LOW);
}
