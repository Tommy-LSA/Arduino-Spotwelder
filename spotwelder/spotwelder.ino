
/*

   This is written for an Arduino Nano

   - Display 128x46 SSD1306 I2C
   -- SCK - Pin A4
   -- SDA - Pin A5

   - Rotary Encoder with switch
   -- CLK - Pin A2
   -- DT  - Pin A3
   -- SW  - Pin A1

   - SSR-40 DA (Solid State Relay)
   -- negative Pin D8
   -- positive Pin VIN

   - Welding Switch (e.g. microswitch)
   -- contact1 Pin GND
   -- contact2 PIN D2
*/

// Display
#include "U8glib.h"
U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_NONE | U8G_I2C_OPT_DEV_0); // I2C / TWI

// Rotary Encoder
#include <RotaryEncoder.h>
RotaryEncoder encoder(A2, A3);

// Timer to hold the weld lock
#include <secTimer.h>
secTimer myTimer;

// EEprom
#include <EEPROM.h>

#define RELAY_PIN 8
#define INTERRUPT_PIN 2

#define KEY_NONE 0
#define KEY_PREV 1
#define KEY_NEXT 2
#define KEY_SELECT 3
#define KEY_BACK 4

#define MENU_ITEMS 4

#define SCREEN_DEFAULT 0
#define SCREEN_VALUE 1
#define SCREEN_MENU 2

#define WELD_LOCKED 0
#define WELD_UNLOCKED 1
#define WELD_LOCK_SECS 2

#define WELD_FIRSTPULSE_DEFAULT 20
#define WELD_SECONDPULSE_DEFAULT 20
#define WELD_DELAY_DEFAULT 20

byte uiActScreen = SCREEN_DEFAULT;
byte KeySelect = A1;

byte debounce;
byte uiKeyCodeFirst = KEY_NONE;
byte uiKeyCodeSecond = KEY_NONE;
byte uiKeyCode = KEY_NONE;
static int pos = 0;

volatile int weld_firstPulse = WELD_FIRSTPULSE_DEFAULT;
volatile int weld_secondPulse = WELD_SECONDPULSE_DEFAULT;
volatile int weld_delay = WELD_DELAY_DEFAULT;
volatile byte weld_locked = WELD_UNLOCKED;

const char *menu_strings[MENU_ITEMS] = {"First Pulse", "Delay", "Second Pulse", "Exit"};

uint8_t menu_current = 0;
uint8_t menu_redraw_required = 0;
uint8_t last_key_code = KEY_NONE;

void uiStep(void)
{

  encoder.tick();
  int newPos = encoder.getPosition();

  if (pos != newPos)
  {
    char output[20];
    sprintf(output, "old: %d new: %d", pos, newPos);
    Serial.print(output);
    Serial.println();
  }

  uiKeyCodeSecond = uiKeyCodeFirst;
  if (pos > newPos)
  {
    uiKeyCodeFirst = KEY_PREV;
  }
  else if (pos < newPos)
  {
    uiKeyCodeFirst = KEY_NEXT;
  }
  else if (digitalRead(KeySelect) == LOW)
  {
    if (debounce == 0)
    {
      uiKeyCodeFirst = KEY_SELECT;
    }
    else
    {
      uiKeyCodeFirst = KEY_NONE;
    }
  }
  else if (digitalRead(KeySelect) == HIGH)
  {
    uiKeyCodeFirst = KEY_NONE;
    debounce = 0;
  }
  else
    uiKeyCodeFirst = KEY_NONE;

  pos = newPos;
  uiKeyCode = uiKeyCodeFirst;
}

void drawScreen(void)
{
  switch (uiActScreen)
  {
    case SCREEN_DEFAULT:
      drawMainScreen();
      break;
    case SCREEN_VALUE:
      drawValueScreen();
      break;
    case SCREEN_MENU:
      drawMenu();
      break;
    default:
      drawMainScreen();
      break;
  }
}

void drawMainScreen(void)
{
  uint8_t i, h;
  u8g_uint_t w, d;
  w = u8g.getWidth();
  u8g.setFont(u8g_font_unifont);
  u8g.setFontRefHeightText();
  u8g.setFontPosTop();
  u8g.setDefaultForegroundColor();
  h = u8g.getFontAscent() - u8g.getFontDescent();
  d = (w - u8g.getStrWidth("= SPOTWELDER =")) / 2;
  u8g.drawStr(d, 2, "= SPOTWELDER =");
  if (weld_locked == WELD_UNLOCKED)
    u8g.drawDisc(w / 2, 64 - 25, 20, U8G_DRAW_ALL);
  else
    u8g.drawCircle(w / 2, 64 - 25, 20, U8G_DRAW_ALL);
}

