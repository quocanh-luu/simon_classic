#include <Arduino.h>
#include <vector>
#include <list>

#include <FastLED.h>

#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

#define LED_RED_PIN 2
#define LED_YELLOW_PIN 3
#define LED_GREEN_PIN 4
#define LED_BLUE_PIN 5

#define NUM_LEDS 200

//Pins muss noch aktualisiert werden
#define BUZZER_RED_PIN 1
#define BUZZER_YELLOW_PIN 2
#define BUZZER_GREEN_PIN 3
#define BUZZER_BLUE_PIN 4

#define BUZZER_CHANNEL_RED 0
#define BUZZER_CHANNEL_YELLOW 1
#define BUZZER_CHANNEL_GREEN 2
#define BUZZER_CHANNEL_BLUE 3

#define SENSOR_RED_PIN 12
#define SENSOR_YELLOW_PIN 13
#define SENSOR_GREEN_PIN 14
#define SENSOR_BLUE_PIN 15

using namespace std;

enum GameState {
  WAITING,
  ONLINE_PLAYING,
  ONLINE_RECONNECTING,
  OFFLINE_PLAYING,
  GAME_OVER
};

enum OnlineGameState {
  IDLE,
  SHOW_SEQUENCE,
  WAIT_FOR_INPUT,
};

enum Color {
  RED,
  YELLOW,
  GREEN,
  BLUE
};

//baue WebSocket Client ein
const char* ssid     = "eduroam";
const char* identity = "inf4353@hs-worms.de";
const char* username = "inf4353";
const char* password = "mocxit12102003"; 

void connectEduroam(){
  WiFi.disconnect(true); //Vergangene Verbindungen löschen
  delay(1000);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, WPA2_AUTH_PEAP, identity, username, password);
  Serial.println("Connecting to eduroam...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("Connected to eduroam");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
};

const char* websockets_server_host = "10.12.223.86"; //server address
const uint16_t websockets_server_port = 3000; //server port
const char* websockets_server_path = "/"; //server path

volatile bool wsConnect = false;
WebSocketsClient webSocket;




//online game variables
bool started = false;
GameState currentState = WAITING;
OnlineGameState onlineState = IDLE;
OnlineGameState previousOnlineState = IDLE;    //State before reconnecting
bool wsMessageReceived = false;
char wsSendBuffer[256];
char wsBuffer[1024];
vector<int> onlineSequence;
int showIndex = 0;
int score = 0;


const int SENSOR_THRESHOLD = 1000;


//Variables for show_sequence
//stepEndTime: Zeit, wann der aktuelle Step (Anzeigen einer Farbe oder Pause) endet
//stepOnPhase: true, wenn gerade eine Farbe angezeigt wird, false, wenn Pause
bool stepOnPhase = false;
unsigned long stepEndTime = 0;

const unsigned long STEP_DURATION = 1000; //Dauer, wie lange eine Farbe gezeigt wird
const unsigned long PAUSE_DURATION = 500; //Pause zwischen den Farben



CRGB Red[NUM_LEDS];
CRGB Yellow[NUM_LEDS];
CRGB Green[NUM_LEDS];
CRGB Blue[NUM_LEDS]; 

// put function declarations here:
void welcomeEffect();
Color intToColor(int value);
// Function to play the game in offline mode
int play();
void show(std::vector<Color> &sequence);
bool checkInput(std::vector<Color> &sequence, std::vector<Color>::iterator &it);
void nextRound(std::vector<Color> &sequence, std::vector<Color>::iterator &it);
bool startGame();
int readDataFromSensor();
//Functions to play game in online mode
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length);
void handleWebSocketMessage();
void startSound(int frequency, Color color);
void stopSound(Color color);

//Functions for buzzer
void startSound(int frequency, Color color);
void stopSound(Color color);



