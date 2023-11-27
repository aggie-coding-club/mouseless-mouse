#include <LittleFS.h>
#include <elk.h>
#include <unordered_map>

#include "pages.h"
#include "display.h"
#include "helpers.h"
#include "pages.h"
#include "mouse.h"
#include "io.h"
#include "elk_interface.h"
#include "imgs/hand.h"
#include "imgs/pointer.h"
#include "imgs/ring.h"
#include "imgs/thumb1.h"
#include "imgs/thumb2.h"
#include "imgs/pinky.h"
#include "imgs/middle.h"
#include "imgs/scrollDown.h"
#include "imgs/scrollUp.h"

// Set memory space allocated to each Elk script in a DOMPage
const size_t ELK_STACK = 4096;

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
    case pageEvent_t::NAV_SELECT:
      callback();
    case pageEvent_t::NAV_CANCEL:
      displayManager->pageStack.pop();
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

DOMPage::DOMPage(Display *display, DisplayManager *displayManager, const char *fileName)
  : DisplayPage(display, displayManager, fileName)  // File name will be used as page title until DOM is loaded for the first time
  , sourceFileName(fileName)
  , dom(nullptr)
  , scripts(std::vector<Script*>())
  , nextSelectableNode{nullptr, 0}
  , selectedNode{nullptr, 0}
  , prevSelectableNode{nullptr, 0}
  , scrollPos(0)
  , scrollTlY(0)
  , selectionIdx(0)
{}

void DOMPage::draw() {
  if (!dom) {
    Serial.println("No page DOM - exiting page");
    displayManager->pageStack.pop();
    displayManager->pageStack.top()->onEvent(pageEvent_t::ENTER);
    return;
  }
  int16_t yPos = 20 - scrollPos - scrollTlY;
  int8_t nodeSelector = selectionIdx;

  // Draw the page by recursing through the DOM tree
  std::function<void(threeml::DOMNode*)> drawDOMNode = [&drawDOMNode, &yPos, &nodeSelector, this](threeml::DOMNode* node){
    // Only plaintext nodes are directly drawn - their appearances are determined by their parent nodes
    if (node->type != threeml::NodeType::PLAINTEXT)
      for (threeml::DOMNode* child : node->children) {
        drawDOMNode(child);
      }
    else {
      // Determine proper text formatting from parent node
      byte textSize = node->parent->type == threeml::NodeType::H1 ? 3 : 2;
      uint16_t textColor = TFT_WHITE;
      if (node->parent->type == threeml::NodeType::A)
        textColor = ACCENT_COLOR;
      else if (node->parent->type == threeml::NodeType::BUTTON)
        textColor = TFT_BLACK;
      display->textFormat(textSize, textColor);

      // Handle extra features related to selectable nodes
      if (node->parent->selectable) {
        // While it's computationally convenient, get the y positions of the current and adjacent selectable nodes
        if (nodeSelector == 1)
          prevSelectableNode = {node, yPos};
        else if (nodeSelector == -1)
          nextSelectableNode = {node, yPos};
        if (yPos + node->height > 10 && yPos < display->buffer->height()) {
          int16_t maxTextWidth = 0;
          // Compute maximum element text width for button bounding box
          if (node->parent->type == threeml::NodeType::BUTTON) {
            for (std::string str : node->plaintext_data)
              maxTextWidth = max(maxTextWidth, display->buffer->textWidth(str.c_str()));
          }
          // Draw selection box around selected element
          if (nodeSelector == 0) {
            display->buffer->fillRect(0, yPos - 1, display->buffer->width(), node->height, SEL_COLOR);
            // If the selected element is a button, draw it in its highlighted color
            if (node->parent->type == threeml::NodeType::BUTTON)
              display->buffer->fillRect(0, yPos - 1, maxTextWidth, node->height, ACCENT_COLOR);
            selectedNode = {node, yPos};
          }
          else if (node->parent->type == threeml::NodeType::BUTTON)
            // Draw unselected buttons in their unhighlighted color
            display->buffer->fillRect(0, yPos - 1, maxTextWidth, node->height, TFT_WHITE);
        }
        --nodeSelector;
      }

      // Draw the text of a plaintext node one line at a time
      for (std::string str : node->plaintext_data) {
        if (yPos > -10 && yPos < display->buffer->height()) {
          display->buffer->drawString(str.c_str(), 0, yPos);
          // If the element is an anchor, underline the text
          if (node->parent->type == threeml::NodeType::A)
            display->buffer->drawFastHLine(0, yPos + 17, display->buffer->textWidth(str.c_str()), ACCENT_COLOR);
        }
        yPos += 10 * textSize;
      }
    }
  };

  // Above was just a function definition - nothing has been drawn yet, so assume there are no selectable nodes
  prevSelectableNode = {nullptr, 0};
  nextSelectableNode = {nullptr, 0};

  // Start the recursive drawing function at the body element
  for (threeml::DOMNode* node : dom->top_level_nodes) {
    if (node->type == threeml::NodeType::BODY)
      drawDOMNode(node);
  }

  // Smooth scrolling yay
  scrollTlY *= 0.75;
}

