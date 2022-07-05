/* Copyright stuff: Free to learn
   Code is laid out for easy reading, while still using some standard techniques
   Optimising the code sections may be an enjoyable challeng: One could, for
   instance,
*/

#include <Arduino.h>
#include <Wire.h>

#include <LiquidCrystal_I2C.h>

#define SCREEN_WIDTH 16
#define SCREEN_HEIGHT 2
#define SCREEN_ADDRESS 0x27
LiquidCrystal_I2C lcd(SCREEN_ADDRESS, SCREEN_WIDTH, SCREEN_HEIGHT);

//#define USE_PUSH_BUTTON
  #define USE_PROX_BUTTON

#define INPUT_POT_ON
#define INPUT_POT_OFF

// #define DHT_PIN   2

#define INPUT_ON_PIN     A0
#define INPUT_OFF_PIN    A1 

#define LCD_SDA A4
#define LCD_SCL A5

#define SESSION_PIN       2
#define LED_PIN_CYCLE_ON  7
#define LED_PIN_CYCLE_OFF 8
#define LED_PIN_ALERT    12
#define LED_PIN_ONBOARD  13

 uint32_t seconds =  1000;
 uint32_t minutes = 60000;

float increment_On  = 60.0 / 1023.0; // 30s units 30 minutes (0 to 1023 = 1024)
float increment_Off = 40.0 / 1023.0; // 15s units 10 minutes (0 to 1023 = 1024)

int analog_On   = 0;
int analog_Off  = 0;

int timeStack_Phase = 0;

uint32_t deBounce_Check =   0;
uint32_t deBounce_Delay = 250;

int state_Button  = LOW;
int state_Session = LOW;
int state_Cycle   = LOW;

uint32_t session_Start_Time = 0;
uint32_t session_Stop_Time = 0;
int cycle_Counter = 0;
// Note: we can further optimize variable declaration by specifying the number of bits that gets used, like uint32_t will declare an unsigned int 64 bits long
uint32_t cycle_HI_Duration = 0;
uint32_t cycle_LO_Duration = 0;
uint32_t cycle_HI_Time = 0;
uint32_t cycle_LO_Time = 0;

uint32_t cycle_Start = 0;
uint32_t cycle_Swap = 0;
uint32_t cycle_Stop = 0;

uint32_t buzzer_Event_Millis[30]; //22 elements in the array 0-21 = 22
uint32_t buzzer_Event_Index = 0;
uint32_t buzzer_Interval_Time = 0;

void PrintToSerial();
bool Prepare_Session(uint32_t sessionStartMillis);
void Prepare_Cycle(uint32_t pulsTime);
void PrintBuzzerTimes(int interval, String message);
void Prepare_Alerts();
void displayAll(uint32_t pulseTime);

void setup()
{
  // put your setup code here, to run once:
  // initialize serial communication at 9600 bits per second:
  /* Remember to set pinmodes - so that the chip can know wat to do with 
  the output information provided
  */
  pinMode(LED_PIN_CYCLE_OFF, OUTPUT);
  pinMode(LED_PIN_CYCLE_ON, OUTPUT);
  pinMode(LED_PIN_ONBOARD, OUTPUT);
  pinMode(LED_PIN_ALERT, OUTPUT);
  pinMode(INPUT_OFF_PIN, INPUT);
  pinMode(INPUT_ON_PIN, INPUT);
  pinMode(SESSION_PIN, INPUT);

  Serial.begin(9600);

  digitalWrite(LED_PIN_ONBOARD, LOW);

  // Serial.println(F("Setup: Starting LCD"));
  lcd.init();
  lcd.backlight();
  // lcd.noBacklight();

  lcd.setCursor(0,0);
  lcd.print("  SMART START   ");  
  lcd.setCursor(0,1);
  lcd.print(F("   H "));
          delay(100);
  lcd.print(F("e "));
          delay(100);
  lcd.print(F("l "));
          delay(100);
  lcd.print(F("l "));
          delay(100);
  lcd.print(F("o    "));
          delay(100);

}

