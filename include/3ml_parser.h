#pragma once

#include <string>
#include <vector>

struct Attribute {
  std::string name;
  std::string value;

  Attribute(std::string name, std::string value) : name(name), value(value) {}
};

struct DirtyDOMPlaintextNode {
  std::string contents;

  DirtyDOMPlaintextNode() {}

  DirtyDOMPlaintextNode(std::string contents) : contents(contents) {}
};

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

  DirtyDOMNode(DirtyDOMPlaintextNode plaintext_node) : plaintext_node(plaintext_node), is_plaintext(true) {}
  DirtyDOMNode(DirtyDOMTagNode tag_node) : tag_node(tag_node), is_plaintext(false) {}
};

struct DirtyDOM {
  std::vector<DirtyDOMNode> top_level_nodes;

  DirtyDOM(std::vector<DirtyDOMNode> top_level_nodes) : top_level_nodes(top_level_nodes) {}

  void add_top_level_node(DirtyDOMNode node);
};

struct Plaintext {
  std::string contents;

  Plaintext() {}

  Plaintext(std::string contents) : contents(contents) {}
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

Attribute parse_attribute(char **cursor);
Tag parse_tag(char **cursor);
std::string remove_escape_codes(std::string escaped);
std::string trim(std::string untrimmed);
DirtyDOM parse_string(const char *str);