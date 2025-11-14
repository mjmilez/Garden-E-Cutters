#include "../../main/interfaces.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include <string>
#include <vector>
#include <cstring>

class WebServerImpl : public IWebServer {
private:
    static const char* TAG;
    httpd_handle_t server = NULL;
    std::vector<CutEvent> latestEvents;
    
    // Generic file serving helper
    static esp_err_t serve_file(httpd_req_t *req, const char* filepath, const char* mime_type) {
        FILE* f = fopen(filepath, strstr(filepath, ".png") ? "rb" : "r");
        if (!f) {
            httpd_resp_send_404(req);
            return ESP_FAIL;
        }
        
        httpd_resp_set_type(req, mime_type);
        httpd_resp_set_hdr(req, "Cache-Control", "max-age=3600");
        
        char chunk[1024];
        size_t read;
        while ((read = fread(chunk, 1, sizeof(chunk), f)) > 0) {
            if (httpd_resp_send_chunk(req, chunk, read) != ESP_OK) {
                fclose(f);
                return ESP_FAIL;
            }
        }
        
        fclose(f);
        httpd_resp_send_chunk(req, NULL, 0);
        return ESP_OK;
    }
    
    // Handler functions
    static esp_err_t index_handler(httpd_req_t *req) {
        return serve_file(req, "/spiffs/web/index.html", "text/html");
    }
    
    static esp_err_t css_handler(httpd_req_t *req) {
        return serve_file(req, "/spiffs/web/style.css", "text/css");
    }
    
    static esp_err_t js_handler(httpd_req_t *req) {
        return serve_file(req, "/spiffs/web/app.js", "application/javascript");
    }
    
    static esp_err_t img_handler(httpd_req_t *req) {
        return serve_file(req, "/spiffs/web/gnv_pic.png", "image/png");
    }
    
    static esp_err_t api_cuts_handler(httpd_req_t *req) {
        WebServerImpl* self = (WebServerImpl*)req->user_ctx;
        
        std::string json = "[";
        for (size_t i = 0; i < self->latestEvents.size(); i++) {
            char buf[256];
            snprintf(buf, sizeof(buf), 
                "{\"id\":%lu,\"lat\":%.6f,\"lon\":%.6f,\"force\":%.2f,\"timestamp\":%lu}",
                (unsigned long)self->latestEvents[i].sequence_id,
                self->latestEvents[i].latitude,
                self->latestEvents[i].longitude,
                self->latestEvents[i].force,
                (unsigned long)self->latestEvents[i].timestamp);
            json += buf;
            if (i < self->latestEvents.size() - 1) json += ",";
        }
        json += "]";
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_send(req, json.c_str(), json.length());
        return ESP_OK;
    }
    
public:
    bool start(int port) override {
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.server_port = port;
        config.max_open_sockets = 7;
        config.max_uri_handlers = 10;
        config.stack_size = 8192;
        config.recv_wait_timeout = 10;
        config.send_wait_timeout = 10;
        
        ESP_LOGI(TAG, "Starting HTTP server on port %d", port);
        
        if (httpd_start(&server, &config) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start HTTP server");
            return false;
        }
        
        // Register all URI handlers
        httpd_uri_t index_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = index_handler,
            .user_ctx = this
        };
        httpd_register_uri_handler(server, &index_uri);
        
        httpd_uri_t css_uri = {
            .uri = "/style.css",
            .method = HTTP_GET,
            .handler = css_handler,
            .user_ctx = this
        };
        httpd_register_uri_handler(server, &css_uri);
        
        httpd_uri_t js_uri = {
            .uri = "/app.js",
            .method = HTTP_GET,
            .handler = js_handler,
            .user_ctx = this
        };
        httpd_register_uri_handler(server, &js_uri);
        
        httpd_uri_t img_uri = {
            .uri = "/gnv_pic.png",
            .method = HTTP_GET,
            .handler = img_handler,
            .user_ctx = this
        };
        httpd_register_uri_handler(server, &img_uri);
        
        httpd_uri_t api_cuts_uri = {
            .uri = "/api/cuts",
            .method = HTTP_GET,
            .handler = api_cuts_handler,
            .user_ctx = this
        };
        httpd_register_uri_handler(server, &api_cuts_uri);
        
        ESP_LOGI(TAG, "HTTP server started successfully");
        return true;
    }
    
    void stop() override {
        if (server) {
            httpd_stop(server);
            server = NULL;
        }
    }
    
    void updateData(const std::vector<CutEvent>& events) override {
        latestEvents = events;
    }
};

const char* WebServerImpl::TAG = "WEB_SERVER";

IWebServer* createWebServer() {
    return new WebServerImpl();
}