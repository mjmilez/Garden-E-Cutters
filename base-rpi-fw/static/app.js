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

const POLL_INTERVAL_MS = 1000;

// "normal" = current behavior, "hdop" = bubble size/opacity driven by HDOP
let mapMode = "normal";

let cutsData = [];

// Leaflet state
let map = null;
let cutLayer = null;
let mapInitialized = false;

// Polling state
let pollTimer = null;
let pollInProgress = false;
let lastCutsSignature = "";

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
  const main = raw.split(".")[0].padStart(6, "0"); // HHMMSS
  const hh = Number(main.slice(0, 2));
  const mm = Number(main.slice(2, 4));
  const ss = Number(main.slice(4, 6));

  if ([hh, mm, ss].some(n => Number.isNaN(n))) return raw;

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

async function fetchCuts() {
  const res = await fetch("/api/cuts", { cache: "no-store" });
  if (!res.ok) throw new Error(`GET /api/cuts failed: ${res.status}`);

  const apiCuts = await res.json();

  cutsData = (Array.isArray(apiCuts) ? apiCuts : []).map(row => ({
    id: row.id,
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

function buildCutsSignature(cuts) {
  if (!Array.isArray(cuts) || cuts.length === 0) {
    return "empty";
  }

  const sorted = [...cuts].sort((a, b) => {
    const aId = Number.isFinite(a.id) ? a.id : -1;
    const bId = Number.isFinite(b.id) ? b.id : -1;
    return bId - aId;
  });

  const newest = sorted[0];

  return JSON.stringify({
    count: sorted.length,
    newestId: newest.id ?? null,
    newestUtc: newest.utc_time ?? "",
    newestLat: newest.latitude ?? null,
    newestLon: newest.longitude ?? null
  });
}

async function pollForCutUpdates() {
  if (pollInProgress) {
    return;
  }

  pollInProgress = true;

  try {
    const previousSignature = lastCutsSignature;

    await fetchCuts();

    const newSignature = buildCutsSignature(cutsData);

    if (newSignature !== previousSignature) {
      lastCutsSignature = newSignature;

      renderTable();

      if (document.querySelector(".tab-content.active")?.id === "map-tab") {
        updateMap(getCutsSorted());
      }
    }
  } catch (err) {
    console.error("Polling failed:", err);
  } finally {
    pollInProgress = false;
  }
}

function toSortKey(cut) {
  const raw = (cut.utc_time == null) ? "" : String(cut.utc_time);
  return raw.split(".")[0].padStart(6, "0");
}

function getCutsSorted() {
  return [...cutsData].sort((a, b) => toSortKey(b).localeCompare(toSortKey(a)));
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

  const body = {
    lat,
    lng: lon,
    timestamp: utcRaw || "0",
  };

  if (!Number.isNaN(hdopVal)) {
    body.hdop = hdopVal;
  }

  // Frontend-only: include a manual date field for future backend work.
  if (dateRaw) {
    body.date = dateRaw;
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
      utc_time: utcRaw || "0",
      latitude: lat,
      longitude: normalizeLonForBounds(lon),
      altitude: 73.0,
      fix_quality: 0,
      geoid_height: 0.0,
      hdop: Number.isNaN(hdopVal) ? 0.0 : hdopVal,
      num_satellites: 0,
      manual_date: dateRaw || "",
    };

    cutsData = [...cutsData, newCut];
    lastCutsSignature = buildCutsSignature(cutsData);

    renderTable();
    updateMap(getCutsSorted());

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
  if (totalCutsValue) {
    totalCutsValue.textContent = cuts.length.toString();
  }

  if (!cuts.length) return;

  for (const cut of cuts) {
    const [lat, lon] = clampLatLon(cut.latitude, cut.longitude);
    const etTime = utcTimeToET(cut.utc_time);

    const popupHtml =
      `<b>Cut ID: ${cut.id ?? ""}</b><br>` +
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

  if (!tbody) return;

  tbody.innerHTML = "";

  const cuts = getCutsSorted();

  if (totalCutsValue) {
    totalCutsValue.textContent = cuts.length.toString();
  }

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
    const dateDisplay = cut.manual_date ?? "";

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
    lastCutsSignature = buildCutsSignature(cutsData);

    renderTable();
    updateMap(getCutsSorted());
  } catch (err) {
    console.error(err);
    // For now, just log the error. Could add a UI message later.
  }
}

async function initDashboard() {
  const statusDot = document.getElementById("connection-status-dot");
  const statusText = document.getElementById("connection-status-text");

  if (statusDot) {
    statusDot.classList.add("connected");
  }

  if (statusText) {
    statusText.textContent = "Connected to Shears";
  }

  try {
    await fetchCuts();
  } catch (err) {
    console.error(err);
    cutsData = [];
  }

  lastCutsSignature = buildCutsSignature(cutsData);

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
        updateMap(getCutsSorted());

        // Leaflet needs a kick when the map tab is shown
        setTimeout(() => {
          if (map) map.invalidateSize();
        }, 50);
      }
    });
  });

  const filterButton = document.getElementById("filter-button");
  if (filterButton) {
    filterButton.addEventListener("click", () => {
      renderTable();
      if (document.querySelector(".tab-content.active")?.id === "map-tab") {
        updateMap(getCutsSorted());
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
      updateMap(getCutsSorted());
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

  if (pollTimer !== null) {
    clearInterval(pollTimer);
  }

  pollTimer = setInterval(pollForCutUpdates, POLL_INTERVAL_MS);
}

document.addEventListener("DOMContentLoaded", initDashboard);