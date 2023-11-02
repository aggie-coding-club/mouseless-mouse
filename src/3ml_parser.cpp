#include "3ml_parser.h"
#include <Arduino.h>
#include <cstring>
#include <string>
#include <vector>

void DirtyDOMTagNode::add_child(DirtyDOMNode child) { children.push_back(child); }

void DirtyDOM::add_top_level_node(DirtyDOMNode node) { top_level_nodes.push_back(node); }

Attribute parse_attribute(const char **cursor) {
  std::string name;
  std::string value;

  const char *name_begin = *cursor;
  while (std::isalpha(**cursor)) {
    (*cursor)++;
  }
  name = std::string(name_begin, *cursor - name_begin);

  while (std::isspace(**cursor) || **cursor == '=') {
    (*cursor)++;
  }
  (*cursor)++;
  const char *value_begin = *cursor;
  while (**cursor != '"') {
    (*cursor)++;
  }
  value = remove_escape_codes(std::string(value_begin, *cursor - value_begin));

  return Attribute(name, value);
}

Tag parse_tag(const char **cursor) {
  std::string name;
  std::vector<Attribute> attributes;
  bool is_closing = false;
  bool is_self_closing = false;

  (*cursor)++;
  if (**cursor == '/') {
    is_closing = true;
    (*cursor)++;
  }
  while (std::isspace(**cursor)) {
    (*cursor)++;
  }

  const char *name_begin = *cursor;
  while (std::isalnum(**cursor)) {
    (*cursor)++;
  }
  name = std::string(name_begin, *cursor - name_begin);

  while (std::isalpha(**cursor)) {
    (*cursor)++;
  }
  while (**cursor != '/' && **cursor != '>') {
    attributes.push_back(parse_attribute(cursor));
    while (std::isspace(**cursor)) {
      (*cursor)++;
    }
  }

  if (**cursor == '/') {
    is_self_closing = true;
    (*cursor)++;
  }
  (*cursor)++;

  return Tag(name, attributes, is_closing, is_self_closing);
}

std::string remove_escape_codes(std::string escaped) {
  std::string result;
  bool in_escape_code = false;
  unsigned int current_escape_num = 0;
  for (auto c : escaped) {
    if (!in_escape_code && c == '&') {
      in_escape_code = true;
    } else if (in_escape_code && '0' <= c && c <= '9') {
      current_escape_num *= 10;
      current_escape_num += c - '0';
    } else if (in_escape_code && c == ';') {
      in_escape_code = false;
      result.push_back(static_cast<char>(current_escape_num));
    } else if (!in_escape_code) {
      result.push_back(c);
    }
  }
  return result;
}

std::string trim(std::string untrimmed) {
  std::size_t cursor = 0;
  while (cursor < untrimmed.size() && std::isspace(untrimmed[cursor])) {
    cursor++;
  }
  std::string start_trimmed = untrimmed.substr(cursor);
  std::string start_trimmed_reversed = std::string(start_trimmed.rbegin(), start_trimmed.rend());
  cursor = 0;
  while (cursor < start_trimmed_reversed.size() && std::isspace(start_trimmed_reversed[cursor])) {
    cursor++;
  }
  std::string trimmed_reversed = start_trimmed_reversed.substr(cursor);
  return std::string(trimmed_reversed.rbegin(), trimmed_reversed.rend());
}

DirtyDOM parse_string(const char *str) {
  std::vector<ParseNode> parse_chain;
  bool is_normal_mode = true;
  const char *cursor = str;
  while (*cursor != '\0') {
    if (is_normal_mode) {
      const char *plaintext_begin = cursor;
      while (*cursor != '<' && *cursor != '\0') {
        cursor++;
      }
      parse_chain.push_back(
          ParseNode(Plaintext(trim(remove_escape_codes(std::string(plaintext_begin, cursor - plaintext_begin))))));
      is_normal_mode = false;
    } else {
      parse_chain.push_back(ParseNode(parse_tag(&cursor)));
      is_normal_mode = true;
    }
  }

  std::vector<DirtyDOMNode> top_level_nodes;
  std::vector<DirtyDOMTagNode> tag_stack;
  for (auto node : parse_chain) {
    if (node.is_tag && node.tag.is_self_closing) {
      auto dom_node = DirtyDOMNode(DirtyDOMTagNode(node.tag.name, node.tag.attributes, std::vector<DirtyDOMNode>()));
      if (tag_stack.empty()) {
        top_level_nodes.push_back(dom_node);
      } else {
        tag_stack.back().add_child(dom_node);
      }
    } else if (node.is_tag && !node.tag.is_closing) {
      auto tag_node = DirtyDOMTagNode(node.tag.name, node.tag.attributes, std::vector<DirtyDOMNode>());
      tag_stack.push_back(tag_node);
    } else if (node.is_tag && node.tag.is_closing) {
      DirtyDOMTagNode current = tag_stack.back();
      tag_stack.pop_back();
      if (tag_stack.empty()) {
        top_level_nodes.push_back(DirtyDOMNode(current));
      } else {
        tag_stack.back().add_child(DirtyDOMNode(current));
      }
    } else if (!node.is_tag) {
      DirtyDOMPlaintextNode current = DirtyDOMPlaintextNode(node.plaintext.contents);
      if (tag_stack.empty()) {
        top_level_nodes.push_back(DirtyDOMNode(current));
      } else {
        tag_stack.back().add_child(DirtyDOMNode(current));
      }
    }
  }

  return DirtyDOM(top_level_nodes);
}
