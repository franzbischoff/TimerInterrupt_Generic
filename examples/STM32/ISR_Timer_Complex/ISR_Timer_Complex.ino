/****************************************************************************************************************************
   ISR_Timer_Complex.ino
   For STM32 boards
   Written by Khoi Hoang

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
/*
   Notes:
   Special design is necessary to share data between interrupt code and the rest of your program.
   Variables usually need to be "volatile" types. Volatile tells the compiler to avoid optimizations that assume
   variable can not spontaneously change. Because your function may change variables while your program is using them,
   the compiler needs this hint. But volatile alone is often not enough.
   When accessing shared variables, usually interrupts must be disabled. Even with volatile,
   if the interrupt changes a multi-byte variable between a sequence of instructions, it can be read incorrectly.
   If your data is multiple variables, such as an array and a count, usually interrupts need to be disabled
   or the entire sequence of your code which accesses the data.

   RPM Measuring uses high frequency hardware timer 1Hz == 1ms) to measure the time from of one rotation, in ms
   then convert to RPM. One rotation is detected by reading the state of a magnetic REED SW or IR LED Sensor
   Asssuming LOW is active.
   For example: Max speed is 600RPM => 10 RPS => minimum 100ms a rotation. We'll use 80ms for debouncing
   If the time between active state is less than 8ms => consider noise.
   RPM = 60000 / (rotation time in ms)

   We use interrupt to detect whenever the SW is active, set a flag then use timer to count the time between active state

   This example will demonstrate the nearly perfect accuracy compared to software timers by printing the actual elapsed millisecs.
   Being ISR-based timers, their executions are not blocked by bad-behaving functions / tasks, such as connecting to WiFi, Internet
   and Blynk services. You can also have many (up to 16) timers to use.
   This non-being-blocked important feature is absolutely necessary for mission-critical tasks.
   You'll see blynkTimer is blocked while connecting to WiFi / Internet / Blynk, and elapsed time is very unaccurate
   In this super simple example, you don't see much different after Blynk is connected, because of no competing task is
   written
*/

#if !( defined(STM32F0) || defined(STM32F1) || defined(STM32F2) || defined(STM32F3)  ||defined(STM32F4) || defined(STM32F7) || \
       defined(STM32L0) || defined(STM32L1) || defined(STM32L4) || defined(STM32H7)  ||defined(STM32G0) || defined(STM32G4) || \
       defined(STM32WB) || defined(STM32MP1) )
  #error This code is designed to run on STM32F/L/H/G/WB/MP1 platform! Please check your Tools->Board setting.
#endif

#define BLYNK_PRINT Serial

//#define BLYNK_DEBUG
#ifdef BLYNK_DEBUG
  #undef BLYNK_DEBUG
#endif

/* Comment this out to disable prints and save space */
#define BLYNK_PRINT Serial

#define USE_BUILTIN_ETHERNET    true
//  If don't use USE_BUILTIN_ETHERNET, and USE_UIP_ETHERNET => use W5x00 with Ethernet library
#define USE_UIP_ETHERNET        false

#if (USE_BUILTIN_ETHERNET)
  #define ETHERNET_NAME     "Built-in LAN8742A Ethernet"
#elif (USE_UIP_ETHERNET)
  #define ETHERNET_NAME     "ENC28J60 Ethernet Shield"
#else
  #define ETHERNET_NAME     "W5x00 Ethernet Shield"
#endif

#define BLYNK_NO_YIELD

#if USE_BUILTIN_ETHERNET
  #include <BlynkSimple_STM32BI_Ethernet.h>
#elif USE_UIP_ETHERNET
  #include <BlynkSimpleUIPEthernet.h>
#else
  #include <BlynkSimpleEthernet.h>
#endif

#define USE_LOCAL_SERVER      true

#if USE_LOCAL_SERVER
  char auth[] = "******";
  char server[] = "account.duckdns.org";
  
  //char server[] = "192.168.2.112";

#else
  char auth[] = "******";
  char server[] = "blynk-cloud.com";
#endif
  
  #define BLYNK_HARDWARE_PORT       8080

