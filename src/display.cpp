#include "display.h"
#include "helpers.h"
#include "io.h"
#include <FS.h>
#include <LittleFS.h>
#include <TFT_eSPI.h>
#include <stack>

// Create a Display object to make double-buffered graphics programming easier
Display::Display(TFT_eSPI *tft, TFT_eSprite *bufferA, TFT_eSprite *bufferB)
    : tft(tft)
    , bufferA(bufferA)
    , bufferB(bufferB)
    , fillColor(TFT_WHITE)
    , strokeColor(TFT_WHITE)
    , brightness(0)
    , buffer(bufferA)
{
  bufferA->createSprite(240, 135);
  bufferB->createSprite(240, 135);
}

// Buffers are non-trivial objects that must be destroyed explicitly
Display::~Display() {
  delete bufferA;
  delete bufferB;
}

// Read words of image data from SPIFFS
uint16_t Display::read16(fs::File &f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

// Read dwords of image data from SPIFFS
uint32_t Display::read32(fs::File &f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}

// Set up the TFT display
void Display::begin() {
  if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED) { // We're waking up from sleep, not a full reset
    tft->writecommand(0x11);                                        // Wake up TFT display
    delay(120);                                                     // Required for TFT power supply to stabilize
  }
  tft->init();
  tft->initDMA();
  tft->setRotation(1); // Landscape, buttons on the right
  tft->fillScreen(TFT_BLACK);
  ledcAttachPin(BACKLIGHT_PIN, PWM_CHANNEL); // Attach the display's backlight to an LED controller channel
  ledcSetup(PWM_CHANNEL, 20000, 8);          // 20 kHz PWM, 8 bit resolution
  ledcWrite(PWM_CHANNEL, BRIGHT_BRIGHTNESS); // This little backlight of mine, I'm gonna let it shine
  brightness = BRIGHT_BRIGHTNESS;            // Update brightness tracker
}

// Put the TFT display's controller to sleep
void Display::sleepMode() {
  tft->writecommand(0x10); // No official library support, but mentioned in GitHub (TFT_eSPI issue 497)
  ledcWrite(PWM_CHANNEL, 0);
  delay(5);
}

// Change the backlight power (does not modify image data)
void Display::dim(uint8_t brightness) {
  this->brightness = brightness;
  ledcWrite(PWM_CHANNEL, brightness);
}

// Black screen
void Display::clear() { buffer->fillSprite(TFT_BLACK); }

// Clear display and flush both buffers
void Display::flush() {
  clear();
  pushChanges();
  clear();
  pushChanges();
}

// Adapted from a TFT_eSPI example sketch
void Display::drawBitmapSPIFFS(const char *filename, uint16_t x, uint16_t y) {
  if ((x >= tft->width()) || (y >= tft->height()))
    return;
  fs::File bmpFS;

  // Open requested file on SD card
  bmpFS = LittleFS.open(filename, FILE_READ);

  if (!bmpFS) {
    Serial.println("File not found");
    return;
  }

  uint32_t seekOffset;
  uint16_t w, h, row, col;
  uint8_t r, g, b;
  uint16_t header = read16(bmpFS);
  // All valid bitmap files start with "BM" (little-endian)
  if (header == 0x4D42) {
    read32(bmpFS);
    read32(bmpFS);
    seekOffset = read32(bmpFS);
    read32(bmpFS);
    w = read32(bmpFS);
    h = read32(bmpFS);

    if ((read16(bmpFS) == 1) && (read16(bmpFS) == 24) && (read32(bmpFS) == 0)) {
      y += h - 1;

      bool oldSwapBytes = buffer->getSwapBytes();
      buffer->setSwapBytes(true);
      bmpFS.seek(seekOffset);

      uint16_t padding = (4 - ((w * 3) & 3)) & 3;
      uint8_t lineBuffer[w * 3 + padding];

      for (row = 0; row < h; row++) {
        bmpFS.read(lineBuffer, sizeof(lineBuffer));
        uint8_t *bptr = lineBuffer;
        uint16_t *tptr = (uint16_t *)lineBuffer;
        // Convert 24 to 16 bit colours
        for (col = 0; col < w; col++) {
          b = *bptr++;
          g = *bptr++;
          r = *bptr++;
          *tptr++ = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
        }
        // Push the pixel row to screen, pushImage will crop the line if needed
        // y is decremented as the BMP image is drawn bottom up
        buffer->pushImage(x, y--, w, 1, (uint16_t *)lineBuffer);
      }
      buffer->setSwapBytes(oldSwapBytes);
    } else
      Serial.println("BMP format not recognized.");
  } else
    Serial.printf("Could not read BMP file \"%s\" (Size: %i). Header: %X\n", filename, bmpFS.size(), header);
  bmpFS.close();
}

