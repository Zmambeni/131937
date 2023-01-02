// include libraries
#include "Adafruit_GFX.h"
#include "MCUFRIEND_kbv.h"
#include "TouchScreen.h"
#include <Servo.h>
#include <TrueRandom.h>

#include "utils.h"    // utils file, in the root directory, contains utility files

// set up coin detection, using esp
#define COIN_IN A5
boolean detected = false;

#define DURATION 10000
int aDuration = 0;
int bDuration = 0;
boolean aCharging = false;
boolean bCharging = false;

int HEIGHT = 0, WIDTH = 0, ST = 0, BH = 0, BW = 0;
int pixel_x, pixel_y;

String password = "";
boolean is_valid = false;

int SCREEN_TYPE = 0; // value to keep track of which screen is open,
// 0 - Welcome Screen, 1 - Enter password, 2 - Confirm passord, 3 - Prompt user to enter cash,
// 4 - Confirm money, 5 Charging screen, done all - 5
// choose port and coin  - 3

const int XP = 6, XM = A2, YP = A1, YM = 7; //240x320 ID=0x9341
const int TS_LEFT = 181, TS_RT = 918, TS_TOP = 946, TS_BOT = 204;

String PORT_A_PASSWORD = "", PORT_B_PASSWORD = "";    // hold passwords for both ports independetyl
bool A_AVAILABLE = true, B_AVAILABLE = true;
int selected_port = -1; // 0 - port A, 1 - port B

MCUFRIEND_kbv tft(A3, A2, A1, A0, A4);
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

Servo servoA, servoB;

const int PORT_A_MOTOR = 10;
const int PORT_B_MOTOR = 11;

const int PORT_A_RELAY = 12;
const int PORT_B_RELAY = 13;

void setup() {
  Serial.begin(9600);

  uint16_t ID = tft.readID();
  tft.begin(ID);
  Serial.println(ID);

  // tft.setRotation(1); change orientation
  tft.fillScreen(WHITE);

  // measurement of tft module
  HEIGHT = tft.height();
  WIDTH = tft.width();

  ST = 70;
  BW = WIDTH / 3;
  BH = ( HEIGHT - ST ) / 4;

  welcomeScreen();
  // drawKeyPads();
  // confirmPassword();
  // enterCoinAndPort();

  pinMode(COIN_IN, INPUT); // setup coin detection, reading from esp32
  digitalWrite(COIN_IN, LOW);

  pinMode(PORT_A_MOTOR, OUTPUT);
  pinMode(PORT_B_MOTOR, OUTPUT);

  servoA.attach(PORT_A_MOTOR);
  servoB.attach(PORT_B_MOTOR);
  servoA.write(90);
  servoB.write(90);

  pinMode(PORT_A_RELAY, OUTPUT);
  pinMode(PORT_B_RELAY, OUTPUT);

  digitalWrite(PORT_A_RELAY, HIGH); // HIGH means off?????
  digitalWrite(PORT_B_RELAY, LOW); 
}

void loop() {
  // put your main code here, to run repeatedly:
  bool is_pressed = calculateIsPressed();

  if (detectCoin() && SCREEN_TYPE == 3) {
    coinInserted();
    SCREEN_TYPE = 4;
    Serial.println("Coin detected");
  }

  if (is_pressed) {
    if (SCREEN_TYPE == 0) {
      // used to detect chosing if you want to charge, or discharge
      inputCoinOrDischargeScreen();
    }
    else if (SCREEN_TYPE == 1) {
      // used to select discharge port
      detectDischargeScreeen();
    }
    else if (SCREEN_TYPE == 2) {
      // used to confirm generated password
      detectConfirmPassword();
    }
    else if (SCREEN_TYPE == 3) {
      // used to detect exit button if both ports are being used
      detectCoinExit();
    }
    else if (SCREEN_TYPE == 4) {
      // used to detect selected port
      detectSelectedPort();
    }
    else if (SCREEN_TYPE == 5) {
      // used to detect exit button in charging screen
      detectChargingScreen();
    }
    else if (SCREEN_TYPE == 6) {
      // used to detect port password
      detectPressedKey();
    }
    else if (SCREEN_TYPE == 7) {
      // detect unlock port
      detectUnlockPort();
    }
    else if (SCREEN_TYPE == 8) {
      detectPortUnlocked();
    }
  }

  // check if time has not ellapsed
  checkCharginDuration();
  
}

