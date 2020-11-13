/****************************************************************************************************************************
   ISR_Timer_4_Switches.ino
   For ESP8266 boards
   Written by Khoi Hoang

   The ESP8266 timers are badly designed, using only 23-bit counter along with maximum 256 prescaler. They're only better than UNO / Mega.
   The ESP8266 has two hardware timers, but timer0 has been used for WiFi and it's not advisable to use. Only timer1 is available.
   The timer1's 23-bit counter terribly can count only up to 8,388,607. So the timer1 maximum interval is very short.
   Using 256 prescaler, maximum timer1 interval is only 26.843542 seconds !!!

   Now even you use all these new 16 ISR-based timers,with their maximum interval practically unlimited (limited only by
   unsigned long miliseconds), you just consume only one Hardware timer and avoid conflicting with other cores' tasks.
   The accuracy is nearly perfect compared to software timers. The most important feature is they're ISR-based timers
   Therefore, their executions are not blocked by bad-behaving functions / tasks.
   This important feature is absolutely necessary for mission-critical tasks.

   Based on SimpleTimer - A timer library for Arduino.
   Author: mromani@ottotecnica.com
   Copyright (c) 2010 OTTOTECNICA Italy

   Based on BlynkTimer.h
   Author: Volodymyr Shymanskyy
   
   Built by Khoi Hoang https://github.com/khoih-prog/TimerInterrupt_Generic
   Licensed under MIT license

   Version: 1.2.0

   Version Modified By   Date      Comments
   ------- -----------  ---------- -----------
   1.1.0   K Hoang      10/11/2020 Initial Super-Library coding to merge all TimerInterrupt Libraries
   1.2.0   K Hoang      12/11/2020 Add STM32_TimerInterrupt Library
*****************************************************************************************************************************/
/* Notes:
   Special design is necessary to share data between interrupt code and the rest of your program.
   Variables usually need to be "volatile" types. Volatile tells the compiler to avoid optimizations that assume
   variable can not spontaneously change. Because your function may change variables while your program is using them,
   the compiler needs this hint. But volatile alone is often not enough.
   When accessing shared variables, usually interrupts must be disabled. Even with volatile,
   if the interrupt changes a multi-byte variable between a sequence of instructions, it can be read incorrectly.
   If your data is multiple variables, such as an array and a count, usually interrupts need to be disabled
   or the entire sequence of your code which accesses the data.

   ISR_Switch demontrates the use of ISR to avoid being blocked by other CPU-monopolizing task

   In this complex example: CPU is connecting to WiFi, Internet and finally Blynk service (https://docs.blynk.cc/)
   Many important tasks are fighting for limited CPU resource in this no-controlled single-tasking environment.
   In certain period, mission-critical tasks (you name it) could be deprived of CPU time and have no chance
   to be executed. This can lead to disastrous results at critical time.
   We hereby will use interrupt to detect whenever the SW is active, then switch ON/OFF a sample relay (lamp)
   We'll see this ISR-based operation will have highest priority, preempts all remaining tasks to assure its
   functionality.

   ISR_Timer_4_Switches demontrates the use of ISR combining with Timer Interrupt to avoid being blocked by
   other CPU-monopolizing task. It also demontrates the usage of struct array for shorten repetitive code.
   In this complex example: CPU is connecting to WiFi, Internet and finally Blynk service (https://docs.blynk.cc/)
   Many important tasks are fighting for limited CPU resource in this no-controlled single-tasking environment.
   In certain period, mission-critical tasks (you name it) could be deprived of CPU time and have no chance
   to be executed. This can lead to disastrous results at critical time.
   We hereby will use interrupt to detect whenever a SW is active, then use a hardware timer to poll and switch
   ON/OFF a corresponding sample relay (lamp)
   We'll see this ISR-based operation will have highest priority, preempts all remaining tasks to assure its
   functionality.
*/

#if !defined(ESP8266)
  #error This code is designed to run on ESP8266 and ESP8266-based boards! Please check your Tools->Board setting.
#endif

#define BLYNK_PRINT Serial

#ifdef BLYNK_DEBUG
  #undef BLYNK_DEBUG
  //#define BLYNK_DEBUG true
#endif

#include <ESP8266WiFi.h>

#define USE_SSL     false

#if USE_SSL
  #include <BlynkSimpleEsp8266_SSL.h>
  #define BLYNK_HARDWARE_PORT     9443
#else
  #include <BlynkSimpleEsp8266.h>
  #define BLYNK_HARDWARE_PORT     8080
#endif

#define USE_LOCAL_SERVER    true

// If local server
#if USE_LOCAL_SERVER
  char blynk_server[]   = "account.duckdns.org";
  //char blynk_server[]   = "192.168.2.110";
