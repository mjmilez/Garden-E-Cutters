# Garden-E-Cutters

## Overview
The E-Cutter project is a agriculture system designed to record cutting events in a watermelon field using **GPS-RTK-based geolocation**. The system uses real time positional data, sensor feedback, and a local data logging pipeline to track when and where each cut occurs.
---

## Completed work
- Researched and evaluated multiple geolocation methods (**GPS**, **DGPS**, **UWB**, **RTK**) based on accuracy and cost.
- Finalized **GPS-RTK** as the primary geolocation solution!!!
- Designed system architecture defining three major layers:
  - **External Interface:** RTK receiver, dashboard, and data entry points.
  - **Internal Systems:** State management (waiting for cut / cut event occurring), data processing, and signal handling.
  - **Persistent State:** Local database for event storage using CSV format.
- Documented all architectural decisions and design trade-offs.

---

## System architecture
The system is composed of three major subsystems:

1. **External interface**
   - **Cut Tool:** The RTK receiver detects when a cut occurs and serves as the data entry point.
   - **Dashboard:** Displays real-time data and logged events using a local map (e.g., Google Maps).

2. **Internal systems**
   - Manages system states:  
     - *Waiting for Cut* – monitors trigger conditions (force sensor, contact switch, or shear detection).  
     - *Cut Event Occurring* – records and transmits event data.
   - Handles real-time detection, state transitions, and data packaging.
   - Connects external inputs to persistent storage.

3. **Persistent state**
   - Local CSV based database hosted on the hub device.
   - Stores the following parameters for each cut event:
     - Timestamp  
     - Latitude / Longitude / Altitude  
     - Fix Type (single, float, fixed)  
     - Horizontal Accuracy
   - Ensures reliable data access for dashboard visualization and future analytics.

**Data flow:**  
RTK Receiver → Internal Processing → Local Database → Dashboard Visualization

---

## Known bugs / limitations
- The WiFi vs. no WiFi network configuration is still under evaluation.


---

## Next steps
- Finalize network communication design (Wi-Fi vs. local base station).
- Implement real time data synchronization between RTK receiver and dashboard.
- Integrate physical sensors for automatic cut detection.
- End to end and field validation testing.
- Full integration testing between the RTK module, microcontroller, and local database still needs to be done.
---
