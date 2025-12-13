const WebSocket = require('ws');

// Verbinde dich zum Server
const ws = new WebSocket('ws://localhost:3000');

ws.on('open', () => {
  console.log('Fake ESP verbunden');

  // Alle 2 Sekunden Status senden
  setInterval(() => {
    const status = {
      type: 'status',
      state: Math.random() > 0.5 ? 'playing' : 'idle', // zufälliger Status
      time_ms: Math.floor(Math.random() * 10000)
    };
    ws.send(JSON.stringify(status));
    console.log('Status gesendet:', status);
  }, 2000);
});

// Server-Nachrichten empfangen (z. B. Befehle von Web UI)
ws.on('message', (data) => {
  console.log('Nachricht vom Server:', data.toString());
});
