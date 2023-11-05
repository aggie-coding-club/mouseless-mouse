#ifndef THREEML_CLEANER_H
#define THREEML_CLEANER_H

#include "3ml_parser.h"
#include <string>
#include <vector>

namespace threeml {

enum class NodeType { PLAINTEXT, TITLE, DIV, HEAD, BODY, SCRIPT, H1, A, BUTTON, INPUT_NODE };

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