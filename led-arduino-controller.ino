/*
 LED Controller using L298N Motor Driver for 2-phase led string lights
*/
#include "config.h"
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <ArduinoMqttClient.h>
#define LED_POS D1 // Positive pin for LED control
#define LED_NEG D2 // Negative pin for LED control
#define DIM_POT A0 // Pin for dimming potentionmenter

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
int mode = 1;
int animationTime = 3000; // Milliseconds for a full "loop" of animation
int minDim = 20;          // In 0-255 scale (PWM)
int curMaxDim = 255;      // Current maximum dim, set by potentiometer (in 0-255 PWM scale)

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
int curDim = 255;            // Dim between 0 and 255, adjusted by animation functions (in 0-255 PWM scale)
int curDimB = 255;           // Same as above, but for use in 2 bank patterns only (symmetrical patterns)
bool dir = true;             // true for up, false for down, used in wavetype animations
bool twoBankPattern = false; // Whether or not this is a pattern that switches between each bank
bool bankPhase = false;      // Phase boolean for if we have twoBankPatter (both banks running patterns in symmetry)

// Wifi passwords
const char *wifiName = WIFI_NAME;
const char *wifiPass = WIFI_PASS;
const char *otaName = OTA_NAME;
const char *otaPass = OTA_PASS;
const char *mqttBroker = MQTT_BROKER;
const int mqttPort = MQTT_PORT;

// MQTT stuff
WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);
const int CMDS_LENGTH = 4;
String cmds[4] = {"mode", "minDim", "maxDim", "animTime"}; // "mode", "minDim", "maxDim", "animTime"

void setup()
{
  Serial.begin(115200); // Start the Serial communication to send messages to the computer
  delay(10);
  Serial.println('\n');

  // Setup WIFI and OTA
  wifiSetup();

  // analogWriteRange(255); // Match range of arduino uno
  // analogWriteFreq(420);  // Match freq of arduino uno
  pinMode(LED_POS, OUTPUT);
  pinMode(LED_NEG, OUTPUT);
  digitalWrite(LED_POS, LOW);
  digitalWrite(LED_NEG, LOW);
}

void loop()
{
  // Handle WiFi chores
  mqttClient.poll();
  MDNS.update();
  ArduinoOTA.handle();

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

/**
 * @brief Set up WIFI and OTA data transfer
 * SSIDs and passwords are setup in config.h
 *
 */
void wifiSetup()
{
  WiFi.begin(wifiName, wifiPass);
  Serial.print("Connecting to ");
  Serial.print(wifiName);
  Serial.println("...");

  int i = 1;

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(i);
    Serial.print(" ");
    i++;
    delay(1000);
  }

  Serial.println('\n');
  Serial.println("Connection established!");
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  WiFi.setSleepMode(WIFI_NONE_SLEEP, 0);
  WiFi.mode(WIFI_STA);

  // MQTT Server
  Serial.print("Attempting to connect to the MQTT broker: ");
  Serial.println(mqttBroker);

  if (!mqttClient.connect(mqttBroker, mqttPort))
  {
    Serial.print("MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());

    while (1)
      ;
  }

  Serial.println("Connected to the MQTT broker!");
  Serial.println();

  // set the message receive callback
  mqttClient.onMessage(onMqttMessage);

  for (int i = 0; i < CMDS_LENGTH; i++)
  {
    String name = otaName;
    String topic = "lights/" + name + "/" + cmds[i];

    Serial.println(topic);
    mqttClient.subscribe(topic);
  }

  ArduinoOTA.setHostname(otaName);
  ArduinoOTA.setPassword(otaPass);

  ArduinoOTA.onStart([]()
                     { Serial.println("Start"); });
  ArduinoOTA.onEnd([]()
                   { Serial.println("\nEnd"); });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                        { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); });

  ArduinoOTA.onError([](ota_error_t error)
                     {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed"); });

  ArduinoOTA.begin();
  Serial.println("OTA ready");

  // this NEEDS to be at the end of the setup for some reason, after OTA setup.
  if (!MDNS.begin(otaName))
  { // Start the mDNS responder for {otaName}.local
    Serial.println("Error setting up MDNS responder!");
  }
  Serial.println("mDNS responder started");
}

/**
 * @brief Handle MQTT Messages
 *
 * @param messageSize
 */
void onMqttMessage(int messageSize)
{
  // we received a message, print out the topic and contents
  // Serial.println("Received a message with topic '");
  String topic = mqttClient.messageTopic();
  // Serial.println(topic);
  int message = mqttClient.readString().toInt();
  // Serial.println(message);

  if (topic.endsWith(cmds[0]))
  {
    // Serial.println("fired " + cmds[0]);
    mode = message;
  }
  else if (topic.endsWith(cmds[1]))
  {
    // Serial.println("fired " + cmds[1]);
    minDim = message;
  }
  else if (topic.endsWith(cmds[2]))
  {
    // Serial.println("fired " + cmds[2]);
    curMaxDim = message;
    curDim = message;
    curDimB = message;
  }
  else if (topic.endsWith(cmds[3]))
  {
    // Serial.println("fired " + cmds[3]);
    animationTime = message;
  }

  // use the Stream interface to print the contents

  // Serial.println();
}

/**
 * =============================
 * Animation functions / helpers
 * =============================
 */

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

  /*If both banks are off, turn off lights */
  if (!bankA && !bankB)
  {
    digitalWrite(LED_POS, LOW);
    digitalWrite(LED_NEG, LOW);
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
 * @brief Turn off
 */
void mode_off()
{
  light_banks(false, false);
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

/**
 * @brief Switch the animation function based on provided mode
 *
 */
void animationFunction()
{
  switch (mode)
  {
  case 0:
    twoBankPattern = false;
    mode_off();
    break;
  case 1:
    twoBankPattern = false;
    mode_steady();
    break;
  case 2:
    twoBankPattern = false;
    mode_blink();
    break;
  case 3:
    twoBankPattern = false;
    mode_full_triangle_fade();
    break;
  case 4:
    twoBankPattern = false;
    mode_full_saw_fade();
    break;
  case 5:
    twoBankPattern = false;
    mode_full_sin_fade();
    break;
  case 6:
    twoBankPattern = true;
    mode_banked_tri_fade();
    break;
  case 7:
    twoBankPattern = true;
    mode_banked_saw_fade();
    break;
  case 8:
    twoBankPattern = true;
    mode_banked_sin_fade();
    break;
  default:
    twoBankPattern = false;
    mode_steady();
    break;
  }
}
