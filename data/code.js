function updateLEDState(state) {
    var statusElement = document.getElementById("led-state"); // Busca el elemento por su ID
    statusElement.textContent = state;
}
function updatePotentiometerValue(value) {
    var potentiometerElement = document.getElementById("potentiometer-value"); // Busca el elemento por su ID
    potentiometerElement.textContent = value;
}
function updateTemperatureAndHumidity(temperature, humidity) {
    document.getElementById('temperature-value').textContent = temperature;
    document.getElementById('humidity-value').textContent = humidity;
}
function updateCircleBasedOnLEDStates() {
    // Realizar una solicitud HTTP GET al servidor para obtener el estado del LED
    fetch('/leds')
        .then(response => response.text())
        .then(ledState => {
            // Obtener el elemento del círculo
            var circle = document.getElementById("circles");
            // Cambiar el color del círculo según el estado del LED
            if (ledState === "ON") {
                circle.style.backgroundColor = "green"; // Si el LED está encendido, el círculo es verde
            } else {
                circle.style.backgroundColor = "red"; // Si el LED está apagado, el círculo es rojo
            }
        })
        .catch(error => console.error('Error:', error));
}
function updateCircleBasedOnLEDStatem() {
    // Realizar una solicitud HTTP GET al servidor para obtener el estado del LED
    fetch('/ledm')
        .then(response => response.text())
        .then(ledState => {
            // Obtener el elemento del círculo
            var circle = document.getElementById("circlem");
            // Cambiar el color del círculo según el estado del LED
            if (ledState === "ON") {
                circle.style.backgroundColor = "green"; // Si el LED está encendido, el círculo es verde
            } else {
                circle.style.backgroundColor = "red"; // Si el LED está apagado, el círculo es rojo
            }
        })
        .catch(error => console.error('Error:', error));
}
/*
setInterval(updateValues, 1000);

function updateValues() {               // para que llame a los metodo de recarga lapagina cada 1 seg
    location.reload(); 
}
*/
function toggleSENS(){
    fetch('/get-potentiometer-value')
    .then(response => response.text())
    .then(data => {
        updatePotentiometerValue(data);
    })
    .catch(error => {
        console.error('Error getting potentiometer value:', error);
    });
}
function toggleLED() {
    fetch('/toggle-led')
    .then(response => response.text())
    .then(data => {
        updateLEDState(data);
    })
    .catch(error => {
        console.error('Error toggling LED:', error);
    });  
    fetch('/get-led-state')
        .then(response => response.text())
        .then(data => {
            updateLEDState(data);
        })
        .catch(error => {
            console.error('Error getting LED state:', error);
        });
}
// Evento al cargar la página para obtener y mostrar el estado actual del LED y el valor del potenciómetro
window.onload = function() {
    fetch('/get-led-state')
        .then(response => response.text())
        .then(data => {
            console.log('Initial LED state:', data);
            updateLEDState(data);
        })
        .catch(error => {
            console.error('Error getting LED state:', error);
        });
    fetch('/get-potentiometer-value')
        .then(response => response.text())
        .then(data => {
            console.log('Initial potentiometer value:', data);
            updatePotentiometerValue(data);
            document.getElementById("fuel").value = data; // Actualizar el valor de la barra de medición
        })
        .catch(error => {
            console.error('Error getting potentiometer value:', error);
        });  
        /// get dht
        fetch('/get-sensor-data')
        .then(response => response.json())
        .then(data => {
            console.log('Sensor data:', data);
            updateTemperatureAndHumidity(data.temperature, data.humidity);
            // Actualiza otros elementos de la página según sea necesario
        })
        .catch(error => {
            console.error('Error getting sensor data:', error);
        });
        
        updateCircleBasedOnLEDStates();
        updateCircleBasedOnLEDStatem();

        };
