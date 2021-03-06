#pragma once

#include <Python.h>
#include "torch/csrc/jit/ir.h"
#include "torch/csrc/toffee.h"
#include <vector>

namespace torch { namespace autograd {

struct PrimSpecContext {
  jit::Graph* graph;
  const std::unordered_map<void*, jit::Node*>* buffer_map;
  int batch_norm_count = 0;
};

struct primspec_unconvertible : public std::runtime_error {
  using std::runtime_error::runtime_error;
};


struct HasPrimSpec {
  // Add some nodes to the Toffee protobuf, under the assumption that this node
  // as a whole has the represented inputs and outputs.  Raises a
  // primspec_unconvertible exception if conversion is not supported.
  virtual jit::node_list primspec(PrimSpecContext* ctx, jit::node_list inputs) = 0;
};

}} // namespace torch::autograd
