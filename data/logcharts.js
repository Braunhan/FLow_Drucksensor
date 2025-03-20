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
    })
    .catch(error => console.error("Fehler beim Laden der Logdaten:", error));

  // 3) Download-Buttons (verwenden den High-Res-Download)
  document.getElementById('downloadPressure').addEventListener('click', () => {
    downloadHighResChart('pressureChart', 'pressure_chart.png', 4);
  });
  document.getElementById('downloadFlow').addEventListener('click', () => {
    downloadHighResChart('flowChart', 'flow_chart.png', 4);
  });
  document.getElementById('downloadCombined').addEventListener('click', () => {
    downloadHighResChart('combinedChart', 'combined_chart.png', 4);
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
      maintainAspectRatio: true,
      aspectRatio: 1.5, // 3:2 Format (Breite : Höhe)
      scales: {
        x: {
          type: 'category',
          title: { display: true, text: 'Datum und Uhrzeit' },
          ticks: {
            autoSkip: true,
            maxTicksLimit: 10
          }
        },
        y: {
          title: { display: true, text: 'Druck (bar)' }
        }
      },
      plugins: { 
        legend: { display: true, position: 'top' }
      }
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
      maintainAspectRatio: true,
      aspectRatio: 1.5,
      scales: {
        x: {
          type: 'category',
          title: { display: true, text: 'Datum und Uhrzeit' },
          ticks: {
            autoSkip: true,
            maxTicksLimit: 10
          }
        },
        y: {
          title: { display: true, text: 'Durchfluss (L/min)' }
        }
      },
      plugins: { 
        legend: { display: true, position: 'top' }
      }
    }
  });
}

// Kombiniertes Diagramm (Druck + Durchfluss) erstellen
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
      maintainAspectRatio: true,
      aspectRatio: 1.5,
      scales: {
        x: {
          type: 'category',
          title: { display: true, text: 'Datum und Uhrzeit' },
          ticks: {
            autoSkip: true,
            maxTicksLimit: 10
          }
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
      plugins: { 
        legend: { display: true, position: 'top' }
      }
    }
  });
}

// Funktion zum Downloaden eines Canvas in hoher Auflösung
function downloadHighResChart(canvasId, filename, scaleFactor = 4) {
  const originalCanvas = document.getElementById(canvasId);
  if (!originalCanvas) {
    console.warn("Canvas nicht gefunden: " + canvasId);
    return;
  }
  const offScreenCanvas = document.createElement('canvas');
  offScreenCanvas.width = originalCanvas.width * scaleFactor;
  offScreenCanvas.height = originalCanvas.height * scaleFactor;
  const ctx = offScreenCanvas.getContext('2d');

  // Weißer Hintergrund, um Transparenzprobleme zu vermeiden
  ctx.fillStyle = "#ffffff";
  ctx.fillRect(0, 0, offScreenCanvas.width, offScreenCanvas.height);

  ctx.scale(scaleFactor, scaleFactor);
  ctx.drawImage(originalCanvas, 0, 0);

  const dataURL = offScreenCanvas.toDataURL("image/png");
  const link = document.createElement('a');
  link.href = dataURL;
  link.download = filename;
  link.click();
}

// Standard-Download (ohne Skalierung) falls benötigt
function downloadChart(canvasId, filename) {
  const canvas = document.getElementById(canvasId);
  if (!canvas) {
    console.warn("Canvas-Element nicht gefunden: " + canvasId);
    return;
  }
  const link = document.createElement('a');
  link.href = canvas.toDataURL("image/png");
  link.download = filename;
  link.click();
}
