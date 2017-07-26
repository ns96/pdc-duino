/**
   A sketch for control of the SCK-200 kits using adafruit touch screen controller
   with Joystick. It as support for priitive close loop support.
*/
#include <Adafruit_ST7735.h> // Hardware-specific library
#include <SPI.h>
#include <Servo.h>

// specify the pins for the ESC and rpm sensor
#define escPin 6
#define rpmPin 2
#define intrPin 0 // On Arduino Uno this should be zero

// initialize the Servo object
Servo ESC1;

// specify whether we using 6V, 9V, or 12V park450 motor and speed resolution
#define RPM_RESOLUTION  15
#define ESC_VOLTS       9

// specify if to use open or cloased
#define isClosedLoop true

// For the breakout, you can use any 2 or 3 pins
// These pins will also work for the 1.8" TFT shield
#define TFT_CS     10
#define TFT_RST    9
#define TFT_DC     8
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);


// joytick related contants
#define Neutral 0
#define Press   1
#define Up      2
#define Down    3
#define Right   4
#define Left    5

// menu related constant
#define mainMenu    0
#define manualMenu  1
#define rampMenu    2

// constants for only clearing certain portion of the screen
#define clearW  120
#define clearH  16

// change ramp mode settings here
// format is low speed, lowspeed time, high speed, high speed time
int rampMode1[] = {800, 30, 2100, 45};
int rampMode2[] = {700, 15, 2500, 20};
int rampMode3[] = {700, 10, 3100, 15};
int rampMode[] = {0, 0, 0, 0};
int rampSpeedIndex = 0;
int rampTimeIndex = 1;

// keep track of the current menu amd menu choice
int currentMenu = mainMenu;
int menuChoice = 0;
int leftCount, rightCount, downCount, upCount, pressCount = 0;

// keep track of set rpm and measured rpms
int setRPM = 2000; // default rpm
int measuredRPM = 0; // the measured rpm
long sumRPM = 0; // use for taking average of rpm
int setPWM = 0; // the pwm which corresponds to the set rpm

// keep track of variables for calculating rpms
int pole_revolutions = 0; // counts the number of resolutions
int rmc = 0; // the amount of times rpms have been measured
unsigned long timeold = 0;

// keep track of the time and whether we are paused or started
int runTime = 0; // current time fpr a particular step
int totalTime = 0; // total time motor has been spinning
boolean isRunning = false;
unsigned long lc = 0; // keep track of time through loop

// keep track of the increment to increase speed by
int incr[] = {10, 50, 100, 500, 1000};
int incr_i = 2; // the index

// keep track of x, y postion to print out set rpm, rpm, time, increment
int srpm_x, mrpm_x, time_x, incr_x = 0;
int srpm_y, mrpm_y, time_y, incr_y = 0;

void setup() {
  // initial the ESC
  ESC1.attach(escPin); // attach the ESC to pin 10
  setSpeed(1000);

  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.println("Initializing PDC-Duino");

  // initialize the 1.8" TFT with Joystick
  tft.initR(INITR_BLACKTAB);

  // initialize the pin for the rpm sensor
  pinMode(rpmPin, INPUT_PULLUP);
  attachInterrupt(intrPin, rpmFunc, FALLING); 
  
  // rotatate the screen
  tft.setRotation(1);

  // set the text size and color here
  tft.setTextColor(ST7735_GREEN);
  tft.setTextSize(2);

  // display the splah screen
  displaySplash();

  // wait for the ESC to initialize
  //setSpeed(2000);
  Serial.println("***Turn ESC ON***");
  delay(8000);
  setSpeed(1000);

  // now draw the menu
  displayMain();
}