void loop()
{
  uint32_t pulseTime = 0;
    /*The fillowing calculation looks strange - dividing by 10 and multiplying by 10 makes no difference?
    When working with integers it does make a difference, by dividing with 10 we shift right by one decimal
    which then gets cut off, because the number is a long integer (having no decimal spaces). 
    By then multiplying it, we shift left one space with a 0. Which means that we rounded milliseconds 
    to the nearest 100th of a second. 1234/10 = 123 then 123 x 10 = 1230
    */
  pulseTime = (millis() / 10) * 10;
   //Becasue we new have a rounded value at the start of each loop, we can that, insted of having
   //to read the clock time via the Millis() function 

  if (pulseTime % 20 == 0)
  {
    timeStack_Phase++; // same as:   stack_Phase = stack_Phase + 1
    if (timeStack_Phase == 10)
    {
        timeStack_Phase = 0;
    }
    // Serial.print("Main Loop: stack_Phase = ");
    // Serial.println(stack_Phase);

    switch (timeStack_Phase)
    {
      case 0:
      //  We reserve case 0 for interrupts or error resets
      //  In this case to send display output to LCD

      if (pulseTime % 200 == 0)
      {
      displayAll(pulseTime);
      }
      // break;
    case 1:
      // First order of business is to regularly read the pot values
      // only applicable when system is NOT in session AND time interval is 1/4 second (250ms)
      if ((state_Session == LOW) && pulseTime % 250 == 0)
      {

        int sensorRead_On  = int(float(analogRead(INPUT_ON_PIN) ) * increment_On );
        int sensorRead_Off = int(float(analogRead(INPUT_OFF_PIN)) * increment_Off);
        // Serial.print("Main Loop: sensorRead_On = ");
        // Serial.print(sensorRead_On);
        // Serial.print(" sensorRead_Off = ");
        // Serial.println(sensorRead_Off);
        if (analog_On != sensorRead_On)
        {
            analog_On = sensorRead_On;
        }
        if (analog_Off != sensorRead_Off)
        {
            analog_Off = sensorRead_Off;
        }
        // Serial.print("Loop case1: PulasTime =");
        // Serial.println(pulseTime);
        if (pulseTime%seconds == 0) {
        // PrintToSerial();
        }
        break;
      }

    case 2:
      /* In second phase we check the state of the Session ON/OFF button
      only applicable when the button is pressed i.e. HIGH
      If it is held for longer than the debounce time, then its a valid press, 
      in which case it then needs to change state.
      */
      if (digitalRead(SESSION_PIN) == HIGH)
      {
        Serial.println("Loop case2:  SESSION_PIN = HIGH");
        if (state_Button == LOW)
        {
          state_Button = HIGH;
          deBounce_Check = pulseTime;
          Serial.print("Loop case2: Previously Button state was LOW - deBounce: ");
          Serial.println(deBounce_Check);
          break;
        }
        else if ((pulseTime - deBounce_Check) >= deBounce_Delay)
        {
          Serial.print("Loop case2: Previously Button state was HIGH - PulsTime: ");
          Serial.println(pulseTime);
          if (state_Session == HIGH)
          {
            state_Session = LOW;
            deBounce_Check = (pulseTime+deBounce_Delay);
            state_Button = LOW;
            digitalWrite(LED_PIN_ALERT, LOW);
            Prepare_Session(0);
            Serial.print("Loop case2: Session State was HIGH, now Stopped at ");
            Serial.println(pulseTime);
            break;
          }
          else
          {
            state_Session = HIGH;
            // Setting the session start time to be 2 seconds from now
            deBounce_Check = (pulseTime+deBounce_Delay);
            state_Button = LOW;
            if (Prepare_Session(pulseTime + 3 * seconds) == true)
            {
              Serial.print("Loop case2: Session State was LOW, now Start... Success at ");
              Serial.print(pulseTime);
              Serial.print(" state = ");
              Serial.println(state_Session);
              break;
            }
            else
            {
              Serial.println("Loop case2: Session State was LOW, but Start... Failure");
              state_Session = LOW;
              break;
            }
          }
        }

        digitalWrite(LED_PIN_ONBOARD, state_Session);
        
      } else {
        state_Button = LOW;
      }

    case 3:
      /* Third phase we set the cycle times, based on current time and cycle counter
      this only gets done once, after the rest cycle started, so current time, approximated
      by the rounded pulse time that gets set at the start of each loop.
      Keep in mind that the loop runs a couple of times per second, so even if 
      all conditions check out, it causes exessive processing if one executes the
      same things repeatedly without atting value. With exception, the smart rule is 
      to execute only the necesarry, as soon as possible. In this case making sure that current time 
      is bigger than the cycle change-over should suffice, but in some cases it may be necesary 
      to introduce another variable just to keep an eye on the state, and change thereof.
      Here we use the introduced state variable to also set outputs for the user to see.
      */
      if (state_Session == HIGH) {
         if ( (pulseTime >= cycle_Swap) && (state_Cycle == HIGH))
      {
        Serial.println("Loop case3: Setting Cycle state to LOW");
        state_Cycle = LOW;
        cycle_LO_Time = pulseTime;
        digitalWrite(LED_PIN_CYCLE_ON,  LOW );
        digitalWrite(LED_PIN_CYCLE_OFF, HIGH);
        digitalWrite(LED_PIN_ALERT, LOW);
        Serial.print("Loop case3: Setting new cycle time for cycle #");
        Serial.println(cycle_Counter);
        Prepare_Cycle(pulseTime);
        Prepare_Alerts();
        cycle_Counter++; // Same as cycle_Couner = cycle_Counter + 1
      }
       if ( (pulseTime > cycle_Start) && (state_Cycle == LOW)) {
         Serial.println("Setting Cycle state to HIGH");
         state_Cycle = HIGH;
         cycle_HI_Time =pulseTime;
         digitalWrite(LED_PIN_CYCLE_ON,  HIGH);
         digitalWrite(LED_PIN_CYCLE_OFF, LOW );
         digitalWrite(LED_PIN_ALERT, LOW);
       }
      }

    case 4:
      // phase four to activate the alerts.buzzer/flasher, if session is on

      if (state_Session == HIGH)
      {
      // Serial.print("Loop case4: State -");
      // Serial.print(state_Session);
      // Serial.print("- Time: ");
      // Serial.print(pulseTime);
      // Serial.print("   index ");
      // Serial.println(buzzer_Event_Index);

        for (int i = 0; i < 30; i++)
        //One can simply iterate over such a small array and check each value for comparison
        //but in larger arrays its nice to keep a pointer in order to work through the array quicker
        {

         //If the Alert event time is not 0, and current time is bigger or equal to the alert time
         if ( (buzzer_Event_Millis[i] != 0) && (buzzer_Event_Millis[i] <= pulseTime) ) {
          //If the alert event index is a even number it indicates ON state
          if (i%2 == 0) 
          {

            digitalWrite(LED_PIN_ALERT, HIGH);
            Serial.print("Loop case4: Set Alert index [");
            Serial.print(i);
            Serial.print("] HIGH at ");
            Serial.println(buzzer_Event_Millis[i]);
            buzzer_Event_Millis[i] = 0;
            // buzzer_Event_Index = i++;
          }
          if (i%2 != 0)
          {
            digitalWrite(LED_PIN_ALERT, LOW);
            Serial.print("Loop case4: Set Alert index [");
            Serial.print(i);
            Serial.print("]  LOW at ");
            Serial.println(buzzer_Event_Millis[i]);
            buzzer_Event_Millis[i] = 0;
            // buzzer_Event_Index = i++;
          }
        }

      }

      //In between one can also create an allert at every minute
      if ( (pulseTime > cycle_Start) && (pulseTime < cycle_Swap-(10*seconds)) ) {
        if (((pulseTime - cycle_Start)%(30*seconds) == 0) && (buzzer_Interval_Time == 0)) {
            digitalWrite(LED_PIN_ALERT, HIGH);  
            Serial.print("Loop case4: Set Minute alert HIGH at: ");
            Serial.println(pulseTime);
            buzzer_Interval_Time = pulseTime;
        }
          if ((buzzer_Interval_Time > 0) && ((pulseTime - buzzer_Interval_Time)>= 100)) {
            digitalWrite(LED_PIN_ALERT, LOW); 
            buzzer_Interval_Time = 0; 
            Serial.print("Loop case4: Set Minute alert LOW at: ");
            Serial.println(pulseTime);
        }
      }

      }
        
    }
  }
}
// ************ End of execution loop ************