void setup() {
  Serial.begin(9600);

  //Buzzer setup
  ledcSetup(BUZZER_CHANNEL_RED, 2000, 8); // 2kHz frequency, 8-bit resolution
  ledcAttachPin(BUZZER_RED_PIN, BUZZER_CHANNEL_RED);
  ledcSetup(BUZZER_CHANNEL_YELLOW, 2000, 8); 
  ledcAttachPin(BUZZER_YELLOW_PIN, BUZZER_CHANNEL_YELLOW);
  ledcSetup(BUZZER_CHANNEL_GREEN, 2000, 8);
  ledcAttachPin(BUZZER_GREEN_PIN, BUZZER_CHANNEL_GREEN);
  ledcSetup(BUZZER_CHANNEL_BLUE, 2000, 8); 
  ledcAttachPin(BUZZER_BLUE_PIN, BUZZER_CHANNEL_BLUE);

  connectEduroam();

  // setup websocket server connection
  webSocket.begin(websockets_server_host, websockets_server_port, websockets_server_path);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(2000);


  FastLED.addLeds<WS2812B, LED_RED_PIN, GRB>(Red, NUM_LEDS);
  FastLED.addLeds<WS2812B, LED_YELLOW_PIN, GRB>(Yellow, NUM_LEDS);
  FastLED.addLeds<WS2812B, LED_GREEN_PIN, GRB>(Green, NUM_LEDS);
  FastLED.addLeds<WS2812B, LED_BLUE_PIN, GRB>(Blue, NUM_LEDS);
}

void loop() {
  webSocket.loop();

  if(wsMessageReceived){
    handleWebSocketMessage();
  }

  if(currentState == WAITING){
    if(startGame()){
      welcomeEffect();
      if(wsConnect){
        currentState = ONLINE_PLAYING;
      } else {
        currentState = OFFLINE_PLAYING;
      }
    }
  }

  if(currentState == ONLINE_PLAYING){
    if(!started){
      StaticJsonDocument<256> startGameDoc;
      startGameDoc["type"] = "event";
      startGameDoc["event"] = "start";

      size_t n = serializeJson(startGameDoc, wsSendBuffer);
      webSocket.sendTXT(wsSendBuffer, n);
      started = true;
      //onlineState = IDLE bedeutet, dass wir nichts machen
      onlineState = IDLE;

      return;
    }
    
    if(onlineState == SHOW_SEQUENCE){
      unsigned long currentTime = millis();
      if(currentTime < stepEndTime){
        //in einer Step-Phase
        return;
      }
      //LED und Sound ist gerade aus
      if(!stepOnPhase){
        //Sequenz fertig anzeigen?
        if(showIndex >= onlineSequence.size()){
          //fertig mit anzeigen
          FastLED.clear();
          FastLED.show();
          onlineState = WAIT_FOR_INPUT;
          return;
        }

        //nächste Farbe anzeigen
        Color color = intToColor(onlineSequence[showIndex]);

        FastLED.clear();

        switch(color){
          case RED:
            fill_solid(Red, NUM_LEDS, CRGB::Red);
            FastLED.show();
            startSound(440, RED); //A4
            break;
          case YELLOW:
            fill_solid(Yellow, NUM_LEDS, CRGB::Yellow);
            FastLED.show();
            startSound(554, YELLOW); //C#5
            break;
          case GREEN:
            fill_solid(Green, NUM_LEDS, CRGB::Green);
            FastLED.show();
            startSound(659, GREEN); //E5
            break;
          case BLUE:
            fill_solid(Blue, NUM_LEDS, CRGB::Blue);
            FastLED.show();
            startSound(784, BLUE); //G5
            break;
        }

        stepEndTime = currentTime + STEP_DURATION;
        stepOnPhase = true;
      }
      else{
        stopSound(intToColor(onlineSequence[showIndex]));
        FastLED.clear();
        FastLED.show();

        showIndex++;
        stepEndTime = currentTime + PAUSE_DURATION;
        stepOnPhase = false;
      }
    }
    if(onlineState == WAIT_FOR_INPUT){  
      static unsigned long lastHitTime[4] = {0, 0, 0, 0};
      const unsigned long DEBOUNCE_DELAY = 300; //300ms Entprellzeit

      int sensorIndex = readDataFromSensor();
      if(sensorIndex != -1){
        //Sensor wurde gedrückt
        unsigned long currentTime = millis();
        if(currentTime - lastHitTime[sensorIndex] < DEBOUNCE_DELAY){
          //zu schnell hintereinander, ignorieren
          return;
        }
        lastHitTime[sensorIndex] = currentTime;
        StaticJsonDocument<256> inputDoc;
        inputDoc["type"] = "event";
        inputDoc["event"] = "input";
        inputDoc["color"] = sensorIndex;
        size_t n = serializeJson(inputDoc, wsSendBuffer);
        webSocket.sendTXT(wsSendBuffer, n);
      }

      return;
    }
  }

  if(currentState == ONLINE_RECONNECTING){
    //blink all leds to indicate reconnecting
    static bool blinkState = false;
    static unsigned long lastBlinkTime = 0;
    const unsigned long BLINK_INTERVAL = 500; //500ms
    unsigned long currentTime = millis();

    if(currentTime - lastBlinkTime >= BLINK_INTERVAL){
      lastBlinkTime = currentTime;
      blinkState = !blinkState;

      if(blinkState){
        fill_solid(Red, NUM_LEDS, CRGB::Yellow);
        fill_solid(Yellow, NUM_LEDS, CRGB::Yellow);
        fill_solid(Green, NUM_LEDS, CRGB::Yellow);
        fill_solid(Blue, NUM_LEDS, CRGB::Yellow);
      } else {
        FastLED.clear();
      }
      FastLED.show();
    }

    if(wsConnect){
      //reconnected
      FastLED.clear();
      FastLED.show();
      currentState = ONLINE_PLAYING;
      if(previousOnlineState == SHOW_SEQUENCE){
        stepOnPhase = false;
        stepEndTime = millis();
        showIndex = 0;
        onlineState = SHOW_SEQUENCE;
      } else {
        onlineState = WAIT_FOR_INPUT;
      }
    }

  }

  if(currentState == OFFLINE_PLAYING){
    int score = play();
    currentState = GAME_OVER;
  }


  if(currentState == GAME_OVER){
    //blink all leds to indicate game over
    fill_solid(Red, NUM_LEDS, CRGB::Red);
    fill_solid(Yellow, NUM_LEDS, CRGB::Red);
    fill_solid(Green, NUM_LEDS, CRGB::Red);
    fill_solid(Blue, NUM_LEDS, CRGB::Red);
    FastLED.show();
    delay(2000);
    FastLED.clear();
    FastLED.show();
    currentState = WAITING;
  };
}




