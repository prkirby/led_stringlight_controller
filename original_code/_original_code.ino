/*
https://create.arduino.cc/projecthub/daniel232/arduino-controlled-power-supply-for-led-string-lights-72ba6b?ref=userrespected&ref_id=212539&offset=0
Premade sketch
*/

const byte pin_P = 10;
const byte pin_N = 9;

// unsigned int frequency = 150;
int halfPeriod = 3333; // (500000 / frequency)
unsigned long currentMicros = 0;
unsigned long previousMicros = 0;

int interval;
int defaultInterval = 5;
unsigned long previousMillis = 0;
unsigned long currentMillis = 0;

bool phase = true;               // true: positive alternation  -  false:  negative alternation
long positive = halfPeriod * .1; // duration of positive alternation
long negative = halfPeriod * .1; // duration of negative alternation
int increment = 1;

void setup()
{
  pinMode(pin_P, OUTPUT); // LED positive
  pinMode(pin_N, OUTPUT); // LED negative
  Serial.begin(2000000);  // (115200);
  interval = defaultInterval;
  digitalWrite(pin_N, HIGH);
  digitalWrite(pin_P, LOW);
}

void loop()
{
  currentMicros = micros();
  // This determines the "Phase" we need to be concerned with for this loop
  if (currentMicros - previousMicros >= halfPeriod)
  {
    previousMicros = currentMicros;
    phase = !phase;
  }

  // Optionally insert the modifications of the "positive" and "negative" times to create effects

  if (positive >= halfPeriod)
  {
    increment = -1;
  }

  if (positive <= 0)
  {
    increment = 1;
  }

  if ((positive < 200) || (positive > (halfPeriod - 200)))
  {
    interval = defaultInterval * 10;
  }
  else
  {
    interval = defaultInterval;
  }

  currentMillis = millis();

  if (currentMillis - previousMillis >= interval)
  {
    previousMillis = currentMillis;
    positive = positive + increment;
    negative = halfPeriod - positive;
  }

  if ((phase == true) && ((currentMicros - previousMicros) < positive))
  {
    digitalWrite(pin_N, LOW);
    digitalWrite(pin_P, HIGH);
  }
  else
  {
    digitalWrite(pin_P, LOW);
  }

  if ((phase == false) && ((currentMicros - previousMicros) < negative))
  {
    digitalWrite(pin_P, LOW);
    digitalWrite(pin_N, HIGH);
  }
  else
  {
    digitalWrite(pin_N, LOW);
  }
}