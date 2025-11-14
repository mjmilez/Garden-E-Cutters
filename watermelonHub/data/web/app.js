const MAP_BOUNDS = {
  minLat: 29.637644159324203,    // bottom edge (smallest latitude)
  maxLat: 29.652020231030388,    // top edge   (largest latitude)
  minLon: -82.37238690307114,    // left edge  (most negative longitude)
  maxLon: -82.33946281502358     // right edge (least negative longitude)
};
// ----- Dummy data -----
let dummyCuts = [];

async function fetchCutsFromESP() {
  try {
    const response = await fetch('/api/cuts');
    const data = await response.json();
    
    // Convert your ESP32 data format to match their expected format
    dummyCuts = data.map(cut => ({
      date: new Date(cut.timestamp * 1000).toISOString().split('T')[0],
      time: new Date(cut.timestamp * 1000).toLocaleTimeString('en-US', {hour: '2-digit', minute:'2-digit'}),
      lat: cut.lat,
      lon: cut.lon,
      forceN: cut.force * 9.81  // Convert kg to Newtons
    }));
    
    // Update whichever view is active
    const activeTab = document.querySelector('.tab-content.active');
    if (activeTab?.id === 'log-tab') {
      renderTable();
    } else if (activeTab?.id === 'map-tab') {
      updateMap(getFilteredCuts());
    }
    
    // Update total cuts counter
    document.getElementById("total-cuts-value").textContent = dummyCuts.length.toString();
    
  } catch (error) {
    console.error('Failed to fetch cuts:', error);
    // Keep existing dummy data as fallback
  }
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

  // ADD: Fetch real data on load
  fetchCutsFromESP();

  // ADD: Auto-refresh every 5 seconds
  setInterval(() => {
    fetchCutsFromESP();
  }, 5000);

  // Status bar - pretend we connected successfully
  const statusDot = document.getElementById("connection-status-dot");
  const statusText = document.getElementById("connection-status-text");
  const totalCutsValue = document.getElementById("total-cuts-value");

  // Check if we can reach the ESP32
  fetch('/api/cuts')
    .then(() => {
      statusDot.classList.add("connected");
      statusText.textContent = "Connected to Hub";
    })
    .catch(() => {
      statusDot.classList.remove("connected");
      statusText.textContent = "Disconnected from Hub";
    });

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
