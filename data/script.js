// Funktion zum periodischen Abrufen der Sensorwerte vom ESP32
function updateData() {
  fetch('/api/sensorwerte')
    .then(response => response.json())
    .then(data => {
      // Aktualisiere die Zeitanzeige mit dem aktuellen Zeitstempel
      document.getElementById('timeDisplay').innerText = 'Zeit: ' + data.time;
      
      // Erstelle HTML für die Drucksensorwerte
      let pressureHtml = '';
      data.pressure.forEach((p, i) => {
        pressureHtml += `<p>Sensor ${i + 1}: ${p.toFixed(3)} bar</p>`;
      });
      document.getElementById('pressureData').innerHTML = pressureHtml;
      
      // Erstelle HTML für die Durchflusswerte
      let flowHtml = `<p>Sensor 1: ${data.flowRate[0].toFixed(2)} L/min (kUm: ${data.cumulativeFlow[0].toFixed(2)} L)</p>` +
                     `<p>Sensor 2: ${data.flowRate[1].toFixed(2)} L/min (kUm: ${data.cumulativeFlow[1].toFixed(2)} L)</p>`;
      document.getElementById('flowData').innerHTML = flowHtml;
    })
    .catch(error => {
      console.error('Fehler beim Abrufen der Daten:', error);
    });
}

// Starte den zyklischen Abruf der Daten (alle 1000 ms = 1 Sekunde)
setInterval(updateData, 1000);
updateData();

// --- Funktionen für die Steuerungsbuttons ---

// Schaltet das Recording (Datenlogging) um
function toggleRecording() {
  fetch('/toggleRecording')
    .then(response => response.text())
    .then(msg => alert(msg))
    .catch(err => console.error(err));
}

// Läd die Logdatei herunter, indem zur entsprechenden URL navigiert wird
function downloadLog() {
  window.location.href = '/downloadlog';
}

// Löscht die Logdatei und zeigt eine Bestätigung an
function deleteLog() {
  fetch('/deleteLog')
    .then(response => response.text())
    .then(msg => alert(msg))
    .catch(err => console.error(err));
}

// Setzt den kumulativen Durchfluss zurück
function clearFlow() {
  fetch('/clearCumulativeFlow')
    .then(response => response.text())
    .then(msg => alert(msg))
    .catch(err => console.error(err));
}

// Funktion, um Datum und Uhrzeit vom Benutzer zu setzen
function setDate() {
  const input = document.getElementById('datetimeInput').value;
  if (!input) {
    alert("Bitte wähle ein Datum und eine Uhrzeit aus.");
    return;
  }
  // Umwandlung in Unix-Timestamp (in Sekunden)
  const timestamp = Math.floor(new Date(input).getTime() / 1000);
  
  // GET-Request an den ESP32-Endpunkt /setTime mit dem Parameter t
  fetch(`/setTime?t=${timestamp}`)
    .then(response => response.text())
    .then(msg => {
      alert(msg);
    })
    .catch(err => {
      console.error("Fehler beim Setzen der Zeit:", err);
    });
}

// Funktion zur Aktualisierung der Kalibrierungswerte für einen Drucksensor
// Liest die Werte aus den Eingabefeldern und sendet sie als GET-Parameter an den Endpoint /updateCalibration
function updateCalibration(sensorIndex) {
  let vMin = document.getElementById("sensor" + (sensorIndex + 1) + "Vmin").value;
  let vMax = document.getElementById("sensor" + (sensorIndex + 1) + "Vmax").value;
  let psiMin = document.getElementById("sensor" + (sensorIndex + 1) + "PSImin").value;
  let psiMax = document.getElementById("sensor" + (sensorIndex + 1) + "PSImax").value;
  
  fetch(`/updateCalibration?sensor=${sensorIndex}&v_min=${vMin}&v_max=${vMax}&psi_min=${psiMin}&psi_max=${psiMax}`)
    .then(response => response.text())
    .then(msg => {
      alert(msg);
    })
    .catch(err => console.error(err));
}

// Neue Funktion zur Auto-Kalibrierung des v_min-Werts
function calibrateVmin(sensorIndex) {
  fetch(`/calibrateVmin?sensor=${sensorIndex}`)
    .then(response => response.text())
    .then(msg => {
      alert(msg);
      // Optional: Extrahiere den neuen v_min-Wert und aktualisiere das zugehörige Eingabefeld
      let regex = /Neuer v_min-Wert = ([0-9.]+) V/;
      let match = msg.match(regex);
      if (match && match[1]) {
        document.getElementById("sensor" + (sensorIndex + 1) + "Vmin").value = parseFloat(match[1]);
      }
    })
    .catch(err => console.error(err));
}
