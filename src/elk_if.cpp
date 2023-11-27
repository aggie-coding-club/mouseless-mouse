#include "elk_if.h"

bool JS::ObjectIFace::exists(struct js *engine, const char *name) {
  if (js_type(js_eval(engine, name, ~0)) != JS_ERR)
    return true;
  return false;
}

bool JS::ObjectIFace::bind(struct js *engine, const char *name) {
  if (exists(engine, name)) {
    this->engine = engine;
    this->target = name;
    return true;
  }
  return false;
}