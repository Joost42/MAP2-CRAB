int treshold = 1;
int detectPin = 12;
bool detect = false;
int led = 4;
int number=0;
hw_timer_t *My_timer = NULL;
hw_timer_t *My_timer2 = NULL;

void IRAM_ATTR onTimer(){
  Serial.println("TIMER1");
      if(number>treshold)  //If in the set of the interrupt time the number more than 1 times, then means have detect moving objects,This value can be adjusted according to the actual situation, which is equivalent to adjust the threshold of detection speed of moving objects.
         {
          Serial.println("HERE");
                   detect = true;
                   digitalWrite(led, detect);    //light led
                   number=0;   //Cleare the number, so that it does not affect the next trigger
         }
        else
              number=0;   //If in the setting of the interrupt time, the number of the interrupt is not reached the threshold value, it is not detected the moving objects, Cleare the number.
}

void IRAM_ATTR onTimer2(){
  Serial.println("GOING TO SLEEP");
      esp_deep_sleep_start();
}

void stateChange()  //Interrupt service function
{
  number++;  //Interrupted once, the number + 1

}

void setup() {
 Serial.begin(115200);
 Serial.println("Starting...\n");
 pinMode (detectPin, INPUT);
 pinMode (led, OUTPUT);
 digitalWrite(led, false);
 esp_sleep_enable_timer_wakeup(5000000);
 My_timer = timerBegin(3, 80, true);
 timerAttachInterrupt(My_timer, &onTimer, true);
 timerAlarmWrite(My_timer, 1000000, true);
 timerAlarmEnable(My_timer); //Just Enable
 
 My_timer2 = timerBegin(2, 80, true);
 timerAttachInterrupt(My_timer2, &onTimer2, true);
 timerAlarmWrite(My_timer2, 5000000, true);
 timerAlarmEnable(My_timer2); //Just Enable
 attachInterrupt(detectPin, stateChange, FALLING); // Set the interrupt function, interrupt pin is digital pin D2, interrupt service function is stateChange (), when the D2 power change from high to low , the trigger interrupt.

}

void loop() {
//Serial.println(number); // Printing the number of times of interruption, which is convenient for debugging.
    delay(1);
    if(detect)  //When a moving object is detected, the ledout is automatically closed after the light 2S, the next trigger can be carried out, and No need to reset. Convenient debugging.
    {
      Serial.println("ok");
        delay(1000);
        detect = false;
        digitalWrite(led, detect);    //turn off led
    }
}
