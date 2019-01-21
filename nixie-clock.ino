#include <HardwareSerial.h>
#include <Wire.h>
#include <RTClib.h>
#include <FiniteStateMachine.h>

#include <Adafruit_RGBLCDShield.h>
#include <exixe.h>

#include "LoopHelpers.h"
#include "TimeHelpers.h"

#define OFF 0x0
#define RED 0x1
#define YELLOW 0x3
#define GREEN 0x2
#define TEAL 0x6
#define BLUE 0x4
#define VIOLET 0x5
#define WHITE 0x7

#define ROWS 2
#define COLS 16

#define DATE_STRING_LENGTH 10
#define TIME_STRING_LENGTH 8

#define TRANSITION_DELAY 300

#define AWAKE_TIME_SECONDS 1800

#define CS_HOURS_DECADE 10
#define CS_HOURS_UNIT   9
#define CS_MIN_DECADE   7
#define CS_MIN_UNIT     6

Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();
RTC_DS3231 rtc;
exixe hours_dec_tube = exixe(CS_HOURS_DECADE);
exixe hours_unit_tube = exixe(CS_HOURS_DECADE);
exixe min_dec_tube = exixe(CS_HOURS_DECADE);
exixe min_unit_tube = exixe(CS_HOURS_DECADE);

char dateString[DATE_STRING_LENGTH + 1];
char timeString[TIME_STRING_LENGTH + 1];

// FSM Definition
State ShowTime = State(showTimeEnter, showTimeUpdate, showTimeExit);
State SetDate = State(setDateEnter, setDateUpdate, setDateExit);
State SetTime = State(setTimeEnter, setTimeUpdate, setTimeExit);

FSM clockStateMachine = FSM(ShowTime);

DateTime newDateTime;
short cursorPosition;
short maxCursorPosition;

DateTime now;

void setup()
{
  Serial.begin(115200);
  Wire.begin();
  rtc.begin();
  lcd.begin(16, 2);
  lcd.setBacklight(WHITE);

  hours_dec_tube.spi_init();

  hours_dec_tube.clear();
  hours_unit_tube.clear();
  min_dec_tube.clear();
  min_unit_tube.clear();

  now = rtc.now();

  hours_dec_tube.set_led(0, 0, 0);
  hours_unit_tube.set_led(0, 0, 0);
  min_dec_tube.set_led(0, 0, 0);
  min_unit_tube.set_led(0, 0, 0);
}

void loop()
{
  uint8_t buttons = lcd.readButtons();

  if (buttons & BUTTON_SELECT)
  {
    if (clockStateMachine.isInState(ShowTime))
    {
      clockStateMachine.transitionTo(SetDate);
    }
    else if (clockStateMachine.isInState(SetDate) && isDateValid(dateString))
    {
      clockStateMachine.transitionTo(SetTime);
    }
    else if (clockStateMachine.isInState(SetTime) && isTimeValid(timeString))
    {
      clockStateMachine.transitionTo(ShowTime);
    }

    delay(TRANSITION_DELAY);
  }

  clockStateMachine.update();
}

void showTimeEnter()
{
  lcd.noCursor();
  lcd.noBlink();
  lcd.setBacklight(WHITE);
}

void showTimeUpdate()
{
  now = rtc.now();

  sprintf(dateString, "%02d/%02d/%d", now.month(), now.day(), now.year());
  sprintf(timeString, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());

  lcd.setCursor(0, 0);
  lcd.print(dateString);

  lcd.setCursor(0, 1);
  lcd.print(timeString);

  show_time_tubes(now.hour(), now.minute());
}

void showTimeExit()
{

}

void setDateEnter()
{
  lcd.clear();
  lcd.setBacklight(GREEN);
  lcd.setCursor(0, 0);
  lcd.print("SET DATE:");
  lcd.setCursor(0, 1);
  newDateTime = rtc.now();
  sprintf(dateString, "%02d/%02d/%04d", newDateTime.month(), newDateTime.day(), newDateTime.year());
  lcd.print(dateString);

  cursorPosition = 1;
  maxCursorPosition = DATE_STRING_LENGTH - 1;

  lcd.setCursor(1, 1);
  lcd.cursor();
}

