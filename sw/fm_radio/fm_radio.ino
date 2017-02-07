/* 
 *  An Arduino sketch to operate a RDA5807M chip based radio using the Radio library of Matthias Hertel, http://www.mathertel.de.
 * This sketch implements a fixed radio station. The volume can be controlled using a rotary encoder.
 *
 * Bert Outtier
 * Copyright (c) 2017 by Bert Outtier.
 */

#include <Arduino.h>
#include <Wire.h>
#include <radio.h>
#include <RDA5807M.h>
#include <EEPROM.h>

// ID of the settings block
#define CONFIG_VERSION "ls1"

// Tell it where to store your config data in EEPROM
#define CONFIG_START 32

// Example settings structure
struct StoreStruct {
  // This is for mere detection if they are your settings
  char version[4];
  // The variables of your settings
  int encoder0Pos; // encoder value (0 to VOLUME_LEVELS*PULSE_PER_VOLUME)
  int currentRadioState; // is the radio on or off?
} storage = {
  CONFIG_VERSION,
  // The default values
  0,
  HIGH
};

// ----- Fixed settings -----
#define FIX_BAND          RADIO_BAND_FM   // The band that will be tuned by this sketch is FM.
#define FIX_STATION       8800            // The station that will be tuned by this sketch is 89.30 MHz.
#define AMP_ENABLE_PIN    A1              // Amp enable pin (A1)
#define ENCODER_A         3               // Encoder pin A INT1
#define ENCODER_B         A2              // Encoder pin B (A2)
#define ENCODER_BUTTON    2               // Encoder button INT0
#define VOLUME_LEVELS     16              // Maximum number of volume levels
#define PULSE_PER_VOLUME  1               // pulses per volume up/down
#define LED_PIN           A0              // indicator LED pin (A0)     
#define OLED_ENABLE_PIN   A3              // enable OLED screen (A3)

int currentVolume;              // current volume value    
int previousAmpState;           // previous amp state (enabled/disabled)
int currentAmpState;            // current amp state (enabled/disabled)
int previousButtonState = LOW;  // Previous encoder button state
int currentButtonState = LOW;   // Current encoder button state
int previousRadioState;         // debounced previous button state (pressed/not pressed)
long time = 0;                  // the last time the encoder button pin was toggled
long debounce = 100;            // the debounce time

RDA5807M radio;    // Create an instance of Class for RDA5807M Chip

void buttonInterruptHandler()
{
  time = millis();
  currentButtonState = digitalRead(ENCODER_BUTTON);
}

void encoderInterruptHandler()
{
  if (digitalRead(ENCODER_B) == LOW) {
    if(storage.encoder0Pos > 0) storage.encoder0Pos--;
  } else {
    if(storage.encoder0Pos < (VOLUME_LEVELS*PULSE_PER_VOLUME)) storage.encoder0Pos++;
  }
}

void loadConfig() {
  // To make sure there are settings, and they are YOURS!
  // If nothing is found it will use the default settings.
  if (EEPROM.read(CONFIG_START + 0) == CONFIG_VERSION[0] &&
      EEPROM.read(CONFIG_START + 1) == CONFIG_VERSION[1] &&
      EEPROM.read(CONFIG_START + 2) == CONFIG_VERSION[2])
    for (unsigned int t=0; t<sizeof(storage); t++)
      *((char*)&storage + t) = EEPROM.read(CONFIG_START + t);
}

void saveConfig() {
  for (unsigned int t=0; t<sizeof(storage); t++)
    EEPROM.write(CONFIG_START + t, *((char*)&storage + t));
}

/// Setup a FM only radio configuration
/// with some debugging on the Serial port
void setup() {
  
  // open the Serial port
  Serial.begin(9600);
  Serial.println("Radio...");
  delay(200);

  //setup pins
  pinMode (OLED_ENABLE_PIN, OUTPUT);
  pinMode (AMP_ENABLE_PIN, OUTPUT);
  pinMode (LED_PIN, OUTPUT);
  pinMode (ENCODER_A,INPUT_PULLUP);
  pinMode (ENCODER_B,INPUT_PULLUP);
  pinMode (ENCODER_BUTTON,INPUT_PULLUP);

  // attach interrupts
  attachInterrupt(digitalPinToInterrupt(ENCODER_BUTTON), buttonInterruptHandler, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_A), encoderInterruptHandler, RISING);

  // load the settings
  loadConfig();

  // set volume to the saved setting
  currentVolume = storage.encoder0Pos/PULSE_PER_VOLUME;  
  if(storage.currentRadioState && currentVolume > 0){
    currentAmpState = HIGH;
  }
  else {
    currentAmpState = LOW;
  }
  previousAmpState = currentAmpState;
  previousRadioState = storage.currentRadioState;

  // set start state
  digitalWrite(OLED_ENABLE_PIN, HIGH); // enable OLED screen
  digitalWrite(AMP_ENABLE_PIN, currentAmpState); // enable amp
  digitalWrite(LED_PIN, currentAmpState); // enable led
  

  // Initialize the Radio 
  radio.init();

  // Enable information to the Serial port
  //radio.debugEnable();

  // Set all radio setting to the fixed values.
  radio.setBandFrequency(FIX_BAND, FIX_STATION);
  radio.setVolume(currentVolume);
  radio.setMono(false);
  radio.setMute(false);

  Serial.println("Init done!");
  
} // setup

void loop() {
//  char s[12];
//  radio.formatFrequency(s, sizeof(s));
//  Serial.print("Station:"); 
//  Serial.println(s);
//  
//  Serial.print("Radio:"); 
//  radio.debugRadioInfo();
//  
//  Serial.print("Audio:"); 
//  radio.debugAudioInfo();
  bool dirty = false;

  // Calculate the volume
  if((storage.encoder0Pos/PULSE_PER_VOLUME) != currentVolume) {
    currentVolume = storage.encoder0Pos/PULSE_PER_VOLUME;
    dirty = true;
    Serial.print("Set volume to ");
    Serial.println(currentVolume, DEC);
    radio.setVolume(currentVolume);
    if(currentVolume > 0) {
      currentAmpState = HIGH;
      storage.currentRadioState = HIGH;
    }
  }


  if (((millis() - time) > debounce) && (previousButtonState != currentButtonState)) {
    // whatever the reading is at, it's been there for longer
    // than the debounce delay, so take it as the actual current state:
    previousButtonState = currentButtonState;

    // only toggle the LED if the new button state is HIGH
    if (currentButtonState == LOW) {
      storage.currentRadioState = !storage.currentRadioState;
      Serial.print("Set radio state: ");
      Serial.println(storage.currentRadioState, DEC);
      dirty = true;
    }
  }
  
  if(storage.currentRadioState && currentVolume > 0) {
    currentAmpState = HIGH;
  }
  else {
    currentAmpState = LOW;
  }

  if(previousAmpState != currentAmpState){
    Serial.print("Set amp: ");
    Serial.println(currentAmpState, DEC);
    digitalWrite(AMP_ENABLE_PIN, currentAmpState);
    digitalWrite(LED_PIN, currentAmpState);
    previousAmpState = currentAmpState;
  }

  if(dirty){
    Serial.println("saving...");
    saveConfig();
    Serial.println("save done!");
  }
} // loop

// End.
