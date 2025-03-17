document.addEventListener('DOMContentLoaded', () => {
  loadCalibrationData();
  setupEventListeners();
});

async function loadCalibrationData() {
  try {
      const response = await fetch('/api/calibration');
      if (!response.ok) throw new Error('Daten konnten nicht geladen werden');
      
      const sensors = await response.json();
      
      sensors.forEach((sensor, index) => {
          const sensorNumber = index + 1;
          document.getElementById(`sensor${sensorNumber}Vmin`).value = sensor.v_min.toFixed(2);
          document.getElementById(`sensor${sensorNumber}Vmax`).value = sensor.v_max.toFixed(2);
          document.getElementById(`sensor${sensorNumber}PSImin`).value = sensor.psi_min.toFixed(1);
          document.getElementById(`sensor${sensorNumber}PSImax`).value = sensor.psi_max.toFixed(1);
      });
  } catch (error) {
      showError('Fehler beim Laden:', error);
  }
}

async function saveSettings(sensorIndex) {
  const sensorNumber = sensorIndex + 1;
  const payload = {
      sensor: sensorIndex,
      v_min: parseFloat(document.getElementById(`sensor${sensorNumber}Vmin`).value),
      v_max: parseFloat(document.getElementById(`sensor${sensorNumber}Vmax`).value),
      psi_min: parseFloat(document.getElementById(`sensor${sensorNumber}PSImin`).value),
      psi_max: parseFloat(document.getElementById(`sensor${sensorNumber}PSImax`).value)
  };

  if (isInvalidCalibration(payload)) {
      alert('Ungültige Werte!\nV_min < V_max und PSI_min < PSI_max erforderlich');
      return;
  }

  try {
      const response = await fetch('/updateCalibration', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(payload)
      });
      
      const result = await response.text();
      alert(result + '\nV-Werte gelten bis zum Neustart!');
      loadCalibrationData();
  } catch (error) {
      showError('Speichern fehlgeschlagen:', error);
  }
}

function isInvalidCalibration(payload) {
  return isNaN(payload.v_min) || 
         isNaN(payload.v_max) || 
         isNaN(payload.psi_min) || 
         isNaN(payload.psi_max) ||
         payload.v_min >= payload.v_max ||
         payload.psi_min >= payload.psi_max;
}

async function autoCalibrate(sensorIndex) {
  try {
      const response = await fetch(`/calibrateVmin?sensor=${sensorIndex}`);
      const result = await response.json();
      
      if(result.status === "success") {
          document.getElementById(`sensor${sensorIndex + 1}Vmin`).value = result.v_min.toFixed(2);
          alert(`V_min kalibriert: ${result.v_min.toFixed(2)} V\nGilt bis zum Neustart!`);
      } else {
          throw new Error('Kalibrierung fehlgeschlagen');
      }
  } catch (error) {
      showError('Kalibrierung fehlgeschlagen:', error);
  }
}

function resetSensor(sensorIndex) {
  const sensorNumber = sensorIndex + 1;
  document.getElementById(`sensor${sensorNumber}Vmin`).value = 0.50;
  document.getElementById(`sensor${sensorNumber}Vmax`).value = 4.50;
  document.getElementById(`sensor${sensorNumber}PSImin`).value = 0.0;
  document.getElementById(`sensor${sensorNumber}PSImax`).value = 10.0;
  alert(`Sensor ${sensorNumber} auf Standard zurückgesetzt`);
}

function setupEventListeners() {
  document.querySelectorAll('input[type="number"]').forEach(input => {
      input.addEventListener('keypress', (e) => {
          if (e.key === 'Enter') {
              const sensorIndex = parseInt(input.id.match(/\d+/)[0]) - 1;
              saveSettings(sensorIndex);
          }
      });
  });
}

function showError(context, error) {
  console.error(context, error);
  alert(`${context}\n${error.message}`);
}