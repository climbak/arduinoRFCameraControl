/************************************************************/
/*
   Chibi Wireless Button
   This is an example of controlling an LED wirelessly. A button
   is connected to one node and an LED is connected to another.
   Actually this code allows each node to have both so both nodes can control
   each other's buttons.

   Directions for use:
   1. Set the DEST_ADDR definition to the destination address of the node
   you want to control.
   2. Set the LED_PIN definition to the pin that has the LED (if any).
   3. Set the BUTTON_PIN definition to the pin that has the button. Remember
   to use a pullup or pulldown on the button output so that it won't float in
   its idle state.
   4. Set the SWITCH_ON and SWITCH_OFF definition to match your button configuration
   5. Upload this sketch to each board.

   Good luck!
   */
/************************************************************/
#include <chibi.h>
#include "U8glib.h"
U8GLIB_SSD1306_128X64 display(U8G_I2C_OPT_NONE | U8G_I2C_OPT_DEV_0);	// I2C / TWI 

#define DEBUG
#ifdef DEBUG
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#endif

// These are general definitions. There are three main things to define
// in this sketch: where the LED is at, where the button is at, and where
// the data will be sent. 
#define DEST_ADDR 3             // the destination address data will be sent to
#define SOLENOID_PIN 7          // the pin that will control the solenoid
#define TRIGGER_PIN 6           // the pin that will detect the pressure switch being triggered
#define BUTTONS_PIN A0          // the pin that will 
#define DEBOUNCE 50             // debounce time
#define SINGLE_PRESS_HOLD 100   // time to hold the solenoid to push a button once
#define ACK_WAIT 100           // wait time for each wait for command received acknowledgement;

// These two definitions will change based on the user hardware. If
// the idle (UP) position of the button is 0, then set SWITCH_OFF to 0 and
// SWITCH_ON to 1. And vice-versa
#define SWITCH_ON 0     // the value that a button press generates on button pin
#define SWITCH_OFF 1    // the value that a button release generates on button pin

// These are the commands. More commands can be added based on the user inputs.
// Then they can be handled in the command handler function. 
#define CMD_ACK 0              // acknowledge cmd received
#define CMD_SOLENOID_OFF 1      // turn the SOLENOID off
#define CMD_SOLENOID_ON 2       // turn the SOLENOID on

// These are the operating modes
#define MODE_TRIGGER 0
#define MODE_CAMERA 1
#define MODE_VIDEO 2

// screen y values for line items and select indicator position
#define LINE_ONE 1
#define LINE_TWO 2
#define LINE_THREE 3
#define LINE_FOUR 4
#define LINE_X 8
#define LINE_ITEM_BASE_Y 20
#define ITEM_INDICATOR_BASE_Y 26

// string resources
#define TITLE_FONT u8g_font_9x15B
#define ITEM_FONT u8g_font_profont12
const char* trigger_string = "Trigger";
const char* camera_string = "Camera";
const char* video_string = "Video";
const char* menu_string = "Menu";
const char* mode_string =         "Mode   : %7s";
const char* delay_string =        "Delay  : %5dms";
const char* shutter_hold_string = "Hold   : %5dms";
const char* video_record_string = "Record : %5d s";
const char* dest_addr_string =    "Tx Addr: %5d";

// tx/rx addresses
int this_addr, dest_addr = 3;

// Store the previous state of the button here. We'll use it to compare against
// the current button state to see if any event occurred.
byte switch_state;

// camera settings
int shutterDelay = 250;
int shutterHold = 1500;
int videoRecordTime = 5;

// var to store the current operating mode
byte mode = MODE_TRIGGER;

// vars to control menu mode
bool menu = false;
byte selectedOption = 1;

// var to keep track of whether we're waiting for an acknowledge before continue
bool waitForAck = false;

// key set up
#define RIGHT_KEY 0
#define UP_KEY 1
#define DOWN_KEY 2
#define LEFT_KEY 3
#define SELECT_KEY 4
unsigned int adc_key_val[5] = { 30, 150, 360, 535, 760 };
byte NUM_KEYS = 5;
byte oldkey = -1;

