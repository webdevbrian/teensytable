// TeensyTable
// v1.0
// BKINNEY 9.4.2017

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <Bounce.h> // All push buttons
#include <SimpleTimer.h>
#include <Adafruit_NeoPixel.h>

AudioPlaySdWav           playSdWav2;
AudioPlaySdWav           playSdWav1;
AudioMixer4              mixer2;
AudioMixer4              mixer1;
AudioOutputI2S           i2s1;
AudioConnection          patchCord1(playSdWav2, 0, mixer1, 1);
AudioConnection          patchCord2(playSdWav2, 1, mixer2, 1);
AudioConnection          patchCord3(playSdWav1, 0, mixer1, 0);
AudioConnection          patchCord4(playSdWav1, 1, mixer2, 0);
AudioConnection          patchCord5(mixer2, 0, i2s1, 1);
AudioConnection          patchCord6(mixer1, 0, i2s1, 0);
AudioControlSGTL5000     sgtl5000_1;

// Use these with the Teensy Audio Shield
#define SDCARD_CS_PIN    10
#define SDCARD_MOSI_PIN  7
#define SDCARD_SCK_PIN   14

// LED Strip setup
#define PIN 21
#define LENGTH 1

Adafruit_NeoPixel strip = Adafruit_NeoPixel(LENGTH, PIN, NEO_GRB + NEO_KHZ800);

/*
 * Pocket triggers
 */
Bounce triggerL1 = Bounce(0, 15); // Left rail, pocket 1
Bounce triggerL2 = Bounce(1, 15); // Left rail, pocket 2
Bounce triggerL3 = Bounce(2, 15); // Left rail, pocket 3
Bounce triggerR3 = Bounce(3, 15); // Right rail, pocket 3
Bounce triggerR2 = Bounce(4, 15); // Right rail, pocket 2
Bounce triggerR1 = Bounce(5, 15); // Right rail, pocket 1

/*
 * Top control buttons
 */
Bounce buttonStart = Bounce(8, 15); // Start - Toggle through games
Bounce buttonSelect = Bounce(20, 15); // Select - Select game
//Bounce buttonA = Bounce(21, 15); // A - Multi function, based on game
//Bounce buttonB = Bounce(20, 15); // B - Multi function, based on game

/*     
 *  Pool table trigger and button layout
 * 
 *      L3  /-----------\  R3
 *         |             |  
 *         |             |
 *         |             |
 *      L2 (             ) R2
 *         |             |
 *         |             |
 *         |             |
 *      L1 \_____________/ R1      
 *      
 *       START | SEL | A | B
 */

/*
 * Global system variables & defaults
 */
SimpleTimer timer;
int systemState = 0;
int menuState = systemState;
int team1Score = 0;
int team2Score = 0;
int pixels = 1;
int frames = 7;
int fps = 3;
bool started = false;
bool paused = false;

void setup() {
  pinMode(0, INPUT_PULLUP); // Left rail pocket 1
  pinMode(1, INPUT_PULLUP); // Left rail pocket 2
  pinMode(2, INPUT_PULLUP); // Left rail pocket 3
  pinMode(3, INPUT_PULLUP); // Right rail pocket 3
  pinMode(4, INPUT_PULLUP); // Right rail pocket 2
  pinMode(5, INPUT_PULLUP); // Right rail pocket 1
  
  pinMode(8, INPUT_PULLUP); // Start
  pinMode(20, INPUT_PULLUP); // Select
  //pinMode(21, INPUT_PULLUP); // A
  //pinMode(7, INPUT_PULLUP); // B

  //strip.begin();
  //strip.show();

  Serial.begin(9600);
  AudioMemory(8);

  // Turn on audio shield, setup base mixer settings
  sgtl5000_1.enable();
  sgtl5000_1.volume(0.8);
  
  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);
  if (!(SD.begin(SDCARD_CS_PIN))) {
    while (1) {
      Serial.println("Unable to access the SD card");
      delay(500);
    }
  }

  // Update state of buttons on initialization
  triggerL1.update();
  triggerL2.update();
  triggerL3.update();
  triggerR1.update();
  triggerR2.update();
  triggerR3.update();
  
  buttonStart.update();
  buttonSelect.update();
  /*
  buttonA.update();
  buttonB.update();
  */
  
  // Set default mixer gain to 50%
  mixer1.gain(0, 0.5);
  mixer1.gain(1, 0.5);
  mixer2.gain(0, 0.5);
  mixer2.gain(1, 0.5);
  delay(1000);
}