void DOMPage::onEvent(pageEvent_t event) {
  switch (event) {
    // Load the page before displaying it (onEvent is a blocking call in the draw task)
    case pageEvent_t::ENTER: {
      load();
    } break;
    // Unload and exit the page, making sure to fire the ENTER event for the receiving page
    case pageEvent_t::NAV_CANCEL: {
      displayManager->pageStack.pop();
      unload();
      displayManager->pageStack.top()->onEvent(pageEvent_t::ENTER);
    } break;
    // Handle scrolling and selection for a down button press
    case pageEvent_t::NAV_DOWN: {
      // Only change the selection if the element that would be selected is fully visible
      if (nextSelectableNode.node && nextSelectableNode.yPos + nextSelectableNode.node->height <= display->buffer->height())
          ++selectionIdx;
      if (dom->height - scrollPos + 20 > display->buffer->height()) {
        scrollPos += 20;
        scrollTlY -= 20;
      }
    } break;
    // Handle scrolling and selection for an up button press
    case pageEvent_t::NAV_UP: {
      // Only change the selection if the element that would be selected is fully visible
      if (prevSelectableNode.node && prevSelectableNode.yPos > 10)
          --selectionIdx;
      if (scrollPos >= 20) {
        scrollPos -= 20;
        scrollTlY += 20;
      }
    } break;
    // Handle selection actions for selectable elements
    case pageEvent_t::NAV_SELECT: {
      // Only process a selection event if the target element is fully visible
      if (selectedNode.node && selectedNode.yPos > 10 && selectedNode.yPos + selectedNode.node->height <= display->buffer->height()) {
        if (selectedNode.node->parent->type == threeml::NodeType::A) {
          Serial.printf("Not yet implemented - navigate to `%s`\n", selectedNode.node->parent->unique_attributes.at("href").c_str());
        }
        else if (selectedNode.node->parent->type == threeml::NodeType::BUTTON) {
          for (Script *script : scripts) {
            jsval_t result = js_eval(script->engine, selectedNode.node->parent->unique_attributes.at("onclick").c_str(), ~0);
            // Serial.printf("Onclick script result: %s\n", js_str(script->engine, result));
            (void) result;
          }
        }
      }
    } break;
    default:
      break;
  }
}

