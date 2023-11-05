#ifndef THREEML_DOCUMENT_H
#define THREEML_DOCUMENT_H

#include "3ml_cleaner.h"
#include "display.h"
#include <string>
#include <vector>

namespace threeml {

using PlaintextData = std::string;
struct ButtonData {
  std::string label;
  std::string callback;

  ButtonData(std::string label, std::string callback) : label(label), callback(callback) {}
};
struct LinkData {
  std::string label;
  std::string points_to;

  LinkData(std::string label, std::string points_to) : label(label), points_to(points_to) {}
};
struct TextInputData {
  std::string contents;
  std::string oninput_callback;

  TextInputData(std::string oninput_callback = "") : contents(""), oninput_callback(oninput_callback) {}
};
struct SliderData {
  std::size_t min;
  std::size_t max;
  std::size_t value;
  std::string oninput_callback;

  SliderData(std::size_t min, std::size_t max, std::string oninput_callback = "")
      : min(min), max(max), value(min), oninput_callback(oninput_callback) {}
};

/// @brief Represents a displayed 3ML element from the renderer perspective.
struct DocumentElement {
  enum class Type { PLAINTEXT, BUTTON, LINK, TEXT_INPUT, SLIDER };
  union {
    PlaintextData plaintext_data;
    ButtonData button_data;
    LinkData link_data;
    TextInputData text_input_data;
    SliderData slider_data;
  };
  std::string id;
  std::string class_;
};

/// @brief Represents a 3ML document from the renderer perspective.
class Document {
private:
  std::string title;
  std::vector<DocumentElement> elements;
  std::string onload_callback;
  std::string onbeforeunload_callback;
  bool has_dom_loaded;

public:
  Document() = default;
  Document(Document &) = delete;
  Document(Document &&) = default;

  void load_dom(DOM dom);
  void render(Display &display);
  void onEvent(pageEvent_t event);
};

} // namespace threeml

#endif