/* == MAIN Mixer CONTROL==
 *  
 * This mixes the sound effects with background music
 * - Strictly a mixer.
 *
 */

void mixerControl() {
  int knob = analogRead(A2);
  float gain1 = (float)knob / 1023.0;
  float gain2 = 1.0 - gain1;
  mixer1.gain(0, gain1);
  mixer1.gain(1, gain2);
  mixer2.gain(0, gain1);
  mixer2.gain(1, gain2);

  delay(10);
}

/* == MAIN VOLUME CONTROL==
 *  
 * Global master volume
 * - Strictly a volume control.
 *
 */

void volumeControl() {
  int knob = analogRead(A3);
  float vol = (float)knob / 1280.0;
  sgtl5000_1.volume(vol);

  delay(10);
}

/* == BGM Handler ==
 *  
 * Handle playing background music
 * 
 * PARAMS: String of the WAV music file name before the extension
 * 
 */

void bgmHandler(String BGM) {
  String BGMFile = BGM + ".wav";
  
  // Check to see if we already have background music playing
  if (playSdWav1.isPlaying() == false) {
    Serial.println("Started playing BGM " + BGMFile+ "\n");
    playSdWav1.play(BGMFile.c_str ());
    delay(10);
  }
}

/* == SFX Handler ==
 *  
 * Handle playing SFX 
 * 
 * PARAMS: String of the WAV SFX file name before the extension
 * 
 */

void sfxHandler(String SFX) {
  String SFXFile = SFX + ".wav";

  Serial.println("Started playing SFX " + SFXFile + "\n");
  playSdWav2.play(SFXFile.c_str ());
  delay(10); // wait for library to parse WAV info
}


/*
 * LED STRIP PATTERNS
 * 24 WS2812B NEOPixels in single strip
 * 4 LEDs per pocket.
 * Pocket order: L1, L2, L3, R3, R2, R1 (see trigger layout above)
 * 
 * L1 LEDs: 1-4
 * L2 LEDs: 5-8
 * L3 LEDs: 9-12
 * R3 LEDs: 13-16
 * R2 LEDs: 17-20
 * R1 LEDs: 21-24
 * 
 * Pattern builder: https://hohmbody.com/flickerstrip/lightwork/
 * - 24 long strip @ 12 FPS @ 6 Frames
 * 
 * 
 * 1. Single pocket patterns
 * 2. Full strip patterns (all pockets)
 */

 // Single pocket patterns
 const byte data[] PROGMEM = {255,0,0,255,255,0,0,255,0,0,255,255,0,0,255,150,0,255,255,0,225};

 // Full strip patterns (all pockets)

/* == LED Strip Handler ==
 *  
 * Handle controlling the LED strip
 * 
 * PARAMS: String of the pattern name defined in the LEDStriphandler
 * 
 */
 int currentFrame = 0;
void LEDStripHandler(String Pattern) {
  /*
   * LED Patterns
   * 
   * 24 LEDs, 24FPS @ 48 Frames
   * 
   */
  
    for (int i=0; i<LENGTH; i++) {
      int pixelIndex = i % pixels;
      int index = currentFrame*pixels*3 + pixelIndex*3;
  
      strip.setPixelColor(i,pgm_read_byte_near(data+index),pgm_read_byte_near(data+index+1),pgm_read_byte_near(data+index+2));
    }
    
    strip.show();
    currentFrame ++;
    if (currentFrame >= frames) currentFrame = 0;
    Serial.print("fired LED\n");
    delay(1000/fps);

  
}