void drawValueScreen(void)
{
  uint8_t i, h;
  u8g_uint_t w, d;
  u8g.setFont(u8g_font_unifont);
  u8g.setFontRefHeightText();
  u8g.setFontPosTop();
  u8g.setDefaultForegroundColor();
  h = u8g.getFontAscent() - u8g.getFontDescent();
  w = u8g.getWidth();
  d = (w - u8g.getStrWidth("=  CHG VALUE  =")) / 2;
  u8g.drawStr(d, 2, "=  CHG VALUE  =");
  d = (w - u8g.getStrWidth(menu_strings[menu_current])) / 2;
  u8g.drawStr(d, 2 * h, menu_strings[menu_current]);

  u8g.setFont(u8g_font_fub17);
  char value[20];
  if (menu_current == 0)
    sprintf(value, "%dms", weld_firstPulse);
  else if (menu_current == 1)
    sprintf(value, "%dms", weld_delay);
  else if (menu_current == 2)
    sprintf(value, "%dms", weld_secondPulse);
  d = (w - u8g.getStrWidth(value)) / 2;
  u8g.drawStr(d, 62, value);
}

void drawMenu(void)
{
  uint8_t i, h;
  u8g_uint_t w, d;
  w = u8g.getWidth();
  //u8g.setFont(u8g_font_6x13);
  u8g.setFont(u8g_font_unifont);
  u8g.setFontRefHeightText();
  u8g.setFontPosTop();
  u8g.setDefaultForegroundColor();
  d = (w - u8g.getStrWidth("==   MENU   ==")) / 2;
  u8g.drawStr(d, 2, "==   MENU   ==");

  u8g.setFont(u8g_font_6x13);
  u8g.setFontRefHeightText();
  u8g.setFontPosTop();
  h = u8g.getFontAscent() - u8g.getFontDescent();
  w = u8g.getWidth();

  for (i = 0; i < MENU_ITEMS; i++)
  {
    //d = (w-u8g.getStrWidth(menu_strings[i]))/2;
    u8g.setDefaultForegroundColor();
    if (i == menu_current)
    {
      u8g.drawBox(0, i * h + 18, w, h);
      u8g.setDefaultBackgroundColor();
    }
    u8g.drawStr(3, i * h + 18, menu_strings[i]);
    if (i < 3)
    {
      char buf[7];
      if (i == 0)
        sprintf(buf, "%dms", weld_firstPulse);
      if (i == 1)
        sprintf(buf, "%dms", weld_delay);
      if (i == 2)
        sprintf(buf, "%dms", weld_secondPulse);
      byte startpos = w - u8g.getStrWidth(buf);
      u8g.drawStr(startpos, i * h + 18, buf);
    }
  }
  menu_redraw_required = 0;
}

void updateMain(void)
{
  switch (uiActScreen)
  {
    case SCREEN_DEFAULT:
      updateMainScreen();
      break;
    case SCREEN_VALUE:
      updateValueScreen();
      break;
    case SCREEN_MENU:
      updateMenuScreen();
      break;
    default:
      updateMainScreen();
      break;
  }
}

void updateMainScreen(void)
{
  if (uiKeyCode == KEY_SELECT && debounce == 0)
  {
    if (uiActScreen != SCREEN_MENU)
    {
      Serial.print("goto Menu");
      Serial.println();
      debounce = 1;
      uiActScreen = SCREEN_MENU;
      menu_redraw_required = 1;
    }
  }
}

void updateValueScreen(void)
{
  if (uiKeyCode == KEY_SELECT && debounce == 0)
  {
    if (uiActScreen != SCREEN_MENU)
    {
      debounce = 1;
      uiActScreen = SCREEN_MENU;
      menu_redraw_required = 1;
    }
  }
  else if (uiKeyCode == KEY_PREV || uiKeyCode == KEY_NEXT)
  {
    if (menu_current == 0)
    {
      if (uiKeyCode == KEY_NEXT)
        weld_firstPulse < 9999 ? weld_firstPulse++ : weld_firstPulse = 0;
      if (uiKeyCode == KEY_PREV)
        weld_firstPulse > 0 ? weld_firstPulse-- : weld_firstPulse = 9999;
    }
    else if (menu_current == 1)
    {
      if (uiKeyCode == KEY_NEXT)
        weld_delay < 9999 ? weld_delay++ : weld_delay = 0;
      if (uiKeyCode == KEY_PREV)
        weld_delay > 0 ? weld_delay-- : weld_delay = 9999;
    }
    else if (menu_current == 2)
    {
      if (uiKeyCode == KEY_NEXT)
        weld_secondPulse < 9999 ? weld_secondPulse++ : weld_secondPulse = 0;
      if (uiKeyCode == KEY_PREV)
        weld_secondPulse > 0 ? weld_secondPulse-- : weld_secondPulse = 9999;
    }
    menu_redraw_required = 1;
  }
}

