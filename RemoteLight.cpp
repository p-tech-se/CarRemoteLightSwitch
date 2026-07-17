#include "Arduino.h"
#include "RemoteLight.h"
#include <EEPROM.h>

RF24 radio(CE_PIN, CSN_PIN);

RemoteLight::RemoteLight(uint32_t id)
{
    _id = id;
    _device = UNKNOWN;
    _lastStatusSentMillis = 0;
    _currentState = 0; // Default all off
}

RemoteLight::~RemoteLight()
{
}

bool RemoteLight::begin()
{
    if (!radio.begin())
    {
        return false;
    }

    radio.setPALevel(RF24_PA_LOW);  // RF24_PA_MAX is default.
    radio.setPayloadSize(sizeof(REMOTE_PAYLOAD));

    uint8_t dev = EEPROM.read(0);

    Serial.print(F("Current device mode: "));
    Serial.println(dev);

    if (dev != RELAY && dev != PANEL) {
        Serial.println(F("Which mode is this? Enter '1' for relay or '2' for panel."));
        while (!Serial.available()) {
            // wait for user input
        }
        char input = Serial.parseInt();

        if (input == 1) {
            _device = RELAY;
            EEPROM.put(0, _device);
        } else if (input == 2) {
            _device = PANEL;
            EEPROM.put(0, _device);

            FastLED.addLeds<NEOPIXEL, FASTLED_DATA_PIN>(leds, CAR_SWITCH_NUM_CHANNELS);
        } else {
            _device = UNKNOWN;
        }
    } else {
        _device = dev;
    }


    if (_device == RELAY)
    {
        radio.stopListening(DEVICE_ADRESSES[0]);
        radio.openReadingPipe(1, DEVICE_ADRESSES[1]);

        // Setup relay pins
        for (int i=0; i<CAR_SWITCH_NUM_CHANNELS; i++) {
            pinMode(channelPinArr[i], OUTPUT);
            digitalWrite(channelPinArr[i], HIGH); // Default off
        }

        // Read state of relay from eeprom
        uint8_t eepromState = EEPROM.read(1);
        if (eepromState > 0) {
            _currentState = eepromState & 0x0F;
        }
    }
    else if (_device == PANEL)
    {
        radio.stopListening(DEVICE_ADRESSES[1]);
        radio.openReadingPipe(1, DEVICE_ADRESSES[0]);

        // Setup input pins
        for (int i=0; i<CAR_SWITCH_NUM_CHANNELS; i++) {
            pinMode(channelPinArr[i], INPUT_PULLUP);
        }
    }
    else
    {
        return false;
    }

    radio.startListening();
    radio.printDetails();
    return true;
}

void RemoteLight::doWork() {
    // RELAY and PANEL
    activateCurrentState();
    handleIncomingData();
    sendStatusIfNeeded();

    // PANEL
    checkPanelSwitches();
}

// PRIVATES
void RemoteLight::activateCurrentState() {
    if (_device == RELAY) {
        // Set pins to enable/disable relay
        for (int i=0; i<CAR_SWITCH_NUM_CHANNELS; i++) {
            if (bitRead(_currentState, i) == 1) {
                digitalWrite(channelPinArr[i], LOW);
            } else {
                digitalWrite(channelPinArr[i], HIGH);
            }
        }


    } else if (_device == PANEL) {
        // Set status LED from _currentState

        for (int i=0; i<CAR_SWITCH_NUM_CHANNELS; i++) {
            if (bitRead(_currentState, i) == 1) {
                leds[i] = CRGB::Blue;
            } else {
                leds[i] = CRGB::Black;
            }
        }

        FastLED.show();
    }
}

void RemoteLight::sendStatusIfNeeded() {
    // Is it time to send status
    if (_lastStatusSentMillis > 0 && (millis() - _lastStatusSentMillis) < RELAY_STATUS_INTERVAL_MS) {
        return;
    }

    remotePayload.id = _id;
    if (_device == RELAY) {
        remotePayload.type = TYPE_RELAY_STATUS;
    }
    else if (_device == PANEL) {
        remotePayload.type = TYPE_PANEL_STATUS;
    }
    else {
        Serial.println(F("UNKNOWN CURRENT DEVICE"));
        clearPayload();
        _lastStatusSentMillis = millis();
        return;
    }

    remotePayload.state = _currentState;

    Serial.println(F("SENDING"));
    printPayload();

    radio.stopListening();
    bool sentOk = radio.write(&remotePayload, sizeof(REMOTE_PAYLOAD));
    if (sentOk) {
        Serial.println(F("Transmission OK"));
    } else {
        Serial.println(F("Transmission failed or timed out"));
    }

    clearPayload();
    radio.startListening();

    _lastStatusSentMillis = millis();
}

bool RemoteLight::recvData() {
    uint8_t pipe;
    if (radio.available(&pipe)) {
        uint8_t bytes = radio.getPayloadSize();
        radio.read(&remotePayload, bytes);
        Serial.println(F("INCOMING"));
        printPayload();

        if (remotePayload.id == _id) {
            return true;
        }

        Serial.print(F("INVALID ID my: "));
        Serial.println(_id);
        clearPayload();
    }

    return false;
}

void RemoteLight::printPayload() {
    Serial.print(F("    ID: "));
    Serial.println(remotePayload.id);
    Serial.print(F("    Type: "));
    Serial.println(remotePayload.type);
    Serial.print(F("    State: "));
    Serial.println(remotePayload.state);
}

void RemoteLight::clearPayload() {
    remotePayload.id = 0;
    remotePayload.type = 0;
    remotePayload.state = 0;
}

void RemoteLight::handleIncomingData() {
    if (!recvData()) {
        return;
    }

    if (_device == RELAY && remotePayload.type == TYPE_PANEL_STATUS) {
        if (_currentState != remotePayload.state) {
            EEPROM.put(1, remotePayload.state);
        }

        _currentState = remotePayload.state;
    }
    else if (_device == PANEL && remotePayload.type == TYPE_RELAY_STATUS) {
        // Status from RELAY, store state for LED
    }
    else {
        Serial.println(F("Unhandled incoming data"));
    }

    clearPayload();
}

void RemoteLight::checkPanelSwitches() {
    // Only for panel
    if (_device != PANEL) {
        return;
    }

    uint8_t tempState = 0;

    for (int i=0; i<CAR_SWITCH_NUM_CHANNELS; i++) {
        if (isPanelSwitchOn(i)) {
            tempState = bitSet(tempState, i);
        }
    }

    // If one or more switches has changed state, send status to relay
    if (tempState != _currentState) {
        Serial.print(F("Panel switch state changed"));
        Serial.print(F("    Current state: "));
        Serial.println(_currentState, BIN);
        Serial.print(F("    New state: "));
        Serial.println(tempState, BIN);

        _currentState = tempState;
        _lastStatusSentMillis = 0;
    }
}

bool RemoteLight::isPanelSwitchOn(uint8_t buttonIdx) {
    int reading = digitalRead(channelPinArr[buttonIdx]);

    if (reading != tempSwitchState[buttonIdx]) {
        // reset the debouncing timer
        tempSwitchStateDebounceMillis[buttonIdx] = millis();
    }

    if ((millis() - tempSwitchStateDebounceMillis[buttonIdx]) > 50) {
        if (reading != switchState[buttonIdx]) {
            switchState[buttonIdx] = reading;
        }
    }

    tempSwitchState[buttonIdx] = reading;

    // Switch is on when pin is low
    return switchState[buttonIdx] == LOW;
}
