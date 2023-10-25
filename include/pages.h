#ifndef DISPLAY_PAGES
#define DISPLAY_PAGES

#include "display.h"
#include <Arduino.h>

// Define a blank placeholder page
class BlankPage : public DisplayPage {
public:
  BlankPage(Display *display, DisplayManager *displayManager, const char *pageName);
  void draw();
  void onEvent(pageEvent_t event);
};

// Allow user input for text fields
class KeyboardPage : public DisplayPage {
private:
  char *textBuffer;
  byte bufferIdx;
  char *field;
  int16_t specialIdx;
  int16_t letterIdx;
  int16_t numberIdx;
  int16_t colIdx;
  int16_t specialTlY;
  int16_t letterTlY;
  int16_t numberTlY;

public:
  KeyboardPage(Display *display, DisplayManager *displayManager, const char *pageName);
  KeyboardPage *operator()(char *field);
  void draw();
  void onEvent(pageEvent_t event);
};

// Generic page prompting users to confirm an action
class ConfirmationPage : public DisplayPage {
private:
  const char *message;
  void (*callback)();

public:
  ConfirmationPage(Display *display, DisplayManager *displayManager, const char *pageName);
  ConfirmationPage *operator()(const char *message, void (*callback)());
  void draw();
  void onEvent(pageEvent_t event);
};

// Page to visualize mouse inputs in real-time
class InputDisplay : public DisplayPage {
private:
  bool lmb, rmb, scrollU, scrollD, lock, calibrate;
  
public:
  InputDisplay(Display* display, DisplayManager* displayManager, const char* pageName);
  void onEvent(pageEvent_t event);
  void onMouseEvent(mouseEvent_t event);
  void draw();
};

#endif