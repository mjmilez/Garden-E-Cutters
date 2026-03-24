/**
 * Garden E-Cutters Dashboard (Leaflet)
 *
 * Dev mode:
 *   - Uses OSM tiles from the internet
 *
 * Pi/offline mode:
 *   - Point Leaflet at your local tile server (or pre-rendered tiles)
 */

const USE_ONLINE_TILES = true;
const LOCAL_TILE_URL = "/tiles/{z}/{x}/{y}.png";
const LOCAL_MIN_ZOOM = 4;
const LOCAL_MAX_ZOOM = 12;

const MAP_BOUNDS = {
  // Florida (rough bbox)
  minLat: 24.396308,
  maxLat: 31.000888,
  minLon: -87.634938,
  maxLon: -80.031362
};

const DEFAULT_MARKER_RADIUS = 6;
const DEFAULT_FILL_OPACITY = 0.8;

const HDOP_MIN = 0.1;
const HDOP_MAX = 10;
const HDOP_MIN_RADIUS = 2;
const HDOP_MAX_RADIUS = 30;

// "normal" = current behavior, "hdop" = bubble size/opacity driven by HDOP
let mapMode = "normal";

let cutsData = [];

// Leaflet state
let map = null;
let cutLayer = null;
let mapInitialized = false;

// If longitude comes in positive but our bounds are negative (US), flip it.
function normalizeLonForBounds(lon) {
  if (typeof lon !== "number") return lon;
  const boundsAreWest = MAP_BOUNDS.maxLon < 0 && MAP_BOUNDS.minLon < 0;
  if (boundsAreWest && lon > 0) return -lon;
  return lon;
}

// API only gives a UTC time-of-day (no date). We treat it as "today" and format in ET.
// This will show EST/EDT correctly based on today.
function utcTimeToET(utcTime) {
  if (!utcTime) return "";

  const raw = String(utcTime).trim();
  // "0" is the default placeholder from manual cuts — not a real time
  if (raw === "0" || raw === "") return "";

  const main = raw.split(".")[0].padStart(6, "0"); // HHMMSS
  const hh = Number(main.slice(0, 2));
  const mm = Number(main.slice(2, 4));
  const ss = Number(main.slice(4, 6));

  if ([hh, mm, ss].some(n => Number.isNaN(n))) return "";
  if (hh > 23 || mm > 59 || ss > 59) return "";

  const now = new Date();
  const y = now.getUTCFullYear();
  const m = now.getUTCMonth();
  const d = now.getUTCDate();
  const dt = new Date(Date.UTC(y, m, d, hh, mm, ss));

  return new Intl.DateTimeFormat("en-US", {
    timeZone: "America/New_York",
    hour: "numeric",
    minute: "2-digit",
    second: "2-digit",
    hour12: true
  }).format(dt);
}

/**
 * Convert utc_date from the API (DDMMYY string from $GNRMC) into "YYYY-MM-DD"
 * so it can be compared against the <input type="date"> filter values.
 *
 * Examples:
 *   "240326" → "2026-03-24"
 *   "010125" → "2025-01-01"
 *   ""       → ""
 */
function utcDateToISO(utcDate) {
  if (!utcDate) return "";
  const raw = String(utcDate).trim();
  if (raw === "0" || raw.length < 6) return "";

  const dd = raw.slice(0, 2);
  const mm = raw.slice(2, 4);
  const yy = raw.slice(4, 6);

  // Validate: day 01-31, month 01-12, year is digits
  const day = Number(dd);
  const month = Number(mm);
  if (Number.isNaN(day) || Number.isNaN(month)) return "";
  if (day < 1 || day > 31 || month < 1 || month > 12) return "";

  // Assume 20xx for two-digit year
  return `20${yy}-${mm}-${dd}`;
}