/**************************************************************************/
// Initialize
/**************************************************************************/
void setup()
{
  // init the hardware
  pinMode(SOLENOID_PIN, OUTPUT);
  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  pinMode(BUTTONS_PIN, INPUT_PULLUP);

  // init the wireless stack
  chibiInit();
  this_addr = chibiGetShortAddr();

  // init the serial port for debug purposes
  Serial.begin(57600);

  // initial state of switch is set to be unpushed
  switch_state = SWITCH_OFF;
}

/**************************************************************************/
// Loop
/**************************************************************************/
void loop()
{
  button_handler();
  command_handler(); 

  display.firstPage();
  do
  {
    display_handler();
  } while (display.nextPage());
}

/**************************************************************************/
//  This function handles any button events. When the button changes state, like
//  if it gets pressed or released, then this function will detect it and send
//  out a command to change the LED on the remote node.
/**************************************************************************/
void button_handler()
{
  // first take care of the trigger mode
  if (mode == MODE_TRIGGER)
  {
    byte switch_val, data[1];

    // get the switch value from the input
    switch_val = digitalRead(TRIGGER_PIN);

    // check if the button has changed state. compare the current button value
    // with the previous value. if its the same, then ignore. if not, then handle 
    // the event.
    if (switch_val != switch_state)
    {
      if (switch_val == SWITCH_ON)
      {
        //the button has been pushed. Send out a command to turn the LED on. then
        // update the button's state for future comparison  
        switch_state = SWITCH_ON;
        data[0] = CMD_SOLENOID_ON;
        DEBUG_PRINTLN("Switch is on.");
        chibiTx(DEST_ADDR, data, 1);
        waitForAck = true;
        int count = 0;
        while (waitForAck && count++ < 20)
        {
          delay(ACK_WAIT);
          command_handler();
        }
        delay(1000); // just wait for a second for good measure
      }
      else
      {
        // the button has been released. Send out a command to turn the LED off. then
        // update the button's state for future comparison  
        switch_state = SWITCH_OFF;
        data[0] = CMD_SOLENOID_OFF;
        DEBUG_PRINTLN("Switch is off.");
        delay(DEBOUNCE);
      }
    }
  }

  // now take care of the menu button presses
  //int adc_key_in = analogRead(BUTTONS_PIN); // read the value from the sensor
  ////DEBUG_PRINT("initial read");
  ////DEBUG_PRINTLN(adc_key_in);
  //int key = get_key(adc_key_in); // convert into key press
  //if (key != oldkey) // if keypress is detected
  //{
  //  delay(DEBOUNCE);
  //  adc_key_in = analogRead(BUTTONS_PIN); // read the value from the sensor
  //  key = get_key(adc_key_in); // convert into key press
  //  if (key != oldkey)
  //  {
  //    oldkey = key;
  //    if (key >= 0)
  //    {
  //      //DEBUG_PRINT("inside debounce");
  //      //DEBUG_PRINTLN(adc_key_in);
  //      //delay(100);
  //      switch (key)
  //      {
  //      case SELECT_KEY: menu = !menu; break;

  //      case UP_KEY: if (!menu) break;
  //        if (selectedOption > 1)
  //          selectedOption--;
  //        else
  //          selectedOption = 3;
  //        break;

  //      case DOWN_KEY: if (!menu || mode == MODE_TRIGGER) break;
  //        if (selectedOption < 3)
  //          selectedOption++;
  //        else
  //          selectedOption = 1;
  //        break;

  //      case LEFT_KEY: if (!menu) break;
  //        switch (selectedOption)
  //        {
  //        case 1: mode = mode == MODE_TRIGGER ? MODE_VIDEO : mode == MODE_VIDEO ? MODE_CAMERA : MODE_TRIGGER; break;
  //        case 2:
  //          if (mode != MODE_TRIGGER && shutterDelay > 0)
  //            shutterDelay -= 250;
  //          break;
  //        case 3:
  //          if (mode == MODE_CAMERA && shutterHold > 0)
  //            shutterHold -= 250;
  //          else if (mode == MODE_VIDEO && videoRecordTime > 0)
  //          {
  //            if (videoRecordTime <= 10)
  //              videoRecordTime -= 1;
  //            else if (videoRecordTime <= 30)
  //              videoRecordTime -= 5;
  //            else if (videoRecordTime <= 60)
  //              videoRecordTime -= 10;
  //            else
  //              videoRecordTime -= 20;
  //          }
  //          break;
  //        }
  //        break;

  //      case RIGHT_KEY: if (!menu) break;
  //        switch (selectedOption)
  //        {
  //        case 1: mode = mode == MODE_TRIGGER ? MODE_CAMERA : mode == MODE_CAMERA ? MODE_VIDEO : MODE_TRIGGER; break;
  //        case 2:
  //          if (mode != MODE_TRIGGER)
  //            shutterDelay += 250;
  //          break;
  //        case 3:
  //          if (mode == MODE_CAMERA && shutterHold < 9999)
  //            shutterHold += 250;
  //          else if (mode == MODE_VIDEO)
  //          {
  //            if (videoRecordTime < 10)
  //              videoRecordTime += 1;
  //            else if (videoRecordTime < 30)
  //              videoRecordTime += 5;
  //            else if (videoRecordTime < 60)
  //              videoRecordTime += 10;
  //            else if (videoRecordTime < 999)
  //              videoRecordTime += 20;
  //          }
  //          break;
  //        }
  //        break;
  //      }
  //    }
  //  }
  //}
}