void setDateUpdate()
{
  uint8_t buttons = lcd.readButtons();

  char dayString[3] = { dateString[3], dateString[4], '\0' };
  char monthString[3] = { dateString[0], dateString[1], '\0' };
  char yearString[5] = { dateString[6], dateString[7], dateString[8], dateString[9], '\0' };

  int day = atoi(dayString);
  int month = atoi(monthString);
  int year = atoi(yearString);

  bool dateChanged = false;
  bool cursorChanged = false;

  if ((buttons & BUTTON_RIGHT) && (cursorPosition < maxCursorPosition))
  {
    delay(TRANSITION_DELAY);
    Serial.write("right\n");
    if (cursorPosition == 1)
    {
      cursorPosition = 4;
    }
    else if (cursorPosition == 4)
    {
      cursorPosition = 9;
    }
    else
    {
      cursorPosition = 1;
    }

    cursorChanged = true;
  }
  else if ((buttons & BUTTON_LEFT) && (cursorPosition > 0))
  {
    delay(TRANSITION_DELAY);
    Serial.write("left\n");
    if (cursorPosition == 9)
    {
      cursorPosition = 4;
    }
    else if (cursorPosition == 4)
    {
      cursorPosition = 1;
    }
    else
    {
      cursorPosition = 9;
    }

    cursorChanged = true;
  }
  else if (buttons & BUTTON_UP)
  {
    delay(TRANSITION_DELAY);
    Serial.write("up\n");

    switch (cursorPosition)
    {
    case 1:
      month = incrementOrLoop(month, 12, 1);
      break;
    case 4:
      if (month == 2)
      {
        if (year % 4 == 0)
        {
          day = incrementOrLoop(day, 29, 1);
        }
        else
        {
          day = incrementOrLoop(day, 28, 1);
        }
      }
      else if (month == 1
        || month == 3
        || month == 5
        || month == 7
        || month == 8
        || month == 10
        || month == 12)
      {
        day = incrementOrLoop(day, 31, 1);
      }
      else
      {
        day = incrementOrLoop(day, 30, 1);
      }
      break;
    case 9:
      year = incrementOrLoop(year, 9999);
      break;
    default:
      break;
    }

    dateChanged = true;
  }
  else if (buttons & BUTTON_DOWN)
  {
    delay(TRANSITION_DELAY);
    Serial.write("down\n");

    switch (cursorPosition)
    {
    case 1:
      month = decrementOrLoop(month, 12, 1);
      break;
    case 4:
      if (month == 2)
      {
        if (year % 4 == 0)
        {
          day = decrementOrLoop(day, 29, 1);
        }
        else
        {
          day = decrementOrLoop(day, 28, 1);
        }
      }
      else if (month == 1
        || month == 3
        || month == 5
        || month == 7
        || month == 8
        || month == 10
        || month == 12)
      {
        day = decrementOrLoop(day, 31, 1);
      }
      else
      {
        day = decrementOrLoop(day, 30, 1);
      }
      break;
    case 9:
      year = decrementOrLoop(year, 9999);
      break;
    default:
      break;
    }

    dateChanged = true;
  }

  if (dateChanged)
  {
    sprintf(dateString, "%02d/%02d/%04d", month, day, year);

    if (!isDateValid(day, month, year))
    {
      lcd.setBacklight(RED);
    }
    else
    {
      lcd.setBacklight(GREEN);
    }

    lcd.setCursor(0, 1);
    lcd.print(dateString);

    dateChanged = false;
    cursorChanged = true;
  }

  if (cursorChanged == true)
  {
    lcd.setCursor(cursorPosition, 1);
    cursorChanged = false;
  }
}

void setDateExit()
{

}

void setTimeEnter()
{
  lcd.clear();
  lcd.setBacklight(GREEN);
  lcd.setCursor(0, 0);
  lcd.print("SET TIME:");
  lcd.setCursor(0, 1);

  newDateTime = rtc.now();
  sprintf(timeString, "%02d:%02d:%02d", newDateTime.hour(), newDateTime.minute(), newDateTime.second());
  lcd.print(timeString);

  cursorPosition = 1;
  maxCursorPosition = TIME_STRING_LENGTH - 1;

  lcd.setCursor(1, 1);
  lcd.cursor();
}

