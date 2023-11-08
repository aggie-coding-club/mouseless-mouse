#include "3ml_app.h"

namespace threeml {

void App::draw_task(void *parameters) {
  App *app = static_cast<App *>(parameters);
  Document *current = &app->current_document;
  for (;;) {
    app->document_change_mutex.lock();
    if (document_has_changed) {
      document_has_changed = false;
      current = &app->current_document;
    }
    current->render(app->display);
    display.pushChanges();
  }
}

} // namespace threeml