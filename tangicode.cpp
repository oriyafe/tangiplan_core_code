// This #include statement was automatically added by the Spark IDE.
#include "neopixel/neopixel.h"



//Wifi & Http
TCPClient client;
char *httpResponse;
int httpResponseSize = 128;
//char urlServer[] = "ec2-54-200-22-27.us-west-2.compute.amazonaws.com";
char urlServer[] = "ec2-52-25-185-126.us-west-2.compute.amazonaws.com";


char urlGetDuration[] = "/TangiPlan/getDuration/";
char urlCloseTask[] = "/TangiPlan/setDuration/";
char urlObjectOn[] = "/TangiPlan/objectOn/";
//char setid[] = "3/";


#define OBJECTID 1

long httpGetTimer;
#define GETREQUESTCHECKDELAY 5000

// NeoPixel
#define NEOPIXEL_PIN D2
#define NUMOFPIXELS 20
#define NEOPIXEL_TYPE WS2812B
#define RED strip.Color(10, 0, 0)
#define GREEN strip.Color(0, 10, 0)
#define BLUE strip.Color(0, 0, 10)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMOFPIXELS, NEOPIXEL_PIN, NEOPIXEL_TYPE);
//Array to keep current color of each led on the strip
uint32_t leds[NUMOFPIXELS] ;
//Fading Led Variables
int colorFadeLevel = 10;
int LastLedToFade = 0;
boolean isRed = false;
boolean isLightGoingUp  = true;
float calc ;
int ledLightIntensity = 5;
int fadeLevelArray[] = {10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1};
long fadeLedCheck = 0;
//End Fading

// Timeframe = total object time if == 5*60*1000 then each led pixel will represent 5*60/NUMOFPIXELS seconds
#define TIMEFRAME (10 * 60 * 1000)
#define TIMECONSTRAINTBETWEENBUTTONCLICKS 10000
int numberOfLedsOn  = NUMOFPIXELS;


//Button Variables
int button = D0;
long timeFromLastClick = 0;
boolean isObjectOn = false;
//State variables
long taskScheduledDuration;
long currentTaskDuration;
long startTaskTime;

long endFillEffectCheck = 0;
int taskId;
int state = 0 ;
bool taskStarted = false;
bool DEBUG = true;
bool isUserFinished = false;
//object that will help in saving to eeprom
long statePrintDelay = millis();
long firstTenSeconds;
bool isFinishedWritingToEEPOnceAfterWifiOn = true;
#define WAKEUPHOUR 6
#define SLEEPHOUR 10
bool isGotTask = false;
String resultCloud = "";

void setup()
{
  //state = Checking to see if eeprom holds previous duration
  state = 777;
  Serial.begin(9600);
  httpGetTimer = millis();
  fillStrip(BLUE, NUMOFPIXELS);
  taskScheduledDuration = 0;
  pinMode(button, INPUT);
  firstTenSeconds = millis();
  //Change this when daylight saving changes
 // saveToEEP(String(" "), 1);
  Time.zone(+3);
  //Serial.end();
  Spark.variable("resultCloud", &resultCloud, STRING);

  // one time alloction for the HTTP response
  httpResponse = (char*) malloc(httpResponseSize * sizeof(char));
}

