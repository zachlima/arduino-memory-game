// Memory game. Watch the pattern, repeat it, one extra step each round.
// A wrong press prints "player,score" to serial for listener.py.

const int MAX_ROUNDS = 50;

int ledPins[3] = { 13, 10, 7 };
int buttonPins[3] = { 12, 9, 6 };
int laneTones[3] = { 523, 587, 659 };  // C, D, E
int buzzer = 3;

int resetButtonPin = 4;
int resetLedPin = 5;
bool forceRestart = false;

int sequence[MAX_ROUNDS];
int currentRound = 3;
int currentPlayer = 0;  // set once at startup by selectPlayer()

int wrong = 1000;
int ready = 440;  // distinct tone used only for the "get ready" cue

void setup() {
  Serial.begin(9600);
  for (int i = 0; i < 3; i++) {
    pinMode(ledPins[i], OUTPUT);
    pinMode(buttonPins[i], INPUT_PULLUP);
  }
  pinMode(resetButtonPin, INPUT_PULLUP);
  pinMode(resetLedPin, OUTPUT);
  randomSeed(analogRead(A0));

  startupSequence();

  currentPlayer = selectPlayer();
  fillNewSequence();
  playReadyCue();
}

// Plays once at power-on
void startupSequence() {
  int chatterNotes[2] = { 900, 700 };

  for (int i = 0; i < 12; i++) {
    tone(buzzer, chatterNotes[i % 2]);
    delay(40);
    noTone(buzzer);
    delay(20);
  }

  delay(200);

  int stabNotes[2] = { 220, 262 };
  for (int i = 0; i < 2; i++) {
    digitalWrite(ledPins[i], HIGH);
    tone(buzzer, stabNotes[i]);
    delay(120);
    digitalWrite(ledPins[i], LOW);
    noTone(buzzer);
    delay(60);
  }

  for (int i = 0; i < 3; i++) {
    digitalWrite(ledPins[i], HIGH);
  }
  tone(buzzer, 330);
  delay(500);
  for (int i = 0; i < 3; i++) {
    digitalWrite(ledPins[i], LOW);
  }
  noTone(buzzer);
  delay(300);
}

// Plays a sequence of tones and pause so the player can tell
// selection has ended before the first pattern starts.
void playReadyCue() {
  delay(1000);
  tone(buzzer, ready);
  delay(300);
  noTone(buzzer);
  delay(1000);
}

// fill once per game. loop() plays the first currentRound entries, so
// each round is the last one plus a step
void fillNewSequence() {
  for (int i = 0; i < MAX_ROUNDS; i++) {
    sequence[i] = random(0, 3);
  }
}

// Turns off all LEDs when partway through a game.
void allOff() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(ledPins[i], LOW);
  }
  noTone(buzzer);
}

// wait for a button press to pick the player, blink to confirm, return 1-3
int selectPlayer() {
  while (true) {
    for (int j = 0; j < 3; j++) {
      if (digitalRead(buttonPins[j]) == LOW) {
        delay(25); // debounce

        if (digitalRead(buttonPins[j]) == LOW) {
          for (int k = 0; k < 2; k++) {
            digitalWrite(ledPins[j], HIGH);
            delay(150);
            digitalWrite(ledPins[j], LOW);
            delay(150);
          }

          // wait for release so it isn't read twice
          while (digitalRead(buttonPins[j]) == LOW) {
            delay(25);
          }

          return j + 1; // j=0 -> Player 1, j=1 -> Player 2, j=2 -> Player 3
        }
      }
    }
  }
}

// Checks the reset button. A single press (debounced) triggers
// a full reset back to player selection.
void checkResetButton() {
  if (digitalRead(resetButtonPin) == LOW) {
    delay(25); // debounce
    if (digitalRead(resetButtonPin) == LOW) {
      while (digitalRead(resetButtonPin) == LOW) {
        delay(10);
      }
      fullReset();
    }
  }
}