// Apply new text formatting to both framebuffers
void Display::textFormat(uint8_t size, uint16_t color) {
  bufferA->setTextSize(size);
  bufferB->setTextSize(size);
  bufferA->setTextColor(color);
  bufferB->setTextColor(color);
}

// Push the active framebuffer to the screen, then swap buffers
void Display::pushChanges() {
  buffer->pushSprite(0, 0);
  buffer = (buffer == bufferA) ? bufferB : bufferA;
}

// Draw the status bar
void Display::drawStatusBar() {
  byte batPercentage = getBatteryPercentage();
  buffer->fillRect(0, 0, 240, SBAR_HEIGHT, ACCENT_COLOR);
  buffer->fillRoundRect(210, 2, 18, 11, 2, BGND_COLOR);
  buffer->fillRoundRect(226, 5, 5, 5, 2, BGND_COLOR);
  buffer->fillRoundRect(212, 4, 14 * (batPercentage / 100.0), 7, 2, TEXT_COLOR);
}

// Draw an animated navigation arrow that shows what a user's input will do
void Display::drawNavArrow(uint16_t x, uint16_t y, bool direction, float progress, uint16_t stroke_color,
                           uint16_t bg_color) {
  progress = min(1.0F, progress);
  int16_t flip = direction ? -1 : 1;
  uint16_t arrowheadX;
  uint16_t arrowheadY;
  float arrowheadAngle;
  float arrowTailAngle;
  float arrowheadAngleRads;
  uint16_t arrowheadLength = 5;
  float arrowheadBreadth = 0.6;
  if (progress < 0.5)
    buffer->drawLine(x, y + 10 * flip, x, y - 5 * flip, stroke_color);
  else if (progress < 0.75)
    buffer->drawLine(x, y + (40 - 60 * progress) * flip, x, y - 5 * flip, stroke_color);
  arrowheadX = 5 - 5 * cos(6.28318 * 3 * min(float(0.25), progress));
  arrowheadY = -5 - 5 * sin(6.28318 * 3 * min(float(0.25), progress));
  if (progress > 0.25) {
    buffer->drawLine(x + 5 * flip, y, x + max(float(-10), 20 - 60 * progress) * flip, y, stroke_color);
    arrowheadX += 15 - 60 * min(float(0.5), progress);
  }
  arrowheadX = arrowheadX * flip + x;
  arrowheadY = arrowheadY * flip + y;
  arrowheadAngle =
      180 * !direction - 90 +
      360 * 3 * min(float(0.25), progress); // TFT_eSPI defines 0 degrees as straight down and positive as clockwise
  arrowTailAngle = 180 * !direction - 90 + 360 * 3 * max(float(0), progress - float(0.75));
  arrowheadAngleRads = 3.14159 * (0.5 + direction) - 6.28318 * 3 * min(float(0.25), progress); // C++ uses real math
  if (progress > 0 && progress < 1)
    buffer->drawArc(x + 5 * flip, y - 5 * flip, 5, 5, arrowTailAngle % 360_pm, arrowheadAngle % 360_pm,
                          stroke_color, bg_color);
  buffer->fillTriangle(arrowheadX, arrowheadY,
                             arrowheadX - arrowheadLength * cos(arrowheadAngleRads - arrowheadBreadth),
                             arrowheadY + arrowheadLength * sin(arrowheadAngleRads - arrowheadBreadth),
                             arrowheadX - arrowheadLength * cos(arrowheadAngleRads + arrowheadBreadth),
                             arrowheadY + arrowheadLength * sin(arrowheadAngleRads + arrowheadBreadth), stroke_color);
}

// Create a base DisplayPage class for other page types to inherit from
DisplayPage::DisplayPage(Display *display, DisplayManager *displayManager, const char *pageName)
    : display(display)
    , displayManager(displayManager)
    , frameCounter(0)
    , pageName(pageName)
{}

// These classes really should be instantiated statically, so these destructors are more of a formality
DisplayPage::~DisplayPage() {}

MenuPage::~MenuPage() { delete[] this->memberPages; }