String generatePassword() {
  // function to generate a four digit number, return as a string for easy manipulation in code
  // return String(random(1000, 9999));  // old way
  return String(TrueRandom.random(1000, 9999));   // new true random 
}

void welcomeScreen() {
  // screeen to display when booting, or in idle mode
  tft.fillScreen(WHITE);  //clear screen
  updateMode("IDLE");

  tft.setCursor(0, 70);
  tft.setTextColor(BLUE);
  tft.setTextSize(3);
  tft.println("  WELCOME TO");
  tft.println("  E-CHARGING");
  tft.println("    BOOTH!");

  tft.setTextSize(1);
  tft.setTextColor(RED);
  tft.setCursor(10, HEIGHT - 20);    // 10 units from bottom
  tft.println("Note: We accept only E5 coin!");

  tft.setTextSize(2);
  tft.setTextColor(WHITE);
  tft.fillRect(0, 175, WIDTH / 2 - 5, BH, BLACK);
  tft.setCursor(BW / 2, 175 + BH / 3);
  tft.print("IN");
  tft.fillRect(WIDTH / 2 + 5, 175, WIDTH / 2 - 5, BH, BLACK);
  tft.setCursor(BW * 2 + BW / 3, 175 + BH / 3);
  tft.print("OUT");
}

void inputCoinOrDischargeScreen() {
  if (pixel_y > 175 && pixel_y < 175 + BH) {
    if (pixel_x > 0 && pixel_x < WIDTH / 2 - 5) {
      // user wants to charge phone
      enterCoinAndPort();
      SCREEN_TYPE = 3;
      delay(100);
    } else if (pixel_x > WIDTH / 2 + 5 && pixel_x < WIDTH) {
      // user wants to disconnect phone
      dischargePhone();
      SCREEN_TYPE = 1;
      delay(100);
    }
  }
}

void dischargePhone() {
  // SCREEN_TYPE = 1
  tft.fillScreen(WHITE);  // clear screen
  updateMode("DISCHARGE");

  // test if a port is available for discharging, if not notify user and
  // navigate back to home screen
  if (A_AVAILABLE && B_AVAILABLE) {
    tft.setCursor(0, 70);
    tft.setTextColor(BLUE);
    tft.setTextSize(3);
    tft.println("  No phones");
    tft.println("  curently");
    tft.println(" plugged in!");

    tft.fillRect(BW, ST + BH * 3, BW, BH, BLACK);
    tft.setCursor(BW + BW / 5, ST + BH * 3 + BH / 2);
    tft.setTextSize(2);
    tft.setTextColor(WHITE);
    tft.println("BACK");

    return;
  }

  // enter cancel button
  tft.fillRect(WIDTH - 30, 0, 50, 40, BLACK);
  tft.setCursor(WIDTH - 20, 10);
  tft.setTextColor(RED);
  tft.setTextSize(2);
  tft.print("X");


  tft.setCursor(10, 70);
  tft.setTextColor(BLACK);
  tft.setTextSize(3);
  tft.println(" Select port");
  tft.println("");
  tft.println(" you want to");
  tft.println("  discharge?");

  // Draw port buttons and their text A - Port A, B - Port B
  tft.setTextSize(2);
  tft.setTextColor(WHITE);


  if (!A_AVAILABLE) {
    tft.fillRect(0, HEIGHT - BH - 40, BW, BH, RED);
    tft.setCursor(BW / 2, HEIGHT - BH - 40 + BH / 3);
    tft.print("A");
  }

  if (!B_AVAILABLE) {
    tft.fillRect(BW * 2, HEIGHT - BH - 40, BW, BH, RED);
    tft.setCursor(BW * 2 + BW / 2, HEIGHT - BH - 40 + BH / 3);
    tft.print("B");
  }

  tft.setTextSize(1);
  tft.setTextColor(RED);
  tft.setCursor(10, HEIGHT - 20);    // 10 units from bottom
  tft.println("NB: Port password required to unlock!!");
}

