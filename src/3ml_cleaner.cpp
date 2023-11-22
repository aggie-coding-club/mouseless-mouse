#include "3ml_cleaner.h"
#include "3ml_error.h"
#include "display.h"
#include <cassert>
#include <functional>
#include <string>
#include <unordered_map>

extern Display display;

namespace threeml {

void verify_slider_attributes(const std::unordered_map<std::string, std::string> &tag_attributes) {
  bool min_encountered = false;
  bool max_encountered = false;
  bool oninput_encountered = false;
  unsigned long long min = 0;
  unsigned long long max = 0;
  for (const auto &attribute : tag_attributes) {
    if (attribute.first == "min") {
      maybe_error(min_encountered, "duplicate min attribute on <input>");
      try {
        min = std::stoull(attribute.second);
      } catch (std::exception &_) {
        maybe_error(true, "invalid min value on <input>");
      }
      min_encountered = true;
    } else if (attribute.first == "max") {
      maybe_error(max_encountered, "duplicate max attribute on <input>");
      try {
        max = std::stoull(attribute.second);
      } catch (std::exception &_) {
        maybe_error(true, "invalid max value on <input>");
      }
      max_encountered = true;
    } else if (attribute.first == "oninput") {
      maybe_error(oninput_encountered, "duplicate oninput attribute on <input>");
      oninput_encountered = true;
    }
  }
  maybe_error(!min_encountered || !max_encountered, "slider inputs need both a min and a max");
  maybe_error(min >= max, "slider input min must be less than max");
}

void verify_body_attributes(const std::unordered_map<std::string, std::string> &tag_attributes) {
  bool onload_encountered = false;
  bool onbeforeunload_encountered = false;
  for (const auto &attribute : tag_attributes) {
    if (attribute.first == "onload") {
      maybe_error(onload_encountered, "duplicate onload attribute on <body>");
      onload_encountered = true;
    }
    else if (attribute.first == "onbeforeunload") {
      maybe_error(onbeforeunload_encountered, "duplicate onbeforeunload attribute on <body>");
      onbeforeunload_encountered = true;
    } 
    // else {
    //   maybe_error(true, "invalid attribute on <body>");
    // }
  }
}

DOMNode::DOMNode(NodeType type, std::vector<Attribute> tag_attributes, std::vector<DOMNode*> children, DOMNode* parent)
    : type(type), height(0), children(children), parent(parent), selectable(false), num_selectable_children(0) {
  std::vector<Attribute> filtered_attributes;
  bool id_encountered = false;
  for (const auto &attribute : tag_attributes) {
    if (attribute.first == "id") {
      maybe_error(id_encountered, "duplicate id");
      id = attribute.second;
      id_encountered = true;
    } else {
      filtered_attributes.push_back(attribute);
    }
  }
  for (Attribute attr : filtered_attributes)
    unique_attributes.insert(attr);
  switch (type) {
  case NodeType::A:
    maybe_error(unique_attributes.size() < 1, "no attribute(s) on <a>");
    maybe_error(unique_attributes.find("href") == unique_attributes.end(), "no `href` attribute on <a>");
    selectable = true;
    break;
  case NodeType::BODY:
    verify_body_attributes(unique_attributes);
    break;
  case NodeType::BUTTON:
    maybe_error(unique_attributes.size() < 1, "no attribute(s) on <button>");
    maybe_error(unique_attributes.find("onclick") == unique_attributes.end(), "no `onclick` attribute on <button>");
    selectable = true;
    break;
  case NodeType::SCRIPT:
    maybe_error(unique_attributes.size() < 1, "no attribute(s) on <script>");
    maybe_error(unique_attributes.find("src") == unique_attributes.end(), "no `src` attribute on <script>");
    break;
  case NodeType::SLIDER:
    verify_slider_attributes(unique_attributes);
    selectable = true;
    break;
  case NodeType::TEXT_INPUT:
    // maybe_error(unique_attributes.size() > 1, "invalid attribute(s) on <input>");
    maybe_error(!unique_attributes.empty() && unique_attributes.find("oninput") == unique_attributes.end(), "invalid attribute on <input>");
    selectable = true;
    break;
  default:
    // maybe_error(!filtered_attributes.empty(), "invalid attribute");
    break;
  }

  for (DOMNode* child : children) {
    num_selectable_children += child->num_selectable_children;
    if (child->selectable) num_selectable_children += 1;
    height += child->height;
  }
}

DOMNode::DOMNode(NodeType type, std::string plaintext_content, std::vector<DOMNode*> children, DOMNode* parent)
  : type(type), height(0), children(children), parent(parent), selectable(false), num_selectable_children(0)
{
  display.textFormat(parent->type == NodeType::H1 ? 3 : 2, TFT_WHITE);
  size_t len = plaintext_content.length();
  size_t start = 0;
  while (plaintext_content.substr(start).length()) {
    height += display.buffer->textsize * 10;
    while (len > 0 && display.buffer->textWidth(plaintext_content.substr(start, len).c_str()) > display.buffer->width()) --len;
    if (len == plaintext_content.substr(start).length()) {
      plaintext_data.push_back(plaintext_content.substr(start));
      break;
    }
    while (len > 0 && !isspace(plaintext_content.at(start + len - 1))) --len;
    maybe_error(len == 0, ("Text wrapping failed: " + plaintext_content).c_str());
    plaintext_data.push_back(plaintext_content.substr(start, len));
    start += len;
    len = plaintext_content.substr(start).length();
  }

  for (DOMNode* child : children) {
    num_selectable_children += child->num_selectable_children;
    if (child->selectable) num_selectable_children += 1;
    height += child->height;
  }
}

DOMNode::~DOMNode() {
  for (DOMNode* child : children)
    delete child;
}

void DOMNode::add_child(DOMNode* child) {
  if (child->type == NodeType::PLAINTEXT && child->plaintext_data.empty()) {
    delete child;
    return;
  }
  maybe_error(type == NodeType::PLAINTEXT, "plaintext nodes cannot have children");
  maybe_error(type == NodeType::SCRIPT, "script nodes cannot have children");
  if (type == NodeType::TITLE || type == NodeType::H1 || type == NodeType::A || type == NodeType::BUTTON) {
    maybe_error(!children.empty() || child->type != NodeType::PLAINTEXT,
                "title, h1, a, and button nodes can only have one child and it must be a plaintext node");
  } else if (type == NodeType::HEAD) {
    maybe_error(child->type != NodeType::SCRIPT && child->type != NodeType::TITLE,
                "only title and script nodes can be children of a head node");
  } else if (type == NodeType::BODY) {
    maybe_error(child->type == NodeType::SCRIPT || child->type == NodeType::TITLE,
                "title and script nodes cannot be children of a body node");
  }
  children.push_back(child);
  num_selectable_children += child->num_selectable_children;
  if (child->selectable) num_selectable_children += 1;
  height += child->height;
}

void DOM::add_top_level_node(DOMNode* node) {
  if (node->type == NodeType::PLAINTEXT && node->plaintext_data.empty()) {
    return;
  }
  maybe_error(node->type != NodeType::HEAD && node->type != NodeType::BODY,
              "top-level DOM nodes must be either head or body nodes");
  top_level_nodes.push_back(node);
  num_selectable_nodes += node->num_selectable_children;
  height += node->height;
}

DOMNode *DOM::get_element_by_id(std::string id) {
  // god bless modern c++
  std::function<DOMNode *(DOMNode *)> search_recursive = [id, &search_recursive](DOMNode *node) {
    if (node->id == id) {
      return node;
    }
    for (auto &child : node->children) {
      auto result = search_recursive(child);
      if (result != nullptr) {
        return result;
      }
    }
    return static_cast<DOMNode *>(nullptr);
  };
  for (auto &top_level : top_level_nodes) {
    auto result = search_recursive(top_level);
    if (result != nullptr) {
      return result;
    }
  }
  return nullptr;
}

DOMNode* clean_node(DirtyDOMNode dirty, DOMNode* parent) {
  if (dirty.is_plaintext) {
    return new DOMNode(NodeType::PLAINTEXT, dirty.plaintext_node, std::vector<DOMNode *>(), parent);
  }
  NodeType type = NodeType::A; // placeholder to suppress a meaningless warning
  if (dirty.tag_node.tag_name == "title") {
    type = NodeType::TITLE;
  } else if (dirty.tag_node.tag_name == "div") {
    type = NodeType::DIV;
  } else if (dirty.tag_node.tag_name == "head") {
    type = NodeType::HEAD;
  } else if (dirty.tag_node.tag_name == "body") {
    type = NodeType::BODY;
  } else if (dirty.tag_node.tag_name == "script") {
    type = NodeType::SCRIPT;
  } else if (dirty.tag_node.tag_name == "h1") {
    type = NodeType::H1;
  } else if (dirty.tag_node.tag_name == "a") {
    type = NodeType::A;
  } else if (dirty.tag_node.tag_name == "button") {
    type = NodeType::BUTTON;
  } else if (dirty.tag_node.tag_name == "input") {
    for (const auto &attribute : dirty.tag_node.attributes) {
      if (attribute.first == "type") {
        if (attribute.second == "text") {
          type = NodeType::TEXT_INPUT;
        } else if (attribute.second == "range") {
          type = NodeType::SLIDER;
        } else {
          maybe_error(true, "invalid input tag type");
        }
      }
    }
  } else {
    maybe_error(true, "invalid tag name");
  }
  DOMNode *result = new DOMNode(type, dirty.tag_node.attributes, std::vector<DOMNode *>(), parent);
  for (auto child : dirty.tag_node.children) {
    result->add_child(clean_node(child, result));
  }
  return result;
}

DOM* clean_dom(DirtyDOM dirty) {
  DOM* result = new DOM();
  for (auto top_level : dirty.top_level_nodes) {
    if (!(top_level.is_plaintext && top_level.plaintext_node.empty())) {
      result->add_top_level_node(clean_node(top_level, nullptr));
    }
  }
  return result;
}

DOM::DOM()
  : num_selectable_nodes(0)
  , height(0)
{}

DOM::~DOM() {
  for (DOMNode *child : top_level_nodes)
    delete child;
}

} // namespace threeml