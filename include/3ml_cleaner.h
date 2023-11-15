#ifndef THREEML_CLEANER_H
#define THREEML_CLEANER_H

#include "3ml_parser.h"
#include <string>
#include <vector>

namespace threeml {

enum class NodeType { PLAINTEXT, TITLE, DIV, HEAD, BODY, SCRIPT, H1, A, BUTTON, SLIDER, TEXT_INPUT };

struct DOMNode {
  NodeType type;
  std::string plaintext_data;
  std::vector<Attribute> unique_attributes;
  std::string id;
  std::vector<DOMNode> children;

  DOMNode(NodeType type, std::string plaintext_content, std::vector<DOMNode> children)
      : type(type), plaintext_data(plaintext_content), children(children) {}
  DOMNode(NodeType type, std::vector<Attribute> tag_attributes, std::vector<DOMNode> children);

  void add_child(DOMNode child);
};

struct DOM {
  std::vector<DOMNode> top_level_nodes;

  void add_top_level_node(DOMNode node);
  DOMNode *get_element_by_id(std::string id);
};

DOMNode clean_node(DirtyDOMNode dirty);
DOM clean_dom(DirtyDOM dirty);

} // namespace threeml

#endif