async function fetchCuts() {
  const res = await fetch("/api/cuts", { cache: "no-store" });
  if (!res.ok) throw new Error(`GET /api/cuts failed: ${res.status}`);

  const apiCuts = await res.json();

  cutsData = (Array.isArray(apiCuts) ? apiCuts : []).map(row => ({
    id: row.id,
    utc_date: row.utc_date ?? "",
    utc_time: row.utc_time,

    latitude: Number(row.latitude),
    longitude: normalizeLonForBounds(Number(row.longitude)),

    altitude: row.altitude != null ? Number(row.altitude) : null,
    fix_quality: row.fix_quality != null ? Number(row.fix_quality) : null,
    geoid_height: row.geoid_height != null ? Number(row.geoid_height) : null,
    hdop: row.hdop != null ? Number(row.hdop) : null,
    num_satellites: row.num_satellites != null ? Number(row.num_satellites) : null
  }));
}

// ── Status polling ────────────────────────────────────────────────

/**
 * Poll /api/status and update the header indicator.
 *
 * Shows one of:
 *   - "Transferring data..."   (green dot, transfer in progress)
 *   - "Last sync: X min ago"   (green dot, idle after a successful transfer)
 *   - "Last sync failed"       (red dot, last transfer had an error)
 *   - "No syncs yet"           (grey/red dot, server just started)
 *   - "Hub offline"            (red dot, can't reach the API at all)
 */
async function fetchStatus() {
  try {
    const res = await fetch("/api/status", { cache: "no-store" });
    if (!res.ok) return;
    const status = await res.json();

    const dot = document.getElementById("connection-status-dot");
    const text = document.getElementById("connection-status-text");

    if (status.transfer_active) {
      dot.className = "connected";
      text.textContent = "Transferring data...";
    } else if (status.last_transfer_time) {
      if (status.last_transfer_ok === false) {
        dot.className = "disconnected";
        text.textContent = "Last sync failed";
      } else {
        dot.className = "connected";
        const ago = Math.floor(Date.now() / 1000 - status.last_transfer_time);
        if (ago < 60) {
          text.textContent = "Last sync: just now";
        } else if (ago < 3600) {
          const mins = Math.floor(ago / 60);
          text.textContent = `Last sync: ${mins} min ago`;
        } else if (ago < 86400) {
          const hrs = Math.floor(ago / 3600);
          text.textContent = `Last sync: ${hrs} hr ago`;
        } else {
          const days = Math.floor(ago / 86400);
          text.textContent = `Last sync: ${days}d ago`;
        }
      }
    } else {
      dot.className = "disconnected";
      text.textContent = "No syncs yet";
    }
  } catch (err) {
    // Can't reach the server at all
    const dot = document.getElementById("connection-status-dot");
    const text = document.getElementById("connection-status-text");
    if (dot) dot.className = "disconnected";
    if (text) text.textContent = "Hub offline";
  }
}

// ── Sorting & filtering ───────────────────────────────────────────

function toSortKey(cut) {
  const dateISO = utcDateToISO(cut.utc_date);   // "YYYY-MM-DD" or ""
  const raw = (cut.utc_time == null) ? "" : String(cut.utc_time);
  const timeKey = raw.split(".")[0].padStart(6, "0");
  return `${dateISO} ${timeKey}`;
}

/**
 * Return cuts filtered by the date range inputs and sorted newest-first.
 * If no date filters are set, returns all cuts sorted.
 */
function getFilteredCuts() {
  const startInput = document.getElementById("timestamp-start");
  const endInput = document.getElementById("timestamp-end");

  const startValue = startInput ? startInput.value : "";  // "YYYY-MM-DD" or ""
  const endValue = endInput ? endInput.value : "";

  let filtered = cutsData;

  // Only filter if at least one date is set
  if (startValue || endValue) {
    filtered = cutsData.filter(cut => {
      const isoDate = utcDateToISO(cut.utc_date);

      // If the cut has no date, exclude it when filtering is active
      if (!isoDate) return false;

      if (startValue && isoDate < startValue) return false;
      if (endValue && isoDate > endValue) return false;
      return true;
    });
  }

  return [...filtered].sort((a, b) => toSortKey(b).localeCompare(toSortKey(a)));
}

/**
 * Convert a user-entered date string into GPS DDMMYY format.
 * Accepts: "MM/DD/YYYY", "MM-DD-YYYY", "YYYY-MM-DD" (from <input type="date">)
 * Returns: "DDMMYY" string, or "" if unparseable.
 */