/* == Back to menu ==
 *  
 * Goes back to the main menu
 * used by all system states
 */

void goBackToMainMenu() {
  stateHandler(0);
}

/* == State Handler ==
 *  
 * Handle system states
 * 
 */
void stateHandler(int State) {

  // MAIN MENU | Default state when turned on
  while(State == 0){

    // Initialize mixer and volume controls
    mixerControl();
    volumeControl();

    //LEDStripHandler("test");

    // Stop all looping background sound SFX
    if (playSdWav1.isPlaying() == true) {
      playSdWav1.stop();
    }
    
    // Welcome message
    if(!started) {
      // Play intro only once
      sfxHandler("uistbtn");
      started = true;
    }

    // Initialize used buttons in state
    buttonStart.update();
    buttonSelect.update();

    // Handle button system state cycling
    if (buttonStart.fallingEdge()) {

      /*
       * 1. Change menuState on cycling of start button
      */

      Serial.print("menuState #");
      Serial.print(menuState);
      Serial.print("  ");

      // Start button cycles through menu options
      // sets systemState
      if (menuState < 6) {
        menuState++;
        Serial.print(menuState);
        Serial.print("  ");
      } else {
        menuState = 0;
      }

      if (menuState == 0) {
        sfxHandler("uimenu");
      } else if (menuState == 1) {
        sfxHandler("uimario");
      } else if (menuState == 2) {
        sfxHandler("uinap");
      } else if (menuState == 3) {
        sfxHandler("uiarnie");
      } else if (menuState == 4) {
        sfxHandler("uifart");
      } else if (menuState == 5) {
        Serial.print("Game 1");
      } else if (menuState == 6) {
        Serial.print("Game 2");
      } 
      
    }

    if (buttonSelect.fallingEdge()) {
      
      /*
       * 1. Read menu
       * 2. Play sfx 
       * 3. Set stateHandler on selected seiection
       * 
      */

      // Go to selected game state
      Serial.print("Select button pressed ");
      stateHandler(menuState);
      
    }
  }

  // MARIO SOUNDBOARD
  while(State == 1){

    /*
     * 1. Handle Buttons for triggers and menu button
     * 2. Handle SFX and music
     * 3. Handle LED effects
     * 4. Handle switching state back to main menu
     * 
    */

    // Initialize mixer and volume controls
    mixerControl();
    volumeControl();

    // Initialize used buttons in state
    triggerL1.update();
    triggerL2.update();
    triggerL3.update();
    triggerR1.update();
    triggerR2.update();
    triggerR3.update();
    buttonStart.update();

    // Loop soundboard background music and play at random
    if (playSdWav1.isPlaying() == false) {
      // rand 0 - 4 (not including 4)
      String rand = random(4);
      String rndBGM = "smbgm" + rand;
      
      bgmHandler(rndBGM);
      delay(10);
    }

    // Handle pocket triggers
    if (triggerL1.fallingEdge()) {
      // Play random soundboard pocket sfx
      String rand = random(11);
      String rndSFX = "sm" + rand;
      
      sfxHandler(rndSFX);

      Serial.print("L1 button pressed ");
    }

    // Handle system pause state to go back to main menu
    if (buttonStart.fallingEdge()) {
      playSdWav1.stop();
      stateHandler(0);
    }

    //LEDStripHandler("redAntCrawl");
  }

  // NAPOLEON SOUNDBOARD
  while(State == 2){

    /*
     * 1. Handle Buttons for triggers and menu button
     * 2. Handle SFX and music
     * 3. Handle LED effects
     * 4. Handle switching state back to main menu
     * 
    */

    
  
    // Initialize mixer and volume controls
    mixerControl();
    volumeControl();
  
    triggerL1.update();
    triggerL2.update();
    triggerL3.update();
    triggerR1.update();
    triggerR2.update();
    triggerR3.update();
    buttonStart.update();
  
    // Loop soundboard background music and play at random
    if (playSdWav1.isPlaying() == false) {
      bgmHandler("ndbgm");
      delay(10);
    }
  
    // Handle pocket triggers
    if (triggerL1.fallingEdge()) {
      // Play random soundboard pocket sfx
      String rand = random(7);
      String rndSFX = "nd" + rand;
      
      sfxHandler(rndSFX);
  
      Serial.print("L1 button pressed ");
    }

    // Handle system pause state to go back to main menu
    if (buttonStart.fallingEdge()) {
      playSdWav1.stop();
      stateHandler(0);
    }

  }
  
  // ARNIE SOUNDBOARD
  while(State == 3){

    // Initialize mixer and volume controls
    mixerControl();
    volumeControl();
  
    triggerL1.update();
    triggerL2.update();
    triggerL3.update();
    triggerR1.update();
    triggerR2.update();
    triggerR3.update();
    buttonStart.update();
  
    // Loop soundboard background music and play at random
    if (playSdWav1.isPlaying() == false) {    
      bgmHandler("asbgm");
      delay(10);
    }
  
    // Handle pocket triggers
    if (triggerL1.fallingEdge()) {
      // Play random soundboard pocket sfx
      String rand = random(4);
      String rndSFX = "asp" + rand;
      
      sfxHandler(rndSFX);
  
      Serial.print("L1 button pressed ");
    }

    // Handle system pause state to go back to main menu
    if (buttonStart.fallingEdge()) {
      playSdWav1.stop();
      stateHandler(0);
    }

  }

  // FART SOUNDBOARD
  while(State == 4){

    // Initialize mixer and volume controls
    mixerControl();
    volumeControl();
  
    triggerL1.update();
    triggerL2.update();
    triggerL3.update();
    triggerR1.update();
    triggerR2.update();
    triggerR3.update();
    buttonStart.update();
  
    // Loop soundboard background music and play at random
    if (playSdWav1.isPlaying() == false) {
      String rand = random(3);
      String rndBGM = "fartbgm" + rand;
      
      bgmHandler(rndBGM);
    }
  
    // Handle pocket triggers
    if (triggerL1.fallingEdge()) {
      // Play random soundboard pocket sfx
      String rand = random(14);
      String rndSFX = "fart" + rand;
      
      sfxHandler(rndSFX);
  
      Serial.print("L1 button pressed ");
    }

    // Handle system pause state to go back to main menu
    if (buttonStart.fallingEdge()) {
      playSdWav1.stop();
      stateHandler(0); // Go to main menu
    }

  }


  // GAME 1
  while(State == 5){

    // Initialize mixer and volume controls
    mixerControl();
    volumeControl();
  
    buttonStart.update();
    
    // Handle system pause state to go back to main menu
    if (buttonStart.fallingEdge()) {
      playSdWav1.stop();
      stateHandler(0);
    }

  }

  // GAME 2
  while(State == 6){
  
    // Initialize mixer and volume controls
    mixerControl();
    volumeControl();
  
    buttonStart.update();
    
    // Handle system pause state to go back to main menu
    if (buttonStart.fallingEdge()) {
      playSdWav1.stop();
      stateHandler(0);
    }
  }
}

/*
              _           __                   
  /\/\   __ _(_)_ __     / /  ___   ___  _ __  
 /    \ / _` | | '_ \   / /  / _ \ / _ \| '_ \ 
/ /\/\ \ (_| | | | | | / /__| (_) | (_) | |_) |
\/    \/\__,_|_|_| |_| \____/\___/ \___/| .__/ 
                                        |_|    
 */

void loop() {
  // Set system state to MAINMENU on startup
  stateHandler(systemState);
}