void detectDischargeScreeen() {
  // detect back button if both ports are available
  if (A_AVAILABLE && B_AVAILABLE) {
    if (pixel_x > BW && pixel_x < BW * 2) {
      if (pixel_y > ST + BH * 3) {
        welcomeScreen();
        SCREEN_TYPE = 0;
      }
    }
    return;
  }

  if (pixel_y > HEIGHT - BH - 40 && pixel_y < HEIGHT - 40) {
    // detect row where both button are drawn
    if (!A_AVAILABLE && pixel_x < BW) {
      // detected port A;
      selected_port = 0;
      drawKeyPads();
      SCREEN_TYPE = 6;
    }
    else if (!B_AVAILABLE && pixel_x > BW * 2) {
      // detected port B for discharge
      selected_port = 1;
      drawKeyPads();
      SCREEN_TYPE = 6;
    }
  }

  if (pixel_y < 40 && pixel_x > WIDTH - 50 ) {
    welcomeScreen();
    SCREEN_TYPE = 0;
  }
}

void drawKeyPads() {
  // SCREEN_TYPE = 6

  tft.fillScreen(WHITE);  //clear screen

  // draw first column
  tft.fillRect(0, ST, BW, BH, BLACK);
  tft.fillRect(0, ST + BH, BW, BH, BLACK);
  tft.fillRect(0, ST + BH * 2, BW, BH, BLACK);
  tft.fillRect(0, ST + BH * 3, BW, BH, RED);

  // draw second column
  tft.fillRect(BW, ST, BW, BH, BLACK);
  tft.fillRect(BW, ST + BH, BW, BH, BLACK);
  tft.fillRect(BW, ST + BH * 2, BW, BH, BLACK);
  tft.fillRect(BW, ST + BH * 3, BW, BH, BLACK);

  // draw thirth column
  // draw first column
  tft.fillRect(BW * 2, ST, BW, BH, BLACK);
  tft.fillRect(BW * 2, ST + BH, BW, BH, BLACK);
  tft.fillRect(BW * 2, ST + BH * 2, BW, BH, BLACK);
  tft.fillRect(BW * 2, ST + BH * 3, BW, BH, GREEN);

  // DISPLAY BOX
  tft.fillRect(0, 0, WIDTH, ST, CYAN);

  tft.fillRect(WIDTH - 80, 0, 80, ST, RED);
  tft.setCursor(WIDTH - 40, 24);
  tft.setTextSize(2);
  tft.setTextColor(WHITE);
  tft.print("X");

  // draw vertical lines
  for (int v = 0; v <= WIDTH; v += BW) tft.drawFastVLine(v, ST, BH * 4, WHITE);
  // draw horizontal lines
  for (int h = ST; h <= HEIGHT; h += BH) tft.drawFastHLine(0, h, WIDTH, WHITE);

  String LABEL[4][3] = {
    { "7", "8", "9" },
    { "4", "5", "6" },
    { "1", "2", "3" },
    { "C", "0", "OK" }
  };

  // draw labels
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 3; j ++) {
      tft.setCursor(BW * j + BW / 2, ST + BH * i + BH / 3);
      tft.setTextSize(2);
      tft.setTextColor(WHITE);
      tft.println(LABEL[i][j]);
    }
  }

}

