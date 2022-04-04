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
int mode = 3;
int animationTime = 2000; // Milliseconds for a full "loop" of animation
int minDim = 20;          // In 0-255 scale (PWM), IE 25% duty cycle is minimum

#define LED_POS 10
#define LED_NEG 9
#define DIM_POT A0

// 50 hz should be considered as minimum dimming.

// Frequency of the LED lights is 150HZ between two polarities by default
// Frequency of phase shift for each LED "Bank" (Lower freq makes things a bit tripper for steady modes)
int halfPeriod = 500000 / 240; // (1000000us / 2 / frequency) (240hz seems to be sweet spot for low fades?)
unsigned long curMicros = 0;
unsigned long prevMicros = 0;
unsigned long prevDimMicros = 0;
unsigned long curMillis = 0;
unsigned long prevMillis = 0;
int curAnimTime = 0;
bool phase = true;   // True Bank_A is active, False Bank_B is active
int curMaxDim = 255; // Current maximum dim, set by potentiometer (in 0-255 PWM scale)
int curDim = 255;    // Dim between 0 and 1.0, adjusted by animation functions (in 0-255 PWM scale)
bool dir = true;     // true for up, false for down, used in wavetype animations

// Animation function pointer
void (*animationFunction)();

void light_banks(bool bankA, bool bankB)
{
  if (phase && bankA) // If we are in phase A, write pulse width out to bankA
  {
    analogWrite(LED_POS, min(curDim, curMaxDim));
    digitalWrite(LED_NEG, LOW);
  }

  if (!phase && bankB) // And vise versa
  {
    digitalWrite(LED_POS, LOW);
    analogWrite(LED_NEG, min(curDim, curMaxDim));
  }
}

/* "Contstant" on, flipping at each phase interval  */
void mode_steady()
{
  light_banks(true, true);
}

/* Swap between each bank at half animation time interval */
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

/* Fade both LED banks together in pulse, in triangle wave form */
void mode_full_fade()
{
  // Scale between minDim and max dim
  curDim = map(curAnimTime, 0, animationTime, minDim, curMaxDim);
  if (!dir)
    curDim = curMaxDim - curDim; // Take inverse if we are going "down" or in reverse
  light_banks(true, true);
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
  case 3:
    animationFunction = mode_full_fade;
    break;
  default:
    break;
  }
}

void loop()
{

  // Sample time, and set phase for which LED Bank we are currently interested in
  // Check for new dimming value
  curMicros = micros();
  curMillis = millis();
  if (curMicros - prevMicros >= halfPeriod)
  {
    phase = !phase;
    prevMicros = curMicros;
    curMaxDim = map(analogRead(DIM_POT), 0, 1023, minDim, 255);
  }

  // Set current anim time based millis
  if (curMillis - prevMillis >= animationTime)
  {
    prevMillis = curMillis;
    dir = !dir;
  }

  curAnimTime = curMillis - prevMillis;

  animationFunction();
}
