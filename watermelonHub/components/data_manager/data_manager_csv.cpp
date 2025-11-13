#include "../../main/interfaces.h"
#include "esp_log.h"
#include <cstdio>
#include <algorithm>

class DataManagerCSV : public IDataManager {
private:
    static const char* TAG;
    std::vector<CutEvent> eventCache;
    const char* CSV_PATH = "/spiffs/cuts.csv";
    
public:
    bool initialize() override {
        ESP_LOGI(TAG, "Initializing CSV Data Manager");
        // Load existing data if any
        loadFromFile();
        return true;
    }
    
    bool storeEvent(const CutEvent& event) override {
        eventCache.push_back(event);
        
        // Append to CSV file
        FILE* f = fopen(CSV_PATH, "a");
        if (f == nullptr) {
            // Create file with header if it doesn't exist
            f = fopen(CSV_PATH, "w");
            if (f) {
                fprintf(f, "sequence,timestamp,latitude,longitude,force,fix_type\n");
            }
        }
        
        if (f) {
            fprintf(f, "%lu,%lu,%.8f,%.8f,%.2f,%d\n",
                    (unsigned long)event.sequence_id, 
                    (unsigned long)event.timestamp,
                    event.latitude, event.longitude,
                    event.force, event.fix_type);
            fclose(f);
            return true;
        }
        
        ESP_LOGE(TAG, "Failed to write to CSV");
        return false;
    }
    
    std::vector<CutEvent> getRecentEvents(int count) override {
        int start = std::max(0, (int)eventCache.size() - count);
        return std::vector<CutEvent>(eventCache.begin() + start, eventCache.end());
    }
    
    std::vector<CutEvent> getAllEvents() override {
        return eventCache;
    }
    
private:
    void loadFromFile() {
        // Load existing CSV data into cache
        FILE* f = fopen(CSV_PATH, "r");
        if (f) {
            char line[256];
            fgets(line, sizeof(line), f);  // Skip header
            
            while (fgets(line, sizeof(line), f)) {
                CutEvent event;
                unsigned long seq_id, timestamp;
                sscanf(line, "%lu,%lu,%f,%f,%f,%hhd",
                       &seq_id, &timestamp,
                       &event.latitude, &event.longitude,
                       &event.force, &event.fix_type);
                event.sequence_id = seq_id;
                event.timestamp = timestamp;
                eventCache.push_back(event);
            }
            fclose(f);
            ESP_LOGI(TAG, "Loaded %zu events from CSV", eventCache.size());
        }
    }
};

const char* DataManagerCSV::TAG = "DATA_MGR_CSV";

IDataManager* createDataManager() {
    return new DataManagerCSV();
}
