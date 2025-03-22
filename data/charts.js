// ======================================================================
// Globale Variablen für Chart-Instanzen
// ======================================================================
let pressureChartInstance = null;
let flowChartInstance = null;
let combinedChartInstance = null;

// ======================================================================
// Farbdefinitionen für die Sensoren
// ======================================================================
const pressureColors = {
  sensor1: "rgba(255, 99, 132, 1)",   // Rot
  sensor2: "rgba(54, 162, 235, 1)",    // Blau
  sensor3: "rgba(255, 206, 86, 1)",    // Gelb
  sensor4: "rgba(75, 192, 192, 1)"     // Grün
};
const flowColors = {
  sensor1: "rgba(153, 102, 255, 1)",   // Lila
  sensor2: "rgba(255, 159, 64, 1)"      // Orange
};

// ======================================================================
// Haupt-Eventlistener: Bei DOMContentLoaded Daten laden, sortieren, Charts erstellen
// ======================================================================
document.addEventListener('DOMContentLoaded', function() {
  fetch('/api/last10min')
    .then(response => response.json())
    .then(data => {
      // 1) Sortierung der Daten nach Zeitstempel
      sortDataByTimestamp(data);

      // 2) Diagramme erzeugen
      createPressureChart(data);
      createFlowChart(data);
      createCombinedChart(data);

      // 3) Buttons für Diagramm-Download aktivieren (optional)
      document.getElementById("downloadPressure").addEventListener("click",
        () => downloadChart("pressureChart", "pressureChart.png"));
      document.getElementById("downloadFlow").addEventListener("click",
        () => downloadChart("flowChart", "flowChart.png"));
      document.getElementById("downloadCombined").addEventListener("click",
        () => downloadChart("combinedChart", "combinedChart.png"));

      // 4) Periodische Aktualisierung alle 1 Sekunden
      setInterval(updateCharts, 1000);
    })
    .catch(error => console.error("Fehler beim Laden der Diagrammdaten:", error));
});

// ======================================================================
// Funktion: Daten sortieren (nach Timestamp-String)
// ======================================================================
function sortDataByTimestamp(data) {
  // Wir gehen davon aus, dass data.timestamps ein Array von Strings im Format "YYYY-MM-DD HH:mm:ss" ist
  // Kombiniere sie mit den Druck- und Durchflusswerten
  const combined = data.timestamps.map((ts, index) => {
    return {
      timestamp: ts, // Als String belassen – Chart.js parst später über parser
      pressure1: data.pressure.sensor1[index],
      pressure2: data.pressure.sensor2[index],
      pressure3: data.pressure.sensor3[index],
      pressure4: data.pressure.sensor4[index],
      flow1: data.flow.sensor1[index],
      flow2: data.flow.sensor2[index]
    };
  });

  // Sortiere aufsteigend nach Datum/Uhrzeit, indem wir die Strings manuell in JS-Date umwandeln
  combined.sort((a, b) => new Date(a.timestamp) - new Date(b.timestamp));

  // Extrahiere die sortierten Arrays zurück in data
  data.timestamps = combined.map(item => item.timestamp);
  data.pressure.sensor1 = combined.map(item => item.pressure1);
  data.pressure.sensor2 = combined.map(item => item.pressure2);
  data.pressure.sensor3 = combined.map(item => item.pressure3);
  data.pressure.sensor4 = combined.map(item => item.pressure4);
  data.flow.sensor1 = combined.map(item => item.flow1);
  data.flow.sensor2 = combined.map(item => item.flow2);
}