void loop()
{

  //THIS IF WILL CAUSE THE CORE TO WAIT FOR 10
  //SECONDS GIVING US TIME TO UPLOAD NEW FIRMWARE IF WE MAKE A MISTAKE AND CAUSE OUR CODE TO FREEZE/CRASH
  if (millis() - firstTenSeconds < 10000) {
    SPARK_WLAN_Loop();
    //DEBUG color red, remove this so user won't see
    // fillStrip(RED * 0.1, NUMOFPIXELS);
    // refreshStrip();
  } else {

    // sent to log that object woke up.
    if(!isObjectOn) {
      sendToLog(urlObjectOn, &isObjectOn);
    }

    //If not inside the morning routine, go to sleep.
    checkIfNeedToGoToSleepBecauseOutsideOfUpHours();
    //If no connection was made or user forgot to asign task we don't want to waste batteries so go to sleep
     checkIfNeedToGoToSleepBecauseNoTaskOrInternet();

    switch (state)
    {
        //Check if stored something in EEPROM - meaning we crashed before we closed the task
        //If so update the server and continue.
        //If not continue
      case 777: {
         // fillStrip(BLUE, 7);
          String arguments = readFromEEP();
          //debug("eeprom:");
          //debug(arguments);
          if (arguments.indexOf("/") != -1) {
            char charBuf[arguments.length() + 1];
            arguments.toCharArray(charBuf, arguments.length() + 1);
            //There is something in the eeprom
            getRequestWithDelay( urlCloseTask, charBuf);
            //check that returned OK meaning we finished the close task request
            if (checkSuccessCloseTask(checkForHttpResult())) {
              finishedCheckingEEP();
              //debug(arguments);
              //Clean the eep (important ! - happens only once).
              saveToEEP(String(' '), 1);
              goToDeepSleep(calcluateHowManySecondsToSleepUntil(WAKEUPHOUR));
            }
          } else {
            //Nothing in the eeprom move along;
            finishedCheckingEEP();
          }

        }
        break;
        
        // Ghost state for debug.
      case 700: {
        //  fillStrip(BLUE, 8);
          // start the normal code
          state = 0;
        }
        break;
      case 0: {
          SPARK_WLAN_Loop();
        //  fillStrip(BLUE, 1);
          // debug
          //fillStrip(BLUE, NUMOFPIXELS);
          refreshStrip();

          char args[] = " ";
          getRequestWithDelay(urlGetDuration, args);
          debug("before what we did");
          String result = checkForHttpResult();
          debug("after what we did");
          resultCloud = result;
          debug("Result:");
          debug(result);
          debug("Result Cloud:");
          debug(resultCloud);
          // OP: cleanups: freeing the alocated HTTP response
          //free (res);

         
          if (result != "") {
            getDurationFromHttpResult(result);
            getTaskIDFromHttpResult(result);
          }
          
          if (taskScheduledDuration != 0) {
              // if there is no new task skip to sleep (state 6)
           if (taskScheduledDuration < 0) {
               state = 6;
           } else {
            state = 1;
            isGotTask = true;
            WiFi.off();
            fillStrip(1, NUMOFPIXELS);
            refreshStrip();
           }
          }
          refreshStrip();
        }
      
        break;
        //standby until first button click
      case 1: {
       //   fillStrip(GREEN, 2);
          //debug("WaitForFirstClick");
          //go dark and wait for first click
          fillStrip(BLUE, 1);
          if (checkButtonState()) {
            state = 2;
          }
        }
        break;
      case 2: {
       //   fillStrip(GREEN, 3);
          //debug("starting Led");
          taskCountDown();

            // if the user pushed the button update to state 3 and boolen isUserFinished update to true.
          if (checkButtonState()) {
          isUserFinished = true;
            state = 3;
          }
    
    // if the time was over before user pushed the button update to state 3 and boolen isUserFinished update to false.
	 if (checkTimeIsUp()) {
              isUserFinished = false;
            state = 3;
          }

        }
        break;
        
        //
      case 3: {
       // finish blue effect.
          if (finishedBlueEffect()) {
            state = 4;
          }
        }
        break;
      case 4: {
        //  fillStrip(GREEN, 5);
          //debug("Going out of sleep");
          WiFi.on();
          WiFi.connect();
          state = 5;
        }
        break;
        //Wait for connection to be made
      case 5: {
    //      fillStrip(GREEN, 6);
        //debug("step 5");
          String durationString = String(currentTaskDuration);
          String isUserFinishedToString = String(isUserFinished);
          String argsString = "/" + durationString + "/" + isUserFinishedToString;
     //     String argsString = "/" + durationString;
          int argsStringLength = argsString.length() + 1;
          //Length + 1 for the end char
          char charBuf[argsStringLength];
          argsString.toCharArray(charBuf, argsStringLength);
          //the if makes sure saving to EEP happens only once, we can only write to eep 100K times before its ruined.
          if (isFinishedWritingToEEPOnceAfterWifiOn) {
                      //debug("step 5.1");

            saveToEEP(argsString, argsString.length());
            isFinishedWritingToEEPOnceAfterWifiOn = false;
          }
          if (WiFi.ready()) {
           //debug("step WIFI.ready:");
            getRequestWithDelay(urlCloseTask, charBuf);
            //check that returned OK meaning we finished the close task request
            if (checkSuccessCloseTask(checkForHttpResult())) {
            //debug("step 6:");

              state = 6;
              //Clearing the eep becaues the task was reported successfully so theres no reason to save it, happens only once.
              saveToEEP(String(" "), 1);
            }
           }else{
              WiFi.on();
          // WiFi.connect();
                 //debug("step 5.2");
          }
        }
        break;

        //Go to sleep till next time
      case 6: {
       //   fillStrip(GREEN, 7);
          //debug("going to deep sleep");
          fillStrip(0, NUMOFPIXELS);
          refreshStrip();
          Spark.sleep(SLEEP_MODE_DEEP, calcluateHowManySecondsToSleepUntil(WAKEUPHOUR));
        }
        break;

    }
    if (millis() - statePrintDelay > 2000) {
      //debug(state);
      statePrintDelay = millis();
      String arguments = readFromEEP();
      //debug("eeprom:");
      //debug(arguments);
      debug("Time: ");
      debug(Time.hour());
    }
    refreshStrip();
  }
}

