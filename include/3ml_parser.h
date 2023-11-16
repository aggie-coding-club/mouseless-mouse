#ifndef THREEML_PARSER_H
#define THREEML_PARSER_H

#include <string>
#include <vector>
#include <unordered_map>

namespace threeml {

using DirtyDOMPlaintextNode = std::string;
using Plaintext = std::string;

// struct Attribute {
//   std::string name;
//   std::string value;

//   Attribute(std::string name, std::string value) : name(name), value(value) {}
// };

using Attribute = std::pair<std::string, std::string>;

struct DirtyDOMNode;

struct DirtyDOMTagNode {
  std::string tag_name;
  std::vector<Attribute> attributes;
  std::vector<DirtyDOMNode> children;

  DirtyDOMTagNode() {}

  DirtyDOMTagNode(std::string tag_name, std::vector<Attribute> attributes, std::vector<DirtyDOMNode> children)
      : tag_name(tag_name), attributes(attributes), children(children) {}

  void add_child(DirtyDOMNode child);
};

struct DirtyDOMNode {
  bool is_plaintext;
  DirtyDOMPlaintextNode plaintext_node;
  DirtyDOMTagNode tag_node;

  DirtyDOMNode() {}

  DirtyDOMNode(DirtyDOMPlaintextNode plaintext_node) : is_plaintext(true), plaintext_node(plaintext_node) {}
  DirtyDOMNode(DirtyDOMTagNode tag_node) : is_plaintext(false), tag_node(tag_node) {}
};

struct DirtyDOM {
  std::vector<DirtyDOMNode> top_level_nodes;

  DirtyDOM(std::vector<DirtyDOMNode> top_level_nodes) : top_level_nodes(top_level_nodes) {}

  void add_top_level_node(DirtyDOMNode node);
};

struct Tag {
  std::string name;
  std::vector<Attribute> attributes;
  bool is_closing;
  bool is_self_closing;

  Tag() {}

  Tag(std::string name, std::vector<Attribute> attributes, bool is_closing, bool is_self_closing)
      : name(name), attributes(attributes), is_closing(is_closing), is_self_closing(is_self_closing) {}
};

struct ParseNode {
  Tag tag;
  Plaintext plaintext;
  bool is_tag;

  ParseNode(Tag tag) : tag(tag), is_tag(true) {}
  ParseNode(Plaintext plaintext) : plaintext(plaintext), is_tag(false) {}
};

Attribute parse_attribute(char *&cursor);
Tag parse_tag(char *&cursor);
std::string remove_escape_codes(std::string escaped);
std::string trim(std::string untrimmed);
DirtyDOM parse_string(const char *str);

} // namespace threeml

#endif