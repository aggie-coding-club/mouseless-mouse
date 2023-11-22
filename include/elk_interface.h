#ifndef MY_DISAPPOINTMENT_IS_IMMEASURABLE_AND_MY_DAY_IS_RUINED
#define MY_DISAPPOINTMENT_IS_IMMEASURABLE_AND_MY_DAY_IS_RUINED

#include <functional>
#include <type_traits>
#include <unordered_map>

#include <Arduino.h>
#include <elk.h>

template <typename T, typename = void>
struct is_function : std::false_type {};

template <typename FN, typename... Args>
struct is_function<std::function<FN(Args...)>> : std::true_type {};

template <typename T, typename U, typename... Ts>
struct get_second_type { typedef U type; };

namespace JS {

  // Associates a C++ type with a JavaScript property
  template <typename T>
  struct PropertyType;

  // Associates a C++ typed value with a PropertyType
  template <typename T>
  struct Property;

  struct Object;
  struct Prototype;



  // Just a name for a property - all property types inherit from BaseProperty so they can be stored together
  struct BaseProperty {
    enum class Type {
      UNDEF_T = JS_UNDEF,
      NULL_T = JS_NULL,
      BOOL_T = JS_TRUE,
      STRING_T = JS_STR,
      NUM_T = JS_NUM,
      ERROR_T = JS_ERR,
      FUNC_OR_OBJ_T = JS_PRIV,
      OBJECT_T,
      FUNCTION_T,
      UNKNOWN_T
    } type;

    const char *name;

    BaseProperty() = delete;
    BaseProperty(const char *name);
    BaseProperty(Type type, const char *name);

    template <typename T>
    bool has_type() {
      PropertyType<T> *yes = dynamic_cast<PropertyType<T>*>(this);
      return bool(yes);
    }

    template <typename T>
    explicit operator PropertyType<T>& () {
      PropertyType<T> *valid = dynamic_cast<PropertyType<T>*>(this);
      if (!valid) {
        Serial.printf("JS -> C++: Invalid dynamic cast of BaseProperty to PropertyType<%s>\n", typeid(T).name());
        while (true);
      }
      return *valid;
    }

    template <typename T>
    explicit operator Property<T>& () {
      Property<T> *valid = dynamic_cast<Property<T>*>(this);
      if (!valid) {
        Serial.printf("JS -> C++: Invalid dynamic cast of BaseProperty to Property<%s>\n", typeid(T).name());
        while (true);
      }
      return *valid;
    }
  };



  template <typename T>
  struct PropertyType : public BaseProperty {
  private:
    template <typename U = T>
    static typename std::enable_if<std::is_arithmetic<U>::value && !std::is_same<U, bool>::value, BaseProperty::Type>::type getType() { return BaseProperty::Type::NUM_T; }
    
    template <typename U = T>
    static typename std::enable_if<std::is_same<U, const char*>::value, BaseProperty::Type>::type getType() { return BaseProperty::Type::STRING_T; }

    template <typename U = T>
    static typename std::enable_if<std::is_same<U, bool>::value, BaseProperty::Type>::type getType() { return BaseProperty::Type::BOOL_T; }

    template <typename U = T>
    static typename std::enable_if<is_function<U>::value, BaseProperty::Type>::type getType() { return BaseProperty::Type::FUNCTION_T; }

    template <typename U = T>
    static typename std::enable_if<std::is_same<U, Object>::value, BaseProperty::Type>::type getType() { return BaseProperty::Type::OBJECT_T; }

    // You'll thank me for this one day, I promise
    template <typename U = T>
    static typename std::enable_if<
      !std::is_arithmetic<U>::value
      && !std::is_same<U, const char*>::value
      && !is_function<U>::value
      && !std::is_same<U, Object>::value
      , BaseProperty::Type
    >::type getType() {
      static_assert(
        std::is_arithmetic<U>::value || std::is_same<U, const char*>::value || is_function<U>::value || std::is_same<U, Object>::value,
        "--- LOOK AT ME ---   The current JS -> C++ interface implementation doesn't support parameters of this type."
      );
      return BaseProperty::Type::UNKNOWN_T;
    }

    Property<T> *makeProperty() {
      return new Property<T>(name);
    }

    Property<T> *makeProperty(T value) {
      return new Property<T>(name, value);
    }