void finishedCheckingEEP() {
  state = 700;
}
//Saves string to eep memory
//The EEPROM is only writeable up to 100,000 times, so careful when using this inside a loop!
void saveToEEP(String str, int length) {
  for (int i = 0; i < 100; i++) {
    if (i < length) {
      EEPROM.write(i, str[i]);
    } else {
      EEPROM.write(i, ' ');
    }
  }
}

String readFromEEP() {
  char letter;
  //just 35 first letters we don't require more at the moment
  char charArray[35];
  for (int i = 0; i < 35; i++) {
    letter = EEPROM.read(i);
    charArray[i] = letter;
  }
  return String(charArray);
}

String sendGetRequest( const char * url, const char * args)
{
  String result = "";
  
if (client.connect({52,25,185,126}, 80)) {
    //debug("connected");
    client.print("GET ");
    client.print(url);
//    client.print(setid);
    client.print(OBJECTID);
 //   client.print(SETID);
    if (sizeof(args) > 3) {
      client.print(args);
    }
    client.println(" HTTP/1.0");
    // client.println("Connection: close");
    client.print("Host: ");
    client.println("52.25.185.126");
    client.println("Accept: text/html, text/plain");
    client.println();
    result = "connected";
  }  else
  {
    //debug("connection failed");
    result = "failed";
  }
  return result;
}

String getRequestWithDelay( char url[], char args[]) {
  if (millis() - httpGetTimer > GETREQUESTCHECKDELAY ) {
    httpGetTimer = millis();
    return sendGetRequest( url, args);
  }
  else return "false";
}

void getDurationFromHttpResult(String result) {
  if (checkForValidResult(result)) {
    //Get only the content without the headers
    String resultContent = result.substring(result.indexOf("\r\n\r\n"));
    //Get the duration
    String durationString = resultContent.substring(resultContent.indexOf(":") + 1);

    taskScheduledDuration = stringToLong(durationString);

    //debug("  Duration : ");
    //debug(taskScheduledDuration);
  }
}

void getTaskIDFromHttpResult(String result) {
  if (checkForValidResult(result)) {
    //Get only the content without the headers
    String resultContent = result.substring(result.indexOf("\r\n\r\n"));
    //Get the taskid
    String taskIdString = resultContent.substring(0, resultContent.indexOf(":"));

    taskId = (int)stringToLong(taskIdString);

    //debug("  taskId : ");
    //debug(taskId);
  }
}

long stringToLong(String str) {
  char longbuf[32]; // make this at least big enough for the whole string
  cleanChars(longbuf, 32);
  str.toCharArray(longbuf, sizeof(longbuf));
  return atol(longbuf);
}

void cleanChars(char* charArray, int length) {
  //make sure theres no garbage in the memory
  for (int j = 0; j < length; j++) {
    charArray[j] = *"";
  }
}

