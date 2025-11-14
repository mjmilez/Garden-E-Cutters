# Garden E-Cutters Web UI – Dynamic Element IDs

## Top Bar
- `connection-status-dot` – visual indicator of BLE connection (color changes based on status).
- `connection-status-text` – text description of connection state ("Connected to Shears", "Disconnected", etc.).
- `total-cuts-value` – numeric display of total cuts recorded in current session / day.

## Tabs
- `log-tab` – container for the Logs view.
- `map-tab` – container for the Map view.

## Filters (Logs)
- `filter-button` – opens filter panel / applies filter.
- `force-threshold-slider` – slider value used to filter cuts by force.
- `force-threshold-value` – text label showing current force threshold.
- `timestamp-start` – start date filter.
- `timestamp-end` – end date filter.
- `gps-fix-type-select` – dropdown to filter by GPS fix type.

## Log Table
- `cut-log-table` – the log table element.
- `cut-log-body` – `<tbody>` where rows for cut events are dynamically inserted.

## Map
- `map-container` – container where the map library (e.g., Leaflet, Google Maps) will render.
- `map-placeholder` – placeholder text when real map is not yet loaded.
