#include "../../main/interfaces.h"
#include "esp_log.h"

// Stub implementation - real BLE is handled by base_ble.c
class BLEHandlerStub : public IBLEHandler {
private:
    static const char* TAG;
    
public:
    bool initialize() override {
        ESP_LOGI(TAG, "BLE Stub initialized (real BLE handled by base_ble.c)");
        return true;
    }
    
    bool isConnected() override {
        // Real connection status is managed by base_ble.c
        return false;
    }
    
    CutEvent* getNextEvent() override {
        // Real data comes via log_transfer_client.c, not this interface
        return nullptr;
    }
    
    void sendAck(uint32_t sequence_id) override {
        // Not used - real BLE handles acknowledgments
        (void)sequence_id;
    }
};

const char* BLEHandlerStub::TAG = "BLE_STUB";

// Factory function
IBLEHandler* createBLEHandler() {
    return new BLEHandlerStub();
}