function userDateToDDMMYY(input) {
  if (!input) return "";
  const raw = input.trim();

  let mm, dd, yyyy;

  if (raw.includes("/")) {
    // MM/DD/YYYY
    const parts = raw.split("/");
    if (parts.length !== 3) return "";
    [mm, dd, yyyy] = parts;
  } else if (raw.length === 10 && raw[4] === "-") {
    // YYYY-MM-DD (ISO / <input type="date">)
    const parts = raw.split("-");
    if (parts.length !== 3) return "";
    [yyyy, mm, dd] = parts;
  } else if (raw.includes("-")) {
    // MM-DD-YYYY
    const parts = raw.split("-");
    if (parts.length !== 3) return "";
    [mm, dd, yyyy] = parts;
  } else {
    return "";
  }

  const day = Number(dd);
  const month = Number(mm);
  const year = Number(yyyy);
  if (Number.isNaN(day) || Number.isNaN(month) || Number.isNaN(year)) return "";
  if (day < 1 || day > 31 || month < 1 || month > 12) return "";

  const yy = String(year).slice(-2);
  return String(day).padStart(2, "0") + String(month).padStart(2, "0") + yy;
}

function clearAddCutMessage() {
  const msgEl = document.getElementById("add-cut-message");
  if (!msgEl) return;
  msgEl.textContent = "";
  msgEl.classList.remove("error", "success");
}

function showAddCutMessage(kind, text) {
  const msgEl = document.getElementById("add-cut-message");
  if (!msgEl) return;
  msgEl.textContent = text;
  msgEl.classList.remove("error", "success");
  if (kind === "error") msgEl.classList.add("error");
  if (kind === "success") msgEl.classList.add("success");
}

async function addCutFromForm() {
  const dateInput = document.getElementById("add-cut-date");
  const latInput = document.getElementById("add-cut-lat");
  const lonInput = document.getElementById("add-cut-lon");
  const utcInput = document.getElementById("add-cut-utc");
  const hdopInput = document.getElementById("add-cut-hdop");

  clearAddCutMessage();

  if (!dateInput || !latInput || !lonInput || !utcInput || !hdopInput) {
    return;
  }

  const dateRaw = dateInput.value.trim();
  const lat = Number.parseFloat(latInput.value);
  const lon = Number.parseFloat(lonInput.value);
  const utcRaw = utcInput.value.trim();
  const hdopRaw = hdopInput.value.trim();
  const hdopVal = hdopRaw === "" ? Number.NaN : Number.parseFloat(hdopRaw);

  if (Number.isNaN(lat) || Number.isNaN(lon)) {
    showAddCutMessage("error", "Latitude and longitude are required and must be numbers.");
    return;
  }

  if (lat < MAP_BOUNDS.minLat || lat > MAP_BOUNDS.maxLat ||
      lon < MAP_BOUNDS.minLon || lon > MAP_BOUNDS.maxLon) {
    showAddCutMessage("error", "Lat/Lon must be inside the Florida map bounds.");
    return;
  }

  // Convert user date (MM/DD/YYYY or similar) to GPS format (DDMMYY)
  const dateConverted = userDateToDDMMYY(dateRaw);
  if (dateRaw && !dateConverted) {
    showAddCutMessage("error", "Date format not recognized. Use MM/DD/YYYY.");
    return;
  }

  const body = {
    lat,
    lng: lon,
    timestamp: utcRaw || "0",
  };

  if (!Number.isNaN(hdopVal)) {
    body.hdop = hdopVal;
  }

  if (dateConverted) {
    body.date = dateConverted;
  }

  try {
    const res = await fetch("/api/cuts", {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify(body),
    });

    if (!res.ok) {
      throw new Error(`POST /api/cuts failed: ${res.status}`);
    }

    // Try to read the newly created ID so we can reflect the row immediately.
    let newId = null;
    try {
      const json = await res.json();
      if (json && typeof json.id === "number") {
        newId = json.id;
      }
    } catch {
      // If parsing fails, we'll still add a row without an ID.
    }

    dateInput.value = "";
    latInput.value = "";
    lonInput.value = "";
    utcInput.value = "";
    hdopInput.value = "";

    // Instead of re-fetching from the server (which doesn't yet persist the date),
    // append a client-side cut so the date shows up immediately in the list.
    const newCut = {
      id: newId,
      utc_date: dateConverted || "",
      utc_time: utcRaw || "0",
      latitude: lat,
      longitude: normalizeLonForBounds(lon),
      altitude: 73.0,
      fix_quality: 0,
      geoid_height: 0.0,
      hdop: Number.isNaN(hdopVal) ? 0.0 : hdopVal,
      num_satellites: 0,
    };

    cutsData = [...cutsData, newCut];

    renderTable();
    updateMap(getFilteredCuts());

    showAddCutMessage("success", "Cut added.");
  } catch (err) {
    console.error(err);
    showAddCutMessage("error", "Failed to add cut. See console for details.");
  }
}

