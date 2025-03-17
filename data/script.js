// ------------------------------------
// Check JSZip / FileSaver
// ------------------------------------
if (typeof JSZip === "undefined") {
  console.warn("JSZip ist nicht geladen. Bitte überprüfe /jszip.min.js!");
}
if (typeof saveAs === "undefined") {
  console.warn("FileSaver ist nicht geladen. Bitte überprüfe /filesaver.min.js!");
}

// ------------------------------------
// Globale Variablen für Diagramme
// ------------------------------------
let pressureChartInstance = null;
let flowChartInstance = null;
let combinedChartInstance = null;

// Farbdefinitionen
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

// ------------------------------------
// 1) Beim Laden der Seite:
//    - Periodische Text-Updates
//    - Diagramme erstellen
//    - Regelmäßig /api/loggingData abrufen -> Diagramme
// ------------------------------------
document.addEventListener('DOMContentLoaded', () => {
  // a) Text-Updates (Sensorwerte) alle 1 Sekunde
  setInterval(updateData, 1000);
  updateData();

  // b) Diagramme initial erstellen
  initLoggingCharts();

  // c) Alle 1 Sekunde loggingData abfragen -> Diagramme aktualisieren
  setInterval(updateLoggingCharts, 1000);

  // d) Buttons zum PNG-Download der Diagramme
  //    (Falls in index.html vorhanden)
  const dlPressureBtn = document.getElementById('downloadPressure');
  if (dlPressureBtn) {
    dlPressureBtn.addEventListener('click', () => {
      downloadChart('pressureChart', 'druck_chart.png');
    });
  }
  const dlFlowBtn = document.getElementById('downloadFlow');
  if (dlFlowBtn) {
    dlFlowBtn.addEventListener('click', () => {
      downloadChart('flowChart', 'flow_chart.png');
    });
  }
  const dlCombinedBtn = document.getElementById('downloadCombined');
  if (dlCombinedBtn) {
    dlCombinedBtn.addEventListener('click', () => {
      downloadChart('combinedChart', 'combined_chart.png');
    });
  }
});

// ------------------------------------
// 2) Periodisches Abrufen der *textuellen* Sensorwerte
// ------------------------------------
function updateData() {
  fetch('/api/sensorwerte')
    .then(response => response.json())
    .then(data => {
      // Zeitanzeige
      document.getElementById('timeDisplay').innerText = 'Zeit: ' + data.time;
      
      // Drucksensorwerte
      let pressureHtml = '';
      data.pressure.forEach((p, i) => {
        pressureHtml += `<p>Sensor ${i+1}: ${p.toFixed(3)} bar</p>`;
      });
      document.getElementById('pressureData').innerHTML = pressureHtml;
      
      // Durchflusswerte
      let flowHtml = `<p>Sensor 1: ${data.flowRate[0].toFixed(2)} L/min (kUm: ${data.cumulativeFlow[0].toFixed(2)} L)</p>
                      <p>Sensor 2: ${data.flowRate[1].toFixed(2)} L/min (kUm: ${data.cumulativeFlow[1].toFixed(2)} L)</p>`;
      document.getElementById('flowData').innerHTML = flowHtml;
    })
    .catch(error => {
      console.error('Fehler beim Abrufen der Daten:', error);
    });
}

