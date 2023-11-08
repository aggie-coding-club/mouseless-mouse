#ifndef THREEML_APP_H
#define THREEML_APP_H

#include "3ml_document.h"
#include <functional>
#include <map>
#include <mutex>
#include <string>

namespace threeml {

class Document;

class App {
private:
  std::map<std::string, std::function<void(Document &)>> callbacks;
  Document &current_document;
  bool document_has_changed;
  std::mutex document_change_mutex;
  Display &display;

  void draw_task(void *parameters);

public:
  App() = default;
  App(App &) = delete;
  App(App &&) = default;

  void register_callback(std::string name, std::function<void(Document &)> callback_fn);
  bool begin();
};

} // namespace threeml

#endif