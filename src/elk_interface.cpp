#include "elk_interface.h"
#include <unordered_map>
#include <memory>
#include <cstdarg>

JS::BaseProperty::BaseProperty(const char *name)
  : type(Type::UNKNOWN_T), name(name)
{}

JS::BaseProperty::BaseProperty(Type type, const char *name)
  : type(type), name(name)
{}

JS::Prototype::Prototype(size_t argc, BaseProperty *argv...)
  : BaseProperty(nullptr)
{
  va_list args;
  va_start(args, argv);

  while (argc--) {
    BaseProperty *arg = va_arg(args, BaseProperty*);
    properties.insert(std::pair<std::string, BaseProperty*>(arg->name, arg));
    properties_vec.push_back(arg);
  }

  va_end(args);
}

JS::Object::Object(const char *name, size_t argc, va_list args)
  : BaseProperty(name)
{
  while (argc--) {
    BaseProperty *arg = va_arg(args, BaseProperty*);
    properties.insert(std::pair<std::string, BaseProperty*>(arg->name, arg));
  }
}

JS::Constructor::Constructor(struct js *env, Prototype *proto)
  : env(env), proto(proto)
{}

JS::Object *JS::Constructor::operator() (const size_t argc, const char *name, void *values...) {
  va_list args;
  va_start(args, values);

  Object *result = nullptr;

  BaseProperty *properties[argc] = { nullptr };
  size_t idx = 0;

  for (std::pair<std::string, BaseProperty*> prop : proto->properties) {
    switch (prop.second->type) {
      case BaseProperty::Type::BOOL_T: {
        bool *arg = va_arg(args, bool*);
        properties[idx] = new Property<bool>(prop.second->name, *arg);
      } break;
      default:
        break;
    }
  }

  delete this;
  return result;
}

JS::Constructor& JS::Prototype::operator[] (struct js *env) {
  Constructor *result = new Constructor(env, this);
  return *result;
}