void setTimeUpdate()
{
  char hourString[3] = { timeString[0], timeString[1], '\0' };
  char minuteString[3] = { timeString[3], timeString[4], '\0' };
  char secondString[5] = { timeString[6], timeString[7], '\0' };

  int hour = atoi(hourString);
  int minute = atoi(minuteString);
  int second = atoi(secondString);

  bool timeChanged = false;
  bool cursorChanged = false;

  uint8_t buttons = lcd.readButtons();

  if ((buttons & BUTTON_RIGHT) && (cursorPosition < maxCursorPosition))
  {
    delay(TRANSITION_DELAY);
    Serial.write("right\n");
    if (cursorPosition == 1)
    {
      cursorPosition = 4;
    }
    else if (cursorPosition == 4)
    {
      cursorPosition = 7;
    }
    else
    {
      cursorPosition = 1;
    }

    cursorChanged = true;
  }
  else if ((buttons & BUTTON_LEFT) && (cursorPosition > 0))
  {
    delay(TRANSITION_DELAY);
    Serial.write("left\n");
    if (cursorPosition == 7)
    {
      cursorPosition = 4;
    }
    else if (cursorPosition == 4)
    {
      cursorPosition = 1;
    }
    else
    {
      cursorPosition = 7;
    }

    cursorChanged = true;
  }
  else if (buttons & BUTTON_UP)
  {
    delay(TRANSITION_DELAY);
    Serial.write("up\n");

    switch (cursorPosition)
    {
    case 1:
      hour = incrementOrLoop(hour, 23);
      break;
    case 4:
      minute = incrementOrLoop(minute, 60);
      break;
    case 9:
      second = incrementOrLoop(second, 60);
      break;
    default:
      break;
    }

    timeChanged = true;
  }
  else if (buttons & BUTTON_DOWN)
  {
    delay(TRANSITION_DELAY);
    Serial.write("down\n");

    switch (cursorPosition)
    {
    case 1:
      hour = decrementOrLoop(hour, 23);
      break;
    case 4:
      minute = decrementOrLoop(minute, 60);
      break;
    case 9:
      second = decrementOrLoop(second, 60);
      break;
    default:
      break;
    }

    timeChanged = true;
  }


  if (timeChanged)
  {
    sprintf(timeString, "%02d:%02d:%02d", hour, minute, second);

    if (!isTimeValid(hour, minute, second))
    {
      lcd.setBacklight(RED);
    }
    else
    {
      lcd.setBacklight(GREEN);
    }

    lcd.setCursor(0, 1);
    lcd.print(timeString);

    timeChanged = false;
    cursorChanged = true;
  }

  if (cursorChanged == true)
  {
    lcd.setCursor(cursorPosition, 1);
    cursorChanged = false;
  }
}

void setTimeExit()
{
  char dayString[3] = { dateString[3], dateString[4], '\0' };
  char monthString[3] = { dateString[0], dateString[1], '\0' };
  char yearString[5] = { dateString[6], dateString[7], dateString[8], dateString[9], '\0' };

  char hourString[3] = { timeString[0], timeString[1], '\0' };
  char minuteString[3] = { timeString[3], timeString[4], '\0' };
  char secondString[5] = { timeString[6], timeString[7], '\0' };

  int day = atoi(dayString);
  int month = atoi(monthString);
  int year = atoi(yearString);

  int hour = atoi(hourString);
  int minute = atoi(minuteString);
  int second = atoi(secondString);

  rtc.adjust(DateTime(year, month, day, hour, minute, second));
}

void show_time_tubes(uint8_t hours, uint8_t minutes)
{
  uint8_t hours_dec = hours / 10;
  uint8_t hours_unit = hours % 10;

  uint8_t min_dec = hours / 10;
  uint8_t min_unit = hours % 10;

  hours_dec_tube.show_digit(hours_dec, 127, 0);
  hours_unit_tube.show_digit(hours_unit, 127, 0);
  min_dec_tube.show_digit(min_dec, 127, 0);
  min_unit_tube.show_digit(min_unit, 127, 0);
}