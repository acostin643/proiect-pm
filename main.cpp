#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

// --- PINI CONFIGURARE HARDWARE ---
#define TFT_CS   10
#define TFT_DC   8
#define TFT_RST  3
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

const int rowPins[4] = {7, 6, 5, 4};
const int colPins[4] = {A2, A3, A4, A5};
const int buzzerPin = 9;
const int potPin = A0;

// --- MATRICEA DE NOTE ȘI DENUMIRI ---
float noteFreqs[4][4] = {
  {261.63, 293.66, 329.63, 0}, // 1=Do,  2=Re,  3=Mi,  A=Efect 1
  {349.23, 392.00, 440.00, 0}, // 4=Fa,  5=Sol, 6=La,  B=Efect 2
  {493.88, 523.25, 587.33, 0}, // 7=Si,  8=Do5, 9=Re5, C=Efect 3
  {659.25, 698.46, 783.99, 0}  // *=Mi5, 0=Fa5, #=Sol5, D=Efect 4
};

const char* noteNames[4][4] = {
  {"DO", "RE", "MI", ""},
  {"FA", "SOL", "LA", ""},
  {"SI", "DO+", "RE+", ""},
  {"MI+", "FA+", "SOL+", ""}
};

// --- VARIABILE DE STARE ---
int effectMode = 0; // 0=Normal, 1=Vibrato, 2=Pitch Bend, 3=Tremolo
String effectName = "NORMAL";
String lastNoteName = "NONE";
int lastPotValue = -1;

void updateDisplay(String note, bool force = false) {
  tft.setTextSize(3);
  
  // Actualizare Efect
  tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
  tft.setCursor(20, 80);
  tft.print("EFECT: ");
  tft.print(effectName);
  tft.print("        "); // Spații pentru curățarea textului vechi

  // Actualizare Notă
  if (note != lastNoteName || force) {
    tft.setCursor(20, 130);
    tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
    tft.print("NOTA: ");
    tft.print(note);
    tft.print("        ");
    lastNoteName = note;
  }

  // Actualizare Potențiometru
  int currentPot = analogRead(potPin);
  if (abs(currentPot - lastPotValue) > 5 || force) {
    tft.setCursor(20, 180);
    tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
    tft.print("POT: ");
    tft.print(currentPot);
    tft.print("    ");
    lastPotValue = currentPot;
  }
}

void setup() {
  Serial.begin(9600);
  
  // Inițializare pini tastatură
  for (int i = 0; i < 4; i++) {
    pinMode(rowPins[i], OUTPUT);
    digitalWrite(rowPins[i], HIGH);
    pinMode(colPins[i], INPUT_PULLUP);
  }
  
  pinMode(buzzerPin, OUTPUT);

  // Inițializare Ecran (Mod Landscape stabil)
  tft.begin();
  tft.setRotation(1); 
  tft.fillScreen(ILI9341_BLACK);
  
  // Header static
  tft.setTextColor(ILI9341_RED);
  tft.setTextSize(3);
  tft.setCursor(20, 20);
  tft.print("SINTETIZATOR 335CA");
  
  // Desenare interfață inițială
  updateDisplay("NONE", true);
}

void loop() {
  bool keyPressed = false;
  float currentFreq = 0;
  String currentNoteName = "NONE";
  int potVal = analogRead(potPin);

  // Scanare tastatură matriceală
  for (int r = 0; r < 4; r++) {
    digitalWrite(rowPins[r], LOW);
    for (int c = 0; c < 4; c++) {
      if (digitalRead(colPins[c]) == LOW) {
        
        // Dacă s-a apăsat o tastă de efect (Coloana 4: A, B, C, D)
        if (c == 3) {
          effectMode = r;
          if (effectMode == 0) effectName = "NORMAL";
          if (effectMode == 1) effectName = "VIBRATO";
          if (effectMode == 2) effectName = "PITCH B.";
          if (effectMode == 3) effectName = "TREMOLO";
          updateDisplay(lastNoteName, true);
          delay(200); // Debounce pentru tasta de efect
        } 
        // Dacă s-a apăsat o notă muzicală
        else {
          currentFreq = noteFreqs[r][c];
          currentNoteName = noteNames[r][c];
          keyPressed = true;
        }
      }
    }
    digitalWrite(rowPins[r], HIGH);
  }

  // Actualizare dinamică ecran în timp ce se cântă
  updateDisplay(currentNoteName);

  // Aplicare efecte audio în timp real
  if (keyPressed && currentFreq > 0) {
    float finalFreq = currentFreq;
    
    switch (effectMode) {
      case 0: // NORMAL
        // Potențiometrul controlează o ușoară deviație sau rămâne sunet pur
        finalFreq = currentFreq;
        tone(buzzerPin, finalFreq);
        break;
        
      case 1: // VIBRATO
        // Modulație de frecvență folosind o funcție sinus raportată la timp
        // Potențiometrul controlează intensitatea/adâncimea vibratoului
        {
          float vibratoDepth = map(potVal, 0, 1023, 0, 50);
          finalFreq = currentFreq + sin(millis() * 0.05) * vibratoDepth;
          tone(buzzerPin, finalFreq);
        }
        break;
        
      case 2: // PITCH BEND
        // Potențiometrul modifică nota de la o octavă mai jos până la una mai sus (0.5x - 2.0x)
        {
          float multiplier = map(potVal, 0, 1023, 50, 200) / 100.0;
          finalFreq = currentFreq * multiplier;
          tone(buzzerPin, finalFreq);
        }
        break;
        
      case 3: // TREMOLO (Întrerupere rapidă a sunetului)
        // Potențiometrul reglează viteza efectului de tremolo
        {
          int speed = map(potVal, 0, 1023, 150, 10);
          tone(buzzerPin, currentFreq);
          delay(speed);
          noTone(buzzerPin);
          delay(speed);
        }
        break;
    }
  } else {
    noTone(buzzerPin); // Oprim sunetul dacă nicio tastă de notă nu e apăsată
  }

  delay(10); // Stabilitate procesor
}