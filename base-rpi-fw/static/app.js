/**
 * Garden E-Cutters Dashboard (Leaflet)
 *
 * Browser test mode:
 *   - Uses online OSM tiles (WORKS in normal internet-connected browser)
 *
 * Offline Raspberry Pi mode:
 *   - Later you'll point Leaflet at your local tiles or MBTiles tile server.
 */

const USE_ONLINE_TILES = true;   // dev laptop: true
const LOCAL_TILE_URL = "/tiles/{z}/{x}/{y}.png"; // Pi offline: served by your Pi web server
const LOCAL_MIN_ZOOM = 4;        // good for state-level viewing
const LOCAL_MAX_ZOOM = 12;       // adjust after you download tiles

// Current bounds you were using (we'll switch this to Florida in Step 3)
const MAP_BOUNDS = {
  // Florida bounding box (approx)
  minLat: 24.396308,   // Key West area
  maxLat: 31.000888,   // North FL / GA line
  minLon: -87.634938,  // Pensacola / western panhandle
  maxLon: -80.031362   // Miami / eastern edge
};

// ----- Data store (pulled from /api/cuts) -----
let cutsData = [];  // will be filled by fetchCuts()

// ---------------- Leaflet state ----------------
let map = null;
let cutLayer = null;
let mapInitialized = false;

function getLatLngBoundsFromConfig() {
  return L.latLngBounds(
    [MAP_BOUNDS.minLat, MAP_BOUNDS.minLon], // SW
    [MAP_BOUNDS.maxLat, MAP_BOUNDS.maxLon]  // NE
  );
}

