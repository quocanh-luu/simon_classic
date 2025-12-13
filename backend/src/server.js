const express = require('express');
const http = require('http');
const WebSocket = require('ws');

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

    // Wenn noch kein ESP-Client registriert ist, nehmen wir diesen als ESP
//   if (!espClient) {
//     espClient = ws;
//     console.log('ESP-Client registriert');
//     // Direkt den letzten Status senden
//     sendToESP(lastGameState);
//   }

  // Nachricht vom Client empfangen
  ws.on('message', function message(data) {
    try {
      const parsed = JSON.parse(data);

      if (parsed.type === 'status') {
        // Status vom ESP empfangen, speichern
        lastGameState = { type: 'state', state: parsed.state, time_ms: parsed.time_ms || 0 };
        console.log('Status vom ESP:', lastGameState);
      } else if (parsed.type === 'command') {
        // Command von Web UI empfangen, direkt an ESP senden
        sendToESP(parsed);
        console.log('Befehl an ESP gesendet:', parsed);
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