// put function definitions here:
void welcomeEffect(){
  
}


int play(){
  vector<Color> sequence;
  int counter = 0;
  int score = 0;
  vector<Color>::iterator it;

  while(true){
    nextRound(sequence, it);
    show(sequence);
    while(it != sequence.end()){
      if(!checkInput(sequence, it)){
        //Game over
        score = counter;
        counter = 0;
        sequence.clear();
        return score;
      }
      it++;
    }
    counter++;
  }

};


void show(vector<Color> &sequence){
  for(auto color : sequence){
    switch(color){
      case RED:
        fill_solid(Red, NUM_LEDS, CRGB::Red);
        FastLED.show();
        break;
      case YELLOW:
        fill_solid(Yellow, NUM_LEDS, CRGB::Yellow);
        FastLED.show();
        break;
      case GREEN:
        fill_solid(Green, NUM_LEDS, CRGB::Green);
        FastLED.show();
        break;
      case BLUE:
        fill_solid(Blue, NUM_LEDS, CRGB::Blue);
        FastLED.show();
        break;
    }
    delay(1000); //wait between colors
    FastLED.clear();
    FastLED.show();
  }
};


bool checkInput(vector<Color> &sequence, vector<Color>::iterator &it){
  int sensorIndex = -1;

  while (sensorIndex == -1){
    sensorIndex = readDataFromSensor();
    delay(5);
  }

  return static_cast<Color>(sensorIndex) == *it;
};


void nextRound(vector<Color> &sequence, vector<Color>::iterator &it){
  sequence.push_back(static_cast<Color>(esp_random() % 4));
  it = sequence.begin();
}


bool startGame(){
  int sensorPin[4] = {SENSOR_RED_PIN, SENSOR_YELLOW_PIN, SENSOR_GREEN_PIN, SENSOR_BLUE_PIN};
  int sensorData[4] = {0, 0, 0, 0};

  
  //Alle 4 Sensor einlesen
  for(int i = 0; i < 4; i++){
    sensorData[i] = analogRead(sensorPin[i]);  
  }

  //Prüfen, ob alle 4 Sensoren gedrückt wurden
  bool allPressed = true;

  for(int i = 0; i < 4; i++){
    if(sensorData[i] < SENSOR_THRESHOLD){ 
      allPressed = false;
      break;
    }
  }  
  
  return allPressed;
}