// ------------------------------------
// 3) Diagramme (Logging-Daten):
//    Initialisierung + Update
// ------------------------------------
function initLoggingCharts() {
  // a) Druckdiagramm
  {
    const ctx = document.getElementById('pressureChart').getContext('2d');
    const datasets = [];
    for (let i = 1; i <= 4; i++) {
      datasets.push({
        label: `Sensor ${i}`,
        data: [], // noch leer
        borderColor: pressureColors[`sensor${i}`],
        backgroundColor: pressureColors[`sensor${i}`],
        fill: false,
        tension: 0.1
      });
    }
    pressureChartInstance = new Chart(ctx, {
      type: 'line',
      data: {
        labels: [],
        datasets: datasets
      },
      options: {
        responsive: true
      }
    });
  }

  // b) Flowdiagramm
  {
    const ctx = document.getElementById('flowChart').getContext('2d');
    const datasets = [];
    for (let i = 1; i <= 2; i++) {
      datasets.push({
        label: `Flow Sensor ${i}`,
        data: [],
        borderColor: flowColors[`sensor${i}`],
        backgroundColor: flowColors[`sensor${i}`],
        fill: false,
        tension: 0.1
      });
    }
    flowChartInstance = new Chart(ctx, {
      type: 'line',
      data: {
        labels: [],
        datasets: datasets
      },
      options: {
        responsive: true
      }
    });
  }

  // c) Kombi-Diagramm (Druck + Flow)
  {
    const ctx = document.getElementById('combinedChart').getContext('2d');
    const datasets = [];
    // 4 Druck -> linker Y
    for (let i = 1; i <= 4; i++) {
      datasets.push({
        label: `Pressure ${i}`,
        data: [],
        borderColor: pressureColors[`sensor${i}`],
        backgroundColor: pressureColors[`sensor${i}`],
        fill: false,
        tension: 0.1,
        yAxisID: 'yPressure'
      });
    }
    // 2 Flow -> rechter Y
    for (let i = 1; i <= 2; i++) {
      datasets.push({
        label: `Flow ${i}`,
        data: [],
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
        labels: [],
        datasets: datasets
      },
      options: {
        responsive: true,
        scales: {
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
        }
      }
    });
  }
}

// Diese Funktion holt /api/loggingData
// und aktualisiert die Diagramme
function updateLoggingCharts() {
  fetch('/api/loggingData')
    .then(r => {
      if (!r.ok) {
        // z.B. 404 -> wirft Fehler
        throw new Error("HTTP " + r.status + " - " + r.statusText);
      }
      return r.json();
    })
    .then(data => {
      // data: { "timestamps": [...],
      //         "pressure": { "sensor1": [...], ... },
      //         "flow": { "sensor1": [...], "sensor2": [...] } }

      // Druckdiagramm
      if (pressureChartInstance) {
        pressureChartInstance.data.labels = data.timestamps;
        pressureChartInstance.data.datasets.forEach((ds, idx) => {
          ds.data = data.pressure[`sensor${idx+1}`]; 
        });
        pressureChartInstance.update();
      }

      // Flowdiagramm
      if (flowChartInstance) {
        flowChartInstance.data.labels = data.timestamps;
        flowChartInstance.data.datasets.forEach((ds, idx) => {
          ds.data = data.flow[`sensor${idx+1}`];
        });
        flowChartInstance.update();
      }

      // Kombi-Diagramm
      if (combinedChartInstance) {
        combinedChartInstance.data.labels = data.timestamps;
        combinedChartInstance.data.datasets.forEach(ds => {
          if (ds.label.includes("Pressure")) {
            const sn = ds.label.match(/\d+/)[0]; // "1", "2", ...
            ds.data = data.pressure[`sensor${sn}`];
          } else if (ds.label.includes("Flow")) {
            const sn = ds.label.match(/\d+/)[0];
            ds.data = data.flow[`sensor${sn}`];
          }
        });
        combinedChartInstance.update();
      }
    })
    .catch(err => console.error("Fehler beim updateLoggingCharts:", err));
}

// ------------------------------------
// PNG-Download je Canvas
// ------------------------------------
function downloadChart(canvasId, filename) {
  const canvas = document.getElementById(canvasId);
  if(!canvas){
    console.warn("Canvas-Element nicht gefunden: "+canvasId);
    return;
  }
  const link = document.createElement('a');
  link.href = canvas.toDataURL('image/png');
  link.download = filename;
  link.click();
}

// ------------------------------------
// 4) Togglen der Aufnahme
// ------------------------------------
function toggleRecording() {
  fetch('/toggleRecording')
    .then(r => r.text())
    .then(msg => {
      alert(msg);
      const btn = document.getElementById('toggleRecordingBtn');
      if (msg.includes("gestartet")) {
        btn.innerText = "Recording stoppen";
        btn.classList.add("recording");
      } else {
        btn.innerText = "Recording starten";
        btn.classList.remove("recording");
      }
    })
    .catch(err => console.error("Fehler bei toggleRecording:", err));
}

// 5) CSV direkt herunterladen
function downloadLog() {
  window.location.href = '/downloadlog';
}

// 6) Logdatei löschen
function deleteLog() {
  fetch('/deleteLog')
    .then(r=>r.text())
    .then(msg=>alert(msg))
    .catch(err=>console.error(err));
}

