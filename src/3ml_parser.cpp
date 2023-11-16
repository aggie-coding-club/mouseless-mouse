#include "3ml_parser.h"
#include "3ml_error.h"
#include <cstring>
#include <string>
#include <vector>

namespace threeml {

void DirtyDOMTagNode::add_child(DirtyDOMNode child) { children.push_back(child); }

void DirtyDOM::add_top_level_node(DirtyDOMNode node) { top_level_nodes.push_back(node); }

Attribute parse_attribute(const char *&cursor) {
  std::string name;
  std::string value;

  const char *name_begin = cursor;
  while (std::isalpha(*cursor)) {
    cursor++;
  }
  name = std::string(name_begin, cursor - name_begin);
  maybe_error(name.empty(), "invalid attribute name");

  bool has_seen_equals_sign = false;
  while (std::isspace(*cursor) || *cursor == '=') {
    maybe_error(has_seen_equals_sign && *cursor == '=', "only one equals sign may be in an attribute");
    maybe_error(*cursor == '\n', "attributes must be on one line");
    if (*cursor == '=') {
      has_seen_equals_sign = true;
    }
    cursor++;
  }
  maybe_error(*cursor != '"', "expected string");
  cursor++;
  const char *value_begin = cursor;
  while (*cursor != '"' && *cursor != '\0') {
    maybe_error(!std::isprint(*cursor) && !std::isspace(*cursor), "unprintable character in string");
    maybe_error(*cursor == '\n', "expected closing quote, found newline");
    cursor++;
  }
  maybe_error(*cursor == '\0', "expected closing quote, found EOF");
  value = remove_escape_codes(std::string(value_begin, cursor - value_begin));
  ++cursor;

  return Attribute(name, value);
}

Tag parse_tag(const char *&cursor) {
  std::string name;
  std::vector<Attribute> attributes;
  bool is_closing = false;
  bool is_self_closing = false;

  cursor++; // always starts on a '<', so we can skip it
  if (*cursor == '/') {
    is_closing = true;
    cursor++;
  }
  while (std::isspace(*cursor)) {
    maybe_error(*cursor == '\n', "did not expect a newline");
    cursor++;
  }

  const char *name_begin = cursor;
  while (std::isalnum(*cursor)) {
    cursor++;
  }
  name = std::string(name_begin, cursor - name_begin);

  while (std::isspace(*cursor)) {
    maybe_error(*cursor == '\n', "did not expect a newline");
    cursor++;
  }
  while (*cursor != '/' && *cursor != '>' && *cursor != '\0') {
    attributes.push_back(parse_attribute(cursor));
    while (std::isspace(*cursor)) {
      maybe_error(*cursor == '\n', "did not expect a newline");
      cursor++;
    }
  }
  maybe_error(*cursor == '\0', "unclosed tag");

  if (*cursor == '/') {
    is_self_closing = true;
    cursor++;
  }
  cursor++;

  maybe_error(is_closing && is_self_closing, "tag cannot be both closing and self-closing");
  maybe_error(is_closing && !attributes.empty(), "closing tags cannot have attributes");

  return Tag(name, attributes, is_closing, is_self_closing);
}

std::string remove_escape_codes(std::string escaped) {
  std::string result;
  bool in_escape_code = false;
  bool expect_hashtag = false;
  unsigned int current_escape_num = 0;
  for (auto c : escaped) {
    if (!in_escape_code && c == '&') {
      in_escape_code = true;
      expect_hashtag = true;
    } else if (in_escape_code && c == '#') {
      maybe_error(!expect_hashtag, "invalid escape code");
      expect_hashtag = false;
    } else if (in_escape_code && '0' <= c && c <= '9') {
      maybe_error(expect_hashtag, "invalid escape code");
      current_escape_num *= 10;
      current_escape_num += c - '0';
      maybe_error(current_escape_num >= 256, "escape code not valid ASCII");
    } else if (in_escape_code && c == ';') {
      maybe_error(!std::isprint(current_escape_num) && !std::isspace(current_escape_num), "unprintable escape code");
      maybe_error(expect_hashtag, "empty escape code");
      in_escape_code = false;
      result.push_back(static_cast<char>(current_escape_num));
    } else if (in_escape_code) {
      maybe_error(true, "invalid character in escape code");
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
        maybe_error(!std::isprint(*cursor) && !isspace(*cursor), "unprintable character in plaintext");
        cursor++;
      }
      std::string processed = trim(remove_escape_codes(std::string(plaintext_begin, cursor - plaintext_begin)));
      parse_chain.push_back(ParseNode(Plaintext(processed)));
      is_normal_mode = false;
    } else {
      parse_chain.push_back(ParseNode(parse_tag(cursor)));
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
      maybe_error(current.tag_name != node.tag.name, "closing tags must match the opening tag");
      tag_stack.pop_back();
      if (tag_stack.empty()) {
        top_level_nodes.push_back(DirtyDOMNode(current));
      } else {
        tag_stack.back().add_child(DirtyDOMNode(current));
      }
    } else if (!node.is_tag) {
      DirtyDOMPlaintextNode current = DirtyDOMPlaintextNode(node.plaintext);
      if (tag_stack.empty()) {
        top_level_nodes.push_back(DirtyDOMNode(current));
      } else {
        tag_stack.back().add_child(DirtyDOMNode(current));
      }
    }
  }

  return DirtyDOM(top_level_nodes);
}

} // namespace threeml