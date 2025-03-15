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
  
  // Läd die Logdatei herunter, indem die Seite zur entsprechenden URL navigiert
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
