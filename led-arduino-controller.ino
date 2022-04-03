/*
 LED Controller using L298N Motor Driver for 2-phase led string lights
*/

/**
 * Different modes available:
 *  1 - "steady"          {Contsant on}
 *  2 - "blink"           {Annoying blinking}
 *  3 - "full-fade"       {Fade both LED banks together in pulse}
 *  4 - "switching-fade"  {Fade each bank in and out}
 */
int mode = 1;
int animationTime = 1000; // Milliseconds for a full "loop" of animation

#define LED_POS 10
#define LED_NEG 9
#define DIM_POT A0

// 50 hz should be considered as minimum dimming.

// Frequency of the LED lights is 150HZ
int halfPeriod = 3333; // (1000000 us / 2 / frequency)
unsigned long curMicros = 0;
unsigned long prevMicros = 0;
unsigned long prevDimMicros = 0;
unsigned long curMillis = 0;
unsigned long prevMillis = 0;
int curAnimTime = 0;
bool phase = true;  // True Bank_A is active, False Bank_B is active
float curDim = 0.2; // Dim between 0 and 1.0

// Animation function pointer
void (*animationFunction)();

void light_banks(bool bankA, bool bankB)
{
  // if we are in a dimming period, cut power instead
  if (curMicros - prevDimMicros > halfPeriod * curDim)
  {
    digitalWrite(LED_POS, LOW);
    digitalWrite(LED_NEG, LOW);

    if (curMicros - prevDimMicros >= halfPeriod)
      prevDimMicros = curMicros;

    return;
  }

  if (phase && bankA)
  {
    digitalWrite(LED_POS, HIGH);
    digitalWrite(LED_NEG, LOW);
  }

  if (!phase && bankB)
  {
    digitalWrite(LED_POS, LOW);
    digitalWrite(LED_NEG, HIGH);
  }
}

/* "Contstant" on, flipping at each phase interval  */
void mode_steady()
{
  light_banks(true, true);
}

void mode_blink()
{
  if (curAnimTime < animationTime / 2)
  {
    light_banks(true, false);
  }
  else
  {
    light_banks(false, true);
  }
}

void setup()
{
  pinMode(LED_POS, OUTPUT);
  pinMode(LED_NEG, OUTPUT);
  Serial.begin(115200);
  digitalWrite(LED_POS, LOW);
  digitalWrite(LED_NEG, LOW);

  switch (mode)
  {
  case 1:
    animationFunction = mode_steady;
    break;
  case 2:
    animationFunction = mode_blink;
    break;
  default:
    break;
  }
}

void loop()
{

  // Sample time, and set phase for which LED Bank we are currently interested in
  curMicros = micros();
  curMillis = millis();
  if (curMicros - prevMicros >= halfPeriod)
  {
    phase = !phase;
    prevMicros = curMicros;
    Serial.println(analogRead(DIM_POT));
  }

  // Set current anim time based millis
  if (curMillis - prevMillis >= animationTime)
    prevMillis = curMillis;

  curAnimTime = curMillis - prevMillis;

  if (curAnimTime > animationTime)
    curAnimTime = 0;

  animationFunction();
}
