#include "../../main/interfaces.h"
#include "esp_log.h"

class WebServerStub : public IWebServer {
private:
    static const char* TAG;
    
public:
    bool start(int port) override {
        ESP_LOGI(TAG, "Web server stub started on port %d", port);
        return true;
    }
    
    void stop() override {
        ESP_LOGI(TAG, "Web server stub stopped");
    }
    
    void updateData(const std::vector<CutEvent>& events) override {
        ESP_LOGI(TAG, "Updated with %d events", events.size());
    }
};

const char* WebServerStub::TAG = "WEB_STUB";

IWebServer* createWebServer() {
    return new WebServerStub();
}
