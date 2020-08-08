#include "Adafruit_FONA.h"

#define FONA_RX 3
#define FONA_TX 4
#define FONA_RST 5

// this is a large buffer for replies
char replybuffer[255];

// this is the phone number
char phoneNumber[30];

// We default to using software serial. If you want to use hardware serial
// (because softserial isnt supported) comment out the following three lines
// and uncomment the HardwareSerial line
#include <SoftwareSerial.h>
SoftwareSerial fonaSS = SoftwareSerial(FONA_TX, FONA_RX);
SoftwareSerial *fonaSerial = &fonaSS;

// Use this one for FONA 3G
Adafruit_FONA_LTE fona = Adafruit_FONA_LTE(FONA_RST);

uint8_t type;

// set pin numbers
const int hookPin = 7;
const int rotaryIsDialingPin = 8;
const int rotaryPulsePin = 9;
const int ringPin = 12;

// constants
const int debounceDelay = 5;

// vars
int needToPrint = 0;
int count;
int startedDialing = 0;
int digits = 0;
int callInProgress = 0;
int lastState = LOW;
int trueState = LOW;
long lastStateChangeTime = 0;
int cleared = 0;
int startedPlayingDialTone = 0;
int stoppedPlayingDialTone = 1;
int hookState = 1;
volatile int isRinging = 0;

/**
 * Setup function.
 */
void setup() {

  Serial.begin(115200);
  fonaSerial->begin(4800);

  if (! fona.begin(*fonaSerial)) {
    Serial.println(F("Couldn't find FONA"));
    while (1);
  }

  type = fona.type();
  Serial.println(F("FONA is OK"));
  Serial.print(F("Found "));

  switch (type) {
    case FONA3G_A:
      Serial.println(F("FONA 3G (American)")); break;
    default:
      Serial.println(F("???")); break;
  }

  // Print module IMEI number.
  char imei[15] = {0}; // MUST use a 16 character buffer for IMEI!
  uint8_t imeiLen = fona.getIMEI(imei);

  if (imeiLen > 0) {
    Serial.print("Module IMEI: "); Serial.println(imei);
  }

  // Setup Pin Modes
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(2, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(2), ring, FALLING);
  digitalWrite(2, HIGH);
  pinMode(hookPin, INPUT);
  digitalWrite(hookPin, HIGH);
  pinMode(rotaryIsDialingPin, INPUT);
  digitalWrite(rotaryIsDialingPin, HIGH);
  pinMode(rotaryPulsePin, INPUT);
  digitalWrite(rotaryPulsePin, HIGH);
  pinMode(ringPin, OUTPUT);

  // Wait for 1 second.
  delay(1000);

  // set to ext speaker
  fona.sendCheckReply(F("AT+CSDVC=3,1"), F("OK"), 500);
  // set echo canceller  
  fona.sendCheckReply(F("AT+CECM=7"), F("OK"), 500);

  // set volume
  fona.sendCheckReply(F("AT+CLVL=1"), F("OK"), 500);
  fona.sendCheckReply(F("AT+CFGRI=0,1"), F("OK"), 500);
  
  // battery check
  fona.sendCheckReply(F("AT+CBC"), F("OK"), 500);

  //reset ri pin
  fona.sendCheckReply(F("AT+CRIRS"), F("OK"), 500);

  // set silent mode to stop ring thru speaker
  fona.sendCheckReply(F("AT+CALM=1"), F("OK"), 500);

  // set sidetone attenuation
  fona.sendCheckReply(F("AT+SIDET=1000"), F("OK"), 500);

  // initialize isRinging
  isRinging = 0;
}

/**
 * The main function.
 * 
*/

