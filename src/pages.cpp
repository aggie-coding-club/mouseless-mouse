#include "pages.h"
#include "display.h"
#include "helpers.h"
#include "pages.h"
#include "mouse.h"
#include "io.h"
#include "imgs/hand.h"
#include "imgs/pointer.h"
#include "imgs/ring.h"
#include "imgs/thumb1.h"
#include "imgs/thumb2.h"
#include "imgs/pinky.h"
#include "imgs/middle.h"
#include "imgs/scrollDown.h"
#include "imgs/scrollUp.h"

// Allow interaction with the externally declared mouseQueue
extern xQueueHandle mouseQueue;

// Not much to do for a blank page
BlankPage::BlankPage(Display *display, DisplayManager *displayManager, const char *pageName)
  : DisplayPage(display, displayManager, pageName)
{}

// Great place for debug stuff
void BlankPage::draw() {
  display->textFormat(2, TFT_WHITE);
  display->buffer->drawString(pageName, 30, 30);
  display->buffer->drawString(String(touchRead(T7)), 30, 60);
  display->drawNavArrow(
    120, 110, pageName[12] & 1,
    0.5 - 0.5 * cos(6.28318 * float(frameCounter % 90) / 90.0),
    0x461F, TFT_BLACK
  );
  frameCounter++;
};

// This page doesn't do a lot, so it only needs to handle exiting
void BlankPage::onEvent(pageEvent_t event) {
  if (event == pageEvent_t::NAV_CANCEL)
    displayManager->pageStack.pop();
};

// Create a keyboard page attached to a display and manager
KeyboardPage::KeyboardPage(Display *display, DisplayManager *displayManager, const char *pageName)
  : DisplayPage(display, displayManager, pageName)
  , textBuffer(new char[32])
  , bufferIdx(0)
  , specialIdx(0)
  , letterIdx(0)
  , numberIdx(0)
  , colIdx(1)
  , specialTlY(0)
  , letterTlY(0)
  , numberTlY(0)
{
  for (byte i = 0; i < 32; i++)
    textBuffer[i] = '\0';
}

// Attach the keyboard page instance to a text field
KeyboardPage *KeyboardPage::operator()(char *field) {
  this->field = field;
  return this;
}

// Draw the keyboard page
void KeyboardPage::draw() {
  display->textFormat(2, TFT_WHITE);
  display->buffer->drawString(textBuffer, 20, 30);
  display->buffer->fillRect(130 + 30 * colIdx, 78, 30, 30, SEL_COLOR);
  for (int16_t i = -1; i < 2; i++) {
    if ((i + specialIdx) % 3_pm == 0) {
      display->buffer->drawLine(140, 85 + 30 * i + specialTlY, 154, 85 + 30 * i + specialTlY, TFT_WHITE);
      display->buffer->drawLine(154, 85 + 30 * i + specialTlY, 154, 99 + 30 * i + specialTlY, TFT_WHITE);
      display->buffer->drawLine(154, 99 + 30 * i + specialTlY, 140, 99 + 30 * i + specialTlY, TFT_WHITE);
      display->buffer->drawLine(140, 99 + 30 * i + specialTlY, 134, 92 + 30 * i + specialTlY, TFT_WHITE);
      display->buffer->drawLine(134, 92 + 30 * i + specialTlY, 140, 85 + 30 * i + specialTlY, TFT_WHITE);
    } else if ((i + specialIdx) % 3_pm == 1) {
      display->buffer->drawLine(140, 99 + 30 * i + specialTlY, 148, 99 + 30 * i + specialTlY, TFT_WHITE);
      display->buffer->drawLine(140, 96 + 30 * i + specialTlY, 140, 99 + 30 * i + specialTlY, TFT_WHITE);
      display->buffer->drawLine(148, 96 + 30 * i + specialTlY, 148, 99 + 30 * i + specialTlY, TFT_WHITE);
    } else if ((i + specialIdx) % 3_pm == 2) {
      display->drawNavArrow(145, 93 + 30 * i + specialTlY, false,
                            0.5 - 0.5 * cos(6.28318 * float(frameCounter % 90) / 90.0), ACCENT_COLOR, BGND_COLOR);
    }
  }
  for (int16_t i = -1; i < 2; i++) {
    display->buffer->drawString(String(char('A' + (i + letterIdx) % 26_pm)), 170, 85 + 30 * i + letterTlY);
  }
  for (int16_t i = -1; i < 2; i++) {
    display->buffer->drawString(String(char('0' + (i + numberIdx) % 10_pm)), 200, 85 + 30 * i + numberTlY);
  }
  specialTlY *= 0.5;
  letterTlY *= 0.5;
  numberTlY *= 0.5;
  frameCounter++;
}

