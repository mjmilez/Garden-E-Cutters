const MAP_BOUNDS = {
  minLat: 29.637644159324203,    // bottom edge (smallest latitude)
  maxLat: 29.652020231030388,    // top edge   (largest latitude)
  minLon: -82.37238690307114,    // left edge  (most negative longitude)
  maxLon: -82.33946281502358     // right edge (least negative longitude)
};
// ----- Dummy data -----
const dummyCuts = [
  {
    date: "2025-11-13",
    time: "10:32 AM",
    lat: 29.650500,          // inside [minLat, maxLat]
    lon: -82.341500,         // inside [minLon, maxLon]
    forceN: 212.4
  },
  {
    date: "2025-11-13",
    time: "10:29 AM",
    lat: 29.648000,
    lon: -82.366000,
    forceN: 219.0
  },
  {
    date: "2025-11-12",
    time: "09:45 AM",
    lat: 29.642000,
    lon: -82.370000,
    forceN: 205.3
  },
  {
    date: "2025-11-11",
    time: "03:15 PM",
    lat: 29.639500,
    lon: -82.347000,
    forceN: 198.7
  }
];

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
      const d = cut.date; // already "YYYY-MM-DD"
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

  // Update total cuts display to match filtered rows
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

  const startValue = startInput.value; // "YYYY-MM-DD" or ""
  const endValue = endInput.value;

  // If both dates are set and start is after end, clamp end to start
  if (startValue && endValue && startValue > endValue) {
    endInput.value = startValue;
  }
}

function updateMap(cuts) {
  const overlay = document.getElementById("map-overlay");
  const container = document.getElementById("map-container");
  const totalCutsValue = document.getElementById("total-cuts-value");

  if (!overlay || !container) return;

  overlay.innerHTML = "";
  totalCutsValue.textContent = cuts.length.toString();
  if (!cuts.length) return;

  const { minLat, maxLat, minLon, maxLon } = MAP_BOUNDS;

  const latSpan = maxLat - minLat || 0.0001;
  const lonSpan = maxLon - minLon || 0.0001;

  cuts.forEach(cut => {
    const lat = Math.min(Math.max(cut.lat, minLat), maxLat);
    const lon = Math.min(Math.max(cut.lon, minLon), maxLon);

    const xRel = (lon - minLon) / lonSpan;        // 0–1 left→right
    const yRel = 1 - (lat - minLat) / latSpan;    // 0–1 top→bottom

    const marker = document.createElement("div");
    marker.className = "map-marker";
    marker.style.left = `${xRel * 100}%`;
    marker.style.top = `${yRel * 100}%`;
    marker.title = `${cut.date} ${cut.time} | ${cut.forceN.toFixed(1)} N`;
    overlay.appendChild(marker);
  });
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
  const totalCutsValue = document.getElementById("total-cuts-value");

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
        // Restore and render the table with current filters
        renderTable();
      } else if (targetId === "map-tab") {
        // Clear the table and draw the filtered cuts on the map
        const tbody = document.getElementById("cut-log-body");
        if (tbody) tbody.innerHTML = "";

        const filteredCuts = getFilteredCuts();
        updateMap(filteredCuts);
      }
    });
  });



  // Filters
  const forceSlider = document.getElementById("force-threshold-slider");
  const forceValue = document.getElementById("force-threshold-value");
  const filterButton = document.getElementById("filter-button");
  const startInput = document.getElementById("timestamp-start");
  const endInput = document.getElementById("timestamp-end");

  // Default date range from data
  initFiltersDefaults();

  // Slider label + live re-render
  forceSlider.addEventListener("input", () => {
    forceValue.textContent = `${forceSlider.value} N`;
  });

  forceSlider.addEventListener("change", renderTable);

  startInput.addEventListener("change", () => {
    normalizeDateRange();
    renderTable();
  });

  endInput.addEventListener("change", () => {
    normalizeDateRange();
    renderTable();
  });

  filterButton.addEventListener("click", renderTable);

  // Initial table render
  renderTable();
}

document.addEventListener("DOMContentLoaded", initDashboard);