void detectPressedKey() {
  bool skip_pads = false; // dummy variable, when this this true, don't print keypads, simply move to next screen

  // if is exit button
  if (pixel_y > 0 && pixel_y < ST && pixel_x > WIDTH - 80 && pixel_x < WIDTH) {
    welcomeScreen();
    SCREEN_TYPE = 0;
    password = "";
    return;
  }

  if (pixel_y > ST && pixel_y < ST + BH) {
    // detect first row; 7, 8, 9

    if (pixel_x < BW ) {
      // found 7
      password += "7";
    } else if (pixel_x > BW && pixel_x < BW * 2) {
      // found 8
      password += "8";
    } else {
      // found 9
      password += "9";
    }

  }
  else if (pixel_y > ST + BH && pixel_y < ST + BH * 2) {
    // detect second row; 4, 5, 6
    if (pixel_x < BW ) {
      // found 4
      password += "4";
    } else if (pixel_x > BW && pixel_x < BW * 2) {
      // found 5
      password += "5";
    } else {
      // found 6
      password += "6";
    }

  }
  else if (pixel_y > ST + BH * 2 && pixel_y < ST + BH * 3) {
    // detect third row; 1, 2, 3
    if (pixel_x < BW ) {
      // found 1
      password += "1";
    } else if (pixel_x > BW && pixel_x < BW * 2) {
      // found 2
      password += "2";
    } else {
      // found 3
      password += "3";
    }
  } else {
    // last row
    if (pixel_x < BW ) {
      // found Clear, C
      password = "";
    } else if (pixel_x > BW && pixel_x < BW * 2) {
      // found 0
      password += "0";
    } else {
      // found Ok
      // check if password is correct and discharge phone
      validatePassword();
      SCREEN_TYPE = 7;
      skip_pads = true;
    }
  }

  // if we are not skipping pads, re-draw keypads, this is dumb and inefficient :)
  if (!skip_pads) {
    // tft.fillScreen(WHITE);
    // tft.fillRect(0, 0, 240, ST, WHITE);  //clear result box
    drawKeyPads();
    tft.setCursor(10, 20);
    tft.setTextSize(4);
    tft.setTextColor(BLACK);
    tft.println(password);
    delay(100);
  }
}

void validatePassword() {
  // SCREEN_TYPE = 7
  tft.fillScreen(WHITE);  // clear screen
  updateMode("VALIDATING");

  if (selected_port == 0) {
    // check if input password is similiar to set port A password
    is_valid  =  (password == PORT_A_PASSWORD);
  } else if (selected_port == 1) {
    is_valid = (password == PORT_B_PASSWORD);
  }
  password = "";    // reset password

  tft.setCursor(10, 70);
  tft.setTextColor(BLACK);
  tft.setTextSize(3);
  tft.print("VALID: ");
  if (is_valid) {
    tft.setTextColor(GREEN);
    tft.println("YES");
  } else {
    tft.setTextColor(RED);
    tft.println("NO");
  }
  tft.setCursor(10, 100);
  tft.setTextColor(BLACK);
  tft.setTextSize(3);
  tft.print("PORT:  ");
  if (selected_port == 0) {
    tft.println("A");
  } else {
    tft.println("B");
  }

  tft.setCursor(10, 150);
  tft.setTextColor(BLACK);
  tft.setTextSize(2);
  if (is_valid) {
    tft.println("Password is valid.");
    tft.println(" Proceed.");
  } else {
    tft.println("Password invalid.");
    tft.println(" Try again!!");
  }

  if (is_valid) {
    tft.fillRect(10, HEIGHT - 50, WIDTH - 20, 40, BLACK);
    tft.setCursor(20, HEIGHT - 50 + 12);
    tft.setTextColor(WHITE);
    tft.println("UNLOCK & DISCHARGE");
  } else {
    tft.fillRect(BW, ST + BH * 3, BW, BH, BLACK);
    tft.setCursor(BW + BW / 5, ST + BH * 3 + BH / 2);
    tft.setTextSize(2);
    tft.setTextColor(WHITE);
    tft.println("BACK");
  }
}

void detectUnlockPort() {
  if (is_valid && pixel_y > HEIGHT - 50 && pixel_y < HEIGHT - 10) {
    if (pixel_x > 10 && pixel_x < WIDTH - 20) {
      // detected unlock and discharge button
      portUnlockedScreen();
      SCREEN_TYPE = 8;
    }
  }
  else if (!is_valid && pixel_x > BW && pixel_x < BW * 2) {
      if (pixel_y > ST + BH * 3) {
        welcomeScreen();
        SCREEN_TYPE = 0;
      }
    }
  
}