bool checkForValidResult(String result) {
  if (result.indexOf("\r\n\r\n") != -1) {
    if (result.indexOf(":") > 0 ) {
      return true;
    } else
      return false;
  } else
    return false;
}



String checkForHttpResult() {
    int position = 0;
    memset(httpResponse, 0, httpResponseSize);
  while (client.available())
  {
      if(position < httpResponseSize)
      {
        char letter = client.read();
        httpResponse[position] = letter;
        position += 1;
      }
      else
      {
          // OP: double the array size and copy old content and read again
          httpResponseSize *= 2;
          char* tempResDouble = (char*)malloc(httpResponseSize*sizeof(char));
          memset(tempResDouble ,0,httpResponseSize);
          strncpy(tempResDouble, httpResponse, httpResponseSize/2);
          free(httpResponse);
          httpResponse = tempResDouble;
      }
  }

  if (!client.connected())
  {
    client.flush();
    client.stop();
  }

  httpResponse[position] = '\0';
  return httpResponse;
}

// Check if the task was closed - check if there is "OK".
bool checkSuccessCloseTask(String result) {
  if (result.length() > 0) {
    if (result.indexOf("OK") != -1) {
      return true;
    }

  }
  return false;
}

// Check if the buttun was clicked.
bool checkButtonState() {
    // TIMECONSTRAINTBETWEENBUTTONCLICKS in order to prevent double-click on the button.  
  if (millis() - timeFromLastClick > TIMECONSTRAINTBETWEENBUTTONCLICKS ) {
    if (digitalRead(button) == 1) {
      timeFromLastClick = millis();
      return true;
    } else return false;
  }
  else return false;
}


void fillStrip(uint32_t color, int numberOfOnPixels) {
  //Fill with color
  for (int i = 0; i < NUMOFPIXELS; i++) {
    // if i is bigger then numberOfOnPixels don`t color it
    if (i >= numberOfOnPixels) {
      leds[i] = strip.Color(0, 0, 0);
    }
    // fill led i with color
    else {
      leds[i] = color;
    }
  }
}

void refreshStrip() {
  // Updates all the leds on the strip
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, leds[i]);
  }
  strip.show();
}


void fadeLedEffect(int ledNumber, uint32_t color) {
  if ( millis() - fadeLedCheck > 120) {

    fadeLedCheck = millis();
    //Fade the last led:
    if (isLightGoingUp) {
      colorFadeLevel++;
    }
    else {
      colorFadeLevel--;
    }
    if (colorFadeLevel >=  10) {
      isLightGoingUp = false;
    }
    else if (colorFadeLevel <= 0) {
      isLightGoingUp = true;
    }
  }

  if (ledNumber > NUMOFPIXELS) {
    //debug("Error in fadeLedEffect:led number larger then number of pixels");
    ledNumber = NUMOFPIXELS;
  } else  if (ledNumber < 0) {
    //debug("Error in fadeLedEffect:led number smaller then 0");
    ledNumber = 0;
  }

  if (color == GREEN) {
    leds[ledNumber] = GREEN * colorFadeLevel;
  } else if (color == RED) {
    leds[ledNumber] = RED * colorFadeLevel;
  } else if (color == BLUE) {
    leds[ledNumber] = BLUE * colorFadeLevel;
  }
}