/**************************************************************************/
//  This is the command handler. I wrote it semi-generically so it can be extended
//  to handle other commands. There are two main parts to it. The first part is to
//  retrieve any data that arrived wirelessly and extract the command. The second
//  part is to handle the command.

//  In a real application, there might be multiple buttons or other input devices
//  that can trigger many different commands. In this case, you just need to add
//  each command to the switch statement and the code to handle it.
/**************************************************************************/
void command_handler()
{
  byte cmd, buf[CHB_MAX_PAYLOAD], data[1];

  // check to see if any new data has been received. if so, then we need to handle it.
  if (chibiDataRcvd())
  {
    // retrieve the data from the chibi stack
    chibiGetData(buf);

    // its assumed that the data will be in the first byte of the buffer since we're only
    // sending one byte.
    cmd = buf[0];

    DEBUG_PRINTLN("Command received");
    // this is the main command handler. it deals with any commands that arrive through the radio.
    switch (cmd)
    {
    case CMD_ACK: waitForAck = false; break;
    case CMD_SOLENOID_OFF:
      digitalWrite(SOLENOID_PIN, 0);
      DEBUG_PRINTLN("Command: SOLENOID OFF");
      data[0] = CMD_ACK;
      chibiTx(DEST_ADDR, data, 1);
      break;

    case CMD_SOLENOID_ON:
      data[0] = CMD_ACK;
      chibiTx(DEST_ADDR, data, 1);
      delay(shutterDelay);
      if (mode == MODE_CAMERA)
        take_picture();
      if (mode == MODE_VIDEO)
        record_video();
      break;
    }
  }
}

// handles display tasks
void display_handler()
{
  set_font(TITLE_FONT);
  
  draw_title();

  set_font(ITEM_FONT);

  if (menu)
  {
    draw_mode_item();
    int y = ITEM_INDICATOR_BASE_Y + ((selectedOption - 1) * 11);
    display.drawDisc(2, y, 2);
  }
  
  // now print the delay, we'll always have that if not trigger mode
  if (mode != MODE_TRIGGER)
  {
    draw_delay_item(menu ? LINE_TWO : LINE_ONE);
  }

  // now print the other options
  switch (mode)
  {
  case MODE_TRIGGER: draw_destAddr_item(menu ? LINE_TWO : LINE_ONE); break;
  case MODE_CAMERA: 
    draw_shutterHold_item(menu ? LINE_THREE : LINE_TWO);
    draw_destAddr_item(menu ? LINE_FOUR : LINE_THREE); 
    break;
  case MODE_VIDEO: 
    draw_recordTime_item(menu ? LINE_THREE : LINE_TWO);
    draw_destAddr_item(menu ? LINE_FOUR : LINE_THREE); 
    break;
  }
}

