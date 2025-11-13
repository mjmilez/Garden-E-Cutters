// ----- Dummy data -----
const dummyCuts = [
  {
    time: "10:32 AM",
    lat: 35.9981,
    lon: -85.7500,
    forceN: 212.4,
    fixType: "RTK Fixed",
    horizAcc: 0.01
  },
  {
    time: "10:31 AM",
    lat: 35.9807,
    lon: -85.7495,
    forceN: 219.0,
    fixType: "RTK Float",
    horizAcc: 0.12
  },
  // add more rows as needed
];

// ----- Initial UI setup -----
function initDashboard() {
  // Status bar - pretend we connected successfully
  const statusDot = document.getElementById("connection-status-dot");
  const statusText = document.getElementById("connection-status-text");
  const totalCutsValue = document.getElementById("total-cuts-value");

  statusDot.classList.add("connected");
  statusText.textContent = "Connected to Shears";
  totalCutsValue.textContent = dummyCuts.length.toString();

  // Populate log table
  const tbody = document.getElementById("cut-log-body");
  dummyCuts.forEach(cut => {
    const tr = document.createElement("tr");

    tr.innerHTML = `
      <td>${cut.time}</td>
      <td>${cut.lat.toFixed(6)}</td>
      <td>${cut.lon.toFixed(6)}</td>
      <td>${cut.forceN.toFixed(1)}</td>
      <td>${cut.fixType}</td>
      <td>${cut.horizAcc.toFixed(3)}</td>
    `;

    tbody.appendChild(tr);
  });

  // Tab switching
  const tabButtons = document.querySelectorAll(".tab-button");
  const tabContents = document.querySelectorAll(".tab-content");

  tabButtons.forEach(btn => {
    btn.addEventListener("click", () => {
      // update active tab button
      tabButtons.forEach(b => b.classList.remove("active"));
      btn.classList.add("active");

      // show corresponding content
      const targetId = btn.getAttribute("data-tab");
      tabContents.forEach(section => {
        section.classList.toggle("active", section.id === targetId);
      });
    });
  });

  // Force slider display
  const forceSlider = document.getElementById("force-threshold-slider");
  const forceValue = document.getElementById("force-threshold-value");
  forceSlider.addEventListener("input", () => {
    forceValue.textContent = `${forceSlider.value} N`;
  });
}

document.addEventListener("DOMContentLoaded", initDashboard);
