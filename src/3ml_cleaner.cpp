#include "3ml_cleaner.h"
#include "3ml_error.h"
#include <cassert>

namespace threeml {

void DOMNode::add_child(DOMNode child) {
  if (child.type == NodeType::PLAINTEXT && child.plaintext_content.empty()) {
    return;
  }
  maybe_error(type == NodeType::PLAINTEXT, "plaintext nodes cannot have children");
  maybe_error(type == NodeType::SCRIPT, "script nodes cannot have children");
  if (type == NodeType::TITLE || type == NodeType::H1 || type == NodeType::A ||
      type == NodeType::BUTTON) {
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
  if (node.type == NodeType::PLAINTEXT && node.plaintext_content.empty()) {
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
    type = NodeType::INPUT_NODE;
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