#if !(USE_BUILTIN_ETHERNET || USE_UIP_ETHERNET)
  #define W5100_CS  10
  #define SDCARD_CS 4
#endif

// These define's must be placed at the beginning before #include "TimerInterrupt_Generic.h"
#define TIMER_INTERRUPT_DEBUG      1

#include "TimerInterrupt_Generic.h"
#include "ISR_Timer_Generic.h"

#define HW_TIMER_INTERVAL_US      100L

volatile uint32_t lastMillis = 0;

// Depending on the board, you can select STM32 Hardware Timer from TIM1-TIM22
// For example, F767ZI can select Timer from TIM1-TIM14
// If you select a Timer not correctly, you'll get a message from ci[ompiler
// 'TIMxx' was not declared in this scope; did you mean 'TIMyy'? 

// Init STM32 timer TIM1
STM32Timer ITimer(TIM1);

// Init STM32_ISR_Timer
// Each STM32_ISR_Timer can service 16 different ISR-based timers
ISR_Timer STM32_ISR_Timer;

// Ibit Blynk Timer
BlynkTimer blynkTimer;

#ifndef LED_BUILTIN
  #define LED_BUILTIN       PB0               // Pin 33/PB0 control on-board LED_GREEN on F767ZI
#endif

#define LED_TOGGLE_INTERVAL_MS        5000L

#define TIMER_INTERVAL_2S             2000L
#define TIMER_INTERVAL_5S             5000L
#define TIMER_INTERVAL_11S            11000L
#define TIMER_INTERVAL_101S           101000L

void TimerHandler(void)
{
  static bool toggle  = false;
  static bool started = false;
  static int timeRun  = 0;

  STM32_ISR_Timer.run();

  // Toggle LED every LED_TOGGLE_INTERVAL_MS = 5000ms = 5s
  if (++timeRun == ( (LED_TOGGLE_INTERVAL_MS * 1000 ) / HW_TIMER_INTERVAL_US) )
  {
    timeRun = 0;

    if (!started)
    {
      started = true;
      pinMode(LED_BUILTIN, OUTPUT);
    }

    //timer interrupt toggles pin LED_BUILTIN
    digitalWrite(LED_BUILTIN, toggle);
    toggle = !toggle;
  }
}

// In STM32, avoid doing something fancy in ISR, for example complex Serial.print with String() argument
// The pure simple Serial.prints here are just for demonstration and testing. Must be eliminate in working environment
// Or you can get this run-time error / crash
void doingSomething2s()
{
  static unsigned long previousMillis = lastMillis;
  unsigned long deltaMillis = millis() - previousMillis;

#if (TIMER_INTERRUPT_DEBUG > 0)
  if (previousMillis > TIMER_INTERVAL_2S)
  {
    Serial.print("2s: Delta ms = ");
    Serial.println(deltaMillis);
  }
#endif

  previousMillis = millis();
}

// In STM32, avoid doing something fancy in ISR, for example complex Serial.print with String() argument
// The pure simple Serial.prints here are just for demonstration and testing. Must be eliminate in working environment
// Or you can get this run-time error / crash
void doingSomething5s()
{
  static unsigned long previousMillis = lastMillis;
  unsigned long deltaMillis = millis() - previousMillis;

#if (TIMER_INTERRUPT_DEBUG > 0)
  if (previousMillis > TIMER_INTERVAL_5S)
  {
    Serial.print("5s: Delta ms = ");
    Serial.println(deltaMillis);
  }
#endif

  previousMillis = millis();
}

// In STM32, avoid doing something fancy in ISR, for example complex Serial.print with String() argument
// The pure simple Serial.prints here are just for demonstration and testing. Must be eliminate in working environment
// Or you can get this run-time error / crash
void doingSomething11s()
{
  static unsigned long previousMillis = lastMillis;
  unsigned long deltaMillis = millis() - previousMillis;

#if (TIMER_INTERRUPT_DEBUG > 0)
  if (previousMillis > TIMER_INTERVAL_11S)
  {
    Serial.print("11s: Delta ms = ");
    Serial.println(deltaMillis);
  }
#endif

  previousMillis = millis();
}