// === New function. Declare before setup. Call from within loop. ===
bool Prepare_Session(uint32_t sessionStartMillis)
{
  // Serial.print("Prepare_Session: With start time: ");
  // Serial.println(sessionStartMillis);
  // --- Set all session times to 0
  session_Start_Time = 0;
  session_Stop_Time = 0;
  // --- Set all Cycle times and counters to 0
  cycle_Counter = 0;
  cycle_Start = 0;
  cycle_Swap = 0;
  cycle_Stop = 0;
  // --- Set the Cycle state
  /* This is an anomily because logically we know that before the session starts, the state is LOW,
  or in this application in rest mode. But because we trigger the cycle update at the change-over point
  between Active and rest, it works to set a HIGH state before start, because that will trigger the 
  change-over event in the main Loop. Thus we're actually setting an imaginary Active state, to kick everything off
  */
 state_Cycle = HIGH;
  /* --- Set all Buzzer times and counters to 0
  in this case we know the number of elements in the buzzer Array,
  but it can also be calculated by dividing the size of the arry by the size of the elements inside it
  */
  int buzzerArrayLength = sizeof(buzzer_Event_Millis) / sizeof(int32_t);
  for (int i = 0; i < buzzerArrayLength; i++)
  {
    buzzer_Event_Millis[i] = 0;
    // we use square brackets to indicate array index, but one can also use the pointer *(buzzer_Event_Millis+i)=0
  }
  // Serial.print("Prepare_Session: set the Buzzer events to 0, for element count ");
  // Serial.println(buzzerArrayLength);

  if ((analog_On == 0) || (sessionStartMillis == 0))
  {
    // Serial.println("Prepare_Session: No Cycle Hi duration provided!");
    return false;
  }
  else
  {
    session_Start_Time = sessionStartMillis;
    // Serial.print("Prepare_Session: New Session started, Millis Time = ");
    // Serial.println(session_Start_Time);
    cycle_HI_Duration = long(analog_On * 30 * seconds);
    // Serial.print("Prepare_Session: analog On ");
    // Serial.print(analog_On);
    // Serial.print(" Analog Off ");
    // Serial.println(analog_Off);
    cycle_LO_Duration = long(analog_Off * 15 * seconds);
    // Serial.print("Prepare_Session: Cycle Hi duration = ");
    // Serial.println(cycle_HI_Duration);
    // Serial.print("Prepare_Session: Cycle LO duration = ");
    // Serial.println(cycle_LO_Duration);
  }
  return true;
}