void taskCountDown(void) {

  if (!taskStarted) {
    currentTaskDuration = 0;
    startTaskTime = millis();
    taskStarted = true;
  }

  currentTaskDuration = millis() - startTaskTime;
  //prevent led blink from the first led
  if (currentTaskDuration == 0) {
    currentTaskDuration++;
  }

  long timeLeft = taskScheduledDuration - currentTaskDuration;
  //debug(String(timeLeft));
  //TODO::  FIX TASKS LONGER THEN TIMEFRAME HALTS THE PROGRAM
  calc = (float)timeLeft / (float)TIMEFRAME;
  if (calc > 1) {
    calc = 1;
  }

  //debug(String(calc));
  numberOfLedsOn  =  (int)(abs(calc * NUMOFPIXELS)) + 1;
  //first led is 0 in the led array
  numberOfLedsOn --;
  if (numberOfLedsOn >= NUMOFPIXELS) {
    numberOfLedsOn = NUMOFPIXELS - 1;
  }
  if (numberOfLedsOn < 0) {
    numberOfLedsOn = 0;
  }
  //If we moved to another lead reset fade.
  if (LastLedToFade != numberOfLedsOn) {
    colorFadeLevel = 10;
    isLightGoingUp = false;
  }

  LastLedToFade = numberOfLedsOn;

  if ( timeLeft > 0) {
    fillStrip(GREEN * ledLightIntensity, numberOfLedsOn  );
    isRed = false;
  }
  else {
    fillStrip(RED * ledLightIntensity, numberOfLedsOn);
    isRed = true;
  }

  if (!isRed) {
    fadeLedEffect(LastLedToFade, GREEN);
  }
  else {
    fadeLedEffect(LastLedToFade, RED);
  }

}

bool finishedBlueEffect() {
  if (millis() - endFillEffectCheck > (NUMOFPIXELS - 1)) {
    endFillEffectCheck = millis();
    numberOfLedsOn++;
    if ( numberOfLedsOn <= NUMOFPIXELS) {
      fillStrip(BLUE, numberOfLedsOn);
    }
    else if ( numberOfLedsOn <= NUMOFPIXELS * 2) {
      fillStrip(BLUE, (NUMOFPIXELS * 2) - numberOfLedsOn);
    }
    else {
      fadeLedEffect(0, BLUE);
      return true;
    }

  }
  return false;
}


//Calculates how many milli seconds to sleep until hourToWakeUp = 0-23
long calcluateHowManySecondsToSleepUntil(int hourToWakeUp) {
  int hours = hourToWakeUp - Time.hour();
  long total = 0;
  if (hours <= 0 ) {
    hours += 24;
  }

  int minutes = Time.minute();
  total = ( ( hours * 60 ) - minutes ) * 60 ;

  return total;
}


void checkIfNeedToGoToSleepBecauseOutsideOfUpHours() {
  //In case the core suddenly wakes up sometime not between the morning routine, go to sleep
  if (Time.hour() < WAKEUPHOUR || Time.hour() > SLEEPHOUR) {
    goToDeepSleep(calcluateHowManySecondsToSleepUntil(WAKEUPHOUR));
  }
}

void checkIfNeedToGoToSleepBecauseNoTaskOrInternet() {
  //If awake for two minutes without a task
  if (millis() > 2 * 60 * 1000) {
    if (!isGotTask) {
      goToDeepSleep(5 * 60);
    }
  }
}

void goToDeepSleep(int secondsToSleep) {
  // If the task was not done  - delete it.
  saveToEEP(String(" "), 1);
  fillStrip(0, NUMOFPIXELS);
  refreshStrip();
  Spark.sleep(SLEEP_MODE_DEEP, secondsToSleep);
}


bool checkTimeIsUp() {
  if ( currentTaskDuration > taskScheduledDuration + TIMEFRAME) {
    return true;
  }
  return false;
}


// send to log method
// isDone is a boolean for one time avtivation
void sendToLog( char url[], bool * isDone) {
       // Wifi.ready() case. else Wifi.on().
        if (WiFi.ready()) {
            // String of current time. (+7200 because of time zone problem)
         String timeStamp = String((Time.now() + 7200));
          String argsString1 = "/" + timeStamp;
          
          int argsStringLength1 = argsString1.length() + 1;
          char charBuf1[argsStringLength1];
          argsString1.toCharArray(charBuf1, argsStringLength1);
          getRequestWithDelay(url, charBuf1);
            if (checkSuccessCloseTask(checkForHttpResult())) {
              *isDone = true;
            }
           }else{
              WiFi.on();
          }
    }


// Print string to serial
void debug(String str) {
  if (DEBUG) {
    Serial.println(str);
  }
}
// Print long to serial
void debug(long str) {
  if (DEBUG) {
    Serial.println(str);
  }
}

// Print int to serial
void debug(int str) {
  if (DEBUG) {
    Serial.println(str);
  }
}
