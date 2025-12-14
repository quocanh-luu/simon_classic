// simonLogic.js

const COLORS = ['red', 'yellow', 'green', 'blue'];

let state = 'WAITING';
let playlist = [];
let inputIndex = 0;

function randomColor() {
  return COLORS[Math.floor(Math.random() * COLORS.length)];
}

function startGame(sendToESP) {
  state = 'PLAYING';
  playlist = [];
  inputIndex = 0;

  nextRound(sendToESP);
}

function nextRound(sendToESP) {
  playlist.push(randomColor());
  inputIndex = 0;

  showSequence(sendToESP);
}

async function showSequence(sendToESP) {
  for (const color of playlist) {
    sendToESP({
      type: 'command',
      action: 'led_on',
      color
    });

    await delay(800);

    sendToESP({
      type: 'command',
      action: 'led_off'
    });

    await delay(300);
  }
}

function handleSensorInput(color, sendToESP) {
  if (state !== 'PLAYING') return;

  if (playlist[inputIndex] === color) {
    inputIndex++;

    // Runde fertig
    if (inputIndex === playlist.length) {
      setTimeout(() => nextRound(sendToESP), 1000);
    }
  } else {
    gameOver(sendToESP);
  }
}

function gameOver(sendToESP) {
  state = 'GAME_OVER';

  sendToESP({
    type: 'command',
    action: 'game_over',
    score: playlist.length - 1
  });

  state = 'WAITING';
}

function delay(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

module.exports = {
  startGame,
  handleSensorInput
};
