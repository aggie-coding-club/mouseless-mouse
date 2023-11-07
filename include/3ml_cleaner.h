#ifndef THREEML_CLEANER_H
#define THREEML_CLEANER_H

#include "3ml_parser.h"
#include <string>
#include <vector>

namespace threeml {

enum class NodeType { PLAINTEXT, TITLE, DIV, HEAD, BODY, SCRIPT, H1, A, BUTTON, INPUT_NODE };

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

struct DOMNode {
  NodeType type;
  std::string plaintext_content;
  std::vector<Attribute> tag_attributes;
  std::vector<DOMNode> children;

  DOMNode(NodeType type, std::string plaintext_content, std::vector<DOMNode> children)
      : type(type), plaintext_content(plaintext_content), children(children) {}
  DOMNode(NodeType type, std::vector<Attribute> tag_attributes, std::vector<DOMNode> children)
      : type(type), tag_attributes(tag_attributes), children(children) {}

  void add_child(DOMNode child);
};

struct DOM {
  std::vector<DOMNode> top_level_nodes;

  void add_top_level_node(DOMNode node);
};

DOMNode clean_node(DirtyDOMNode dirty);
DOM clean_dom(DirtyDOM dirty);

}

#endif