void Prepare_Cycle(uint32_t pulsTime)
{

  if (session_Start_Time == 0)
  {
    // Serial.println("Prepare_Cycle: No Session time found!");
  }
  else
  {
    // Serial.print("Prepare_Cycle: #");
    // Serial.print(cycle_Counter);
    // Serial.print(" from session start ");
    // Serial.println(session_Start_Time);
    // Serial.print("Prepare_Cycle: analog On ");
    // Serial.print(analog_On);
    // Serial.print(" Analog Off ");
    // Serial.println(analog_Off);
    /*Remember that we set the cycle durations when we set the session
    as the Analog input counter x interval x seconds(1000 milliseconds).
    We then multiply the sum of Hi and Lo durations with the cycle counter
    so that we can always count the cycle from the session start time
    and not the previous cycle end time. (Its just nicer to have a fixed point)
    Also means that we can set the next cycle start time befor the previous low cycle is complete.
    Therefore we technically don't even need to record the cycle stop time,
    because that would also be the next cycle start time.
    Without having to compare current time to cycle time, one can just reset to 0,
    therefore if it is 0, set it to the next time interval.
    */
  //  Serial.print("Prepare_Cycle: START at: ");
  //  Serial.print(cycle_Start);
  //  Serial.print(" to SWAP at:");
  //  Serial.println(cycle_Swap);

    if (cycle_Start <= pulsTime)
    {
      cycle_Start = session_Start_Time + (cycle_HI_Duration + cycle_LO_Duration) * cycle_Counter;
      // Serial.print("Calculate Cycle: HI from: ");
      // Serial.println(cycle_Start);
    }
    if (cycle_Swap <= pulsTime)
    {
      // Serial.print("Calculate Cycle: Start = ");
      // Serial.print(cycle_Start);
      // Serial.print(" with duration: ");
      // Serial.println(cycle_HI_Duration);
      cycle_Swap = cycle_Start + cycle_HI_Duration;
      // Serial.print("Calculate Cycle: Swap = ");
      // Serial.println(cycle_Swap);
    }
  }
}

