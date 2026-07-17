#include <stdint.h>
#ifndef RemoteLight_h
#define RemoteLight_h

#include <SPI.h>
#include "RF24.h"
#include <FastLED.h>

// PIN config
#define CE_PIN 9
#define CSN_PIN 10
#define FASTLED_DATA_PIN 4

enum DEVICE {
    UNKNOWN = 0,
    RELAY = 1,
    PANEL = 2
};

#define RELAY_STATUS_INTERVAL_MS    5000

#define TYPE_RELAY_STATUS 1
#define TYPE_PANEL_STATUS 2

#define CAR_SWITCH_NUM_CHANNELS 4
#define PIN_CHANNEL_1 2
#define PIN_CHANNEL_2 3
#define PIN_CHANNEL_3 5
#define PIN_CHANNEL_4 6



struct REMOTE_PAYLOAD {
    uint32_t id;
    uint8_t type;
    uint8_t state;
};

const uint8_t DEVICE_ADRESSES[][6] = { "RELAY", "PANEL" };

class RemoteLight
{
    private:
        uint32_t _id;
        DEVICE _device;
        unsigned long _lastStatusSentMillis;
        uint8_t _currentState;
        uint8_t _currentLedState;

        CRGB leds[CAR_SWITCH_NUM_CHANNELS];

        REMOTE_PAYLOAD remotePayload;

        int switchState[4] = {HIGH, HIGH, HIGH, HIGH};
        int tempSwitchState[4] = {HIGH, HIGH, HIGH, HIGH};
        long tempSwitchStateDebounceMillis[4] = {0, 0, 0, 0};

        uint8_t channelPinArr[4] = {PIN_CHANNEL_1, PIN_CHANNEL_2, PIN_CHANNEL_3, PIN_CHANNEL_4};

        bool isTimeToSendRelayStatus();
        void sendRelayStatus();

        // RELAY and PANEL
        void handleIncomingData();
        void activateCurrentState();
        void sendStatusIfNeeded();
        void sendData(uint8_t type, uint8_t state);
        bool recvData();

        // PANEL
        void checkPanelSwitches();
        bool isPanelSwitchOn(uint8_t buttonIdx);

        // DEBUG
        void printPayload();
        void clearPayload();

    public:
        RemoteLight(uint32_t id);
        ~RemoteLight();

        bool begin();
        void doWork();
};

#endif