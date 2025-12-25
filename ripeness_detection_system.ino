#include <Wire.h>
#include "Adafruit_AS726x.h"
#include "Adafruit_TCS34725.h"
#include "rgb_lcd.h"  // Grove RGB LCD

#define BUZZER_PIN 8
const int sampleSize = 5;

Adafruit_AS726x as7263;
Adafruit_TCS34725 tcs = Adafruit_TCS34725(
  TCS34725_INTEGRATIONTIME_300MS,
  TCS34725_GAIN_1X
);
rgb_lcd lcd;

// Circular buffers
float redNIR_values[sampleSize];
float blueNIR_values[sampleSize];
float greenNIR_values[sampleSize];
float violetNIR_values[sampleSize];
uint16_t r_values[sampleSize];
uint16_t g_values[sampleSize];
uint16_t b_values[sampleSize];
uint16_t lux_values[sampleSize];

int sampleIndex = 0;
bool bufferFilled = false;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  if (!as7263.begin()) {
    Serial.println("AS7263 not detected!");
    while (1);
  }
  Serial.println("AS7263 detected!");

  if (!tcs.begin()) {
    Serial.println("TCS34725 not detected!");
    while (1);
  }
  Serial.println("TCS34725 detected!");

  // LCD Init
  lcd.begin(16, 2);
  lcd.setRGB(0, 0, 255);
  lcd.print("Detecting Color");
}

void beep(int count, int duration = 100, int gap = 100) {
  for (int i = 0; i < count; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(duration);
    digitalWrite(BUZZER_PIN, LOW);
    delay(gap);
  }
}

void loop() {
  // --- Read AS7263 ---
  as7263.startMeasurement();
  while (!as7263.dataReady()) delay(5);

  float redNIR = as7263.readCalibratedRed();
  float orangeNIR = as7263.readCalibratedOrange();
  float yellowNIR = as7263.readCalibratedYellow();
  float greenNIR = as7263.readCalibratedGreen();
  float blueNIR = as7263.readCalibratedBlue();
  float violetNIR = as7263.readCalibratedViolet();

  // --- Read TCS34725 ---
  uint16_t r, g, b, c;
  uint16_t colorTemp, lux;
  tcs.getRawData(&r, &g, &b, &c);
  colorTemp = tcs.calculateColorTemperature(r, g, b);
  lux = tcs.calculateLux(r, g, b);

  // --- Store in circular buffer ---
  redNIR_values[sampleIndex] = redNIR;
  blueNIR_values[sampleIndex] = blueNIR;
  greenNIR_values[sampleIndex] = greenNIR;
  violetNIR_values[sampleIndex] = violetNIR;
  r_values[sampleIndex] = r;
  g_values[sampleIndex] = g;
  b_values[sampleIndex] = b;
  lux_values[sampleIndex] = lux;

  sampleIndex++;
  if (sampleIndex >= sampleSize) {
    sampleIndex = 0;
    bufferFilled = true;
  }

  // --- LCD: Apple Color Detection ---
  lcd.setCursor(0, 1);
  lcd.print("Apple: ");
  if (r > g && r > b && r > 150) {
    lcd.print("Red   ");
    lcd.setRGB(255, 0, 0);
  } else if (g > r && g > b && g > 150) {
    lcd.print("Green ");
    lcd.setRGB(0, 255, 0);
  } else if (r > 100 && g > 100 && b < 80) {
    lcd.print("Yellow");
    lcd.setRGB(255, 255, 0);
  } else {
    lcd.print("Unknown");
    lcd.setRGB(100, 100, 100);
  }

  // --- Ripeness Check ---
  if (bufferFilled) {
    float avgRedNIR = 0, avgBlueNIR = 0, avgGreenNIR = 0, avgVioletNIR = 0;
    float avgR = 0, avgG = 0, avgB = 0, avgLux = 0;

    for (int i = 0; i < sampleSize; i++) {
      avgRedNIR += redNIR_values[i];
      avgBlueNIR += blueNIR_values[i];
      avgGreenNIR += greenNIR_values[i];
      avgVioletNIR += violetNIR_values[i];
      avgR += r_values[i];
      avgG += g_values[i];
      avgB += b_values[i];
      avgLux += lux_values[i];
    }

    avgRedNIR /= sampleSize;
    avgBlueNIR /= sampleSize;
    avgGreenNIR /= sampleSize;
    avgVioletNIR /= sampleSize;
    avgR /= sampleSize;
    avgG /= sampleSize;
    avgB /= sampleSize;
    avgLux /= sampleSize;

    // Ripeness logic (buzzer only)
    if (avgR >= 100 &&
        avgLux >= 100 &&
        avgBlueNIR >= 18 &&
        avgVioletNIR >= 80) {
      beep(2);  // Ripe
    } else if (avgR > 100 && avgLux > 118 &&
               avgBlueNIR > 6 && avgBlueNIR < 20 &&
               avgVioletNIR > 70) {
      beep(1);  // Unripe
    } else if (avgVioletNIR < 50 && avgBlueNIR < 10 && avgLux < 100) {
      beep(3);  // Overripe
    }

    // --- Serial Output of Averages ---
    Serial.println("----- Averaged Sensor Data -----");
    Serial.print("Avg RGB  -> R: "); Serial.print(avgR);
    Serial.print(" G: "); Serial.print(avgG);
    Serial.print(" B: "); Serial.println(avgB);

    Serial.print("Avg NIR  -> 610nm: "); Serial.print(avgRedNIR);
    Serial.print(" 810nm: "); Serial.print(avgBlueNIR);
    Serial.print(" 760nm: "); Serial.print(avgGreenNIR);
    Serial.print(" 860nm: "); Serial.println(avgVioletNIR);
    Serial.println("---------------------------------");
  }

  delay(1500);
}