// Draw the menu if no subpage is active; otherwise, draw the active subpage
void MenuPage::draw() {
  // Draw the menu, highlighting item # subpageIdx
  display->textFormat(2, TFT_WHITE);
  for (byte i = 0; i < numPages; i++) {
    if (15 + i * 30 + menuTlY < -15)
      continue; // No reason to draw things that are out of frame
    if (15 + i * 30 + menuTlY > 135)
      break;
    if (i == subpageIdx) {
      display->buffer->fillRect(0, 15 + i * 30 + selectionTlY + menuTlY, 240, 30, SEL_COLOR);
      if (displayManager->upButton->isPressed || displayManager->downButton->isPressed) {
        Button *activeButton =
            displayManager->upButton->isPressed ? displayManager->upButton : displayManager->downButton;
        display->drawNavArrow(220, 30 + i * 30 + selectionTlY + menuTlY, displayManager->upButton->isPressed,
                              pow(millis() - activeButton->pressTimestamp, 2) / pow(LONGPRESS_TIME, 2), ACCENT_COLOR,
                              SEL_COLOR);
      }
    }
    display->buffer->drawString(memberPages[i]->pageName, 30, 25 + i * 30 + menuTlY);
  }
  int16_t targetMenuTlY = min(0, 40 - 30 * subpageIdx);
  menuTlY += 0.25 * (targetMenuTlY - menuTlY);
  selectionTlY *= 0.75;
  this->frameCounter++;
}

// Callback runs when an event appears in the event queue
void MenuPage::onEvent(pageEvent_t event) {
  switch (event) {
  case pageEvent_t::ENTER:
    break;
  case pageEvent_t::EXIT:
    break;
  case pageEvent_t::NAV_PRESS:
    break;
  case pageEvent_t::NAV_UP:
    if (subpageIdx > 0) {
      subpageIdx--;
      selectionTlY += 30;
    }
    break;
  case pageEvent_t::NAV_DOWN:
    if (subpageIdx < numPages - 1) {
      subpageIdx++;
      selectionTlY -= 30;
    }
    break;
  case pageEvent_t::NAV_SELECT: {
    memberPages[subpageIdx]->onEvent(pageEvent_t::ENTER);
    displayManager->pageStack.push(memberPages[subpageIdx]);
  } break;
  case pageEvent_t::NAV_CANCEL:
    displayManager->pageStack.pop();
    break;
  }
}

// Create the home page of the display which contains a menu page to open upon a button press
HomePage::HomePage(Display *display, DisplayManager *displayManager, const char *pageName, MenuPage *mainMenu)
    : DisplayPage(display, displayManager, pageName), mainMenu(mainMenu) {}

extern uint32_t ulp_ctr;
// Draw a home page
void HomePage::draw() {
  display->textFormat(2, TFT_WHITE);
  display->buffer->drawString("Battery Life:", 30, 30);
  display->buffer->drawString(String(ulp_ctr & 65535), 30, 90);
  display->textFormat(1, TFT_WHITE);
  display->buffer->drawString(String(__TIME__) + " " + String(__DATE__), 30, 120);
  display->textFormat(3, TFT_WHITE);
  display->buffer->drawString(String(getBatteryPercentage()) + "%", 30, 50);
  frameCounter++;
}

// Callback runs when an event appears in the event queue
void HomePage::onEvent(pageEvent_t event) {
  switch (event) {
  case pageEvent_t::NAV_SELECT:
    displayManager->pageStack.push(mainMenu);
    break;
  default:
    break;
  }
}

// Create a DisplayManager object to control page navigation and manage which page is displayed
DisplayManager::DisplayManager(Display *display) : display(display), lastEventFrame(0) {
  eventQueue = xQueueCreate(4, sizeof(pageEvent_t));
}

// Set the homepage (has to be done after instantiation because HomePage needs a DisplayManager)
void DisplayManager::setHomepage(HomePage *homepage) {
  assert(this->pageStack.empty());
  pageStack.push(homepage);
}

// Pass Button instances to DisplayManager so the display pages can read the button statuses
void DisplayManager::attachButtons(Button *upButton, Button *downButton) {
  this->upButton = upButton;
  this->downButton = downButton;
}

// Receive button events, handle display dimming, draw the active page, and draw the status bar
void DisplayManager::draw() {
  // Forward any events to the active page
  if (uxQueueMessagesWaiting(eventQueue)) {
    pageEvent_t event;
    xQueueReceive(eventQueue, &event, 0);
    lastEventFrame = frameCtr;
    if (display->brightness < BRIGHT_BRIGHTNESS)
      display->dim(BRIGHT_BRIGHTNESS);
    this->pageStack.top()->onEvent(event);
  }
  // Dim the display after a period of inactivity
  else if (display->brightness > DIM_BRIGHTNESS && lastEventFrame + INACTIVITY_TIME < frameCtr)
    display->dim(max((int)DIM_BRIGHTNESS, display->brightness - 5));

  // Draw the status bar and the active page
  pageStack.top()->draw();
  display->drawStatusBar();
  display->textFormat(1, TFT_BLACK);
  display->buffer->drawString(pageStack.top()->pageName, (240 - display->buffer->textWidth(pageStack.top()->pageName)) / 2, 4);
  frameCtr++;
}