void portUnlockedScreen() {
  // SCREEN_TYPE = 8
  tft.fillScreen(WHITE);  //clear screen
  updateMode("DONE");

  tft.fillRect(10, HEIGHT - 50, WIDTH - 20, 40, BLACK);
  tft.setCursor(40, HEIGHT - 50 + 10);
  tft.setTextColor(GREEN);
  tft.println("DONE");
 
  if (selected_port == 0) {
    A_AVAILABLE = true;
  } else if (selected_port == 1) {
    B_AVAILABLE = true;
  }

  tft.setCursor(10, 50);
  tft.setTextColor(BLACK);
  tft.setTextSize(2);
  tft.println("   DONE ALL STEPS, ");
  tft.println("");
  tft.println("    Unlocking....");

  tft.fillRect(10, HEIGHT - 50, WIDTH - 20, 40, BLACK);
  tft.setCursor(90, HEIGHT - 50 + 10);
  tft.setTextColor(WHITE);
  tft.println("DONE");

  // open port door
  unlockPortAndDisconnect(selected_port);

  selected_port = -1;      // reset selected port
}

void detectPortUnlocked() {
  if (pixel_y > HEIGHT - 50 && pixel_y < HEIGHT - 10) {
    if (pixel_x > 10 && pixel_x < WIDTH - 20) {
      // detected confirm and lock button
      welcomeScreen();
      SCREEN_TYPE = 0;
    }
  }
}

void enterCoinAndPort() {
  // SCREEN_TYPE = 3

  tft.fillScreen(WHITE);  //clear screen
  updateMode("COIN");

  // enter cancel button
  tft.fillRect(WIDTH - 30, 0, 50, 40, BLACK);
  tft.setCursor(WIDTH - 20, 10);
  tft.setTextColor(RED);
  tft.setTextSize(2);
  tft.print("X");

  if (!A_AVAILABLE && !B_AVAILABLE) {
    tft.setCursor(0, 100);
    tft.setTextColor(RED);
    tft.setTextSize(2);
    tft.println(" NO PORT AVAILABLE");

    tft.fillRect(BW, ST + BH * 3, BW, BH, BLACK);
    tft.setCursor(BW + BW / 5, ST + BH * 3 + BH / 2);
    tft.setTextSize(2);
    tft.setTextColor(WHITE);
    tft.println("BACK");
    return;
  }

  //
  tft.setCursor(0, 50);
  tft.setTextColor(BLUE);
  tft.setTextSize(2);
  tft.println("    Enter E5 coin,");
  tft.println("       then");
  tft.println("choose any available");
  tft.println("       port?");

  tft.setCursor(0, 130);
  tft.setTextColor(ORANGE);
  tft.setTextSize(7);
  tft.println("  E0");

  tft.setTextSize(1);
  tft.setTextColor(RED);
  tft.setCursor(10, HEIGHT - 20);    // 10 units from bottom
  tft.println("NB: Green: Available -- Red: Used!");
}

void detectCoinExit() {
  // detect X button for exiting current screeen
  if (pixel_y < 40 && pixel_x > WIDTH - 50 ) {
    welcomeScreen();
    SCREEN_TYPE = 0;
  }

  // detect BACK button
  if (pixel_x > BW && pixel_x < BW * 2) {
    if (pixel_y > ST + BH * 3) {
      welcomeScreen();
      SCREEN_TYPE = 0;
    }
  }
}