  public:
    PropertyType() = delete;
    PropertyType(const char *name) : BaseProperty{PropertyType<T>::getType(), name} {}
  };
  


  template <typename T>
  struct Property : public PropertyType<T> {
    bool defined;
    T value;

    Property() = delete;
    Property(const char *name) : PropertyType<T>{name}, defined(false) {}
    Property(const char *name, T value) : PropertyType<T>{name}, defined(true), value(value) {}

    // Note that the pointer returned is NOT NECESSARILY pointing to a Property<> with template argument T
    template <typename U = T>
    static typename std::enable_if<std::is_arithmetic<U>::value, Property<U>*>::type get(struct js *engine, const char *name) {
      jsval_t target = js_eval(engine, name, ~0);
      Property<U> *result = nullptr;
      switch (js_type(target)) {
        case JS_UNDEF:
          result = new Property<U>(name);
          break;
        // TBD: How to implement JS_NULL
        case JS_TRUE:
          result = new Property<U>(name, true);
          break;
        case JS_FALSE:
          result = new Property<U>(name, false);
          break;
        case JS_STR: {
          size_t *lenDummy;
          char *strValue = js_getstr(engine, target, lenDummy);
          double numValue = atof(strValue);
          if (numValue == 0 && *strValue != '0')
            throw std::invalid_argument("In fetching numerical parameter from JavaScript: Could not convert string to float");
          result = new Property<U>(name, numValue);
        } break;
        case JS_NUM:
          result = new Property<U>(name, js_getnum(target));
          break;
        case JS_ERR:
          throw std::invalid_argument("In fetching parameter from JavaScript: Fetch from JS returned error type");
          break;
        default:
          throw std::invalid_argument("In fetching numerical parameter from JavaScript: The JS type is not convertible to a number");
          break;
      }
      return result;
    }

    template <typename U = T>
    static typename std::enable_if<std::is_same<U, const char*>::value, Property<U>*>::type get(struct js *engine, const char *name) {
      jsval_t target = js_eval(engine, name, ~0);
      Property<U> *result = nullptr;
      if (js_type(target) == JS_STR) {
        size_t *lenDummy;
        result = new Property<U>(name, js_getstr(engine, target, lenDummy));
      }
      else {
        result = new Property<U>(name, js_str(engine, target));
      }
      return result;
    }
    
    operator T& () {
      if (!defined) {
        Serial.printf("JS -> C++: Attempt to cast `undefined` to type %s (May have been implicit!)\n", typeid(T).name());
        while (true);
      }
      return value;
    }
  };




  struct Object : public BaseProperty {
    std::unordered_map<std::string, BaseProperty*> properties;

    template <typename... Ts>
    Object(const char *name, Property<Ts>*... properties) : Object(name, sizeof...(Ts), properties...) {}
    Object(const char *name, const size_t argc, BaseProperty *argv...) : Object(name, argc, (va_start(_vlist, argv), _vlist)) { va_end(_vlist); }
    Object(const char *name, const size_t argc, va_list args);

  private:
    va_list _vlist;
  };



  struct Constructor {
private:
    Constructor(struct js *js, Prototype *proto);

public:
    struct js *env;
    Prototype *proto;
    friend class Prototype;

    Object *operator() (const size_t argc, void *values...);

    template <typename... Ts>
    Object *operator() (Ts... values) {
      if (sizeof...(Ts) == proto->properties.size()) {
        // NYI
        return this->operator() (sizeof...(values), &values...);
        // std::tuple<Property<Ts>...>
      }
      // We're not gonna worry about this for now...
      // std::tuple<Ts...>{properties...};
    }
  };



  struct Prototype : public BaseProperty {
    std::unordered_map<std::string, BaseProperty*> properties;

    Prototype(const size_t argc, BaseProperty *argv...);

    template <typename... Ts>
    Prototype(PropertyType<Ts>*... properties) : Prototype(sizeof...(Ts), properties...) {}
    //   : BaseProperty(nullptr)
    // {
    //   using dummy = int[];
    //   (void)dummy{(this->properties.insert(std::pair<std::string, BaseProperty*>(properties->name, properties)), 0)...};
    // }

    Constructor& operator[] (struct js *env) {
        Constructor *result = new Constructor(env, this);
        return *result;
    }
  };

};  // namespace JS

#endif