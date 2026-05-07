#include <IRremote.h>

#define IR_PIN 2   // try 3 if 2 doesn't work

void setup() {
  Serial.begin(9600);
  IrReceiver.begin(IR_PIN, ENABLE_LED_FEEDBACK);
  Serial.println("IR Receiver Ready...");
}

void loop() {
  if (IrReceiver.decode()) {

    Serial.println("----- RAW DATA -----");

    uint16_t len = IrReceiver.irparams.rawlen;

    Serial.print("Length: ");
    Serial.println(len);

    for (uint16_t i = 1; i < len; i++) {   // skip index 0
      Serial.print(IrReceiver.irparams.rawbuf[i] * MICROS_PER_TICK);
      Serial.print(", ");
    }

    Serial.println();
    Serial.println("--------------------");

    IrReceiver.resume();  // ready for next signal
  }
}