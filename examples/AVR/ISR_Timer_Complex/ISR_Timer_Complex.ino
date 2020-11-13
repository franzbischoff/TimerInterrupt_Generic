/****************************************************************************************************************************
   ISR_Timer_Complex.ino
   For Arduino AVR boards (UNO, Nano, Mega, etc. )
   Written by Khoi Hoang
  
   TCNTx - Timer/Counter Register. The actual timer value is stored here.
   OCRx - Output Compare Register
   ICRx - Input Capture Register (only for 16bit timer)
   TIMSKx - Timer/Counter Interrupt Mask Register. To enable/disable timer interrupts.
   TIFRx - Timer/Counter Interrupt Flag Register. Indicates a pending timer interrupt. 

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

/****************************************************************************************************************************
   This example will demonstrate the nearly perfect accuracy compared to software timers by printing the actual elapsed millisecs.
   Being ISR-based timers, their executions are not blocked by bad-behaving functions / tasks, such as connecting to WiFi, Internet
   and Blynk services. You can also have many (up to 16) timers to use.
   This non-being-blocked important feature is absolutely necessary for mission-critical tasks.
   You'll see blynkTimer is blocked while connecting to WiFi / Internet / Blynk, and elapsed time is very unaccurate
   In this super simple example, you don't see much different after Blynk is connected, because of no competing task is
   written
*****************************************************************************************************************************/
/****************************************************************************************************************************
    This example is currently written for Arduino Mega 2560 with ESP-01 WiFi or Mega2560-WiFi-R3
    You can easily convert to UNO and ESP-01
    Mega: Digital pin 18 – 21,2 and 3 can be used to provide hardware interrupt from external devices.
    UNO/Nano: Digital pin 2 and 3 can be used to provide hardware interrupt from external devices.
    To upload program to MEGA2560+WiFi, only turn ON SW 3+4 (USB <-> MCU).
    To run MEGA+WiFi combined, turn ON SW 1+2 (MCU <-> ESP) and SW 3+4 (USB <-> MCU)
 *****************************************************************************************************************************/

#define BLYNK_PRINT Serial
//#define BLYNK_DEBUG true

//These define's must be placed at the beginning before #include "TimerInterrupt.h"
#define TIMER_INTERRUPT_DEBUG      0

#define USE_TIMER_1     true
#define USE_TIMER_2     false
#define USE_TIMER_3     false
#define USE_TIMER_4     false
#define USE_TIMER_5     false

#include "TimerInterrupt_Generic.h"
#include "ISR_Timer_Generic.h"

#include <ESP8266_Lib.h>
#include <BlynkSimpleShieldEsp8266.h>

#define BLYNK_HARDWARE_PORT     8080

#define USE_LOCAL_SERVER        true

// If local server
#if USE_LOCAL_SERVER
char blynk_server[]   = "yourname.duckdns.org";
//char blynk_server[]   = "192.168.2.110";
#else
char blynk_server[]   = "";
#endif

char auth[]     = "****";
char ssid[]     = "****";
char pass[]     = "****";

//Mega2560
// Hardware Serial on Mega, Leonardo, Micro...
#define EspSerial Serial3   //Serial1

// Your MEGA <-> ESP8266 baud rate:
#define ESP8266_BAUD 115200

ESP8266 wifi(&EspSerial);

#define OUTPUT_PIN       2         // Connect LED to this output pin

#define HW_TIMER_INTERVAL_MS        10

#define WIFI_TIMEOUT      20000L

volatile uint32_t lastMillis = 0;

// Init BlynkTimer
ISR_Timer ISR_Timer1;

// Init BlynkTimer
BlynkTimer blynkTimer;

#define LED_TOGGLE_INTERVAL_MS      2000L

void TimerHandler(void)
{
  static bool toggle = false;
  static bool started = false;

  static int timeRun      = 0;

  ISR_Timer1.run();

  // Toggle LED every 10 x 200 = 2000ms = 2s
  if (++timeRun == (LED_TOGGLE_INTERVAL_MS / HW_TIMER_INTERVAL_MS) )
  {
    timeRun = 0;

    if (!started)
    {
      started = true;
      pinMode(OUTPUT_PIN, OUTPUT);
    }

#if (TIMER_INTERRUPT_DEBUG > 0)
    Serial.println("Delta ms = " + String(millis() - lastMillis));
    lastMillis = millis();
#endif

    //timer interrupt toggles pin OUTPUT_PIN
    digitalWrite(OUTPUT_PIN, toggle);
    toggle = !toggle;
  }
}

