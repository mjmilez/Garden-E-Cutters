#ifndef INTERFACES_H
#define INTERFACES_H

#include <string>
#include <vector>


struct CutEvent {
	uint32_t sequence_id;
	uint32_t timestamp;
	float latitude;
	float longitude;
	float force;
	uint8_t fix_type;
	bool acknowledged;
};

class IBLEHandler {
public:
	virtual ~IBLEHandler() = default;
	virtual bool initialize() = 0;
	virtual bool isConnected() = 0;
	virtual CutEvent* getNextEvent() = 0; // Returns nullptr if no new data
	virtual void sendAck(uint32_t sequence_id) = 0;
};

class IWebServer {
public:
	virtual ~IWebServer() = default;
	virtual bool start(int port) = 0;
	virtual void stop() = 0;
	virtual void updateData(const std::vector<CutEvent>& events) = 0;
};

// TODO: implement interface for data management

class IDataManager {
public:
	virtual ~IDataManager() = default;
	virtual bool initialize() = 0;
	virtual bool storeEvent(const CutEvent& event) = 0;
	virtual std::vector<CutEvent> getRecentEvents(int count) = 0;
	virtual std::vector<CutEvent> getAllEvents() = 0;
};


#endif