// 7) Kumulativen Durchfluss reset
function clearFlow() {
  fetch('/clearCumulativeFlow')
    .then(r=>r.text())
    .then(msg=>alert(msg))
    .catch(err=>console.error(err));
}

// 8) Zeit setzen
function setDate() {
  const input = document.getElementById('datetimeInput').value;
  if(!input){
    alert("Bitte wähle ein Datum und eine Uhrzeit.");
    return;
  }
  const timestamp = Math.floor(new Date(input).getTime()/1000);
  fetch(`/setTime?t=${timestamp}`)
    .then(r=>r.text())
    .then(msg=>alert(msg))
    .catch(err=>console.error(err));
}

// 9) updateCalibration
function updateCalibration(sensorIndex) {
  let vMin = document.getElementById("sensor" + (sensorIndex + 1) + "Vmin").value;
  let vMax = document.getElementById("sensor" + (sensorIndex + 1) + "Vmax").value;
  let psiMin = document.getElementById("sensor" + (sensorIndex + 1) + "PSImin").value;
  let psiMax = document.getElementById("sensor" + (sensorIndex + 1) + "PSImax").value;
  fetch(`/updateCalibration?sensor=${sensorIndex}&v_min=${vMin}&v_max=${vMax}&psi_min=${psiMin}&psi_max=${psiMax}`)
    .then(r=>r.text())
    .then(msg=>alert(msg))
    .catch(err=>console.error(err));
}

// 10) calibrateVmin
function calibrateVmin(sensorIndex) {
  fetch(`/calibrateVmin?sensor=${sensorIndex}`)
    .then(r=>r.text())
    .then(msg=> {
      alert(msg);
      const regex = /Neuer v_min-Wert = ([0-9.]+) V/;
      const match = msg.match(regex);
      if (match && match[1]) {
        document.getElementById("sensor" + (sensorIndex + 1) + "Vmin").value = parseFloat(match[1]);
      }
    })
    .catch(err=>console.error(err));
}

// 11) CSV + Diagramme -> ZIP
async function downloadAllData() {
  if (typeof saveAs === "undefined") {
    alert("FileSaver ist nicht geladen.");
    return;
  }
  if (typeof JSZip === "undefined") {
    alert("JSZip ist nicht geladen.");
    return;
  }
  const zip = new JSZip();
  const timestamp = getFileTimestampJS();
  
  // a) CSV
  try {
    const csvResp = await fetch('/downloadlog');
    if(!csvResp.ok) throw new Error("CSV (HTTP "+csvResp.status+")");
    const csvText = await csvResp.text();
    zip.file(timestamp+"_Rohdaten.csv", csvText);
  } catch(e) {
    console.error("Fehler beim CSV:", e);
    alert("Fehler beim Laden der CSV");
    return;
  }

  // b) Canvas -> PNG
  function getCanvasAsBase64(id){
    const c=document.getElementById(id);
    if(!c)return null;
    return c.toDataURL("image/png").split(",")[1];
  }
  const pData = getCanvasAsBase64("pressureChart");
  if(pData) zip.file(timestamp+"_Druck.png", pData, {base64:true});
  const fData = getCanvasAsBase64("flowChart");
  if(fData) zip.file(timestamp+"_Flow.png", fData, {base64:true});
  const cData = getCanvasAsBase64("combinedChart");
  if(cData) zip.file(timestamp+"_Kombiniert.png", cData, {base64:true});

  // c) ZIP generieren
  try {
    const blob=await zip.generateAsync({type:"blob"});
    saveAs(blob, timestamp+"_Log_Diagramme.zip");
  } catch(err) {
    console.error("Fehler beim ZIP:", err);
    alert("Fehler beim Erstellen der ZIP-Datei");
  }
}

// Hilfsfunktion Dateiname
function getFileTimestampJS(){
  const now=new Date();
  const day=String(now.getDate()).padStart(2,'0');
  const month=String(now.getMonth()+1).padStart(2,'0');
  const year=now.getFullYear();
  const hh=String(now.getHours()).padStart(2,'0');
  const mm=String(now.getMinutes()).padStart(2,'0');
  return `${day}-${month}-${year}_${hh}-${mm}`;
}