void draw_title()
{
  if (menu)
  {
    display.drawStr(0, 0, menu_string);
  }
  else
  {
    switch (mode)
    {
    case MODE_TRIGGER: display.drawStr(0, 0, trigger_string); break;
    case MODE_CAMERA: display.drawStr(0, 0, camera_string); break;
    case MODE_VIDEO: display.drawStr(0, 0, video_string); break;
    }
  }

  char addrBuf[4];
  sprintf(addrBuf, "%3d", this_addr);
  display.drawStr(102, 0, addrBuf);
}

void draw_mode_item()
{
  char modeBuf[16];
  switch (mode)
  {
  case MODE_TRIGGER: sprintf(modeBuf, mode_string, trigger_string); break;
  case MODE_CAMERA: sprintf(modeBuf, mode_string, camera_string); break;
  case MODE_VIDEO: sprintf(modeBuf, mode_string, video_string); break;
  }
  display.drawStr(LINE_X, LINE_ITEM_BASE_Y, modeBuf);
}

void draw_delay_item(byte lineNum)
{
  char delayBuf[16];
  sprintf(delayBuf, delay_string, shutterDelay);
  int y = LINE_ITEM_BASE_Y + ((lineNum - 1) * 11);
  display.drawStr(LINE_X, y, delayBuf);
}

void draw_shutterHold_item(byte lineNum)
{
  char holdBuf[16];
  sprintf(holdBuf, shutter_hold_string, shutterHold);
  int y = LINE_ITEM_BASE_Y + ((lineNum - 1) * 11);
  display.drawStr(LINE_X, y, holdBuf);
}

void draw_recordTime_item(byte lineNum)
{
  char recordBuf[16];
  sprintf(recordBuf, video_record_string, videoRecordTime);
  int y = LINE_ITEM_BASE_Y + ((lineNum - 1) * 11);
  display.drawStr(LINE_X, y, recordBuf);
}

void draw_destAddr_item(byte lineNum)
{
  char destBuf[16];
  sprintf(destBuf, dest_addr_string, dest_addr);
  int y = LINE_ITEM_BASE_Y + ((lineNum - 1) * 11);
  display.drawStr(LINE_X, y, destBuf);
}

// Convert ADC value to key number
int get_key(unsigned int input)
{
  DEBUG_PRINT("inside get_key");
  DEBUG_PRINTLN(input);
  int k;
  for (k = 0; k < NUM_KEYS; k++)
  {
    if (input < adc_key_val[k])
      return k;
  }
  if (k >= NUM_KEYS)
    k = -1; // No valid key pressed

  return k;
}

// font setter
void set_font(const u8g_fntpgm_uint8_t *font)
{
  display.setFont(font);
  display.setFontRefHeightExtendedText();
  display.setDefaultForegroundColor();
  display.setFontPosTop();
}

// command functions
void take_picture()
{
  digitalWrite(SOLENOID_PIN, 1);
  DEBUG_PRINTLN("Command: SOLENOID ON");
  delay(shutterHold);
  digitalWrite(SOLENOID_PIN, 0);
  DEBUG_PRINTLN("Command: SOLENOID OFF");
}

void record_video()
{
  digitalWrite(SOLENOID_PIN, 1);
  DEBUG_PRINTLN("Command: SOLENOID ON");
  delay(SINGLE_PRESS_HOLD);
  digitalWrite(SOLENOID_PIN, 0);
  DEBUG_PRINTLN("Command: SOLENOID OFF");
  delay(videoRecordTime);
  digitalWrite(SOLENOID_PIN, 1);
  DEBUG_PRINTLN("Command: SOLENOID ON");
  delay(SINGLE_PRESS_HOLD);
  digitalWrite(SOLENOID_PIN, 0);
  DEBUG_PRINTLN("Command: SOLENOID OFF");
}
