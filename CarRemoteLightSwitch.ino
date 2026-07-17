#include "RemoteLight.h"
#include "printf.h"

RemoteLight remoteLight(1234);

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    // some boards need to wait to ensure access to serial over USB
  }

  printf_begin();

  // initialize the transceiver on the SPI bus
  if (!remoteLight.begin()) {
    Serial.println(F("radio hardware is not responding!!"));
    while (1) {}  // hold in infinite loop
  }
}

void loop() {
  remoteLight.doWork();
}

