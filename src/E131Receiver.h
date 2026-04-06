#pragma once
#include <Arduino.h>
#include <AsyncUDP.h>

// Forward declaration to link with led_engine.cpp
extern void handleStreamData(uint16_t universe, uint8_t* dmxData, uint16_t length);

class E131Receiver {
private:
    AsyncUDP udp;
    unsigned long lastPacketTime = 0;
    bool streaming = false;

public:
    void begin() {
        // E1.31 standard UDP port
        if (udp.listen(5568)) {
            udp.onPacket([this](AsyncUDPPacket packet) {
                uint8_t* data = packet.data();
                size_t len = packet.length();

                // FIX: The E1.31 ACN packet identifier is "ASC-E1.17". 
                // data[7] must be a dash '-', not an 'N'.
                if (len >= 126 && data[1] == 0x10 && data[4] == 'A' && data[5] == 'S' && data[6] == 'C' && data[7] == '-') {
                    
                    // Extract Universe (bytes 113-114) and Data Length (bytes 123-124)
                    uint16_t universe = (data[113] << 8) | data[114];
                    uint16_t dmxLength = (data[123] << 8) | data[124];
                    
                    // DMX data payload starts at byte 125
                    uint8_t* dmxData = &data[125];

                    // Send to the rendering engine
                    handleStreamData(universe, dmxData, dmxLength);
                    
                    this->lastPacketTime = millis();
                    this->streaming = true;
                }
            });
            Serial.println("[E1.31] Listening on UDP 5568");
        }
    }

    bool isStreaming() {
        // If no packets arrive for 2 seconds, drop the stream and revert to Lua
        if (streaming && (millis() - lastPacketTime > 2000)) {
            streaming = false;
        }
        return streaming;
    }
};