// Check the joystick position
int CheckJoystick() {
  int joystickState = analogRead(3);

  if (joystickState < 50) return Right;
  if (joystickState < 150) return Up;
  if (joystickState < 250) return Press;
  if (joystickState < 500) return Left;
  if (joystickState < 650) return Down;

  /*
  if (joystickState < 50) return Right;
  if (joystickState < 250) return Up;
  if (joystickState < 350) return Press;
  if (joystickState < 550) return Left;
  if (joystickState < 850) return Down;
  */
  
  return Neutral;
}

void clearScreen() {
  tft.fillScreen(ST7735_BLACK);
  tft.setCursor(0, 0);
}

// display the splay screen
void displaySplash() {
  clearScreen();

  tft.println("    INSTRAS\n");
  tft.println("  SCIENTIFIC\n");
  tft.println("   PDC-DUINO  ");
  tft.println("    ver 0.3  ");
}

// display the main
void displayMain() {
  currentMenu = mainMenu;
  clearScreen();

  tft.println("MAIN MENU\n");

  if (menuChoice == 0) {
    tft.println(" *MANUAL");
    tft.println("  RAMP #1");
    tft.println("  RAMP #2");
    tft.println("  RAMP #3");
  } else if (menuChoice == 1) {
    tft.println("  MANUAL");
    tft.println(" *RAMP #1");
    tft.println("  RAMP #2");
    tft.println("  RAMP #3");
  } else if (menuChoice == 2) {
    tft.println("  MANUAL");
    tft.println("  RAMP #1");
    tft.println(" *RAMP #2");
    tft.println("  RAMP #3");
  } else {
    tft.println("  MANUAL");
    tft.println("  RAMP #1");
    tft.println("  RAMP #2");
    tft.println(" *RAMP #3");
  }

  if(isClosedLoop) {
    tft.println("\nclosed-loop");
  } else {
    tft.println("\nopen-loop");
  }
}

void loop() {
  int joy = CheckJoystick();

  switch (joy)
  {
    case Left:
      leftCount++;
      //Serial.println("Left");
      break;
    case Right:
      rightCount++;
      //Serial.println("Right");
      break;
    case Up:
      upCount++;
      //Serial.println("Up");
      break;
    case Down:
      downCount++;
      //Serial.println("Down");
      break;
    case Press:
      pressCount;
      //Serial.println("Press");
      break;
  }

  delay(200); // pause to give button presses a chance to register only once or so

  if (joy != Neutral) {
    processJoystick(joy);
  }

  lc++;

  // if we are running refresh the screen and check to see if to advance or stop the motor
  // when in ramp mode
  if (isRunning) {
    if (lc % 5 == 0) { // update every second
      lc = 0;
      runTime++;
      totalTime++;
      
      updateScreen();

      // check to see if we are in ramp mode and need to increase the motor speed
      if (currentMenu == rampMenu) {
        if (runTime > rampMode[rampTimeIndex]) {
          // move to the next speed bump
          rampSpeedIndex += 2;
          rampTimeIndex += 2;

          if (rampTimeIndex == 3) {
            // move the motor to the new speed
            setRPM = rampMode[rampSpeedIndex];
            int pwm = computePWM(setRPM);
            setSpeed(pwm);
            runTime = 0;

            // redraw the whole screen so we indicate which ramp steep we are on
            displayRamp(2);
          } else {
            // stop the motor from running
            setRunning();

            // redraw the display
            displayRamp(1);
          }
        }
      }
    }
  }

  // this code block handles measuring rpm and close d loop control
  // get the desired rpm and set speed
  if (pole_revolutions >= 70) {
    rmc++;
    detachInterrupt(intrPin);

    int rpm = 8.6 * 1000 / (millis() - timeold) * pole_revolutions;
    timeold = millis();
    pole_revolutions = 0;

    sumRPM += rpm;

    // print out the rpms
    if (rmc % 5 == 0) {
      measuredRPM = sumRPM / 5;
      //Serial.print("Average RPM: ");
      Serial.print(totalTime);
      Serial.print("\t");
      Serial.print(setPWM);
      Serial.print("\t");
      Serial.println(rpm);
      
      sumRPM = 0;
    }

    if (rmc > 2 && isClosedLoop) {
      int pwm = computeNewPWM(rpm);
      setSpeed(pwm);
    }

    attachInterrupt(intrPin, rpmFunc, FALLING);
  }
}