// ======================================================================
// Funktion: Charts jede Sekunde updaten
// ======================================================================
function updateCharts() {
  fetch('/api/last10min')
    .then(response => response.json())
    .then(data => {
      // Sortiere neu empfangene Daten
      sortDataByTimestamp(data);

      // Pressure-Chart
      if (pressureChartInstance) {
        pressureChartInstance.data.labels = data.timestamps;
        pressureChartInstance.data.datasets.forEach((dataset, index) => {
          dataset.data = data.pressure[`sensor${index + 1}`];
        });
        pressureChartInstance.update();
      }

      // Flow-Chart
      if (flowChartInstance) {
        flowChartInstance.data.labels = data.timestamps;
        flowChartInstance.data.datasets.forEach((dataset, index) => {
          dataset.data = data.flow[`sensor${index + 1}`];
        });
        flowChartInstance.update();
      }

      // Kombiniertes Chart
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
    .catch(error => console.error("Fehler beim Aktualisieren der Diagrammdaten:", error));
}

// ======================================================================
// 1) Druckdiagramm erstellen
// ======================================================================
function createPressureChart(data) {
  const ctx = document.getElementById('pressureChart').getContext('2d');

  if (pressureChartInstance) {
    pressureChartInstance.destroy();
  }

  // DataSets
  const datasets = [];
  for (let i = 1; i <= 4; i++) {
    datasets.push({
      label: `Sensor ${i}`,
      data: data.pressure[`sensor${i}`],
      borderColor: pressureColors[`sensor${i}`],
      backgroundColor: pressureColors[`sensor${i}`],
      fill: false,
      showLine: true,
      tension: 0.1
    });
  }

  pressureChartInstance = new Chart(ctx, {
    type: 'line',
    data: {
      labels: data.timestamps,
      datasets: datasets
    },
    options: {
      responsive: true,
      scales: {
        x: {
          type: 'category', // Anzeige der Zeitstempel als Text
          title: { display: true, text: 'Datum und Uhrzeit' },
          ticks: {
            autoSkip: true,
            maxTicksLimit: 20
          }
        },
        y: {
          title: { display: true, text: 'Druck (bar)' }
        }
      },
      plugins: {
        legend: { display: true }
      }
    }
  });
}

// ======================================================================
// 2) Durchflussdiagramm erstellen
// ======================================================================
function createFlowChart(data) {
  const ctx = document.getElementById('flowChart').getContext('2d');

  if (flowChartInstance) {
    flowChartInstance.destroy();
  }

  const datasets = [];
  for (let i = 1; i <= 2; i++) {
    datasets.push({
      label: `Flow Sensor ${i}`,
      data: data.flow[`sensor${i}`],
      borderColor: flowColors[`sensor${i}`],
      backgroundColor: flowColors[`sensor${i}`],
      fill: false,
      showLine: true,
      tension: 0.1
    });
  }

  flowChartInstance = new Chart(ctx, {
    type: 'line',
    data: {
      labels: data.timestamps,
      datasets
    },
    options: {
      responsive: true,
      scales: {
        x: {
          type: 'category', // Anzeige der Zeitstempel als Text
          title: { display: true, text: 'Datum und Uhrzeit' },
          ticks: {
            autoSkip: true,
            maxTicksLimit: 20
          }
        },
        y: {
          title: { display: true, text: 'Durchfluss (L/min)' }
        }
      },
      plugins: {
        legend: { display: true }
      }
    }
  });
}

// ======================================================================
// 3) Kombiniertes Diagramm (Druck + Durchfluss)
// ======================================================================
function createCombinedChart(data) {
  const ctx = document.getElementById('combinedChart').getContext('2d');

  if (combinedChartInstance) {
    combinedChartInstance.destroy();
  }

  const datasets = [];

  // Drucksensoren (linke Y-Achse)
  for (let i = 1; i <= 4; i++) {
    datasets.push({
      label: `Pressure Sensor ${i}`,
      data: data.pressure[`sensor${i}`],
      borderColor: pressureColors[`sensor${i}`],
      backgroundColor: pressureColors[`sensor${i}`],
      fill: false,
      showLine: true,
      tension: 0.1,
      yAxisID: 'yPressure'
    });
  }
  // Durchflusssensoren (rechte Y-Achse)
  for (let i = 1; i <= 2; i++) {
    datasets.push({
      label: `Flow Sensor ${i}`,
      data: data.flow[`sensor${i}`],
      borderColor: flowColors[`sensor${i}`],
      backgroundColor: flowColors[`sensor${i}`],
      fill: false,
      showLine: true,
      tension: 0.1,
      yAxisID: 'yFlow'
    });
  }

  combinedChartInstance = new Chart(ctx, {
    type: 'line',
    data: {
      labels: data.timestamps,
      datasets
    },
    options: {
      responsive: true,
      scales: {
        x: {
          type: 'category', // Anzeige der Zeitstempel als Text
          title: { display: true, text: 'Datum und Uhrzeit' },
          ticks: {
            autoSkip: true,
            maxTicksLimit: 20
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
        legend: { display: true }
      }
    }
  });
}

// ======================================================================
// Funktion: Canvas-Inhalt als PNG herunterladen
// ======================================================================
function downloadChart(canvasId, filename) {
  const canvas = document.getElementById(canvasId);
  if (!canvas) return;
  const link = document.createElement('a');
  link.href = canvas.toDataURL('image/png');
  link.download = filename;
  link.click();
}
