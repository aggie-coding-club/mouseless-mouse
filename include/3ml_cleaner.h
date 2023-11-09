#ifndef THREEML_CLEANER_H
#define THREEML_CLEANER_H

#include "3ml_parser.h"
#include <string>
#include <vector>

namespace threeml {

struct DOMNode;

enum class NodeType { PLAINTEXT, TITLE, DIV, HEAD, BODY, SCRIPT, H1, A, BUTTON, SLIDER, TEXT_INPUT };

using PlaintextData = std::string;
struct ButtonData {
  std::string label;
  std::string onclick_callback;

  ButtonData(const std::vector<Attribute> &tag_attributes, const std::vector<DOMNode> &tag_children);
};
struct LinkData {
  std::string label;
  std::string points_to;

  LinkData(const std::vector<Attribute> &tag_attributes, const std::vector<DOMNode> &tag_children);
};
struct TextInputData {
  std::string contents;
  std::string oninput_callback;

  TextInputData(const std::vector<Attribute> &tag_attributes, const std::vector<DOMNode> &tag_children);
};
struct SliderData {
  std::size_t min;
  std::size_t max;
  std::size_t value;
  std::string oninput_callback;

  SliderData(const std::vector<Attribute> &tag_attributes, const std::vector<DOMNode> &tag_children);
};
struct BodyData {
  std::string onload_callback;
  std::string onbeforeunload_callback;

  BodyData(const std::vector<Attribute> &tag_attributes, const std::vector<DOMNode> &tag_children);
};
struct ScriptData {
  std::string script_filename;

  ScriptData(const std::vector<Attribute> &tag_attributes, const std::vector<DOMNode> &tag_children);
};

struct DOMNode {
  NodeType type;
  union {
    PlaintextData plaintext_data;
    ButtonData button_data;
    LinkData link_data;
    TextInputData text_input_data;
    SliderData slider_data;
    BodyData body_data;
    ScriptData script_data;
  };
  std::string id;
  std::vector<DOMNode> children;

  DOMNode(NodeType type, std::string plaintext_content, std::vector<DOMNode> children)
      : type(type), plaintext_data(plaintext_content), children(children) {}
  DOMNode(NodeType type, std::vector<Attribute> tag_attributes, std::vector<DOMNode> children);
  ~DOMNode();
  DOMNode(const DOMNode &original);

  void add_child(DOMNode child);
};

struct DOM {
  std::vector<DOMNode> top_level_nodes;

  void add_top_level_node(DOMNode node);
};

DOMNode clean_node(DirtyDOMNode dirty);
DOM clean_dom(DirtyDOM dirty);

} // namespace threeml

#endif