// process jystick input
void processJoystick(int joy) {
  if (currentMenu == mainMenu) {
    if (joy == Up) {
      menuChoice--;
      if (menuChoice < 0) menuChoice = 0;
      displayMain();
    } else if (joy == Down) {
      menuChoice++;
      if (menuChoice > 3) menuChoice = 3;
      displayMain();
    } else if (joy == Press) {
      if (menuChoice == 0) {
        displayManual();
      } else {
        // initialzed the measured rpm and runTime to zero
        measuredRPM = 0;
        runTime = 0;

        // assign the correct ramp parameters
        if (menuChoice == 1) {
          memcpy(rampMode, rampMode1, 4 * sizeof(int));
        } else if (menuChoice == 2) {
          memcpy(rampMode, rampMode2, 4 * sizeof(int));
        } else {
          memcpy(rampMode, rampMode3, 4 * sizeof(int));
        }

        displayRamp(1);
      }
    }
  } else if (currentMenu == manualMenu) {
    if (joy == Left && leftCount >= 2) {
      leftCount = 0;
      isRunning = false;
      setSpeed(1000);
      displayMain(); // return to main menu
    } else if (joy == Up) {
      printSRPM(1);
    } else if (joy == Down) {
      printSRPM(-1);
    } else if (joy == Right) {
      printINCR();
    } else if (joy == Press) {
      setRunning();
    }
  } else if (currentMenu == rampMenu) {
    if (joy == Left && leftCount >= 2) {
      leftCount = 0;
      isRunning = false;
      setSpeed(1000);
      displayMain();
    } else if (joy == Press) {
      setRunning();
    }
  }
}

// update the rpm and time on the screen
void updateScreen() {
  printMRPM();
  printTIME();
}

// display the mnaul menu actions
void displayManual() {
  currentMenu = manualMenu;
  clearScreen();

  tft.println("MANUAL MODE\n");

  tft.print("S_RPM: "); // the set rpm
  srpm_x = tft.getCursorX(); // store the x and y for updating rpm
  srpm_y = tft.getCursorY();
  tft.println(setRPM);

  tft.print("M_RPM: "); // the measured rpms
  mrpm_x = tft.getCursorX(); // store the x and y for updating rpm
  mrpm_y = tft.getCursorY();
  tft.println("0");

  tft.print("TIME : "); // the measured rpms
  time_x = tft.getCursorX(); // store the x and y for updating rpm
  time_y = tft.getCursorY();
  tft.println("0");

  tft.print("\nINCR : "); // the increment to move rpms
  incr_x = tft.getCursorX(); // store the x and y for updating increment
  incr_y = tft.getCursorY();
  tft.println(incr[incr_i]);
}

// display the mnaul menu actions
void displayRamp(int step) {
  // finally draw the screen
  currentMenu = rampMenu;
  clearScreen();

  tft.print("RAMP MODE #");
  tft.println(menuChoice);
  tft.println();

  if (step == 1) {
    tft.print("*MIN ");
  } else {
    tft.print(" MIN ");
  }
  tft.print(padLeft(rampMode[0]));
  tft.print(" ");
  tft.println(rampMode[1]);

  if (step == 2) {
    tft.print("*MAX ");
  } else {
    tft.print(" MAX ");
  }
  tft.print(padLeft(rampMode[2]));
  tft.print(" ");
  tft.println(rampMode[3]);

  tft.print("\nRPM : "); // the measured rpms
  mrpm_x = tft.getCursorX(); // store the x and y for updating rpm
  mrpm_y = tft.getCursorY();
  tft.println(measuredRPM);

  tft.print("TIME: "); // the measured rpms
  time_x = tft.getCursorX(); // store the x and y for updating rpm
  time_y = tft.getCursorY();
  tft.println(runTime);
}

