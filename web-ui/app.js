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

// ----- Dummy data (NO forceN anymore) -----
const dummyCuts = [
  { date: "2025-11-13", time: "10:32 AM", lat: 29.650500, lon: -82.341500 },
  { date: "2025-11-13", time: "10:29 AM", lat: 29.648000, lon: -82.366000 },
  { date: "2025-11-12", time: "09:45 AM", lat: 29.642000, lon: -82.370000 },
  { date: "2025-11-11", time: "03:15 PM", lat: 29.639500, lon: -82.347000 }
];

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

// ----- Helpers for sorting/filtering -----

// Turn date+time into a sortable key "YYYY-MM-DD HH:MM" in 24h format
function toSortKey(cut) {
  const [timePart, ampm] = cut.time.split(" ");
  let [hour, minute] = timePart.split(":").map(Number);

  if (ampm === "PM" && hour !== 12) hour += 12;
  if (ampm === "AM" && hour === 12) hour = 0;

  const hourStr = String(hour).padStart(2, "0");
  const minuteStr = String(minute).padStart(2, "0");
  return `${cut.date} ${hourStr}:${minuteStr}`;
}

function getFilteredCuts() {
  const startInput = document.getElementById("timestamp-start");
  const endInput = document.getElementById("timestamp-end");

  const startValue = startInput.value; // "YYYY-MM-DD" or ""
  const endValue = endInput.value;

  return dummyCuts
    .filter(cut => {
      if (!startValue && !endValue) return true;
      const d = cut.date;
      if (startValue && d < startValue) return false;
      if (endValue && d > endValue) return false;
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
    tr.innerHTML = `
      <td>${cut.date}</td>
      <td>${cut.time}</td>
      <td>${cut.lat.toFixed(6)}</td>
      <td>${cut.lon.toFixed(6)}</td>
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
    const [lat, lon] = clampLatLon(cut.lat, cut.lon);

    const popupHtml =
      `<b>${cut.date} ${cut.time}</b><br>` +
      `Lat: ${lat.toFixed(6)}<br>` +
      `Lon: ${lon.toFixed(6)}`;

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
  const startInput = document.getElementById("timestamp-start");
  const endInput = document.getElementById("timestamp-end");

  const sortedDates = [...new Set(dummyCuts.map(c => c.date))].sort();
  if (sortedDates.length) {
    startInput.value = sortedDates[0];
    endInput.value = sortedDates[sortedDates.length - 1];
  }
}

function initDashboard() {
  // Status bar - pretend we connected successfully
  const statusDot = document.getElementById("connection-status-dot");
  const statusText = document.getElementById("connection-status-text");

  statusDot.classList.add("connected");
  statusText.textContent = "Connected to Shears";

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