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

function toSortKey(cut) {
  const raw = (cut.utc_time == null) ? "" : String(cut.utc_time);
  return raw.split(".")[0].padStart(6, "0");
}

function getCutsSorted() {
  return [...cutsData].sort((a, b) => toSortKey(b).localeCompare(toSortKey(a)));
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

    const popupHtml =
      `<b>Cut ID: ${cut.id ?? ""}</b><br>` +
      `ET Time: ${etTime || ""}<br>` +
      `Lat: ${lat.toFixed(6)}<br>` +
      `Lon: ${lon.toFixed(6)}<br>` +
      `Alt: ${cut.altitude ?? ""}<br>` +
      `Fix: ${cut.fix_quality ?? ""}<br>` +
      `HDOP: ${cut.hdop ?? ""}<br>` +
      `Sats: ${cut.num_satellites ?? ""}`;

    L.circleMarker([lat, lon], {
      radius: 6,
      weight: 2,
      fillOpacity: 0.8
    })
      .bindPopup(popupHtml)
      .addTo(cutLayer);
  }
}

function renderTable() {
  const tbody = document.getElementById("cut-log-body");
  const totalCutsValue = document.getElementById("total-cuts-value");

  tbody.innerHTML = "";

  const cuts = getCutsSorted();
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

    tr.innerHTML = `
      <td>${cut.id ?? ""}</td>
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

async function initDashboard() {
  const statusDot = document.getElementById("connection-status-dot");
  const statusText = document.getElementById("connection-status-text");

  statusDot.classList.add("connected");
  statusText.textContent = "Connected to Shears";

  try {
    await fetchCuts();
  } catch (err) {
    console.error(err);
    cutsData = [];
  }

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

  renderTable();
}

document.addEventListener("DOMContentLoaded", initDashboard);