function initLeafletMapIfNeeded() {
  if (mapInitialized) return;

  const mapDiv = document.getElementById("leaflet-map");
  if (!mapDiv) return;

  const bounds = getLatLngBoundsFromConfig();

  map = L.map("leaflet-map", { zoomControl: true });

  if (USE_ONLINE_TILES) {
    // Online tiles for development/testing
    L.tileLayer("https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png", {
      maxZoom: 19,
      attribution: "&copy; OpenStreetMap contributors"
    }).addTo(map);
  } else {
    // Offline tiles served locally by your Raspberry Pi (or local server)
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

// ----- API fetch + normalization (minimal additions) -----

// Fix common “Florida longitude comes in positive” issue.
// Only flip sign when your configured map bounds are in the Western Hemisphere.
function normalizeLonForBounds(lon) {
  if (typeof lon !== "number") return lon;
  const boundsAreWest = MAP_BOUNDS.maxLon < 0 && MAP_BOUNDS.minLon < 0;
  if (boundsAreWest && lon > 0) return -lon;
  return lon;
}

async function fetchCuts() {
  const res = await fetch("/api/cuts", { cache: "no-store" });
  if (!res.ok) throw new Error(`GET /api/cuts failed: ${res.status}`);

  const apiCuts = await res.json();

  // Keep raw fields so the table can show all columns.
  // Also normalize lat/lon to numbers (and lon sign for Florida bounds).
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

// ----- Helpers for sorting/filtering -----

// Sort newest first by utc_time (string HHMMSS.xx). If missing, push to end.
function toSortKey(cut) {
  const raw = (cut.utc_time == null) ? "" : String(cut.utc_time);
  const main = raw.split(".")[0].padStart(6, "0"); // "192928"
  return main; // lexicographically sortable
}

function getFilteredCuts() {
  const startInput = document.getElementById("timestamp-start");
  const endInput = document.getElementById("timestamp-end");

  const startValue = startInput.value; // "YYYY-MM-DD" or ""
  const endValue = endInput.value;

  // No real dates in API yet, so:
  // - If no filter set => return all
  // - If user sets dates => still return all (filters are effectively no-op)
  // Keeping this minimal to avoid breaking UI until API provides a date field.
  const ignoreDateFilter = (!startValue && !endValue);

  return cutsData
    .filter(() => {
      if (ignoreDateFilter) return true;
      return true;
    })
    .sort((a, b) => toSortKey(b).localeCompare(toSortKey(a))); // latest first
}

// ----- Table rendering -----
function renderTable() {
  const tbody = document.getElementById("cut-log-body");
  const totalCutsValue = document.getElementById("total-cuts-value");

  tbody.innerHTML = "";

  const filteredCuts = getFilteredCuts();
  totalCutsValue.textContent = filteredCuts.length.toString();

  filteredCuts.forEach(cut => {
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

    tr.innerHTML = `
      <td>${cut.id ?? ""}</td>
      <td>${cut.utc_time ?? ""}</td>
      <td>${lat}</td>
      <td>${lon}</td>
      <td>${altitude}</td>
      <td>${cut.fix_quality ?? ""}</td>
      <td>${geoid}</td>
      <td>${hdop}</td>
      <td>${cut.num_satellites ?? ""}</td>
    `;
    tbody.appendChild(tr);
  });
}

function normalizeDateRange() {
  const startInput = document.getElementById("timestamp-start");
  const endInput = document.getElementById("timestamp-end");

  const startValue = startInput.value;
  const endValue = endInput.value;

  if (startValue && endValue && startValue > endValue) {
    endInput.value = startValue;
  }
}

// ----- Leaflet map update -----
function clampLatLon(lat, lon) {
  const clampedLat = Math.min(Math.max(lat, MAP_BOUNDS.minLat), MAP_BOUNDS.maxLat);
  const clampedLon = Math.min(Math.max(lon, MAP_BOUNDS.minLon), MAP_BOUNDS.maxLon);
  return [clampedLat, clampedLon];
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

    const popupHtml =
      `<b>Cut ID: ${cut.id ?? ""}</b><br>` +
      `UTC Time: ${cut.utc_time ?? ""}<br>` +
      `Lat: ${lat.toFixed(6)}<br>` +
      `Lon: ${lon.toFixed(6)}<br>` +
      `Alt: ${cut.altitude ?? ""}<br>` +
      `Fix: ${cut.fix_quality ?? ""}<br>` +
      `HDOP: ${cut.hdop ?? ""}<br>` +
      `Sats: ${cut.num_satellites ?? ""}`;

    const marker = L.circleMarker([lat, lon], {
      radius: 6,
      weight: 2,
      fillOpacity: 0.8
    });

    marker.bindPopup(popupHtml);
    marker.addTo(cutLayer);
  }
}

// ----- Initial UI setup -----
function initFiltersDefaults() {
  // No real dates in the API; leave date inputs blank (user can still set them, but they won't filter).
  const startInput = document.getElementById("timestamp-start");
  const endInput = document.getElementById("timestamp-end");
  startInput.value = "";
  endInput.value = "";
}

async function initDashboard() {
  // Status bar - pretend we connected successfully
  const statusDot = document.getElementById("connection-status-dot");
  const statusText = document.getElementById("connection-status-text");

  statusDot.classList.add("connected");
  statusText.textContent = "Connected to Shears";

  // Load cuts from API
  try {
    await fetchCuts();
  } catch (err) {
    console.error(err);
    cutsData = []; // keep UI stable
  }

  // Tabs
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

        // Leaflet needs this when the map becomes visible after being hidden
        setTimeout(() => {
          if (map) map.invalidateSize();
        }, 50);
      }
    });
  });

  // Filters
  const filterButton = document.getElementById("filter-button");
  const startInput = document.getElementById("timestamp-start");
  const endInput = document.getElementById("timestamp-end");

  initFiltersDefaults();

  function applyFilters() {
    const activeTab = document.querySelector(".tab-content.active")?.id;
    if (activeTab === "map-tab") {
      updateMap(getFilteredCuts());
    } else {
      renderTable();
    }
  }

  startInput.addEventListener("change", () => {
    normalizeDateRange();
    applyFilters();
  });

  endInput.addEventListener("change", () => {
    normalizeDateRange();
    applyFilters();
  });

  filterButton.addEventListener("click", applyFilters);

  // Initial table render
  renderTable();
}

document.addEventListener("DOMContentLoaded", initDashboard);