#else
  char blynk_server[]   = "";
#endif

char auth[]     = "****";
char ssid[]     = "****";
char pass[]     = "****";

// These define's must be placed at the beginning before #include "ESP8266TimerInterrupt.h"
// Don't define TIMER_INTERRUPT_DEBUG > 2. Only for special ISR debugging only. Can hang the system.
#define TIMER_INTERRUPT_DEBUG       1

#include "TimerInterrupt_Generic.h"

// Init ESP8266 timer
ESP8266Timer ITimer;
#define TIMER_INTERVAL_MS           100

#define DEBOUNCE_TIME               25
#define LONG_BUTTON_PRESS_TIME_MS   10
#define DEBUG_ISR                   0

#define NUMBER_OF_LAMPS             4

// It's suggested to use #define's to centralize the pins' assignment in one place
// so that if you need to change, just one place to do, avoiding mistakes

#define VPIN0             V1
#define VPIN1             V2
#define VPIN2             V3
#define VPIN3             V4

#define TAC_SW0_PIN       D3
#define RELAY_0_PIN       D1

#define TAC_SW1_PIN       D5
#define RELAY_1_PIN       D0

#define TAC_SW2_PIN       D6
#define RELAY_2_PIN       D2

#define TAC_SW3_PIN       D7
#define RELAY_3_PIN       D4

#define LAMPSTATE_PIN0    V5
#define LAMPSTATE_PIN1    V6
#define LAMPSTATE_PIN2    V7
#define LAMPSTATE_PIN3    V8

//Blynk Color in format #RRGGBB
#define BLYNK_GREEN     "#23C48E"
#define BLYNK_RED       "#D3435C"

WidgetLED  LampStatus0(LAMPSTATE_PIN0);
WidgetLED  LampStatus1(LAMPSTATE_PIN1);
WidgetLED  LampStatus2(LAMPSTATE_PIN2);
WidgetLED  LampStatus3(LAMPSTATE_PIN3);

void ICACHE_RAM_ATTR Falling0();
void ICACHE_RAM_ATTR Rising0();

void ICACHE_RAM_ATTR Falling1();
void ICACHE_RAM_ATTR Rising1();

void ICACHE_RAM_ATTR Falling2();
void ICACHE_RAM_ATTR Rising2();

void ICACHE_RAM_ATTR Falling3();
void ICACHE_RAM_ATTR Rising3();

// This is a struct array, used to simplify programming code and eliminate repetitive code
// It also reduce code size by reduce number of functions, especially important in ISR code in ICACHE_RAM.

typedef void (*isr_func)(void);

typedef struct
{
  const int TacSwitch;
  const int RelayPin;
  const int vPin;
  const int lampStateVPin;
  WidgetLED* LED;
  volatile unsigned long  lastDebounceTime;
  volatile bool           buttonPressed;
  volatile bool           alreadyTriggered;
  volatile bool           LampState;
  volatile bool           SwitchReset;
  isr_func                func_falling;
  isr_func                func_rising;
} Lamp_Property_t;

Lamp_Property_t Lamps[NUMBER_OF_LAMPS] =
{
  { TAC_SW0_PIN, RELAY_0_PIN, VPIN0, LAMPSTATE_PIN0, &LampStatus0, 0, false, false, false, true, Falling0, Rising0 },
  { TAC_SW1_PIN, RELAY_1_PIN, VPIN1, LAMPSTATE_PIN1, &LampStatus1, 0, false, false, false, true, Falling1, Rising1 },
  { TAC_SW2_PIN, RELAY_2_PIN, VPIN2, LAMPSTATE_PIN2, &LampStatus2, 0, false, false, false, true, Falling2, Rising2 },
  { TAC_SW3_PIN, RELAY_3_PIN, VPIN3, LAMPSTATE_PIN3, &LampStatus3, 0, false, false, false, true, Falling3, Rising3 }
};


void ICACHE_RAM_ATTR ButtonCheck();
void ICACHE_RAM_ATTR ToggleRelay();

const int resetpin    = 10;

unsigned int myWiFiTimeout        =  3200L;  //  3.2s WiFi connection timeout   (WCT)
unsigned int buttonInterval       =  500L;   //  0.5s update button state


BlynkTimer Timer;

BLYNK_CONNECTED()
{
  static int index;

  for (index = 0; index < NUMBER_OF_LAMPS; index++)
  {
    Lamps[index].LED->on();
    Blynk.virtualWrite(Lamps[index].vPin, LOW);
    Blynk.setProperty(Lamps[index].lampStateVPin, "color", Lamps[index].LampState ? BLYNK_RED : BLYNK_GREEN );
    Blynk.syncAll();
  }
}