// delay() that still polls the reset button. returns true if a reset
// fired, in which case just return out of loop()
bool waitOrReset(int ms) {
  for (int elapsed = 0; elapsed < ms; elapsed += 5) {
    checkResetButton();
    if (forceRestart) {
      return true;
    }
    delay(5);
  }
  return false;
}

// don't let a button held over from playback count as an answer
bool waitForAllButtonsReleased() {
  while (true) {
    bool anyHeld = false;
    for (int j = 0; j < 3; j++) {
      if (digitalRead(buttonPins[j]) == LOW) {
        anyHeld = true;
      }
    }
    if (!anyHeld) {
      return false;
    }
    if (waitOrReset(10)) {
      return true;
    }
  }
}
// When reset button is pressed, flashes the reset LED, resets round
// counter, and resets from player selection.
void fullReset() {
  allOff();

  for (int k = 0; k < 4; k++) {
    digitalWrite(resetLedPin, HIGH);
    delay(100);
    digitalWrite(resetLedPin, LOW);
    delay(100);
  }

  startupSequence();

  currentRound = 3;
  currentPlayer = selectPlayer();
  fillNewSequence();
  playReadyCue();

  forceRestart = true;
}

void loop() {
  // reset may have fired last pass. cleared here so unwinds can just return
  if (forceRestart) {
    forceRestart = false;
    allOff();
  }

  // play back the first currentRound steps
  for (int i = 0; i < currentRound; i++) {
    int lane = sequence[i];

    digitalWrite(ledPins[lane], HIGH);
    tone(buzzer, laneTones[lane]);
    bool interrupted = waitOrReset(500);

    digitalWrite(ledPins[lane], LOW);
    noTone(buzzer);
    if (interrupted || waitOrReset(500)) {
      return;
    }
  }

  if (waitForAllButtonsReleased()) {
    return;
  }

  for (int i = 0; i < currentRound; i++) {
    int correctLane = sequence[i];
    bool answered = false;

    while (!answered) {
      checkResetButton();
      if (forceRestart) {
        break;
      }

      for (int j = 0; j < 3; j++) {
        if (digitalRead(buttonPins[j]) != LOW) {
          continue;
        }

        delay(25); // debounce
        if (digitalRead(buttonPins[j]) != LOW) {
          continue;
        }

        if (j == correctLane) {
          digitalWrite(ledPins[j], HIGH);
          tone(buzzer, laneTones[j]);
          bool interrupted = waitOrReset(500);

          digitalWrite(ledPins[j], LOW);
          noTone(buzzer);
          if (interrupted || waitOrReset(500)) {
            break;
          }

          answered = true;
          while (digitalRead(buttonPins[j]) == LOW) {
            if (waitOrReset(25)) {
              break;
            }
          }
          break;
        }

        // currentRound starts at 3, so -3 is rounds survived (first loss = 0)
        Serial.print(currentPlayer);
        Serial.print(",");
        Serial.println(currentRound - 3);

        if (!blinkAllLeds()) {
          waitOrReset(1000);
        }
        currentRound = 3;
        fillNewSequence();
        waitForAllButtonsReleased();
        return;
      }
    }

    if (forceRestart) {
      return; // abandons this round; loop() restarts fresh above
    }
  }

  if (waitOrReset(1000)) {
    return;
  }
  if (currentRound < MAX_ROUNDS) {
    currentRound++;
  }
}

// game over flash. true if a reset cut it short
bool blinkAllLeds() {
  for (int i = 0; i < 5; i++) {
    for (int j = 0; j < 3; j++) {
      digitalWrite(ledPins[j], HIGH);
    }
    tone(buzzer, wrong);
    bool interrupted = waitOrReset(300);

    for (int j = 0; j < 3; j++) {
      digitalWrite(ledPins[j], LOW);
    }
    noTone(buzzer);
    if (interrupted || waitOrReset(300)) {
      return true;
    }
  }
  return false;
}
