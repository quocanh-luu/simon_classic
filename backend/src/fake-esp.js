const WebSocket = require('ws');

const ws = new WebSocket('ws://localhost:3000');

ws.on('open', () => {
    console.log('🧪 Mock-ESP verbunden');

    ws.send(JSON.stringify({
      type: 'hello',
      role: 'esp'
    }));
  });

ws.on('message', (data) => {
  const msg = JSON.parse(data);
  console.log('📩 Server → ESP:', msg);

  // Simuliere LED-Anzeige
  if (msg.action === 'led_on') {
    console.log(`💡 LED: ${msg.color}`);

    // Simuliere richtigen Tastendruck nach 500ms
    setTimeout(() => {
      ws.send(JSON.stringify({
        type: 'sensor',
        color: msg.color   // korrekt gedrückt
      }));
    }, 500);
  }

  if (msg.action === 'game_over') {
    console.log('💀 GAME OVER');
  }
});