// Event handling for the keyboard
void KeyboardPage::onEvent(pageEvent_t event) {
  switch (event) {
  case pageEvent_t::NAV_UP: {
    if (colIdx == 0) {
      specialIdx--;
      specialTlY -= 30;
    } else if (colIdx == 1) {
      letterIdx--;
      letterTlY -= 30;
    } else if (colIdx == 2) {
      numberIdx--;
      numberTlY -= 30;
    }
  } break;
  case pageEvent_t::NAV_DOWN: {
    if (colIdx == 0) {
      specialIdx++;
      specialTlY += 30;
    } else if (colIdx == 1) {
      letterIdx++;
      letterTlY += 30;
    } else if (colIdx == 2) {
      numberIdx++;
      numberTlY += 30;
    }
  } break;
  case pageEvent_t::NAV_SELECT: {
    if (colIdx == 0 && specialIdx % 3_pm == 2) {
      memcpy(field, textBuffer, bufferIdx);
      for (byte i = 0; i < 32; i++)
        textBuffer[i] = '\0';
      specialIdx = 0;
      letterIdx = 0;
      numberIdx = 0;
      colIdx = 1;
      displayManager->pageStack.pop();
    } else if (bufferIdx < 31) {
      if (colIdx == 0 && specialIdx % 3_pm == 0 && bufferIdx > 0)
        textBuffer[--bufferIdx] = '\0';
      else if (colIdx == 0 && specialIdx % 3_pm == 1)
        textBuffer[bufferIdx++] = ' ';
      else if (colIdx == 1)
        textBuffer[bufferIdx++] = char('A' + letterIdx % 26_pm);
      else if (colIdx == 2)
        textBuffer[bufferIdx++] = char('0' + numberIdx % 10_pm);
    }
  } break;
  case pageEvent_t::NAV_CANCEL:
    --colIdx %= 3_pm;
    break;
  default:
    break;
  }
}

// Create a confirmation page attached to a display and manager
ConfirmationPage::ConfirmationPage(Display *display, DisplayManager *displayManager, const char *pageName)
  : DisplayPage(display, displayManager, pageName)
{}

// Attach the confirmation page to a specific prompt message and action
ConfirmationPage *ConfirmationPage::operator()(const char *message, void (*callback)()) {
  this->message = message;
  this->callback = callback;
  return this;
}

// Draw the confirmation page
void ConfirmationPage::draw() {
  display->textFormat(2, TFT_WHITE);
  display->buffer->drawString(message, 10, 30);
}

// Event handling for the confirmation page
void ConfirmationPage::onEvent(pageEvent_t event) {
  switch (event) {
    case pageEvent_t::NAV_CANCEL:
      displayManager->pageStack.pop();
      break;
    case pageEvent_t::NAV_SELECT:
      callback();
      break;
    default:
      break;
  }
}



// Create a page that displays a real-time visualization of mouse inputs
InputDisplay::InputDisplay(Display* display, DisplayManager* displayManager, const char* pageName)
  : DisplayPage(display, displayManager, pageName)
  , lmb(false)
  , rmb(false)
  , scrollU(false)
  , scrollD(false)
  , lock(false)
  , calibrate(false)
{}