// In STM32, avoid doing something fancy in ISR, for example complex Serial.print with String() argument
// The pure simple Serial.prints here are just for demonstration and testing. Must be eliminate in working environment
// Or you can get this run-time error / crash
void doingSomething101s()
{
  static unsigned long previousMillis = lastMillis;
  unsigned long deltaMillis = millis() - previousMillis;

#if (TIMER_INTERRUPT_DEBUG > 0)
  if (previousMillis > TIMER_INTERVAL_101S)
  {
    Serial.print("101s: Delta ms = ");
    Serial.println(deltaMillis);
  }
#endif

  previousMillis = millis();
}

#define BLYNK_TIMER_MS        2000L

// Here is software Timer, you can do somewhat fancy stuffs without many issues.
// But always avoid
// 1. Long delay() it just doing nothing and pain-without-gain wasting CPU power.Plan and design your code / strategy ahead
// 2. Very long "do", "while", "for" loops without predetermined exit time.
void blynkDoingSomething2s()
{
  static unsigned long previousMillis = lastMillis;
  Serial.println("blynkDoingSomething2s: Delta programmed ms = " + String(BLYNK_TIMER_MS) + ", actual = " + String(millis() - previousMillis));
  previousMillis = millis();
}

void setup()
{
  Serial.begin(115200);
  while (!Serial);
  
  Serial.println("\nStarting ISR_Timer_Complex on " + String(BOARD_NAME));
  Serial.println("Version : " + String(TIMER_INTERRUPT_GENERIC_VERSION));
  Serial.println("CPU Frequency = " + String(F_CPU / 1000000) + " MHz");

  // You need this timer for non-critical tasks. Avoid abusing ISR if not absolutely necessary.
  blynkTimer.setInterval(BLYNK_TIMER_MS, blynkDoingSomething2s);

  // Interval in microsecs
  if (ITimer.attachInterruptInterval(HW_TIMER_INTERVAL_US, TimerHandler))
  {
    lastMillis = millis();
    Serial.println("Starting  ITimer OK, millis() = " + String(lastMillis));
  }
  else
    Serial.println("Can't set ITimer correctly. Select another freq. or interval");

  // Just to demonstrate, don't use too many ISR Timers if not absolutely necessary
  // You can use up to 16 timer for each STM32_ISR_Timer
  STM32_ISR_Timer.setInterval(TIMER_INTERVAL_2S, doingSomething2s);
  STM32_ISR_Timer.setInterval(TIMER_INTERVAL_5S, doingSomething5s);
  STM32_ISR_Timer.setInterval(TIMER_INTERVAL_11S, doingSomething11s);
  STM32_ISR_Timer.setInterval(TIMER_INTERVAL_101S, doingSomething101s);

#if !(USE_BUILTIN_ETHERNET || USE_UIP_ETHERNET)
  pinMode(SDCARD_CS, OUTPUT);
  digitalWrite(SDCARD_CS, HIGH); // Deselect the SD card
#endif

#if USE_LOCAL_SERVER
  Blynk.begin(auth, server, BLYNK_HARDWARE_PORT);
#else
  Blynk.begin(auth);
  // You can also specify server:
  //Blynk.begin(auth, server, BLYNK_HARDWARE_PORT);
#endif

  if (Blynk.connected())
  {
    Serial.print(F("IP = "));
    Serial.println(Ethernet.localIP());
  }
}

#define BLOCKING_TIME_MS      3000L

void loop()
{
  static bool needWiFiBegin = true;

  Blynk.run();

  // This unadvised blocking task is used to demonstrate the blocking effects onto the execution and accuracy to Software timer
  // You see the time elapse of STM32_ISR_Timer still accurate, whereas very unaccurate for Software Timer
  // The time elapse for 2000ms software timer now becomes 3000ms (BLOCKING_TIME_MS)
  // While that of STM32_ISR_Timer is still prefect.
  delay(BLOCKING_TIME_MS);

  // You need this Software timer for non-critical tasks. Avoid abusing ISR if not absolutely necessary
  // You don't need to and never call STM32_ISR_Timer.run() here in the loop(). It's already handled by ISR timer.
  blynkTimer.run();
}
