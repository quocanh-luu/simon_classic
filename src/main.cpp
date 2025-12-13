#include <Arduino.h>
#include <vector>
#include <FastLED.h>

#define LED_RED_PIN 2
#define LED_YELLOW_PIN 3
#define LED_GREEN_PIN 4
#define LED_BLUE_PIN 5

#define NUM_LEDS 200

#define SENSOR_RED_PIN 12
#define SENSOR_YELLOW_PIN 13
#define SENSOR_GREEN_PIN 14
#define SENSOR_BLUE_PIN 15

const int SENSOR_THRESHOLD = 1000;

using namespace std;

CRGB Red[NUM_LEDS];
CRGB Yellow[NUM_LEDS];
CRGB Green[NUM_LEDS];
CRGB Blue[NUM_LEDS]; 

enum GameState {
  WAITING,
  PLAYING,
  GAME_OVER
};

enum Color {
  RED,
  YELLOW,
  GREEN,
  BLUE
};

GameState currentState = WAITING;

// put function declarations here:
void welcomeEffect();
int play();
void show(std::vector<Color> &playlist);
bool checkInput(std::vector<Color> &playlist, std::vector<Color>::iterator &it);
void nextRound(std::vector<Color> &playlist, std::vector<Color>::iterator &it);
bool startGame();
int readDataFromSensor();


void setup() {
  // put your setup code here, to run once:
  FastLED.addLeds<WS2812B, LED_RED_PIN, GRB>(Red, NUM_LEDS);
  FastLED.addLeds<WS2812B, LED_YELLOW_PIN, GRB>(Yellow, NUM_LEDS);
  FastLED.addLeds<WS2812B, LED_GREEN_PIN, GRB>(Green, NUM_LEDS);
  FastLED.addLeds<WS2812B, LED_BLUE_PIN, GRB>(Blue, NUM_LEDS);
}

void loop() {
  // put your main code here, to run repeatedly:
  while(currentState == WAITING){
    if(startGame()){
      currentState = PLAYING;
    }
  }

  if(currentState == PLAYING){
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
  vector<Color> playlist;
  int counter = 0;
  int score = 0;
  vector<Color>::iterator it;

  while(true){
    nextRound(playlist, it);
    show(playlist);
    while(it != playlist.end()){
      if(!checkInput(playlist, it)){
        //Game over
        score = counter;
        counter = 0;
        playlist.clear();
        return score;
      }
      it++;
    }
    counter++;
  }

};


void show(vector<Color> &playlist){
  for(auto color : playlist){
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


bool checkInput(vector<Color> &playlist, vector<Color>::iterator &it){
  int sensorIndex = -1;

  while (sensorIndex == -1){
    sensorIndex = readDataFromSensor();
    delay(5);
  }

  return static_cast<Color>(sensorIndex) == *it;
};


void nextRound(vector<Color> &playlist, vector<Color>::iterator &it){
  playlist.push_back(static_cast<Color>(esp_random() % 4));
  it = playlist.begin();
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
    if(sensorData[i] > 1000){  //Schwellenwert muss noch angepasst werden
      return i;
    }
  }

  return -1;
}