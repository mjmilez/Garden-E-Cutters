# Garden E-Cutters Web UI – Dynamic Element IDs

## Top Bar

- `top-bar` – overall header bar for the dashboard (contains title and status bar).
- `status-bar` – container for connection and total cuts status.
- `connection-status-dot` – visual indicator of ESP/shears connection state
  (CSS classes `connected` / `disconnected` change the dot color).
- `connection-status-text` – text description of connection state  
  (e.g., "Connected to Shears", "Disconnected from Shears").
- `total-cuts-label` – static label text "Total Cuts:".
- `total-cuts-value` – numeric display of the **currently filtered** cuts:  
  - On the **Logs** tab, shows the number of rows in the filtered log table.  
  - On the **Map** tab, shows the number of markers plotted for the filtered cuts.

---

## Tabs

- `tabs` – `<nav>` that contains the tab buttons.
- `log-tab` – `<section>` container for the **Logs** view.  
  Shown/hidden via `tab-button` click handlers.
- `map-tab` – `<section>` container for the **Map** view.  
  Shown/hidden via `tab-button` click handlers.

> Note: The tab buttons themselves use the `tab-button` **class** with  
> `data-tab` attributes pointing to `log-tab` or `map-tab`.

---

## Filters (Logs)

- `log-filters` – container for all log filter controls.
- `filter-button` – button that triggers filtering using current control values
  (also used as an explicit “apply filters” control).
- `force-threshold-slider` – `<input type="range">` used to filter out cuts
  whose `forceN` is below the selected threshold.
- `force-threshold-value` – text label showing the current force threshold
  value in Newtons (e.g., "150 N"). Updated live on slider movement.
- `timestamp-start` – `<input type="date">` for the **start** of the timestamp
  filter range. The code enforces that this date is never after `timestamp-end`.
- `timestamp-end` – `<input type="date">` for the **end** of the timestamp
  filter range. Adjusted/clamped if necessary so that `timestamp-start ≤ timestamp-end`.

---

## Log Table

- `cut-log-table` – `<table>` element that displays the filtered cut events.
  Columns: **Date**, **Time**, **Latitude**, **Longitude**, **Force (N)**.
- `cut-log-body` – `<tbody>` where table rows for cut events are dynamically
  inserted by `renderTable()` based on the current filters.

---

## Map

- `map-container` – main map region that:
  - Renders the calibrated static background image (`gnv_pic.png`) of the field.
  - Maintains a fixed aspect ratio matching the image so GPS-to-pixel mapping
    remains accurate.
- `map-overlay` – absolutely positioned container on top of the background
  where JS inserts marker elements for each filtered cut.
  Each marker is a `div` with class `map-marker` styled as a red pin.
- `map-tooltip` – floating tooltip element used to display details for the
  cut under the mouse. Its content and position are updated on marker
  hover and clamped so the tooltip stays within the map bounds.

---