function initLeafletMapIfNeeded() {
  if (mapInitialized) return;

  const mapDiv = document.getElementById("leaflet-map");
  if (!mapDiv) return;

  const bounds = L.latLngBounds(
    [MAP_BOUNDS.minLat, MAP_BOUNDS.minLon],
    [MAP_BOUNDS.maxLat, MAP_BOUNDS.maxLon]
  );

  map = L.map("leaflet-map", { zoomControl: true });

  if (USE_ONLINE_TILES) {
    L.tileLayer("https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png", {
      maxZoom: 19,
      attribution: "&copy; OpenStreetMap contributors"
    }).addTo(map);
  } else {
    L.tileLayer(LOCAL_TILE_URL, {
      minZoom: LOCAL_MIN_ZOOM,
      maxZoom: LOCAL_MAX_ZOOM,
      attribution: "Offline tiles"
    }).addTo(map);
  }

  cutLayer = L.layerGroup().addTo(map);

  map.fitBounds(bounds);
  map.setMaxBounds(bounds.pad(0.25));

  mapInitialized = true;
}

function clampLatLon(lat, lon) {
  const clampedLat = Math.min(Math.max(lat, MAP_BOUNDS.minLat), MAP_BOUNDS.maxLat);
  const clampedLon = Math.min(Math.max(lon, MAP_BOUNDS.minLon), MAP_BOUNDS.maxLon);
  return [clampedLat, clampedLon];
}

function hdopToRadiusAndOpacity(hdopRaw) {
  if (typeof hdopRaw !== "number" || Number.isNaN(hdopRaw)) {
    return {
      radius: DEFAULT_MARKER_RADIUS,
      fillOpacity: DEFAULT_FILL_OPACITY
    };
  }

  // Clamp HDOP to a sane range so outliers don't dominate.
  const hdop = Math.min(HDOP_MAX, Math.max(HDOP_MIN, hdopRaw));

  // Map HDOP linearly to radius: lower HDOP -> smaller radius, higher HDOP -> larger.
  const t = (hdop - HDOP_MIN) / (HDOP_MAX - HDOP_MIN); // 0..1
  const radius = HDOP_MIN_RADIUS + t * (HDOP_MAX_RADIUS - HDOP_MIN_RADIUS);

  // Slightly reduce fill opacity for worse HDOP so bad points look softer.
  const opacityMin = 0.4;
  const opacityMax = DEFAULT_FILL_OPACITY;
  const fillOpacity = opacityMax - t * (opacityMax - opacityMin);

  return { radius, fillOpacity };
}