void coinInserted() {
  // SCREEEN_TYPE = 4

  tft.fillScreen(WHITE);  //clear screen
  updateMode("PORT");

  // enter cancel button
  tft.fillRect(WIDTH - 30, 0, 50, 40, BLACK);
  tft.setCursor(WIDTH - 20, 10);
  tft.setTextColor(RED);
  tft.setTextSize(2);
  tft.print("X");

  // Draw port buttons and their text A - Port A, B - Port B
  tft.setTextSize(2);
  tft.setTextColor(BLACK);

  if (A_AVAILABLE) {
    tft.fillRect(0, HEIGHT - BH - 40, BW, BH, GREEN);
    tft.setCursor(BW / 2, HEIGHT - BH - 40 + BH / 3);
    tft.print("A");
  }

  if (B_AVAILABLE) {
    tft.fillRect(BW * 2, HEIGHT - BH - 40, BW, BH, GREEN);
    tft.setCursor(BW * 2 + BW / 2, HEIGHT - BH - 40 + BH / 3);
    tft.print("B");
  }

  //
  tft.setCursor(0, 50);
  tft.setTextColor(BLUE);
  tft.setTextSize(2);
  tft.println("    Coin Detected!");
  tft.println("");
  tft.println("Select Any Available");
  tft.println("       port?");

  tft.setCursor(0, 130);
  tft.setTextColor(ORANGE);
  tft.setTextSize(7);
  tft.println("  E5");

  tft.setTextSize(1);
  tft.setTextColor(RED);
  tft.setCursor(10, HEIGHT - 20);    // 10 units from bottom
  tft.println("NB: Green: Available -- Red: Used!");
}

void detectSelectedPort() {
  if (pixel_y > HEIGHT - BH - 40 && pixel_y < HEIGHT - 40) {
    // detect row where both button are drawn

    if (A_AVAILABLE && pixel_x < BW) {
      // detected port A
      selected_port = 0;
      confirmPassword();
      SCREEN_TYPE = 2;
    }
    else if (B_AVAILABLE && pixel_x > BW * 2) {
      // detected port B
      selected_port = 1;
      confirmPassword();
      SCREEN_TYPE = 2;
    }
  }

  if (pixel_y < 40 && pixel_x > WIDTH - 50 ) {
    welcomeScreen();
    SCREEN_TYPE = 0;
  }
}

void confirmPassword() {
  tft.fillScreen(WHITE);  //clear screen
  updateMode("PASSWORD");

  // CANCEL BUTTON
  tft.fillRect(WIDTH - 30, 0, 50, 40, BLACK);
  tft.setCursor(WIDTH - 20, 10);
  tft.setTextColor(RED);
  tft.setTextSize(2);
  tft.print("X");

  tft.setCursor(10, 70);
  tft.setTextSize(2);
  tft.setTextColor(BLACK);
  tft.println("Generated password:");

  tft.setCursor(10, 110);
  tft.setTextSize(4);
  if (selected_port == 0) {
    PORT_A_PASSWORD =  generatePassword();
    tft.setTextColor(GREEN);
    tft.println("   " + PORT_A_PASSWORD);

    tft.setCursor(10, 160);
    tft.setTextSize(2);
    tft.setTextColor(BLUE);
    tft.println("Port: A");
  } else {
    PORT_B_PASSWORD = generatePassword();
    tft.setTextColor(GREEN);
    tft.println("   " + PORT_B_PASSWORD);

    tft.setCursor(10, 170);
    tft.setTextSize(2);
    tft.setTextColor(BLUE);
    tft.println("Port: B");
  }

  tft.setCursor(10, 200);
  tft.setTextSize(2);
  tft.setTextColor(BLACK);
  tft.println("Save this password");
  tft.println(" somewhere,");
  tft.println(" you'll need it to");
  tft.println(" unlock this booth!");

  tft.fillRect(10, HEIGHT - 50, WIDTH - 20, 40, BLACK);
  tft.setCursor(40, HEIGHT - 50 + 10);
  tft.setTextColor(GREEN);
  tft.println("LOCK & CHARGE");
}

void detectConfirmPassword() {
  if (pixel_y > HEIGHT - 50 && pixel_y < HEIGHT - 10) {
    if (pixel_x > 10 && pixel_x < WIDTH - 20) {
      // detected confirm and lock button
      chargingScreen();
      SCREEN_TYPE = 5;
    }
  }
  if (pixel_y < 40 && pixel_x > WIDTH - 50 ) {
    enterCoinAndPort();
    SCREEN_TYPE = 3;
  }
}

