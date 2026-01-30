/**
 * Garden E-Cutters Dashboard (Leaflet version)
 *
 * Browser testing (internet):
 *   USE_ONLINE_TILES = true  -> OSM tiles
 *
 * ESP32 offline:
 *   USE_ONLINE_TILES = false
 *   OFFLINE_IMAGE.enabled = true
 *   Put /field.jpg in SPIFFS and serve it
 */

const USE_ONLINE_TILES = true; // <--- set false for ESP32 offline

const MAP_BOUNDS = {
  minLat: 29.637644159324203,    // bottom edge (smallest latitude)
  maxLat: 29.652020231030388,    // top edge   (largest latitude)
  minLon: -82.37238690307114,    // left edge  (most negative longitude)
  maxLon: -82.33946281502358     // right edge (least negative longitude)
};

// Offline image overlay settings (ESP32 offline mode)
// If you have a field image, set enabled=true and ensure it's served at OFFLINE_IMAGE.url
const OFFLINE_IMAGE = {
  enabled: false,     // set true when you actually have /field.jpg available
  url: "/field.jpg"
};

// ----- Dummy data (replace later with fetch("/api/cuts")) -----
const dummyCuts = [
  { date: "2025-11-13", time: "10:32 AM", lat: 29.650500, lon: -82.341500, forceN: 212.4 },
  { date: "2025-11-13", time: "10:29 AM", lat: 29.648000, lon: -82.366000, forceN: 219.0 },
  { date: "2025-11-12", time: "09:45 AM", lat: 29.642000, lon: -82.370000, forceN: 205.3 },
  { date: "2025-11-11", time: "03:15 PM", lat: 29.639500, lon: -82.347000, forceN: 198.7 }
];

// ---------------- Leaflet state ----------------
let map = null;
let cutLayer = null;
let mapInitialized = false;

function getLatLngBoundsFromConfig() {
  // Leaflet uses [southWest, northEast]
  return L.latLngBounds(
    [MAP_BOUNDS.minLat, MAP_BOUNDS.minLon],
    [MAP_BOUNDS.maxLat, MAP_BOUNDS.maxLon]
  );
}

function initLeafletMapIfNeeded() {
  if (mapInitialized) return;

  const mapDiv = document.getElementById("leaflet-map");
  if (!mapDiv) return;

  const bounds = getLatLngBoundsFromConfig();

  map = L.map("leaflet-map", {
    zoomControl: true
  });

  if (USE_ONLINE_TILES) {
    // Browser test tiles (won't work offline)
    L.tileLayer("https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png", {
      maxZoom: 20,
      attribution: "&copy; OpenStreetMap contributors"
    }).addTo(map);
  } else {
    // Offline mode: best path on ESP32 is an image overlay
    if (OFFLINE_IMAGE.enabled) {
      L.imageOverlay(OFFLINE_IMAGE.url, bounds).addTo(map);
    } else {
      // If you haven't added an image yet, at least show a rectangle boundary
      L.rectangle(bounds, { weight: 2 }).addTo(map);
    }
  }

  // Layer group for markers
  cutLayer = L.layerGroup().addTo(map);

  // Fit view to your bounds
  map.fitBounds(bounds);

  // Optional: constrain panning near the area
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
  const forceSlider = document.getElementById("force-threshold-slider");
  const startInput = document.getElementById("timestamp-start");
  const endInput = document.getElementById("timestamp-end");

  const minForce = Number(forceSlider.value);
  const startValue = startInput.value; // "YYYY-MM-DD" or ""
  const endValue = endInput.value;

  return dummyCuts
    .filter(cut => cut.forceN >= minForce)
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
      <td>${cut.forceN.toFixed(1)}</td>
    `;
    tbody.appendChild(tr);
  });
}

function normalizeDateRange() {
  const startInput = document.getElementById("timestamp-start");
  const endInput = document.getElementById("timestamp-end");

  const startValue = startInput.value;
  const endValue = endInput.value;

  // If both dates are set and start is after end, clamp end to start
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
      `Lon: ${lon.toFixed(6)}<br>` +
      `Force: ${cut.forceN.toFixed(1)} N`;

    // Circle markers scale better than default pin icons when you have many points
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

        // Leaflet needs this when shown after being hidden
        setTimeout(() => {
          if (map) map.invalidateSize();
        }, 50);
      }
    });
  });

  // Filters
  const forceSlider = document.getElementById("force-threshold-slider");
  const forceValue = document.getElementById("force-threshold-value");
  const filterButton = document.getElementById("filter-button");
  const startInput = document.getElementById("timestamp-start");
  const endInput = document.getElementById("timestamp-end");

  initFiltersDefaults();

  forceSlider.addEventListener("input", () => {
    forceValue.textContent = `${forceSlider.value} N`;
  });

  function applyFilters() {
    const activeTab = document.querySelector(".tab-content.active")?.id;
    if (activeTab === "map-tab") {
      updateMap(getFilteredCuts());
    } else {
      renderTable();
    }
  }

  forceSlider.addEventListener("change", applyFilters);

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