function updateMap(cuts) {
  initLeafletMapIfNeeded();
  if (!map || !cutLayer) return;

  cutLayer.clearLayers();

  const totalCutsValue = document.getElementById("total-cuts-value");
  totalCutsValue.textContent = cuts.length.toString();

  if (!cuts.length) return;

  for (const cut of cuts) {
    const [lat, lon] = clampLatLon(cut.latitude, cut.longitude);
    const etTime = utcTimeToET(cut.utc_time);
    const isoDate = utcDateToISO(cut.utc_date);
    const dateDisplay = isoDate ? isoDate.slice(5, 7) + "/" + isoDate.slice(8, 10) + "/" + isoDate.slice(0, 4) : "";

    const popupHtml =
      `<b>Cut ID: ${cut.id ?? ""}</b><br>` +
      (dateDisplay ? `Date: ${dateDisplay}<br>` : "") +
      `ET Time: ${etTime || ""}<br>` +
      `Lat: ${lat.toFixed(6)}<br>` +
      `Lon: ${lon.toFixed(6)}<br>` +
      `Alt: ${cut.altitude ?? ""}<br>` +
      `Fix: ${cut.fix_quality ?? ""}<br>` +
      `HDOP: ${cut.hdop ?? ""}<br>` +
      `Sats: ${cut.num_satellites ?? ""}`;

    if (mapMode === "hdop" &&
        typeof cut.hdop === "number" &&
        !Number.isNaN(cut.hdop) &&
        cut.hdop > 0) {
      // In HDOP mode, treat hdop as an exact radius in meters.
      L.circle([lat, lon], {
        radius: cut.hdop,
        weight: 1,
        fillOpacity: 0.3
      })
        .bindPopup(popupHtml)
        .addTo(cutLayer);
    } else {
      // Normal mode or missing/invalid HDOP: fall back to pixel-based marker.
      L.circleMarker([lat, lon], {
        radius: DEFAULT_MARKER_RADIUS,
        weight: 2,
        fillOpacity: DEFAULT_FILL_OPACITY
      })
        .bindPopup(popupHtml)
        .addTo(cutLayer);
    }
  }
}

function renderTable() {
  const tbody = document.getElementById("cut-log-body");
  const totalCutsValue = document.getElementById("total-cuts-value");

  tbody.innerHTML = "";

  const cuts = getFilteredCuts();
  totalCutsValue.textContent = cuts.length.toString();

  for (const cut of cuts) {
    const tr = document.createElement("tr");

    const lat = (typeof cut.latitude === "number" && !Number.isNaN(cut.latitude))
      ? cut.latitude.toFixed(6)
      : "";

    const lon = (typeof cut.longitude === "number" && !Number.isNaN(cut.longitude))
      ? cut.longitude.toFixed(6)
      : "";

    const altitude = (typeof cut.altitude === "number" && !Number.isNaN(cut.altitude))
      ? cut.altitude.toFixed(2)
      : (cut.altitude ?? "");

    const geoid = (typeof cut.geoid_height === "number" && !Number.isNaN(cut.geoid_height))
      ? cut.geoid_height.toFixed(2)
      : (cut.geoid_height ?? "");

    const hdop = (typeof cut.hdop === "number" && !Number.isNaN(cut.hdop))
      ? cut.hdop.toFixed(2)
      : (cut.hdop ?? "");

    const etTime = utcTimeToET(cut.utc_time);
    const isoDate = utcDateToISO(cut.utc_date);
    const dateDisplay = isoDate ? isoDate.slice(5, 7) + "/" + isoDate.slice(8, 10) + "/" + isoDate.slice(0, 4) : "";

    tr.innerHTML = `
      <td><input type="checkbox" class="cut-select-checkbox" value="${cut.id ?? ""}"></td>
      <td>${cut.id ?? ""}</td>
      <td>${dateDisplay}</td>
      <td>${etTime || (cut.utc_time ?? "")}</td>
      <td>${lat}</td>
      <td>${lon}</td>
      <td>${altitude}</td>
      <td>${cut.fix_quality ?? ""}</td>
      <td>${geoid}</td>
      <td>${hdop}</td>
      <td>${cut.num_satellites ?? ""}</td>
    `;
    tbody.appendChild(tr);
  }
}

async function deleteSelectedCuts() {
  const checkboxes = document.querySelectorAll(".cut-select-checkbox:checked");
  const ids = Array.from(checkboxes)
    .map(cb => Number.parseInt(cb.value, 10))
    .filter(id => !Number.isNaN(id));

  if (!ids.length) {
    return;
  }

  try {
    const res = await fetch("/api/points/delete", {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify({ ids }),
    });

    if (!res.ok) {
      throw new Error(`POST /api/points/delete failed: ${res.status}`);
    }

    await fetchCuts();
    renderTable();
    updateMap(getFilteredCuts());
  } catch (err) {
    console.error(err);
  }
}

// ── Polling loop ──────────────────────────────────────────────────

let pollTimer = null;
const POLL_INTERVAL = 5000;   // 5 seconds
let _lastCutsJSON = "";       // track whether data actually changed