int readDataFromSensor(){
  //Array für Messwerte
  int sensorPin[4] = {SENSOR_RED_PIN, SENSOR_YELLOW_PIN, SENSOR_GREEN_PIN, SENSOR_BLUE_PIN};  
  int sensorData[4];

  //Alle 4 Sensor einlesen
  for(int i = 0; i < 4; i++){
    sensorData[i] = analogRead(sensorPin[i]);  
  }

  //Prüfen, welcher Sensor gedrückt wurde
  for(int i = 0; i < 4; i++){
    if(sensorData[i] > 3000){  //Schwellenwert muss noch angepasst werden
      return i;
    }
  }

  return -1;
}


//Buzzer Funktion
void startSound(int frequency, Color color){
  int channel;
  switch(color){
    case RED:
      channel = BUZZER_CHANNEL_RED;
      break;
    case YELLOW:
      channel = BUZZER_CHANNEL_YELLOW;
      break;
    case GREEN:
      channel = BUZZER_CHANNEL_GREEN;
      break;
    case BLUE:
      channel = BUZZER_CHANNEL_BLUE;
      break;
  }
  ledcWriteTone(channel, frequency);
  ledcWrite(channel, 200); 
};

void stopSound(Color color){
  int channel;
  switch(color){
    case RED:
      channel = BUZZER_CHANNEL_RED;
      break;
    case YELLOW:
      channel = BUZZER_CHANNEL_YELLOW;
      break;
    case GREEN:
      channel = BUZZER_CHANNEL_GREEN;
      break;
    case BLUE:
      channel = BUZZER_CHANNEL_BLUE;
      break;
  }
  ledcWrite(channel, 0);   
  ledcWriteTone(channel, 0);
};



// WebSocket event handler
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    //Falls die Verbindung getrennt wurde während des Spiels wird onlineState gespeichert und falls ShowSequence alles zurückgesetzt
    case WStype_DISCONNECTED: {
      Serial.println("WebSocket Disconnected");
      wsConnect = false;
      if(currentState == ONLINE_PLAYING){
        previousOnlineState = onlineState;
        currentState = ONLINE_RECONNECTING;
        onlineState = IDLE;

        FastLED.clear();
        FastLED.show();
        stepOnPhase = false;
        stepEndTime = 0;
      }
      break;
    };
    case WStype_CONNECTED: {
      Serial.println("WebSocket Connected");
      wsConnect = true;
      StaticJsonDocument<256> connectDoc;
      connectDoc["type"] = "hello";
      connectDoc["role"] = "esp";
      size_t n = serializeJson(connectDoc, wsSendBuffer);
      webSocket.sendTXT(wsSendBuffer, n);
      break;
    };
    case WStype_TEXT: {
      if(wsMessageReceived){
        //previous message not yet processed
        break;
      }

      if(length > sizeof(wsBuffer) - 1) {
        return; //message too long
      }
      memcpy(wsBuffer, payload, length);
      wsBuffer[length] = '\0';
      wsMessageReceived = true;
      break;
    };
  }
}

void handleWebSocketMessage() {
  if(wsMessageReceived) {
    wsMessageReceived = false;
    //handle message in wsBuffer
    StaticJsonDocument<768> doc;
    DeserializationError error = deserializeJson(doc, wsBuffer);
    if(error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.f_str());
      return;
    }

    //typ ist erstmal nur "command"
    const char* type = doc["type"];
    if (!type) return;

    if(strcmp(type, "command") == 0) {
      //command ist entweder "show_sequence" oder "game_over"
      //show_sequence enthält ein array "sequence"
      //game_over enthält den score
      const char* command = doc["command"];

      if (!command) return;

      if(strcmp(command, "show_sequence") == 0) {
        onlineSequence.clear();
        JsonArray seq = doc["sequence"].as<JsonArray>();
        for(JsonVariant value : seq) {
          onlineSequence.push_back(value.as<int>());
        }
        showIndex = 0;
        stepOnPhase = false;
        stepEndTime = millis();
        onlineState = SHOW_SEQUENCE;
      }
      else if(strcmp(command, "game_over") == 0) {
        score = doc["score"];
        started = false;
        currentState = GAME_OVER;
      }
    }
  }
}

Color intToColor(int value){
  switch(value){
    case 0:
      return RED;
    case 1:
      return YELLOW;
    case 2:
      return GREEN;
    case 3:
      return BLUE;
    default:
      return RED; //default case
  }
};