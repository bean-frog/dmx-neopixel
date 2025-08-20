#include <Conceptinetics.h>
#include <SoftwareSerial.h>

#define TX_PIN 6
#define RX_PIN 7  // Not used
#define START_BYTE 0xA1

DMX_Slave dmxSlave(9);                      // 9 DMX channels
SoftwareSerial outSerial(RX_PIN, TX_PIN);   // RX, TX

uint8_t lastValues[9];

void setup() {
  dmxSlave.enable();
  dmxSlave.setStartAddress(1);              // DMX start at channel 1
  outSerial.begin(57600);                   // Must match Mega
}

void loop() {
  bool changed = false;
  uint8_t values[9];

  for (int i = 0; i < 9; i++) {
    values[i] = dmxSlave.getChannelValue(i + 1);
    if (values[i] != lastValues[i]) {
      changed = true;
    }
  }

  if (changed) {
    outSerial.write(START_BYTE);
    for (int i = 0; i < 9; i++) {
      outSerial.write(values[i]);
      lastValues[i] = values[i];
    }
  }

  delay(10);  // Minimal CPU use, no visual disruption
}