#define index0              0
#define index1              1
#define index2              2
#define index3              3

BLYNK_WRITE(VPIN0)
{
  if (param.asInt())
  {
    Lamps[index0].alreadyTriggered = true;
    ToggleRelay();
  }
}

BLYNK_WRITE(VPIN1)
{
  if (param.asInt())
  {
    Lamps[index1].alreadyTriggered = true;
    ToggleRelay();
  }
}

BLYNK_WRITE(VPIN2)
{
  if (param.asInt())
  {
    Lamps[index2].alreadyTriggered = true;
    ToggleRelay();
  }
}

BLYNK_WRITE(VPIN3)
{
  if (param.asInt())
  {
    Lamps[index3].alreadyTriggered = true;
    ToggleRelay();
  }
}

void ICACHE_RAM_ATTR Rising0()
{
  unsigned long currentTime  = millis();

  if ( digitalRead(Lamps[index0].TacSwitch) && (currentTime > Lamps[index0].lastDebounceTime + DEBOUNCE_TIME) )
  {
    Lamps[index0].buttonPressed = false;
    Lamps[index0].lastDebounceTime = currentTime;
    attachInterrupt(digitalPinToInterrupt(Lamps[index0].TacSwitch), Lamps[index0].func_falling, FALLING);
  }
}

void ICACHE_RAM_ATTR Rising1()
{
  unsigned long currentTime  = millis();

  if ( digitalRead(Lamps[index1].TacSwitch) && (currentTime > Lamps[index1].lastDebounceTime + DEBOUNCE_TIME) )
  {
    Lamps[index1].buttonPressed = false;
    Lamps[index1].lastDebounceTime = currentTime;
    attachInterrupt(digitalPinToInterrupt(Lamps[index1].TacSwitch), Lamps[index1].func_falling, FALLING);
  }
}

void ICACHE_RAM_ATTR Rising2()
{
  unsigned long currentTime  = millis();

  if ( digitalRead(Lamps[index2].TacSwitch) && (currentTime > Lamps[index2].lastDebounceTime + DEBOUNCE_TIME) )
  {
    Lamps[index2].buttonPressed = false;
    Lamps[index2].lastDebounceTime = currentTime;
    attachInterrupt(digitalPinToInterrupt(Lamps[index2].TacSwitch), Lamps[index2].func_falling, FALLING);
  }
}

void ICACHE_RAM_ATTR Rising3()
{
  unsigned long currentTime  = millis();

  if ( digitalRead(Lamps[index3].TacSwitch) && (currentTime > Lamps[index3].lastDebounceTime + DEBOUNCE_TIME) )
  {
    Lamps[index3].buttonPressed = false;
    Lamps[index3].lastDebounceTime = currentTime;
    attachInterrupt(digitalPinToInterrupt(Lamps[index3].TacSwitch), Lamps[index3].func_falling, FALLING);
  }
}

void ICACHE_RAM_ATTR Falling0()
{
  unsigned long currentTime  = millis();

  if ( !digitalRead(Lamps[index0].TacSwitch) && (currentTime > Lamps[index0].lastDebounceTime + DEBOUNCE_TIME))
  {
    Lamps[index0].lastDebounceTime = currentTime;
    Lamps[index0].buttonPressed = true;
    attachInterrupt(digitalPinToInterrupt(Lamps[index0].TacSwitch), Lamps[index0].func_rising, RISING);
  }
}

void ICACHE_RAM_ATTR Falling1()
{
  unsigned long currentTime  = millis();

  if ( !digitalRead(Lamps[index1].TacSwitch) && (currentTime > Lamps[index1].lastDebounceTime + DEBOUNCE_TIME))
  {
    Lamps[index1].lastDebounceTime = currentTime;
    Lamps[index1].buttonPressed = true;
    attachInterrupt(digitalPinToInterrupt(Lamps[index1].TacSwitch), Lamps[index1].func_rising, RISING);
  }
}

void ICACHE_RAM_ATTR Falling2()
{
  unsigned long currentTime  = millis();

  if ( !digitalRead(Lamps[index2].TacSwitch) && (currentTime > Lamps[index2].lastDebounceTime + DEBOUNCE_TIME))
  {
    Lamps[index2].lastDebounceTime = currentTime;
    Lamps[index2].buttonPressed = true;
    attachInterrupt(digitalPinToInterrupt(Lamps[index2].TacSwitch), Lamps[index2].func_rising, RISING);
  }
}

void ICACHE_RAM_ATTR Falling3()
{
  unsigned long currentTime  = millis();

  if ( !digitalRead(Lamps[index3].TacSwitch) && (currentTime > Lamps[index3].lastDebounceTime + DEBOUNCE_TIME))
  {
    Lamps[index3].lastDebounceTime = currentTime;
    Lamps[index3].buttonPressed = true;
    attachInterrupt(digitalPinToInterrupt(Lamps[index3].TacSwitch), Lamps[index3].func_rising, RISING);
  }
}