// print the set RPM
void printSRPM(int change) {
  setRPM += incr[incr_i] * change;

  // verify and correct the speed for valid ranges
  if (setRPM < 0) setRPM = 0;
  if (setRPM > 10000) setRPM = 10000;

  if (isRunning) {
    int pwm = computePWM(setRPM);
    setSpeed(pwm);
  }

  tft.fillRect(srpm_x, srpm_y, clearW, clearH, ST7735_BLACK);
  tft.setCursor(srpm_x, srpm_y);
  tft.print(setRPM);
}

// print the measured rpm
void printMRPM() {
  tft.fillRect(mrpm_x, mrpm_y, clearW, clearH, ST7735_BLACK);
  tft.setCursor(mrpm_x, mrpm_y);
  tft.print(measuredRPM);
}

// void print the time
void printTIME() {
  tft.fillRect(time_x, time_y, clearW, clearH, ST7735_BLACK);
  tft.setCursor(time_x, time_y);
  tft.print(runTime);
}

void printINCR() {
  incr_i++;
  if (incr_i >= 5) incr_i = 0;

  tft.fillRect(incr_x, incr_y, clearW, clearH, ST7735_BLACK);
  tft.setCursor(incr_x, incr_y);
  tft.print(incr[incr_i]);
}

void setSpeed(int pwm) {
  if (pwm != setPWM) {
    setPWM = pwm;
    //Serial.print("Setting ESC > ");
    //Serial.println(setPWM);
    ESC1.writeMicroseconds(setPWM);
  }
}

// function to either start or stop the motor from spinning
void setRunning() {
  if (!isRunning) {
    int pwm;

    if (currentMenu == manualMenu) {
      pwm = computePWM(setRPM);
    } else {
      // must be the ramp mode
      rampSpeedIndex = 0;
      rampTimeIndex = 1;
      setRPM = rampMode[rampSpeedIndex];
      pwm = computePWM(setRPM); // start at the lower speed
    }

    setSpeed(pwm);
    isRunning = true;
  } else {
    setSpeed(1000); // pause motor
    isRunning = false;
    runTime = 0;
    measuredRPM = 0;
    printMRPM();
    printTIME();
  }
}

// function to calculate the new pwm signal based on current and set rpms
int computeNewPWM(int rpm) {
  int diff = rpm - setRPM;
  int adiff = fabs(diff);

  // see how much to change the PWM base on how far way we are from desired rpm
  int change = 1;
  for (int i = 3; i <= 6; i++) {
    if (adiff > i * RPM_RESOLUTION) {
      change++;
      break;
    }
  }

  if (diff > 0 && adiff > RPM_RESOLUTION) {
    return setPWM - change;
  } else if (diff < 0 && adiff > RPM_RESOLUTION) {
    return setPWM + change;
  } else {
    return setPWM;
  }
}

// get the speed pwm given a certain rpm
int computePWM(int rpm) {
  long crpm = 0;
  int p = 0;

  for (p = 1000; p < 2000; p++) {
    //int crpm = -21457 +  * pww + -0.007 * square(pwm);
    if (ESC_VOLTS == 12) {
      crpm = -36745L + 50L * p + -0.014 * pow(p, 2);
    } else if (ESC_VOLTS == 9) {
      crpm = -26608L + 36L * p + -0.010 * pow(p, 2);
    } else {
      // assume 6 volts
      crpm = -17110L + 21.78L * p + -0.00564 * pow(p, 2);
    }

    if (fabs(rpm - crpm) < 20) {
      break;
    }
  }

  return p;
}

//Each rotation, this interrupt function runs 7 times for the 7 poles of the motor
void rpmFunc() {
  pole_revolutions++;
}

// padd a number
String padLeft(int number) {
  char buff[5];
  sprintf(buff, "%04d", number);
  return String(buff);
}