void loop() {

  hookState = digitalRead(hookPin);
  int rotaryIsNotDialingState = digitalRead(rotaryIsDialingPin);
  int rotaryPulseState = digitalRead(rotaryPulsePin);

  // if it's ringing and the hook is down and no call is in progress
  if ((hookState == HIGH) && (callInProgress == 0) && (isRinging == 1)) {
    Serial.println(hookState);
    Serial.println(isRinging);
    ringBell();
  }

  if (isRinging == 1 && hookState == LOW) {
    callInProgress = 1;
    isRinging = 0;
    fona.sendCheckReply(F("ATA"), F("OK"), 500);
    fona.sendCheckReply(F("AT+CRIRS"), F("OK"), 500);
  }

  // if hook is down and a call is in progress, end the call
  if ((hookState == 1) && (callInProgress == 1)) {
    // end call
    fona.sendCheckReply(F("AT+CHUP"), F("OK"), 1000);
    Serial.println("end call");
    callInProgress = 0;
    startedDialing = 0;
    startedPlayingDialTone = 0;
    digits = 0;
    isRinging = 0;
  }

  // if hook is down and there is no call in progress
  if ((hookState == 1) && (callInProgress == 0)) {
    // they hung up before dialing or haven't yet started dialing
    // if haven't already stopped dial tone, stop it now.
    if (!stoppedPlayingDialTone) {
      // stop playing dial tone
      fona.sendCheckReply(F("AT+CPTONEEXT"), F("OK"), 1000);
      Serial.println("hung up, stop playing dial tone");

      stoppedPlayingDialTone = 1;
      startedPlayingDialTone = 0;
      startedDialing = 0;
      digits = 0;
    }
  }

  if ((hookState == 0) && (callInProgress == 1)) {
    Serial.println("Call in progress");
  }

  if (hookState == 0 && callInProgress == 0) {

    // handset is off the hook
    // play dial tone
    if (!startedDialing) {
      if (!startedPlayingDialTone) {
        Serial.println("play dial tone");
        fona.sendCheckReply(F("AT+CPTONEEXT=2"), F("OK"), 1000);
        startedPlayingDialTone = 1;
        stoppedPlayingDialTone = 0;
        digits = 0;
      }
    }

    if (!rotaryIsNotDialingState && (!startedDialing)) {
      fona.sendCheckReply(F("AT+CPTONEEXT"), F("OK"), 1000);
      Serial.println("Started dialing");
      startedDialing = 1;
    }

    if (rotaryIsNotDialingState) {
      // the phone is not dialing

      if (digits == 11) {
        // call the number
        int i;
        for (i = 0; i < 12; i = i + 1) {
          Serial.print(phoneNumber[i]);
        }
        if (!fona.callPhone(phoneNumber)) {
          Serial.println(F("Failed"));
        } else {
          Serial.println(F("Sent!"));
        }
        Serial.println("Call the number");
        // set sidetone vol
        fona.sendCheckReply(F("AT+SIDET=4000"), F("OK"), 1000);
        fona.sendCheckReply(F("AT+CLVL=1"), F("OK"), 1000);
        // after the call is placed, set callInProgress to true
        callInProgress = 1;
        digits = 0;
      }

      if (needToPrint) {
        // if it's only just finished being dialed, we need to send the number down the serial
        // line and reset the count. We mod the count by 10 because '0' will send 10 pulses.
        phoneNumber[digits] = char((count % 10) + 48);
        // add the number to the correct point in an array

        needToPrint = 0;
        count = 0;
        cleared = 0;
        digits++;
      }
    }

    if (rotaryPulseState != lastState) {
      lastStateChangeTime = millis();
    }
    if ((millis() - lastStateChangeTime) > debounceDelay) {
      // debounce - this happens once it's stablized
      if (rotaryPulseState != trueState) {
        // this means that the switch has either just gone from closed->open or vice versa.
        trueState = rotaryPulseState;
        if (trueState == HIGH) {
          // increment the count of pulses if it's gone high.
          count++;
          needToPrint = 1; // we'll need to print this number (once the dial has finished rotating)
        }
      }
    }
    lastState = rotaryPulseState;
  }
}

void ring() {
  // see if it's already ringing
  if (isRinging == 1) {
    return;
  } else {
    // otherwise if the hook is down and no call is in progress, set isringing to true
    if ((hookState == 1) && (callInProgress == 0)) {
      isRinging = 1;
    }
  }
}

void ringBell() {

  for (int i=0; i < 40; i++) {
    if (digitalRead(hookPin)) {
      digitalWrite(ringPin, HIGH);
      delay(20);
    } else {
      digitalWrite(ringPin, LOW);
      return;
    }
    if (digitalRead(hookPin)) {
    digitalWrite(ringPin, LOW);
    delay(30);
    } else {
      digitalWrite(ringPin, LOW);
      return;
    }
  }
  for (int i=0; i < 8; i++) {
    if (digitalRead(hookPin)) {
    delay(500);
    } else {
       digitalWrite(ringPin, LOW);
      return;
    }
  }
}