void Prepare_Alerts()
{
/*The alerts are a practical indication of when a session starts
instead of just stop-start, we provide a 2 second warning before start, and 10 second alert before end.
The one thing to keep in mind is that these alerts are meant to trigger before the cycle changes
and if the new cycle start time was updated during the rest cycle then we don't want to update 
the next batch of alerts before the previous onese came into play
*/
  const int longBeep  = 750;
  const int medBeep   = 200;
  const int shortBeep =  50;

  buzzer_Event_Index = 0;

  
  // Firstly the alerts prior to the cycle start.
  // cycle Hi Start alert 3 short beeps
  // Serial.print("Prepare Cycle: Start at: ");
  // Serial.println(cycle_Start);
  for (int i = 0; i < 6; i=i+2)
  {
    if (i == 0)
    {
      buzzer_Event_Millis[i] = cycle_Start - (3 * seconds);
    }
    else
    {
      buzzer_Event_Millis[i] = (buzzer_Event_Millis[i - 1]+medBeep);
    }
    buzzer_Event_Millis[i + 1] = buzzer_Event_Millis[i] + medBeep;
    // PrintBuzzerTimes(i, "Cycle Start short");
  }
  // cycle Hi Start alert 1 Long beep
  buzzer_Event_Millis[6] = cycle_Start - longBeep;
  buzzer_Event_Millis[7] = cycle_Start;
  // PrintBuzzerTimes(6, "Cycle Start Long");

  // Then the alerts before the rest cycle
  // cycle Swap alert 10 short beeps
  for (int i = 8; i < 26; i=i+2)
  {
    buzzer_Event_Millis[i] = cycle_Swap - ((26-i) * (seconds/2));
    buzzer_Event_Millis[i + 1] = buzzer_Event_Millis[i] + shortBeep;
    // PrintBuzzerTimes(i, "Cycle Swap Half ");
  }
  // cycle Swap alert Long beep
  buzzer_Event_Millis[28] = cycle_Swap - longBeep;
  buzzer_Event_Millis[29] = cycle_Swap;
  // PrintBuzzerTimes(20, "Cycle end Long");
  
}

void PrintBuzzerTimes(int interval, String message)
{
  Serial.print("Prepare_Alerts: Interval ");
  Serial.print(interval);
  Serial.print(".) ");
  Serial.print(message);
  Serial.print(" start = ");
  Serial.print(buzzer_Event_Millis[interval]);
  Serial.print(" Stop = ");
  Serial.print(buzzer_Event_Millis[interval + 1]);
  Serial.print(" state = ");
  Serial.println(state_Session);
}

void PrintToSerial()
{
  Serial.print("PrintToSerial: Time On: ");
  Serial.print(analog_On / 2);
  Serial.print(":");
  Serial.print((analog_On % 2) * 30);
  Serial.print("   ");
  Serial.print("Time Off: ");
  Serial.print(analog_Off / 4);
  Serial.print(":");
  Serial.print((analog_Off % 4) * 15);
  Serial.println();
}

