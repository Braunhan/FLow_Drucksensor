// Globale Variablen für Chart-Instanzen
let pressureChartInstance = null;
let flowChartInstance = null;
let combinedChartInstance = null;

// Farbdefinitionen für Druck- und Flow-Sensoren
const pressureColors = {
  sensor1: "rgba(255, 99, 132, 1)",    
  sensor2: "rgba(54, 162, 235, 1)",    
  sensor3: "rgba(255, 206, 86, 1)",    
  sensor4: "rgba(75, 192, 192, 1)"
};
const flowColors = {
  sensor1: "rgba(153, 102, 255, 1)",    
  sensor2: "rgba(255, 159, 64, 1)"
};

// Beim Laden der Seite
document.addEventListener('DOMContentLoaded', () => {
  // 1) Hole den Logdatei-Namen aus URL-Parametern, z.B. ?log=/log_YYYYMMDD_HHMMSS.csv
  const urlParams = new URLSearchParams(window.location.search);
  const logFileName = urlParams.get('log');
  if (!logFileName) {
    alert("Keine Logdatei angegeben (?log=...).");
    return;
  }

  // 2) Daten von /api/logdata?name=... laden und Diagramme erstellen
  fetch('/api/logdata?name=' + encodeURIComponent(logFileName))
    .then(response => response.json())
    .then(data => {
      createPressureChart(data);
      createFlowChart(data);
      createCombinedChart(data);
      
      // Wenn du willst, kannst du hier KEIN periodisches updateCharts() machen,
      // weil es eine fixe Logdatei ist (keine Echtzeit).
      // Man könnte aber manuell "updateLogCharts()" erlauben, falls man neu einlesen will.
    })
    .catch(error => console.error("Fehler beim Laden der Logdaten:", error));

  // 3) Download-Buttons
  document.getElementById('downloadPressure').addEventListener('click', () => {
    downloadChart('pressureChart', 'pressure_chart.png');
  });
  document.getElementById('downloadFlow').addEventListener('click', () => {
    downloadChart('flowChart', 'flow_chart.png');
  });
  document.getElementById('downloadCombined').addEventListener('click', () => {
    downloadChart('combinedChart', 'combined_chart.png');
  });
});

// Druckdiagramm erstellen
function createPressureChart(data) {
  const ctx = document.getElementById('pressureChart').getContext('2d');
  const datasets = [];
  for (let i = 1; i <= 4; i++) {
    datasets.push({
      label: `Sensor ${i}`,
      data: data.pressure[`sensor${i}`],
      borderColor: pressureColors[`sensor${i}`],
      backgroundColor: pressureColors[`sensor${i}`],
      fill: false,
      tension: 0.1
    });
  }

  pressureChartInstance = new Chart(ctx, {
    type: 'line',
    data: {
      labels: data.timestamps,
      datasets: datasets,
    },
    options: {
      responsive: true,
      scales: {
        x: {
          type: 'category', // da Zeitstempel in data.timestamps evtl. nur Strings sind
          title: { display: true, text: 'Zeit' }
        },
        y: {
          title: { display: true, text: 'Druck (bar)' }
        }
      },
      plugins: { legend: { display: true } }
    }
  });
}

// Durchflussdiagramm erstellen
function createFlowChart(data) {
  const ctx = document.getElementById('flowChart').getContext('2d');
  const datasets = [];
  for (let i = 1; i <= 2; i++) {
    datasets.push({
      label: `Flow Sensor ${i}`,
      data: data.flow[`sensor${i}`],
      borderColor: flowColors[`sensor${i}`],
      backgroundColor: flowColors[`sensor${i}`],
      fill: false,
      tension: 0.1
    });
  }

  flowChartInstance = new Chart(ctx, {
    type: 'line',
    data: {
      labels: data.timestamps,
      datasets: datasets,
    },
    options: {
      responsive: true,
      scales: {
        x: {
          type: 'category',
          title: { display: true, text: 'Zeit' }
        },
        y: {
          title: { display: true, text: 'Durchfluss (L/min)' }
        }
      },
      plugins: { legend: { display: true } }
    }
  });
}

// Kombiniertes Diagramm (Druck + Durchfluss)
function createCombinedChart(data) {
  const ctx = document.getElementById('combinedChart').getContext('2d');
  const datasets = [];

  // Drucksensoren auf linker Y-Achse
  for (let i = 1; i <= 4; i++) {
    datasets.push({
      label: `Pressure Sensor ${i}`,
      data: data.pressure[`sensor${i}`],
      borderColor: pressureColors[`sensor${i}`],
      backgroundColor: pressureColors[`sensor${i}`],
      fill: false,
      tension: 0.1,
      yAxisID: 'yPressure'
    });
  }
  // Durchfluss-Sensoren auf rechter Y-Achse
  for (let i = 1; i <= 2; i++) {
    datasets.push({
      label: `Flow Sensor ${i}`,
      data: data.flow[`sensor${i}`],
      borderColor: flowColors[`sensor${i}`],
      backgroundColor: flowColors[`sensor${i}`],
      fill: false,
      tension: 0.1,
      yAxisID: 'yFlow'
    });
  }

  combinedChartInstance = new Chart(ctx, {
    type: 'line',
    data: {
      labels: data.timestamps,
      datasets: datasets,
    },
    options: {
      responsive: true,
      scales: {
        x: {
          type: 'category',
          title: { display: true, text: 'Zeit' }
        },
        yPressure: {
          type: 'linear',
          position: 'left',
          title: { display: true, text: 'Druck (bar)' }
        },
        yFlow: {
          type: 'linear',
          position: 'right',
          title: { display: true, text: 'Durchfluss (L/min)' },
          grid: { drawOnChartArea: false }
        }
      },
      plugins: { legend: { display: true } }
    }
  });
}

// Diagramm als PNG herunterladen (gleich wie in charts.js)
function downloadChart(canvasId, filename) {
  const canvas = document.getElementById(canvasId);
  const link = document.createElement('a');
  link.href = canvas.toDataURL('image/png');
  link.download = filename;
  link.click();
}
