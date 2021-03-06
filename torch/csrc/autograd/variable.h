#pragma once

#include <Python.h>
#include <mutex>
#include <memory>
#include <functional>
#include <ATen/ATen.h>

#include "torch/csrc/jit/ir.h"
#include "torch/csrc/autograd/function_hook.h"
#include "torch/csrc/autograd/variable_version.h"
#include "torch/csrc/Types.h"

namespace torch { namespace jit { namespace tracer {

struct TracingState;

}}}

namespace torch { namespace autograd {

struct Function;

extern const char* ERR_BACKWARD_TWICE;

struct Variable : std::enable_shared_from_this<Variable> {
  struct ValueTracingState {
    std::weak_ptr<torch::jit::tracer::TracingState> state;
    // it's only valid to use this field if !state.exired()
    torch::jit::Node* trace = nullptr;

    void reset() {
      state.reset();
      trace = nullptr;
    }
  };

  struct SavedVariable {
    SavedVariable()
      : data()
      , version()
      , expected_version(-1) {}

    SavedVariable(const Variable& variable, bool with_grad_fn)
      : data(variable.data)
      , has_grad_fn(variable.grad_fn != nullptr)
      , grad_fn(with_grad_fn ? variable.grad_fn : nullptr)
      , grad_accumulator(variable.grad_accumulator)
      , version(variable.version_counter->new_saved_ref())
      , requires_grad(variable.requires_grad)
      , is_volatile(false)
      , expected_version(**variable.version_counter)
      , tracing_state(variable.tracing_state) {}

    at::Tensor data;
    // The gradient function associated with this node. If has_grad_fn
    // is false, then this is a leaf node. Note that the grad_fn is not saved if
    // it would create a circular reference. In that case, the grad_fn must be
    // passed in to the unpack function when reconstructing the Variable.
    bool has_grad_fn;
    std::shared_ptr<Function> grad_fn;
    std::weak_ptr<Function> grad_accumulator;
    std::unique_ptr<VariableVersion> version;
    bool requires_grad;
    bool is_volatile;
    int expected_version;
    ValueTracingState tracing_state;

    std::shared_ptr<Variable> unpack(std::shared_ptr<Function> saved_for=nullptr);

    at::Tensor unpack_data(std::shared_ptr<Function> saved_for=nullptr) {
      auto var = unpack(saved_for);
      return var ? var->data : at::Tensor();
    }
  };

  // WARNING: this registers the Variable as a new output
  Variable(
      at::Tensor data,
      std::shared_ptr<Function> grad_fn);

  Variable(
      at::Tensor data,
      bool requires_grad,
      bool is_volatile);

  std::shared_ptr<Function> get_grad_accumulator();

  inline SavedVariable save(Function* saved_for) {
    return SavedVariable(*this, grad_fn.get() != saved_for);
  }

  inline SavedVariable save(bool with_grad_fn) {
    return SavedVariable(*this, with_grad_fn);
  }

  static inline SavedVariable save_opt(Variable* var, Function* saved_for) {
    return var ? var->save(saved_for) : SavedVariable();
  }

  // TODO: should be at::Tensor&& if we are taking ownership?
  static inline std::shared_ptr<Variable> of(at::Tensor data, bool is_volatile=false) {
    if (!data.defined()) {
      return std::shared_ptr<Variable>();
    }
    return std::make_shared<Variable>(data, false, is_volatile);
  }

  at::Tensor data;
  std::shared_ptr<Function> grad_fn;
  std::shared_ptr<Variable> grad;
  std::unique_ptr<VariableVersion> version_counter;
  std::vector<std::shared_ptr<FunctionPreHook>> hooks;
  std::weak_ptr<Function> grad_accumulator;
  std::mutex grad_accumulator_lock;
  bool requires_grad;
  bool is_volatile;
  // The "output number" of this variable; e.g., if this variable
  // was the second output of a function, then output_nr == 1.
  // We use this to make sure we can setup the backwards trace
  // correctly when this variable is passed to another function.
  int output_nr;
  PyObject *pyobj;  // weak reference

  // For use in torch::jit::tracer
  ValueTracingState tracing_state;
};

using SavedVariable = Variable::SavedVariable;

}} // namespace torch::autograd