void DOMPage::load() {
  scrollPos = 0;
  scrollTlY = 0;
  selectionIdx = 0;
  loadDOM();
  // Abort if loading the DOM failed
  if (!dom) return;
  for (threeml::DOMNode *node : dom->top_level_nodes)
    // Process metadata in the head element
    if (node->type == threeml::NodeType::HEAD) {
      for (threeml::DOMNode *child : node->children) {
        // Find the page title and change DisplayPage::pageName accordingly
        if (child->type == threeml::NodeType::TITLE) {
          for (threeml::DOMNode *text : child->children)
            if (text->type == threeml::NodeType::PLAINTEXT && !text->plaintext_data.empty())
              pageName = text->plaintext_data.front().c_str();
        }
        // Load scripts as they are encountered
        else if (child->type == threeml::NodeType::SCRIPT) {
          loadScript(child);
        }
      }
    }
    // With all scripts loaded, execute the `onload` callback if it exists
    else if (node->type == threeml::NodeType::BODY) {
      if (node->unique_attributes.find("onload") != node->unique_attributes.end())
        for (Script *script : scripts) {
          jsval_t result = js_eval(script->engine, node->unique_attributes.at("onload").c_str(), ~0);
          // Serial.printf("Onload script result: %s\n", js_str(script->engine, result));
          (void) result;
        }
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
| NOTE:                                                                                                             |
| Large memory allocations in the loading functions are performed with `calloc` not because I am a masochist but    |
| because the ESP32 does not support standard try/catch exception handling. Rather than throwing `std::bad_alloc`,  |
| `calloc` will return `nullptr`, which can be handled gracefully by cancelling the page load.                      |
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void DOMPage::loadDOM() {
  // Read the source code from the source file
  File sourceFile = LittleFS.open(sourceFileName);
  if (!sourceFile) {
    Serial.printf("3ML source file `%s` not found\n", sourceFileName);
    return;
  }
  size_t fileSize = sourceFile.available();
  char *sourceCode = (char*)calloc(fileSize + 1, sizeof(char));
  if (!sourceCode) {
    Serial.println("Failed to allocate space for 3ML source code buffer - likely out of memory");
    sourceFile.close();
    return;
  }
  sourceFile.readBytes(sourceCode, fileSize);
  sourceCode[fileSize] = '\0';
  sourceFile.close();
  // Construct a DOM tree from the source code
  dom = threeml::clean_dom(threeml::parse_string(sourceCode));
  // The DOM tree copies everything from the source code, so it can be freed
  free(sourceCode);
  // Serial.printf("DOMPage loaded - free heap: %i\n", xPortGetFreeHeapSize());
}

Script::Script()
  : memory((char*)calloc(ELK_STACK, sizeof(char)))
  , engine(js_create(memory, ELK_STACK))
{}

Script::~Script() {
  free(memory);
}

// JS::Prototype Bob(
//   new JS::PropertyType<int>("id"),
//   new JS::PropertyType<const char*>("content"),
//   new JS::PropertyType<std::function<int(const char*)>>("sayHello")
// );

// struct js *myJs;
// JS::Object *myBob = Bob[myJs]("Bobbothy",
//   7,
//   "Hello, World!",
//   (std::function<int(const char*)>)[](const char *str){ Serial.println(str); return 1; }
// );

// Register standard JavaScript bindings necessary for working with a 3ML page
void Script::register3MLBindings() {
  // Implement `console.log`
  jsval_t consoleObject = js_mkobj(engine);       // Create `console` object
  js_set(engine, consoleObject, "log", js_mkfun(  // Create `log` method and attach it to `console`
    [](struct js *eng, jsval_t *args, int nargs) {
      for (int8_t i = 0; i < nargs; ++i) {
        if (js_type(*(args + i)) == JS_STR) {
          size_t *lenDummy = new size_t(0);
          Serial.print(js_getstr(eng, *(args + i), lenDummy));
          delete lenDummy;
        }
        else
          Serial.print(js_str(eng, *(args + i)));
        if (i < nargs - 1)
          Serial.print(' ');
      }
      Serial.println();
      return js_mkval(JS_UNDEF);
    }
  ));
  js_set(engine, js_glob(engine), "console", consoleObject);  // Attach `console` to the root namespace
}

void DOMPage::loadScript(threeml::DOMNode *script) {
  // Open the source file
  const char *sourceFileName = script->unique_attributes.at("src").c_str();
  File sourceFile = LittleFS.open(sourceFileName);
  if (!sourceFile) {
    Serial.printf("Elk source file `%s` not found\n", sourceFileName);
    return;
  }
  // Create a Script object
  Script *result = new Script();
  if (!result->engine) {
    Serial.println("Failed to create Elk script engine - likely out of memory");
    delete result;
    return;
  }
  // Read the source code from the source file
  size_t fileSize = sourceFile.available();
  char *sourceCode = (char*)calloc(fileSize + 1, sizeof(char));
  if (!sourceCode) {
    Serial.println("Failed to allocate space for Elk source code buffer - likely out of memory");
    sourceFile.close();
    delete result;
    return;
  }
  sourceFile.readBytes(sourceCode, fileSize);
  sourceCode[fileSize] = '\0';
  sourceFile.close();
  // Make 3ML bindings available to the script when it runs
  result->register3MLBindings();
  // Run the script
  jsval_t scriptResult = js_eval(result->engine, sourceCode, ~0);
  // Elk copies all necessary information to the runtime space, so the source code is no longer necessary
  free(sourceCode);
  // Serial.printf("Script result (not a callback): %s\n", js_str(result->engine, scriptResult));
  (void) scriptResult;

  // The script engine now contains all variables and functions registered by the source code - save it for callback execution
  scripts.push_back(result);
}

void DOMPage::unload() {
  // While everything is still loaded, execute the `onbeforeunload` callback if it exists
  for (threeml::DOMNode *node : dom->top_level_nodes) {
    if (node->type == threeml::NodeType::BODY) {
      if (node->unique_attributes.find("onbeforeunload") != node->unique_attributes.end()) {
        for (Script *script : scripts) {
          jsval_t result = js_eval(script->engine, node->unique_attributes.at("onbeforeunload").c_str(), ~0);
          // Serial.printf("Onbeforeunload script result: %s\n", js_str(script->engine, result));
          (void) result;
        }
      }
    }
  }
  // Free the memory occupied by each script
  for (Script *script : scripts)
    delete script;
  // Deregister all scripts in the scripts vector - their pointers are all invalid
  scripts.clear();
  // Free the memory occupied by the DOM tree
  delete dom;
}