// main/hub_controller.h
#ifndef HUB_CONTROLLER_H
#define HUB_CONTROLLER_H

#include "interfaces.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

class HubController {
private:
    IBLEHandler* bleHandler;
    IWebServer* webServer;
    IDataManager* dataManager;
    
    QueueHandle_t eventQueue;
    bool running;
    
    static const char* TAG;

public:
    HubController();
    ~HubController();
    
    // Set components (dependency injection)
    void setBLEHandler(IBLEHandler* handler) { bleHandler = handler; }
    void setWebServer(IWebServer* server) { webServer = server; }
    void setDataManager(IDataManager* manager) { dataManager = manager; }
    
    // Main control
    bool initialize();
    void start();
    void stop();
    
    // Core processing tasks
    void processIncomingData();
    void updateWebInterface();
    void performDataMaintenance();
};

#endif
