// main/hub_controller.cpp
#include "hub_controller.h"
#include "esp_log.h"
#include <algorithm>

const char* HubController::TAG = "HUB_CONTROLLER";

HubController::HubController() 
    : bleHandler(nullptr), webServer(nullptr), dataManager(nullptr), running(false) {
    eventQueue = xQueueCreate(100, sizeof(CutEvent));
}

HubController::~HubController() {
    if (eventQueue) {
        vQueueDelete(eventQueue);
    }
}

bool HubController::initialize() {
    ESP_LOGI(TAG, "Initializing Hub Controller");
    
    // Initialize data manager
    if (dataManager && !dataManager->initialize()) {
        ESP_LOGE(TAG, "Failed to initialize data manager");
        return false;
    }
    
    // Initialize BLE
    if (bleHandler && !bleHandler->initialize()) {
        ESP_LOGE(TAG, "Failed to initialize BLE handler");
        return false;
    }
    
    // Start web server
    if (webServer && !webServer->start(80)) {
        ESP_LOGE(TAG, "Failed to start web server");
        return false;
    }
    
    ESP_LOGI(TAG, "Hub Controller initialized successfully");
    return true;
}

void HubController::processIncomingData() {
    if (!bleHandler || !dataManager) return;
    
    // Check for new data from BLE
    CutEvent* event = bleHandler->getNextEvent();
    if (event != nullptr) {
        ESP_LOGI(TAG, "Received event #%lu: Lat=%.6f, Lon=%.6f, Force=%.2f", 
                 (unsigned long)event->sequence_id, event->latitude, event->longitude, event->force);
        
        // Store in database
        if (dataManager->storeEvent(*event)) {
            // Send acknowledgment
            bleHandler->sendAck(event->sequence_id);
            ESP_LOGI(TAG, "Event stored and acknowledged");
        } else {
            ESP_LOGE(TAG, "Failed to store event");
        }
        
        delete event;  // Clean up
    }
}

void HubController::updateWebInterface() {
    if (!webServer || !dataManager) return;
    
    // Get recent events and push to web server
    auto events = dataManager->getRecentEvents(100);
    webServer->updateData(events);
}

void HubController::start() {
    running = true;
    ESP_LOGI(TAG, "Hub Controller started");
}

void HubController::stop() {
    running = false;
    if (webServer) webServer->stop();
    ESP_LOGI(TAG, "Hub Controller stopped");
}
