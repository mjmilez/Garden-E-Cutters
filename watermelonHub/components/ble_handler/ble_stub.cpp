#include "../../main/interfaces.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <cstdlib>

// Stub implementation for testing - your friend will replace this
class BLEHandlerStub : public IBLEHandler {
private:
    static const char* TAG;
    int fake_counter = 0;
    
public:
    bool initialize() override {
        ESP_LOGI(TAG, "BLE Stub initialized");
        return true;
    }
    
    bool isConnected() override {
        return true;  // Always "connected" in stub
    }
    
    CutEvent* getNextEvent() override {
        // Generate fake data every 5 seconds
        static uint32_t last_time = 0;
        uint32_t now = esp_timer_get_time() / 1000000;  // Convert to seconds
        
        if (now - last_time > 5) {
            last_time = now;
            CutEvent* event = new CutEvent();
            event->sequence_id = fake_counter++;
            event->timestamp = now;
            event->latitude = 29.6436 + (fake_counter * 0.0001);  // Gainesville, FL area
            event->longitude = -82.3549 + (fake_counter * 0.0001);
            event->force = 10.5 + (rand() % 50) / 10.0;  // Random force 10.5-15.5
            event->fix_type = 3;  // RTK fixed
            return event;
        }
        return nullptr;
    }
    
    void sendAck(uint32_t sequence_id) override {
        ESP_LOGI(TAG, "ACK sent for sequence #%lu", (unsigned long)sequence_id);
    }
};

const char* BLEHandlerStub::TAG = "BLE_STUB";

// Factory function
IBLEHandler* createBLEHandler() {
    return new BLEHandlerStub();
}
