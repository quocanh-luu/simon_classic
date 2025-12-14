const express = require('express');
const http = require('http');
const WebSocket = require('ws');
const simon = require('./game/gameLogic');

const app = express();
app.use(express.static('frontend/src'));


const server = http.createServer(app);
const wss = new WebSocket.Server({ server });

let espClient = null;
let lastGameState = { type: 'state', state: 'idle', time_ms: 0 };


function sendToESP(obj) {
    if (espClient && espClient.readyState === WebSocket.OPEN) {
        espClient.send(JSON.stringify(obj));

    }
} 


wss.on('connection', function connection(ws, req) {
    console.log('Client connected:', req.socket.remoteAddress);

   
  
  // Nachricht vom Client empfangen
  ws.on('message', function message(data) {
  try {
    const parsed = JSON.parse(data);

    if (parsed.type === 'hello' && parsed.role === 'esp') {
      espClient = ws;
      console.log('ESP-Client verbunden');
      return;
    }

    // ─────────────────────────────
    // STATUS vom ESP (optional)
    // ─────────────────────────────
    if (parsed.type === 'status') {
      lastGameState = {
        type: 'state',
        state: parsed.state,
        time_ms: parsed.time_ms || 0
      };

      console.log('ESP Status:', parsed.state);
    }

    // ─────────────────────────────
    // COMMAND von der WEB-UI
    // ─────────────────────────────
    else if (parsed.type === 'command') {

      if (parsed.action === 'start') {
        console.log('Spielstart von UI');
        simon.startGame(sendToESP);
      }

    }

    // ─────────────────────────────
    // SENSOR-EVENT vom ESP
    // ─────────────────────────────
    else if (parsed.type === 'sensor') {
      console.log('Sensor gedrückt:', parsed.color);

      // Farbe an Simon-Logik weitergeben
      simon.handleSensorInput(parsed.color, sendToESP);
    }

  } catch (err) {
    console.error('Ungültiges JSON vom Client', err);
  }
});

  // Verbindung geschlossen
  ws.on('close', () => {
    if (ws === espClient) {
      console.log('ESP-Client getrennt');
      espClient = null;
    } else {
      console.log('Web-UI-Client getrennt');
    }
  });
});

const PORT = 3000;
server.listen(PORT, () => {
  console.log(`Server läuft auf http://localhost:${PORT}`);
});



// REST-ROUTES
//app.use("/simon", require("./api/routes"));

//WebSockets
// require("./sockets/mcuSocket")(server);
// require("./sockets/uiSocket")(server);

// server.listen(3001, () => console.log('Server running on port 3001'));