#ifndef THREEML_CLEANER_H
#define THREEML_CLEANER_H

#include "3ml_parser.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace threeml {

enum class NodeType { PLAINTEXT, TITLE, DIV, HEAD, BODY, SCRIPT, H1, A, BUTTON, SLIDER, TEXT_INPUT, ROOT };

struct DOMNode {
  NodeType type;
  std::vector<std::string> plaintext_data;
  uint16_t height;
  std::unordered_map<std::string, std::string> unique_attributes;
  std::string id;
  std::vector<DOMNode*> children;
  DOMNode* parent;
  bool selectable;
  size_t num_selectable_children;

  DOMNode(NodeType type, std::string plaintext_content, std::vector<DOMNode*> children, DOMNode* parent);
  DOMNode(NodeType type, std::vector<Attribute> tag_attributes, std::vector<DOMNode*> children, DOMNode* parent);
  ~DOMNode();

  void add_child(DOMNode* child);
};

struct DOM {
  std::vector<DOMNode*> top_level_nodes;
  size_t num_selectable_nodes;
  uint16_t height;

  void add_top_level_node(DOMNode* node);
  DOMNode *get_element_by_id(std::string id);

  DOM();
  ~DOM();
};

DOMNode* clean_node(DirtyDOMNode dirty, DOMNode* parent);
DOM* clean_dom(DirtyDOM dirty);

} // namespace threeml

#endif