void updateMenuScreen(void)
{
  if (uiKeyCode != KEY_NONE && last_key_code == uiKeyCode)
  {
    return;
  }
  last_key_code = uiKeyCode;

  switch (uiKeyCode)
  {
    case KEY_NEXT:
      menu_current++;
      if (menu_current >= MENU_ITEMS)
        menu_current = 0;
      menu_redraw_required = 1;
      break;
    case KEY_PREV:
      if (menu_current == 0)
        menu_current = MENU_ITEMS;
      menu_current--;
      menu_redraw_required = 1;
      break;

    case KEY_SELECT:

      // Save Values
      if (menu_current == 3)
      {
        if (uiActScreen != SCREEN_DEFAULT && debounce == 0)
        {
          debounce = 1;
          uiActScreen = SCREEN_DEFAULT;
          menu_redraw_required = 1;
          writeEprom();
          Serial.print("goto save main");
          Serial.println();
        }
      }
      else
      {
        if (uiActScreen != SCREEN_VALUE && debounce == 0)
        {
          debounce = 1;
          uiActScreen = SCREEN_VALUE;
          menu_redraw_required = 1;
          Serial.print("goto value page");
          Serial.println();
        }
      }
      break;
  }
}

void setup()
{
  // rotate screen, if required
  // u8g.setRot180();

  // Encoder switch
  pinMode(KeySelect, INPUT);

  // Setting interrupt for button
  pinMode(INTERRUPT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), welding, LOW);

  // Set relay default off
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);

  // initialize serial
  Serial.begin(57600);
  Serial.println("Spotwelder Ready.");

  // read parameters from eprom
  readEprom();

  // encoder button
  pinMode(KeySelect, INPUT);

  // force initial redraw
  menu_redraw_required = 1;
}

void loop()
{

  // check for key press
  uiStep();

  if (menu_redraw_required != 0)
  {
    u8g.firstPage();
    do
    {
      // redraw screen
      drawScreen();
    } while (u8g.nextPage());
    // lock redraw
    menu_redraw_required = 0;
  }

  // update Screens (Keys)
  updateMain();

  // unlock welding after 2 seconds (debounce button)
  int seconds = myTimer.readTimer();
  if (seconds > WELD_LOCK_SECS && weld_locked == WELD_LOCKED)
  {
    weld_locked = WELD_UNLOCKED;
    menu_redraw_required = 1;
    Serial.print("Welding unlocked");
    Serial.println();
  }
}

void welding(void)
{

  if (weld_locked == WELD_UNLOCKED)
  {

    Serial.print("WELDING");
    Serial.println();

    if (weld_firstPulse > 0)
    {
      digitalWrite(RELAY_PIN, LOW);
      delayMicroseconds(weld_firstPulse * 1000);
      digitalWrite(RELAY_PIN, HIGH);

      Serial.print("First pulse Done");
      Serial.println();
    }

    if (weld_delay > 0)
    {
      delayMicroseconds(weld_delay * 1000);
      Serial.print("Delay Done");
      Serial.println();
    }

    if (weld_secondPulse > 0)
    {
      digitalWrite(RELAY_PIN, LOW);
      delayMicroseconds(weld_secondPulse * 1000);
      digitalWrite(RELAY_PIN, HIGH);

      Serial.print("Welding Done");
      Serial.println();
    }
    // lock welding (debounce button)
    weld_locked = WELD_LOCKED;
    menu_redraw_required = 1;
    myTimer.startTimer(); //start the timer

    Serial.print("Welding locked");
    Serial.println();
  }
}

void writeEprom(void)
{
  EEPROM.write(0, highByte(weld_firstPulse));
  EEPROM.write(1, lowByte(weld_firstPulse));
  EEPROM.write(2, highByte(weld_secondPulse));
  EEPROM.write(3, lowByte(weld_secondPulse));
  EEPROM.write(4, highByte(weld_delay));
  EEPROM.write(5, lowByte(weld_delay));
}

void readEprom(void)
{
  byte high = EEPROM.read(0);
  byte low = EEPROM.read(1);
  int value = word(high, low);

  if (value > 3000 || value < 0)
  {
    weld_firstPulse = WELD_FIRSTPULSE_DEFAULT;
  }
  else
  {
    weld_firstPulse = value;
  }

  high = EEPROM.read(2);
  low = EEPROM.read(3);
  value = word(high, low);

  if (value > 3000 || value < 0)
  {
    weld_secondPulse = WELD_SECONDPULSE_DEFAULT;
  }
  else
  {
    weld_secondPulse = value;
  }

  high = EEPROM.read(4);
  low = EEPROM.read(5);
  value = word(high, low);

  if (value > 3000 || value < 0)
  {
    weld_delay = WELD_DELAY_DEFAULT;
  }
  else
  {
    weld_delay = value;
  }

  char out[50];
  sprintf(out, "First Pulse %dms\nDelay %dms\nSecond Pulse %dms", weld_firstPulse, weld_delay, weld_secondPulse);
  Serial.print(out);
  Serial.println();
}

