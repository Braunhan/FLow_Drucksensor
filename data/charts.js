// Globale Variablen für Chart-Instanzen
let pressureChartInstance = null;
let flowChartInstance = null;
let combinedChartInstance = null;

// Farbvorgaben für die Sensoren (festgelegt in Konstanten)
const pressureColors = {
  sensor1: "rgba(255, 99, 132, 1)",    // Rot
  sensor2: "rgba(54, 162, 235, 1)",     // Blau
  sensor3: "rgba(255, 206, 86, 1)",     // Gelb
  sensor4: "rgba(75, 192, 192, 1)"      // Grün
};
const flowColors = {
  sensor1: "rgba(153, 102, 255, 1)",    // Lila
  sensor2: "rgba(255, 159, 64, 1)"      // Orange
};

document.addEventListener('DOMContentLoaded', function() {
  fetch('/api/last10min')
    .then(response => response.json())
    .then(data => {
      // UNIX-Zeitstempel (in Sekunden) in Date-Objekte umwandeln
      data.timestamps = data.timestamps.map(ts => new Date(ts * 1000));
      
      createPressureChart(data);
      createFlowChart(data);
      createCombinedChart(data);
      
      // Füge Event-Listener für die Download-Buttons hinzu
      document.getElementById("downloadPressure").addEventListener("click", () => downloadChart("pressureChart", "pressureChart.png"));
      document.getElementById("downloadFlow").addEventListener("click", () => downloadChart("flowChart", "flowChart.png"));
      document.getElementById("downloadCombined").addEventListener("click", () => downloadChart("combinedChart", "combinedChart.png"));
      
      // Starte periodisches Aktualisieren – alle 1 Sekunde
      setInterval(updateCharts, 1000);
    })
    .catch(error => console.error("Fehler beim Laden der Diagrammdaten:", error));
});


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
          type: 'time',
          time: {
            unit: 'minute',
            tooltipFormat: 'dd.MM.yyyy HH:mm:ss', // Format für Tooltips
            displayFormats: {
              minute: 'dd.MM.yyyy HH:mm' // Angezeigtes Format für Minuten
            }
          },
          title: { display: true, text: 'Datum und Uhrzeit' }
        },
        y: {
          title: { display: true, text: 'Druck (bar)' }
        }
      },
      plugins: { legend: { display: true } }
    }
  });
}

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
          type: 'time',
          time: {
            unit: 'minute',
            tooltipFormat: 'dd.MM.yyyy HH:mm:ss', // Format für Tooltips
            displayFormats: {
              minute: 'dd.MM.yyyy HH:mm' // Angezeigtes Format für Minuten
            }
          },
          title: { display: true, text: 'Datum und Uhrzeit' }
        },
        y: {
          title: { display: true, text: 'Durchfluss (L/min)' }
        }
      },
      plugins: { legend: { display: true } }
    }
  });
}

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
  // Flow Sensoren auf rechter Y-Achse
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
          type: 'time',
          time: {
            unit: 'minute',
            tooltipFormat: 'dd.MM.yyyy HH:mm:ss', // Format für Tooltips
            displayFormats: {
              minute: 'dd.MM.yyyy HH:mm' // Angezeigtes Format für Minuten
            }
          },
          title: { display: true, text: 'Datum und Uhrzeit' }
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

// Funktion, um die Diagramme zu aktualisieren
function updateCharts() {
  fetch('/api/last10min')
    .then(response => response.json())
    .then(data => {
      // Update Pressure Chart
      if (pressureChartInstance) {
        pressureChartInstance.data.labels = data.timestamps;
        pressureChartInstance.data.datasets.forEach((dataset, index) => {
          dataset.data = data.pressure[`sensor${index + 1}`];
        });
        pressureChartInstance.update();
      }
      
      // Update Flow Chart
      if (flowChartInstance) {
        flowChartInstance.data.labels = data.timestamps;
        flowChartInstance.data.datasets.forEach((dataset, index) => {
          dataset.data = data.flow[`sensor${index + 1}`];
        });
        flowChartInstance.update();
      }
      
      // Update Combined Chart
      if (combinedChartInstance) {
        combinedChartInstance.data.labels = data.timestamps;
        combinedChartInstance.data.datasets.forEach((dataset) => {
          if (dataset.label.includes("Pressure")) {
            const sensorNum = dataset.label.match(/\d+/)[0];
            dataset.data = data.pressure[`sensor${sensorNum}`];
          } else if (dataset.label.includes("Flow")) {
            const sensorNum = dataset.label.match(/\d+/)[0];
            dataset.data = data.flow[`sensor${sensorNum}`];
          }
        });
        combinedChartInstance.update();
      }
    })
    .catch(error => console.error("Fehler beim Aktualisieren der Diagramme:", error));
}

function downloadChart(canvasId, filename) {
  const canvas = document.getElementById(canvasId);
  const link = document.createElement('a');
  link.href = canvas.toDataURL('image/png');
  link.download = filename;
  link.click();
}