void heartBeatPrint(void)
{
  static int num = 1;

  if (Blynk.connected())
  {
    Serial.print("B");
  }
  else
  {
    Serial.print("F");
  }

  if (num == 40)
  {
    Serial.println();
    num = 1;
  }
  else if (num++ % 10 == 0)
  {
    Serial.print(" ");
  }
}

void checkButton()
{
  static int index;

  heartBeatPrint();

  for (index = 0; index < NUMBER_OF_LAMPS; index++)
  {
    if (Lamps[index].LampState)
      Blynk.setProperty(Lamps[index].lampStateVPin, "color", BLYNK_RED);
    else
      Blynk.setProperty(Lamps[index].lampStateVPin, "color", BLYNK_GREEN);
  }
}

// Need only one for 4 SWs
void ICACHE_RAM_ATTR HWCheckButton()
{
  static int index;

  for (index = 0; index < NUMBER_OF_LAMPS; index++)
  {
    if (!Lamps[index].alreadyTriggered && Lamps[index].buttonPressed)
    {
      Lamps[index].alreadyTriggered = true;
    }
    ButtonCheck();
  }
}

void ICACHE_RAM_ATTR ButtonCheck()
{
  boolean SwitchState;
  static int index;

  for (index = 0; index < NUMBER_OF_LAMPS; index++)
  {
    SwitchState = (digitalRead(Lamps[index].TacSwitch));

    if (!SwitchState && Lamps[index].SwitchReset)
    {
      ToggleRelay();
      Lamps[index].SwitchReset = false;
    }
    else if (SwitchState)
    {
      Lamps[index].SwitchReset = true;
    }
  }
}

void ICACHE_RAM_ATTR ToggleRelay()
{
  static int index;

  for (index = 0; index < NUMBER_OF_LAMPS; index++)
  {
    if (Lamps[index].alreadyTriggered)
    {
      // Reset status
      Lamps[index].alreadyTriggered = false;

      if (Lamps[index].LampState)
      {
#if (TIMER_INTERRUPT_DEBUG > 0)
        Serial.println("Toggle OFF Relay " + String(index));
#endif

        digitalWrite(Lamps[index].RelayPin, LOW);
        Lamps[index].LampState = false;
      }
      else
      {
#if (TIMER_INTERRUPT_DEBUG > 0)
        Serial.println("Toggle ON Relay " + String(index));
#endif

        digitalWrite(Lamps[index].RelayPin, HIGH);
        Lamps[index].LampState = true;
      }
    }
  }
}

void setup()
{
  Serial.begin(115200);
  while (!Serial);
  
  delay(200);

  Serial.println("\nStarting ISR_Timer_4_Switches on " + String(ARDUINO_BOARD));
  Serial.println("Version : " + String(TIMER_INTERRUPT_GENERIC_VERSION));
  Serial.println("CPU Frequency = " + String(F_CPU / 1000000) + " MHz");

  for (int index = 0; index < NUMBER_OF_LAMPS; index++)
  {
    pinMode(Lamps[index].RelayPin, OUTPUT);
    digitalWrite(Lamps[index].RelayPin, LOW);

    pinMode(Lamps[index].TacSwitch, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(Lamps[index].TacSwitch), Lamps[index].func_falling, FALLING);
  }

  pinMode(resetpin, INPUT_PULLUP);

  // Use only one to check all 4
  // Interval in microsecs, so MS to multiply by 1000
  // Be sure to place this HW Timer well ahead blocking calls, because it needs to be initialized.
  if (ITimer.attachInterruptInterval(TIMER_INTERVAL_MS * 1000, HWCheckButton))
    Serial.println("Starting  ITimer OK, millis() = " + String(millis()));
  else
    Serial.println("Can't set ITimer. Select another freq. or interval");

  unsigned long startWiFi = millis();

  WiFi.begin(ssid, pass);

  do
  {
    delay(200);
    if ( (WiFi.status() == WL_CONNECTED) || (millis() > startWiFi + myWiFiTimeout) )
      break;
  } while (WiFi.status() != WL_CONNECTED);

  Blynk.config(auth, blynk_server, BLYNK_HARDWARE_PORT);
  Blynk.connect();

  if (Blynk.connected())
    Serial.println("Blynk connected");
  else
    Serial.println("Blynk not connected yet");

  // Use only one to check all 4
  Timer.setInterval(buttonInterval, checkButton);
}

void loop()
{
  Blynk.run();
  Timer.run();
}