//==========================================================================

void displayAll(uint32_t pulseTime) {

if (state_Session == LOW) {
  uint32_t HI_min = (analog_On / 2);
  uint32_t HI_sec = ((analog_On % 2) * 30);
  uint32_t LO_min = (analog_Off / 4);
  uint32_t LO_sec = ((analog_Off % 4) * 15);
     Serial.print("DisplayAll: Active ");
     Serial.print(HI_min);
     Serial.print(":");
     Serial.print(HI_sec);
     Serial.print("  Rest ");
     Serial.print(LO_min);
     Serial.print(":");
     Serial.println(LO_sec);
  // Set ACTIVE 00:00
   lcd.setCursor(0,0);
  
  lcd.print("Set Active ");
  if(HI_min < 10) {lcd.print("0");}
  lcd.print(HI_min);
  lcd.print(":");
  if(HI_sec == 0) {lcd.print("0");}
  lcd.print(HI_sec);   
  // Set  REST  00:00
  lcd.setCursor(0,1);

  lcd.print("Set   Rest ");
  if(LO_min < 10) {lcd.print("0");}
  lcd.print(LO_min);
  lcd.print(":");
  if(LO_sec == 0) {lcd.print("0");}
  lcd.print(LO_sec);

  } else {
    lcd.setCursor(0,0);
        uint32_t TOTAL_min = ((pulseTime-session_Start_Time)/seconds)/60;
        uint32_t TOTAL_sec = ((pulseTime-session_Start_Time)/seconds)%60;
        if (TOTAL_min > 60) {
          TOTAL_min = 0;
          TOTAL_sec = 0;
        }
        lcd.print("Total time ");
        if (TOTAL_min<10) {lcd.print("0");}
        lcd.print(TOTAL_min);
        lcd.print(":");
        if (TOTAL_sec<10) {lcd.print("0");}
        lcd.print(TOTAL_sec); 
//Total time 00:00
        if (pulseTime%5000==0){
          Serial.print("DisplayAll: Writing Total ");
          Serial.print(TOTAL_min);
          Serial.print(" min  ");
          Serial.print(TOTAL_sec);
          Serial.println(" sec");
        }

  lcd.setCursor(0,1);
      if (cycle_Counter < 10) {lcd.print("0");}
      lcd.print(cycle_Counter);

      if (state_Cycle == LOW) {
    // Serial.print("DisplayAll: REST pulse ");
    //  Serial.print(pulseTime);
    //  Serial.print(" swap ");
    //  Serial.println(cycle_Swap);    
        uint32_t REST_min = ((pulseTime-cycle_LO_Time)/seconds)/60;
        uint32_t REST_sec = ((pulseTime-cycle_LO_Time)/seconds)%60;
        // "   Rest time 00:00"
        lcd.print("    REST ");
        if (REST_min<10) {lcd.print("0");}
        lcd.print(REST_min);
        lcd.print(":");
        if (REST_sec<10) {lcd.print("0");}
        lcd.print(REST_sec);  
    // Serial.print("DisplayAll: Writing Rest ");
    //  Serial.print(REST_min);
    //  Serial.print("m ");
    //  Serial.println(REST_sec);

    }  else {
   
        uint32_t ACTIVE_min = ((pulseTime-cycle_HI_Time)/seconds)/60;
        uint32_t ACTIVE_sec = ((pulseTime-cycle_HI_Time)/seconds)%60;
        // "Active 00:00"
        lcd.print("  ACTIVE ");
        if (ACTIVE_min<10) {lcd.print("0");}
        lcd.print(ACTIVE_min);
        lcd.print(":");
        if (ACTIVE_sec<10) {lcd.print("0");}
        lcd.print(ACTIVE_sec); 
        
    // Serial.print("DisplayAll: Writing Active ");
    //  Serial.print(ACTIVE_min);
    //  Serial.print("m ");
    //  Serial.println(ACTIVE_sec);

    }
  }
}

//==========================================================================