async function pollLoop() {
  try {
    await fetchCuts();

    // Only re-render if the data actually changed — avoids
    // stomping on filter inputs mid-edit and applying filters
    // before the user clicks the Filter button.
    const newJSON = JSON.stringify(cutsData);
    if (newJSON !== _lastCutsJSON) {
      _lastCutsJSON = newJSON;
      renderTable();

      if (document.querySelector(".tab-content.active")?.id === "map-tab") {
        updateMap(getFilteredCuts());
      }
    }
  } catch (err) {
    console.error("Polling failed:", err);
  }

  // Also poll status
  await fetchStatus();

  // Schedule next poll
  pollTimer = setTimeout(pollLoop, POLL_INTERVAL);
}

// ── Init ──────────────────────────────────────────────────────────

async function initDashboard() {
  // Set initial status to unknown — fetchStatus() will update it
  const statusDot = document.getElementById("connection-status-dot");
  const statusText = document.getElementById("connection-status-text");
  if (statusDot) statusDot.className = "disconnected";
  if (statusText) statusText.textContent = "Connecting...";

  try {
    await fetchCuts();
    _lastCutsJSON = JSON.stringify(cutsData);
  } catch (err) {
    console.error(err);
    cutsData = [];
  }

  // Fetch status right away
  await fetchStatus();

  const tabButtons = document.querySelectorAll(".tab-button");
  const tabContents = document.querySelectorAll(".tab-content");

  tabButtons.forEach(btn => {
    btn.addEventListener("click", () => {
      tabButtons.forEach(b => b.classList.remove("active"));
      btn.classList.add("active");

      const targetId = btn.getAttribute("data-tab");
      tabContents.forEach(section => {
        section.classList.toggle("active", section.id === targetId);
      });

      if (targetId === "log-tab") {
        renderTable();
      } else if (targetId === "map-tab") {
        updateMap(getFilteredCuts());

        // Leaflet needs a kick when the map tab is shown
        setTimeout(() => {
          if (map) map.invalidateSize();
        }, 50);
      }
    });
  });

  // Filter button — applies date range filter to table and map
  const filterButton = document.getElementById("filter-button");
  if (filterButton) {
    filterButton.addEventListener("click", () => {
      renderTable();
      if (document.querySelector(".tab-content.active")?.id === "map-tab") {
        updateMap(getFilteredCuts());
      }
    });
  }

  const addCutButton = document.getElementById("add-cut-button");
  if (addCutButton) {
    addCutButton.addEventListener("click", (evt) => {
      evt.preventDefault();
      addCutFromForm();
    });
  }

  const deleteSelectedButton = document.getElementById("delete-selected-button");
  if (deleteSelectedButton) {
    deleteSelectedButton.addEventListener("click", (evt) => {
      evt.preventDefault();
      deleteSelectedCuts();
    });
  }

  const mapModeNormalBtn = document.getElementById("map-mode-normal");
  const mapModeHdopBtn = document.getElementById("map-mode-hdop");
  const mapModeButtons = [mapModeNormalBtn, mapModeHdopBtn].filter(Boolean);

  function setMapMode(newMode) {
    mapMode = newMode;
    mapModeButtons.forEach(btn => {
      if (!btn) return;
      const isActive =
        (newMode === "normal" && btn.id === "map-mode-normal") ||
        (newMode === "hdop" && btn.id === "map-mode-hdop");
      btn.classList.toggle("map-mode-active", isActive);
    });

    if (document.querySelector(".tab-content.active")?.id === "map-tab") {
      updateMap(getFilteredCuts());
    }
  }

  if (mapModeNormalBtn) {
    mapModeNormalBtn.addEventListener("click", (evt) => {
      evt.preventDefault();
      setMapMode("normal");
    });
  }

  if (mapModeHdopBtn) {
    mapModeHdopBtn.addEventListener("click", (evt) => {
      evt.preventDefault();
      setMapMode("hdop");
    });
  }

  renderTable();

  // Start the polling loop for live data + status updates
  pollTimer = setTimeout(pollLoop, POLL_INTERVAL);
}

document.addEventListener("DOMContentLoaded", initDashboard);