// Visualize the mouse input states
void InputDisplay::draw() {
  display->textFormat(2, TFT_WHITE);
  display->buffer->drawString(pageName, 30, 30);
  display->buffer->pushImage(60,60,64,64, hand);
  if(lmb)
    display->buffer->pushImage(60+12,60+24,7,11, thumb1);
  if(rmb)
    display->buffer->pushImage(60+12,60+35,7,11, thumb2);
  if(scrollU) {
    display->buffer->pushImage(60+29,60+33,5,3, scrollUp);
    display->buffer->pushImage(60+27, 60+6, 9, 19, middle);
  }
  if(scrollD)
    display->buffer->pushImage(60+29,60+40,5,3, scrollDown);
  if(lock)
    display->buffer->pushImage(60+37,60+9,7,17,ring);
  if(calibrate)
    display->buffer->pushImage(60+45,60+20,5,15, pinky);
}

// Handle mouse events via a callback that doesn't disturb the queue
void InputDisplay::onMouseEvent(mouseEvent_t event) {
  switch(event) {
    case mouseEvent_t::LMB_PRESS:
      lmb = true;
      break;
    case mouseEvent_t::LMB_RELEASE:
      lmb = false;
      break;
    case mouseEvent_t::RMB_PRESS:
      rmb = true;
      break;
    case mouseEvent_t::RMB_RELEASE:
      rmb = false;
      break;
    case mouseEvent_t::SCROLL_PRESS:
      scrollU = true;
      scrollD = true;
      break;
    case mouseEvent_t::SCROLL_RELEASE:
      scrollU = false;
      scrollD = false;
      break;
    case mouseEvent_t::LOCK_PRESS:
      lock = true;
      break;
    case mouseEvent_t::LOCK_RELEASE:
      lock = false;
      break;
    case mouseEvent_t::CALIBRATE_PRESS:
      calibrate = true;
      break;
    case mouseEvent_t::CALIBRATE_RELEASE:
      calibrate = false;
      break;
    default:
      break;
  }
}

// Handle relevant navigation events
void InputDisplay::onEvent(pageEvent_t event) {
  if (event == pageEvent_t::NAV_CANCEL)

    displayManager->pageStack.pop();
}

// Fun playground
DebugPage::DebugPage(Display *display, DisplayManager *displayManager, const char *pageName)
  : DisplayPage(display, displayManager, pageName)
{}

// Great place for debug stuff
void DebugPage::draw() {
  display->textFormat(2, TFT_WHITE);
  //frameCounter++; No animations being currently tested
};

// Currently testing page events
void DebugPage::onEvent(pageEvent_t event) {
  switch(event) {
    case pageEvent_t::NAV_DOWN:
      Serial.println("Hello World!");
      break;
    case pageEvent_t::NAV_CANCEL:
      this->displayManager->pageStack.pop();
      break;
    default:
      break;
  }
};

InlineSlider::InlineSlider(Display *display, DisplayManager *displayManager, const char *pageName, changeCallback_t onChange)
  : DisplayPage(display, displayManager, pageName)
  , sliderValue(0)
  , onChange(onChange)
{}

void InlineSlider::draw() {
  // Draw the underlying menu page
  displayManager->pageStack.pop();
  MenuPage *menuPage = reinterpret_cast<MenuPage*>(displayManager->pageStack.top());
  displayManager->pageStack.push(this);
  menuPage->draw();
  display->buffer->fillRect(0, menuPage->selectionY, 240, 30, SEL_COLOR);
  display->buffer->drawRect(39, menuPage->selectionY + 5, 162, 20, TFT_WHITE);
  display->buffer->fillRect(40, menuPage->selectionY + 6, 10 * sliderValue, 18, ACCENT_COLOR);
}

void InlineSlider::onEvent(pageEvent_t event) {
  switch(event) {
    case pageEvent_t::NAV_UP:
      if (sliderValue > 0) onChange(--sliderValue);
      break;
    case pageEvent_t::NAV_DOWN:
      if (sliderValue < 16) onChange(++sliderValue);
      break;
    case pageEvent_t::NAV_CANCEL:
      displayManager->pageStack.pop();
      break;
    case pageEvent_t::NAV_SELECT:
      break;
    default:
      break;
  }
}