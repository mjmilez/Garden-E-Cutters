// =============================
// MAP BOUNDS
// =============================
const MAP_BOUNDS = {
  minLat: 29.637644159324203,
  maxLat: 29.652020231030388,
  minLon: -82.37238690307114,
  maxLon: -82.33946281502358
};

// =============================
// GLOBAL DATA STORE
// =============================
let cutsData = [];

// =============================
// FETCH DATA FROM FLASK
// =============================
async function fetchCuts() {
  try {
    const res = await fetch("/api/cuts", {
      headers: { "Accept": "application/json" }
    });

    if (!res.ok) throw new Error(`API error: ${res.status}`);

    const raw = await res.json();

    cutsData = (Array.isArray(raw) ? raw : []).map(row => {
      // Parse UTC time like "192928.00"
      const utcRaw = row.utc_time || "";
      const clean = utcRaw.split(".")[0].padStart(6, "0");

      const hours = parseInt(clean.slice(0, 2), 10);
      const minutes = parseInt(clean.slice(2, 4), 10);
      const seconds = parseInt(clean.slice(4, 6), 10);

      const now = new Date();

      // Create UTC date object
      const utcDate = new Date(Date.UTC(
        now.getUTCFullYear(),
        now.getUTCMonth(),
        now.getUTCDate(),
        hours,
        minutes,
        seconds
      ));

      // Convert to Eastern time automatically (DST aware)
      const localTime = utcDate.toLocaleString("en-US", {
        timeZone: "America/New_York",
        hour: "2-digit",
        minute: "2-digit",
        hour12: true
      });

      const localDate = utcDate.toLocaleDateString("en-CA", {
        timeZone: "America/New_York"
      });

      return {
        id: row.id,
        date: localDate, // YYYY-MM-DD
        time: localTime,
        lat: Number(row.latitude),
        lon: -Math.abs(Number(row.longitude)), // force west negative
        altitude: Number(row.altitude),
        satellites: Number(row.num_satellites),
        fixQuality: row.fix_quality,
        hdop: Number(row.hdop)
      };
    });

  } catch (err) {
    console.error("Failed to fetch cuts:", err);
    cutsData = [];
  }
}

// =============================
// SORTING
// =============================
function toSortKey(cut) {
  const [timePart, ampm] = cut.time.split(" ");
  let [hour, minute] = timePart.split(":").map(Number);

  if (ampm === "PM" && hour !== 12) hour += 12;
  if (ampm === "AM" && hour === 12) hour = 0;

  return `${cut.date} ${String(hour).padStart(2, "0")}:${String(minute).padStart(2, "0")}`;
}

// =============================
// FILTERING (DATE ONLY)
// =============================
function getFilteredCuts() {
  const startInput = document.getElementById("timestamp-start");
  const endInput = document.getElementById("timestamp-end");

  const startValue = startInput.value;
  const endValue = endInput.value;

  return cutsData
    .filter(cut => {
      if (!startValue && !endValue) return true;
      const d = cut.date;
      if (startValue && d < startValue) return false;
      if (endValue && d > endValue) return false;
      return true;
    })
    .sort((a, b) => toSortKey(b).localeCompare(toSortKey(a)));
}

// =============================
// TABLE RENDERING
// =============================
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
      <td>${cut.altitude.toFixed(1)} m</td>
      <td>${cut.satellites}</td>
      <td>${cut.fixQuality}</td>
      <td>${cut.hdop.toFixed(1)}</td>
    `;
    tbody.appendChild(tr);
  });
}

// =============================
// MAP UPDATE
// =============================
function updateMap(cuts) {
  const overlay = document.getElementById("map-overlay");
  const container = document.getElementById("map-container");
  const totalCutsValue = document.getElementById("total-cuts-value");
  const tooltip = document.getElementById("map-tooltip");

  overlay.innerHTML = "";
  totalCutsValue.textContent = cuts.length.toString();
  tooltip.style.opacity = 0;

  if (!cuts.length) return;

  const { minLat, maxLat, minLon, maxLon } = MAP_BOUNDS;
  const latSpan = maxLat - minLat || 0.0001;
  const lonSpan = maxLon - minLon || 0.0001;

  cuts.forEach(cut => {
    const lat = Math.min(Math.max(cut.lat, minLat), maxLat);
    const lon = Math.min(Math.max(cut.lon, minLon), maxLon);

    const xRel = (lon - minLon) / lonSpan;
    const yRel = 1 - (lat - minLat) / latSpan;

    const marker = document.createElement("div");
    marker.className = "map-marker";
    marker.style.left = `${xRel * 100}%`;
    marker.style.top = `${yRel * 100}%`;

    const tooltipText =
      `${cut.date} ${cut.time}\n` +
      `Lat: ${cut.lat.toFixed(6)}\n` +
      `Lon: ${cut.lon.toFixed(6)}\n` +
      `Alt: ${cut.altitude.toFixed(1)} m\n` +
      `Satellites: ${cut.satellites}`;

    marker.addEventListener("mouseenter", () => {
      tooltip.textContent = tooltipText;
      tooltip.style.opacity = 1;
      tooltip.style.display = "block";

      const containerRect = container.getBoundingClientRect();
      const markerRect = marker.getBoundingClientRect();
      const tooltipRect = tooltip.getBoundingClientRect();

      const markerCenterX =
        markerRect.left + markerRect.width / 2 - containerRect.left;
      const markerTopY = markerRect.top - containerRect.top;

      const padding = 4;
      let left = markerCenterX - tooltipRect.width / 2;
      const minLeft = padding;
      const maxLeft = containerRect.width - tooltipRect.width - padding;
      left = Math.max(minLeft, Math.min(maxLeft, left));

      tooltip.style.left = `${left}px`;
      tooltip.style.top = `${markerTopY}px`;
    });

    marker.addEventListener("mouseleave", () => {
      tooltip.style.opacity = 0;
    });

    overlay.appendChild(marker);
  });
}

// =============================
// DATE DEFAULTS
// =============================
function initFiltersDefaults() {
  const startInput = document.getElementById("timestamp-start");
  const endInput = document.getElementById("timestamp-end");

  const sortedDates = [...new Set(cutsData.map(c => c.date))].sort();
  if (sortedDates.length) {
    startInput.value = sortedDates[0];
    endInput.value = sortedDates[sortedDates.length - 1];
  }
}

// =============================
// INIT DASHBOARD
// =============================
async function initDashboard() {
  const statusDot = document.getElementById("connection-status-dot");
  const statusText = document.getElementById("connection-status-text");

  statusDot.classList.add("connected");
  statusText.textContent = "Connected to Shears";

  await fetchCuts();
  initFiltersDefaults();
  renderTable();

  // Tab switching
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

      if (targetId === "map-tab") {
        updateMap(getFilteredCuts());
      } else {
        renderTable();
      }
    });
  });

  // Date filtering
  document.getElementById("timestamp-start")
    .addEventListener("change", renderTable);

  document.getElementById("timestamp-end")
    .addEventListener("change", renderTable);
}

document.addEventListener("DOMContentLoaded", initDashboard);