void chargingScreen() {
  tft.fillScreen(WHITE);  // clear screen
  updateMode("DONE");

  if (selected_port == 0) {
    A_AVAILABLE = false;
  } else if (selected_port == 1) {
    B_AVAILABLE = false;
  }
  
  tft.setCursor(10, 50);
  tft.setTextColor(GREEN);
  tft.setTextSize(2);
  tft.println("   DONE ALL STEPS, ");
  tft.println("");
  tft.println("    NOW LOCKED");
  tft.println("        AND");
  tft.println("     CHARGING");

  tft.fillRect(10, HEIGHT - 50, WIDTH - 20, 40, BLACK);
  tft.setCursor(90, HEIGHT - 50 + 10);
  tft.setTextColor(WHITE);
  tft.println("DONE");

  // lock port and start charging
  lockPortAnCharge(selected_port);

  selected_port = -1;      // reset selected port
}

void detectChargingScreen() {
  if (pixel_y > HEIGHT - 50 && pixel_y < HEIGHT - 10) {
    if (pixel_x > 10 && pixel_x < WIDTH - 20) {
      // detected confirm and lock button
      welcomeScreen();
      SCREEN_TYPE = 0;
    }
  }
}

void updateMode(String mode) {
  // function to update the current status of our system
  // three mode: 1. IDLE, 2. CHARGING, 3. ERROR (password errors)
  tft.setCursor(10, 10);
  tft.setTextColor(RED);
  tft.setTextSize(2);
  tft.print("MODE: " + mode);
}

void lockPortAnCharge(int selected_port) {
  // close and start charging
  if (selected_port == 0) {
    servoA.write(0);
    digitalWrite(PORT_A_RELAY, LOW);
    aDuration = millis();
    aCharging = true;
  } else {
    servoB.write(0);
    digitalWrite(PORT_B_RELAY, HIGH);
    bDuration = millis();
    bCharging = true;
  }
}

void unlockPortAndDisconnect(int selected_port) {
  // open and disconnect charging
  if (selected_port == 0) {
    servoA.write(90);
    digitalWrite(PORT_A_RELAY, HIGH);
    aCharging = false;
  } else {
    servoB.write(90);
    digitalWrite(PORT_B_RELAY, LOW);
    bCharging = false;
  }
}

bool calculateIsPressed(void) {
  TSPoint p = ts.getPoint();
  pinMode(YP, OUTPUT);      //restore shared pins
  pinMode(XM, OUTPUT);
  digitalWrite(YP, HIGH);   //because TFT control pins
  digitalWrite(XM, HIGH);
  bool pressed = (p.z > MINPRESSURE && p.z < MAXPRESSURE);

  if (pressed) {
    pixel_x = map(p.x, TS_LEFT, TS_RT, 0, 240); //.kbv makes sense to me
    pixel_y = map(p.y, TS_TOP, TS_BOT, 0, 320);

    Serial.print("pixel x: ");
    Serial.println(pixel_x);
    Serial.print("pixel y:  ");
    Serial.println(pixel_y);
    Serial.println();
  }
  return pressed;
}

boolean detectCoin() {
  /**
     Fucntion to detecte if coin was inserted.
     Note: I use two microcontrollers, this will be detected from ESP32
     and notifified via an analog pin to UNO
  */
  int val = analogRead(COIN_IN);
  boolean temp = false;
  if (val >= 600) {
    temp = true;
  }
  return temp;
}

void checkCharginDuration() {
  // check for A
  if (aCharging) {
    // if a is charging, check if time has ellased
    if ( (millis() - aDuration ) > DURATION) {
      // time has ellapsed
      digitalWrite(PORT_A_RELAY, HIGH);
    } else {
      aDuration = millis();
    }
  }

  // check for B
  if (bCharging) {
    // if b is charging, check if time has ellapsed
    if ( (millis() - bDuration ) > DURATION) {
      // time has ellapsed
      digitalWrite(PORT_B_RELAY, LOW);
    } else {
      bDuration = millis();
    }
  }
}
