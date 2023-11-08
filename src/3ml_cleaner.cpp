#include "3ml_cleaner.h"
#include "3ml_error.h"
#include <cassert>

namespace threeml {

DOMNode::DOMNode(NodeType type, std::vector<Attribute> tag_attributes, std::vector<DOMNode> children)
    : type(type), children(children) {
  std::vector<Attribute> filtered_attributes;
  bool id_encountered = false;
  for (const auto &attribute : tag_attributes) {
    if (attribute.name == "id") {
      maybe_error(id_encountered, "duplicate id");
      id = attribute.value;
      id_encountered = true;
    } else {
      filtered_attributes.push_back(attribute);
    }
  }
  switch (type) {
  case NodeType::A:
    link_data = LinkData(filtered_attributes, children);
    break;
  case NodeType::BODY:
    body_data = BodyData(filtered_attributes, children);
    break;
  case NodeType::BUTTON:
    button_data = ButtonData(filtered_attributes, children);
    break;
  case NodeType::SCRIPT:
    script_data = ScriptData(filtered_attributes, children);
    break;
  case NodeType::SLIDER:
    slider_data = SliderData(filtered_attributes, children);
    break;
  case NodeType::TEXT_INPUT:
    text_input_data = TextInputData(filtered_attributes, children);
    break;
  default:
    maybe_error(!filtered_attributes.empty(), "invalid attribute");
  }
}

DOMNode::~DOMNode() {
  switch (type) {
  case NodeType::A:
    link_data.~LinkData();
    break;
  case NodeType::BUTTON:
    button_data.~ButtonData();
    break;
  case NodeType::TEXT_INPUT:
    text_input_data.~TextInputData();
    break;
  case NodeType::PLAINTEXT:
    plaintext_data.~PlaintextData();
    break;
  case NodeType::SLIDER:
    slider_data.~SliderData();
    break;
  }
}

DOMNode::DOMNode(const DOMNode &original) {
  type = original.type;
  id = original.id;
  switch (type) {
  case NodeType::A:
    link_data = original.link_data;
    break;
  case NodeType::BUTTON:
    button_data = original.button_data;
    break;
  case NodeType::TEXT_INPUT:
    text_input_data = original.text_input_data;
    break;
  case NodeType::PLAINTEXT:
    plaintext_data = original.plaintext_data;
    break;
  case NodeType::SLIDER:
    slider_data = original.slider_data;
    break;
  }
}

void DOMNode::add_child(DOMNode child) {
  if (child.type == NodeType::PLAINTEXT && child.plaintext_data.empty()) {
    return;
  }
  maybe_error(type == NodeType::PLAINTEXT, "plaintext nodes cannot have children");
  maybe_error(type == NodeType::SCRIPT, "script nodes cannot have children");
  if (type == NodeType::TITLE || type == NodeType::H1 || type == NodeType::A || type == NodeType::BUTTON) {
    maybe_error(!children.empty() || child.type != NodeType::PLAINTEXT,
                "title, h1, a, and button nodes can only have one child and it must be a plaintext node");
  } else if (type == NodeType::HEAD) {
    maybe_error(child.type != NodeType::SCRIPT && child.type != NodeType::TITLE,
                "only title and script nodes can be children of a head node");
  } else if (type == NodeType::BODY) {
    maybe_error(child.type == NodeType::SCRIPT || child.type == NodeType::TITLE,
                "title and script nodes cannot be children of a body node");
  }
  children.push_back(child);
}

void DOM::add_top_level_node(DOMNode node) {
  if (node.type == NodeType::PLAINTEXT && node.plaintext_data.empty()) {
    return;
  }
  maybe_error(node.type != NodeType::HEAD && node.type != NodeType::BODY,
              "top-level DOM nodes must be either head or body nodes");
  top_level_nodes.push_back(node);
}

DOMNode clean_node(DirtyDOMNode dirty) {
  if (dirty.is_plaintext) {
    return DOMNode(NodeType::PLAINTEXT, dirty.plaintext_node, std::vector<DOMNode>());
  }
  NodeType type;
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
      if (attribute.name == "type") {
        if (attribute.value == "text") {
          type = NodeType::TEXT_INPUT;
        } else if (attribute.value == "range") {
          type = NodeType::SLIDER;
        } else {
          maybe_error(true, "invalid input tag type");
        }
      }
    }
  } else {
    maybe_error(true, "invalid tag name");
  }
  auto result = DOMNode(type, dirty.tag_node.attributes, std::vector<DOMNode>());
  for (auto child : dirty.tag_node.children) {
    result.add_child(clean_node(child));
  }
  return result;
}

DOM clean_dom(DirtyDOM dirty) {
  DOM result;
  for (auto top_level : dirty.top_level_nodes) {
    if (!(top_level.is_plaintext && top_level.plaintext_node.empty())) {
      result.add_top_level_node(clean_node(top_level));
    }
  }
  return result;
}

} // namespace threeml