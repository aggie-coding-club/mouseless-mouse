#ifndef I_GIVE_UP
#define I_GIVE_UP

#include <vector>
#include <tuple>
#include <Arduino.h>
#include <elk.h>

template <typename T, typename = void>
struct is_function : std::false_type {};

template <typename FN, typename... Args>
struct is_function<std::function<FN(Args...)>> : std::true_type {};

template <typename T, typename U = void>
struct get_return_type {};

template <typename R, typename... As>
struct get_return_type<std::function<R(As...)>> { typedef R type; };

template <typename T, typename U = void>
struct get_arg_types {};

template <typename R, typename... As>
struct get_arg_types<std::function<R(As...)>> { typedef std::tuple<As...> types; };

template <typename T, typename U = void>
struct function_sig {};

namespace JS {

  struct ObjectIFace {
  private:
    template <typename T, typename U = void>
    struct js_cpp_fn {};

    template <typename R, typename... As>
    struct js_cpp_fn<std::function<R(As...)>> {
      static std::function<R(As...)> *make(const char *name) {
        return new std::function<R(As...)>([engine, name](As... args){
          std::string eval_str(name);
          eval_str += '(';
          auto dummy = {(eval_str += std::string(args), eval_str += ',')...};
          eval_str.pop_back();
          eval_str += ");";
          jsval_t result = js_eval(engine, eval_str.c_str(), ~0);
          return get<R>(result);
        });
      }

      template <size_t ID>
      static struct cpp_fn {
        static std::function<R(As...)> *fn_ptr;

        cpp_fn(std::function<R(As...)> *fn_ptr) {
          ObjectIFace::js_cpp_fn<R, As...>::cpp_fn<ID>::fn_ptr = fn_ptr;
        }

        operator () (struct js *js, jsval_t *args, int nargs) {
          // TODO
        }
      }
    };

    template <typename T>
    typename std::enable_if<std::is_arithmetic<T>::value, T*>::type get_impl(jsval_t result) {
      T *conversion = nullptr;
      switch (js_type(result)) {
        case JS_ERR: {
          Serial.println("Attempt to fetch JS value resulted in error");
          while (true);
        } break;
        case JS_FALSE: {
          conversion = new T(false);
        } break;
        case JS_TRUE: {
          conversion = new T(true);
        } break;
        case JS_UNDEF:
        case JS_NULL: {
          conversion = nullptr;
        } break;
        case JS_STR: {
          size_t *lenDummy;
          conversion = new T(atof(js_getstr(engine, result, lenDummy)));
        } break;
        case JS_NUM: {
          conversion = new T(js_getnum(result));
        } break;
        default: {
          Serial.println("JS -> C++: Object type is not convertible to integral type");
        } break;
      }
      return conversion;
    }

    template <typename T>
    typename std::enable_if<std::is_same<T, const char*>::value, T>::type get_impl(jsval_t result) {
      T conversion = nullptr;
      if (js_type(result) == JS_ERR) {
        Serial.println("Attempt to fetch JS value resulted in error");
        while (true);
      }
      else if (js_type(result) == JS_STR) {
        size_t *lenDummy;
        conversion = js_getstr(engine, result, lenDummy);
      }
      else {
        conversion = js_str(engine, result);
      }
      return conversion;
    }

  public:
    struct js *engine;
    const char *target;
    std::vector<const char*> properties;
    
    static bool exists(struct js *engine, const char *target);
    bool bind(struct js *engine, const char *target);

    template <typename T>
    typename std::enable_if<!is_function<T>::value, T*>::type get(const char *name) {
      jsval_t result = js_eval(engine, name, ~0);
      return get_impl<T>(result);
    }

    template <typename F>
    typename std::enable_if<is_function<F>::value, F*>::type get(const char *name) {
      return js_cpp_fn<F>::make(name);
    }

    template <typename T>
    typename std::enable_if<std::is_arithmetic<T>::value && !std::is_same<T, bool>::value, void>::type
    set(const char *name, T value) {
      jsval_t self = js_eval(engine, target, ~0);
      jsval_t result = js_mknum(value);
      js_set(engine, self, name, result);
    }

    template <typename T>
    typename std::enable_if<std::is_same<T, const char*>::value, void>::type
    set(const char *name, T value) {
      jsval_t self = js_eval(engine, target, ~0);
      jsval_t result = js_mkstr(engine, value, ~0);
      js_set(engine, self, name, result);
    }

    template <typename T>
    typename std::enable_if<std::is_same<T, bool>::value, void>::type
    set(const char *name, T value) {
      jsval_t self = js_eval(engine, target, ~0);
      jsval_t result = js_mkval(value ? JS_TRUE : JS_FALSE);
      js_set(engine, self, name, result);
    }
  };

};

#endif