void doingSomething2s()
{
  static unsigned long previousMillis = lastMillis;
  Serial.println("doingSomething2s: Delta ms = " + String(millis() - previousMillis));
  previousMillis = millis();
}

void doingSomething5s()
{
  static unsigned long previousMillis = lastMillis;
  Serial.println("doingSomething5s: Delta ms = " + String(millis() - previousMillis));
  previousMillis = millis();
}

void doingSomething10s()
{
  static unsigned long previousMillis = lastMillis;
  Serial.println("doingSomething10s: Delta ms = " + String(millis() - previousMillis));
  previousMillis = millis();
}

void doingSomething50s()
{
  static unsigned long previousMillis = lastMillis;
  Serial.println("doingSomething50s: Delta ms = " + String(millis() - previousMillis));
  previousMillis = millis();
}

#define BLYNK_TIMER_MS        2000L

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

  // Set ESP8266 baud rate
  EspSerial.begin(ESP8266_BAUD);
  delay(10);

  Serial.println("\nStarting ISR_Timer_Complex on Arduino AVR board");
  Serial.println("Version : " + String(TIMER_INTERRUPT_GENERIC_VERSION));
  Serial.println("CPU Frequency = " + String(F_CPU / 1000000) + " MHz");
  
  Serial.print("ESPSerial using ");
  Serial.println(ESP8266_BAUD);

  Serial.println("\nStarting Timer Interrupt");

  // Select Timer 1-2 for UNO, 0-5 for MEGA
  // Timer 2 is 8-bit timer, only for higher frequency
  ITimer1.init();

  // Using ATmega328 used in UNO => 16MHz CPU clock ,
  // For 16-bit timer 1, 3, 4 and 5, set frequency from 0.2385 to some KHz
  // For 8-bit timer 2 (prescaler up to 1024, set frequency from 61.5Hz to some KHz

  // Interval in millisecs
  if (ITimer1.attachInterruptInterval(HW_TIMER_INTERVAL_MS, TimerHandler))
  {
    lastMillis = millis();
    Serial.println("Starting  ITimer1 OK, millis() = " + String(lastMillis));
  }
  else
    Serial.println("Can't set ITimer1 correctly. Select another freq. or interval");

  // Just to demonstrate, don't use too many ISR Timers if not absolutely necessary
  ISR_Timer1.setInterval(2000L, doingSomething2s);
  ISR_Timer1.setInterval(5000L, doingSomething5s);
  ISR_Timer1.setInterval(10000L, doingSomething10s);
  ISR_Timer1.setInterval(50000L, doingSomething50s);

  // You need this timer for non-critical tasks. Avoid abusing ISR if not absolutely necessary.
  blynkTimer.setInterval(BLYNK_TIMER_MS, blynkDoingSomething2s);

  Blynk.begin(auth, wifi, ssid, pass, blynk_server, BLYNK_HARDWARE_PORT);

  if (Blynk.connected())
    Serial.println("Blynk connected");
  else
    Serial.println("Blynk not connected yet");
}

#define BLOCKING_TIME_MS      3000L

void loop()
{
  static unsigned long previousMillis = lastMillis;

  Blynk.run();

  // This unadvised blocking task is used to demonstrate the blocking effects onto the execution and accuracy to Software timer
  // You see the time elapse of ISR_Timer still accurate, whereas very unaccurate for Software Timer
  // The time elapse for 2000ms software timer now becomes 3000ms (BLOCKING_TIME_MS)
  // While that of ISR_Timer is still prefect.
  delay(BLOCKING_TIME_MS);

  // You need this Software timer for non-critical tasks. Avoid abusing ISR if not absolutely necessary
  // You don't need to and never call ISR_Timer.run() here in the loop(). It's already handled by ISR timer.
  blynkTimer.run();
}
