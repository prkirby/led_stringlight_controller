/*
 LED Controller using L298N Motor Driver for 2-phase led string lights
*/

/**
 * Different modes available:
 *  1 - "Steady"                  {Constant on}
 *  2 - "Blink"                   {Annoying blinking}
 *  3 - "Full Fade - Triangle"    {Fade both LED banks together in pulse - Triangle waveform}
 *  4 - "Full Fade - Saw"         {Fade both LED banks together in pulse - Saw waveform}
 *  5 - "Full Fade - Sine"        {Fade both LED banks together in pulse - Sine waveform}
 *  6 - "Banked Fade - Triangle"  {Fade each LED Banks in and out in pulse - Triangle waveform} NOTE: Doesnt get close to minDIM for some reason
 *  7 - "Banked Fade - Saw"       {Fade each LED Banks in and out in pulse - Saw waveform}
 *  8 - "Banked Fade - Sine"      {Fade each LED Banks in and out in pulse - Sine waveform}
 */
int mode = 8;
int animationTime = 10000; // Milliseconds for a full "loop" of animation
int minDim = 0;            // In 0-255 scale (PWM)
/**
 * DO NOT EDIT BELOW HERE
 * - unless intentional of course....
 */
#define LED_POS D1 // Positive pin for LED control
#define LED_NEG D2 // Negative pin for LED control
#define DIM_POT A0 // Pin for dimming potentionmenter

// 50 hz should be considered as minimum dimming.

// Frequency of the LED lights is 150HZ between two polarities by default
// Frequency of phase shift for each LED "Bank" (Lower freq makes things a bit tripper for steady modes)
// IMPORTANT: frequency seems to be needed to be dialed on a string by string basis
int halfPeriod = 500000 / 120; // (1000000us / 2 / frequency) (240hz seems to be sweet spot for low fades?)
unsigned long curMicros = 0;
unsigned long prevMicros = 0;
unsigned long prevDimMicros = 0;
unsigned long curMillis = 0;
unsigned long prevMillis = 0;
int curAnimTime = 0;
bool phase = true;           // True Bank_A is active, False Bank_B is active
int curMaxDim = 255;         // Current maximum dim, set by potentiometer (in 0-255 PWM scale)
int curDim = 255;            // Dim between 0 and 255, adjusted by animation functions (in 0-255 PWM scale)
int curDimB = 255;           // Same as above, but for use in 2 bank patterns only (symmetrical patterns)
bool dir = true;             // true for up, false for down, used in wavetype animations
bool twoBankPattern = false; // Whether or not this is a pattern that switches between each bank
bool bankPhase = false;      // Phase boolean for if we have twoBankPatter (both banks running patterns in symmetry)

void (*animationFunction)(); // Animation function pointer

/**
 * @brief Light up either of the two LED banks, based on current state variables such as phase, and dimness
 *
 * @param bankA Is bankA currently in animation cycle
 * @param bankB Is bankB currently in animation cycle
 */
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
    analogWrite(LED_NEG, min(twoBankPattern ? curDimB : curDim, curMaxDim));
  }
}

/**
 * @brief Contstant on, flipping at each phase interval
 */
void mode_steady()
{
  light_banks(true, true);
}

/**
 * @brief Swap between each bank at half animation time interval
 */
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

/**
 * @brief Map the diming based on animation time linrarly
 * @return int
 */
int linearDimMap(bool dir = true)
{
  int from = dir ? minDim : curMaxDim;
  int to = dir ? curMaxDim : minDim;

  return map(curAnimTime, 0.0, animationTime, from, to);
}

/**
 * @brief Get the Cur Rads based on animation percent
 *
 * @return double
 */
double getCurRads()
{
  double animPercent = double(curAnimTime) / double(animationTime);
  return animPercent * PI;
}

/**
 * @brief Map the diming value based on a sin wav function
 * @return int
 */
int sinDimMap(double rads)
{
  double sinVal = sin(rads);
  if (sinVal < 0)
    sinVal *= -1;
  return map(double(255.0 * sinVal), 0.0, 255.0, minDim, curMaxDim);
}

/**
 * @brief Phase shift the provided dimming 180deg out of phase
 *
 * @param dim Dim level you want to phase shift
 * @return int
 */
int phasedDim(int dim)
{
  // Offset the dim by half the current available scale
  int phasedDim = dim + ((curMaxDim - minDim) / 2);
  if (phasedDim > curMaxDim)
    phasedDim = minDim + (phasedDim - curMaxDim);
  return phasedDim;
}

/**
 * @brief Fade both LED banks together in pulse, in triangle wave form
 * @note There is a weird flash on the down edge of the cycle
 */
void mode_full_triangle_fade()
{
  // Scale between minDim and max dim
  curDim = linearDimMap(dir);
  light_banks(true, true);
}

/**
 * @brief Fade both LED Banks together in ramp pulse
 */
void mode_full_saw_fade()
{
  // Scale between minDim and max dim
  curDim = linearDimMap();
  light_banks(true, true);
}

/**
 * @brief Fade both LED banks in sin wave
 */
void mode_full_sin_fade()
{
  curDim = sinDimMap(getCurRads());
  light_banks(true, true);
}

/**
 * @brief Fade each bank separately in triangle wave
 */
void mode_banked_tri_fade()
{
  curDim = linearDimMap(dir);
  curDimB = linearDimMap(!dir);
  light_banks(true, true);
}

/**
 * @brief Fade each bank separately in saw wave
 */
void mode_banked_saw_fade()
{
  int linearDim = linearDimMap();
  curDim = linearDim;
  curDimB = phasedDim(linearDim);
  light_banks(true, true);
}

/**
 * @brief Fade each bank separately in sin wave, phase shifted
 */
void mode_banked_sin_fade()
{
  double curRads = getCurRads();
  curDim = sinDimMap(curRads);
  curDimB = sinDimMap(curRads + (PI / 2));
  light_banks(true, true);
}

void setup()
{
  // analogWriteRange(255); // Match range of arduino uno
  // analogWriteFreq(420);  // Match freq of arduino uno
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
    animationFunction = mode_full_triangle_fade;
    break;
  case 4:
    animationFunction = mode_full_saw_fade;
    break;
  case 5:
    animationFunction = mode_full_sin_fade;
    break;
  case 6:
    twoBankPattern = true;
    animationFunction = mode_banked_tri_fade;
    break;
  case 7:
    twoBankPattern = true;
    animationFunction = mode_banked_saw_fade;
    break;
  case 8:
    twoBankPattern = true;
    animationFunction = mode_banked_sin_fade;
    break;
  default:
    animationFunction = mode_steady;
    break;
  }
}

void loop()
{
  // Sample time, and set phase for which LED Bank we are currently interested in
  // Check for new dimming value
  curMicros = micros();
  curMillis = millis();

  // Switch "phase" at appropriate halfPeriod Intervals
  if (curMicros - prevMicros >= halfPeriod)
  {
    phase = !phase;
    prevMicros = curMicros;
    // Ignore analog ready for now (esp8266 use 0.0 -> 1v analog pin read, which I don't have set up)
    // curMaxDim = map(analogRead(DIM_POT), 0, 1023, minDim, 255);
  }

  // Set current anim time based millis
  if (curMillis - prevMillis >= animationTime)
  {
    prevMillis = curMillis;
    dir = !dir;
  }

  curAnimTime = curMillis - prevMillis;

  animationFunction();
  // delay(500);
}
