// Copyright 2024 The XLS Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "xls/dslx/type_system_v2/typecheck_module_v2.h"

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/substitute.h"
#include "xls/common/status/status_macros.h"
#include "xls/dslx/errors.h"
#include "xls/dslx/frontend/ast.h"
#include "xls/dslx/frontend/ast_node_visitor_with_default.h"
#include "xls/dslx/frontend/ast_utils.h"
#include "xls/dslx/frontend/pos.h"
#include "xls/dslx/import_data.h"
#include "xls/dslx/type_system/type_info.h"
#include "xls/dslx/type_system_v2/inference_table.h"
#include "xls/dslx/type_system_v2/inference_table_to_type_info.h"
#include "xls/dslx/type_system_v2/type_annotation_utils.h"
#include "xls/dslx/warning_collector.h"

namespace xls::dslx {
namespace {

// A visitor that walks an AST and populates an `InferenceTable` with the
// encountered info.
class PopulateInferenceTableVisitor : public AstNodeVisitorWithDefault {
 public:
  PopulateInferenceTableVisitor(Module& module, InferenceTable& table,
                                const FileTable& file_table)
      : module_(module), table_(table), file_table_(file_table) {}

  absl::Status HandleConstantDef(const ConstantDef* node) override {
    XLS_ASSIGN_OR_RETURN(
        const NameRef* variable,
        table_.DefineInternalVariable(InferenceVariableKind::kType,
                                      const_cast<ConstantDef*>(node),
                                      GenerateInternalTypeVariableName(node)));
    XLS_RETURN_IF_ERROR(table_.SetTypeVariable(node, variable));
    XLS_RETURN_IF_ERROR(table_.SetTypeVariable(node->name_def(), variable));
    XLS_RETURN_IF_ERROR(table_.SetTypeVariable(node->value(), variable));
    if (node->type_annotation() != nullptr) {
      XLS_RETURN_IF_ERROR(
          table_.SetTypeAnnotation(node->name_def(), node->type_annotation()));
    }
    return DefaultHandler(node);
  }

  absl::Status HandleConstRef(const ConstRef* node) override {
    return PropagateDefToRef(node);
  }

  absl::Status HandleNameRef(const NameRef* node) override {
    return PropagateDefToRef(node);
  }

  absl::Status HandleNumber(const Number* node) override {
    TypeAnnotation* annotation = node->type_annotation();
    if (annotation == nullptr) {
      XLS_ASSIGN_OR_RETURN(annotation,
                           CreateAnnotationSizedToFit(module_, *node));
      // Treat `true` and `false` like they have intrinsic bool annotations.
      // Otherwise, consider an annotation we add to be an auto-annotation that
      // is "negotiable".
      if (node->number_kind() != NumberKind::kBool) {
        auto_literal_annotations_.insert(annotation);
      }
    }
    return table_.SetTypeAnnotation(node, annotation);
  }

  absl::Status HandleBinop(const Binop* node) override {
    // Any `Binop` should be a descendant of some context-setting node and
    // should have a type that was set when its parent was visited.
    const NameRef* type_variable = *table_.GetTypeVariable(node);
    if (GetBinopSameTypeKinds().contains(node->binop_kind())) {
      XLS_RETURN_IF_ERROR(table_.SetTypeVariable(node->lhs(), type_variable));
      XLS_RETURN_IF_ERROR(table_.SetTypeVariable(node->rhs(), type_variable));
    } else {
      return absl::UnimplementedError(
          absl::StrCat("Type inference version 2 is a work in progress and "
                       "does not yet support the expression: ",
                       node->ToString()));
    }
    return DefaultHandler(node);
  }

  absl::Status HandleXlsTuple(const XlsTuple* node) override {
    // When we come in here with an example like:
    //   const FOO: (u32, (s8, u32)) = (4, (-2, 5));
    //
    // the table will look like this before descent into this function:
    //   Node               Annotation          Variable
    //   -----------------------------------------------
    //   FOO                (u32, (s8, u32))    T0
    //   (4, (-2, 5))                           T0
    //
    // and this function will make it look like this:
    //   Node               Annotation          Variable
    //   -----------------------------------------------
    //   FOO                (u32, (s8, u32))    T0
    //   (4, (-2, 5))       (var:M0, var:M1)    T0
    //   4                                      M0
    //   (-2, 5)                                M1
    //
    // Recursive descent will ultimately put auto annotations for the literals
    // in the table. Upon conversion of the table to type info, unification of
    // the LHS annotation with the variable-based RHS annotation will be
    // attempted.

    // Create the M0, M1, ... variables and apply them to the members.
    std::vector<TypeAnnotation*> member_types;
    member_types.reserve(node->members().size());
    for (int i = 0; i < node->members().size(); ++i) {
      Expr* member = node->members()[i];
      XLS_ASSIGN_OR_RETURN(const NameRef* member_variable,
                           table_.DefineInternalVariable(
                               InferenceVariableKind::kType, member,
                               GenerateInternalTypeVariableName(member)));
      XLS_RETURN_IF_ERROR(table_.SetTypeVariable(member, member_variable));
      member_types.push_back(
          module_.Make<TypeVariableTypeAnnotation>(member_variable));
    }
    // Annotate the whole tuple expression as (var:M0, var:M1, ...).
    XLS_RETURN_IF_ERROR(table_.SetTypeAnnotation(
        node, module_.Make<TupleTypeAnnotation>(node->span(), member_types)));
    return DefaultHandler(node);
  }

  absl::Status HandleArray(const Array* node) override {
    // When we come in here with an example like:
    //   const FOO = [u32:4, u32:5];
    //
    // the table will look like this before descent into this function:
    //   Node               Annotation          Variable
    //   -----------------------------------------------
    //   FOO                                    T0
    //   [u32:4, u32:5]                         T0
    //
    // and this function will make it look like this:
    //   Node               Annotation          Variable
    //   -----------------------------------------------
    //   FOO                                    T0
    //   [u32:4, u32:5]     var:T1[2]           T0
    //   u32:4                                  T1
    //   u32:5                                  T1
    //
    // Recursive descent will ultimately put annotations on the elements in the
    // table. Upon conversion of the table to type info, unification of any LHS
    // annotation with the variable-based RHS annotation will be attempted, and
    // this unification will fail if the array is inadequately annotated (e.g.
    // no explicit annotation on a zero-size or elliptical array).

    // An empty array can't end with an ellipsis, even if unification is
    // possible.
    if (node->has_ellipsis() && node->members().empty()) {
      return TypeInferenceErrorStatus(
          node->span(), nullptr,
          "Array cannot have an ellipsis (`...`) without an element to repeat.",
          file_table_);
    }

    // If the array has no members, we can't infer an RHS element type and
    // therefore can't annotate the RHS. The success of the later unification
    // will depend on whether the LHS is annotated.
    if (node->members().empty()) {
      return absl::OkStatus();
    }

    // Create a variable for the element type, and assign it to all the
    // elements.
    XLS_ASSIGN_OR_RETURN(
        const NameRef* element_type_variable,
        table_.DefineInternalVariable(InferenceVariableKind::kType,
                                      const_cast<Array*>(node),
                                      GenerateInternalTypeVariableName(node)));
    for (Expr* member : node->members()) {
      XLS_RETURN_IF_ERROR(
          table_.SetTypeVariable(member, element_type_variable));
    }
    Expr* element_count = module_.Make<Number>(
        node->span(), absl::StrCat(node->members().size()), NumberKind::kOther,
        /*type_annotation=*/nullptr);
    XLS_RETURN_IF_ERROR(table_.SetTypeAnnotation(
        node,
        module_.Make<ArrayTypeAnnotation>(
            node->span(),
            module_.Make<TypeVariableTypeAnnotation>(element_type_variable),
            element_count,
            /*dim_is_min=*/node->has_ellipsis())));
    return DefaultHandler(node);
  }

  absl::Status HandleFunction(const Function* node) {
    // Create a variable for the function return type, and use it to unify the
    // formal and actual return type. Eventually, we should create the notion of
    // "function type" annotations, and use them here instead of annotating the
    // function with just the return type. However, until we have constructs
    // like lambdas, that would be overkill.
    XLS_ASSIGN_OR_RETURN(
        const NameRef* type_variable,
        table_.DefineInternalVariable(InferenceVariableKind::kType,
                                      const_cast<Function*>(node),
                                      GenerateInternalTypeVariableName(node)));
    const TypeAnnotation* return_type = GetReturnType(*node);
    XLS_RETURN_IF_ERROR(table_.SetTypeVariable(node, type_variable));
    XLS_RETURN_IF_ERROR(table_.SetTypeVariable(node->body(), type_variable));
    XLS_RETURN_IF_ERROR(table_.SetTypeAnnotation(node, return_type));
    return DefaultHandler(node);
  }

  absl::Status HandleStatementBlock(const StatementBlock* node) {
    // A statement block may have a type variable imposed at a higher level of
    // the tree. For example, in
    //     `const X = { statement0; ...; statementN }`
    // or
    //     `fn foo() -> u32 { statement0; ...; statementN }`
    //
    // we will have imposed a type variable on the statement block upon hitting
    // the `ConstantDef` or `Function`. In such cases, we need to propagate the
    // statement block's type variable to `statementN`, if it is an `Expr`, in
    // order for unification to ensure that it's producing the expected type.
    std::optional<const NameRef*> variable = table_.GetTypeVariable(node);
    if (!node->trailing_semi() && !node->statements().empty() &&
        variable.has_value()) {
      const Statement* last_statement =
          node->statements()[node->statements().size() - 1];
      if (std::holds_alternative<Expr*>(last_statement->wrapped())) {
        XLS_RETURN_IF_ERROR(table_.SetTypeVariable(
            std::get<Expr*>(last_statement->wrapped()), *variable));
      }
    }
    return DefaultHandler(node);
  }

  absl::Status HandleInvocation(const Invocation* node) {
    if (const auto& name_ref = dynamic_cast<const NameRef*>(node->callee());
        name_ref != nullptr &&
        std::holds_alternative<const NameDef*>(name_ref->name_def())) {
      const NameDef* def = std::get<const NameDef*>(name_ref->name_def());
      if (const auto* fn = dynamic_cast<Function*>(def->definer())) {
        if (fn->IsParametric()) {
          return absl::UnimplementedError(
              "Type inference version 2 is a work in progress and does not yet "
              "support parametric functions.");
        } else {
          return HandleNonParametricInvocation(node, *fn);
        }
      } else {
        return TypeInferenceErrorStatus(
            node->callee()->span(), nullptr,
            absl::Substitute("Invocation callee `$0` is not a function",
                             node->callee()->ToString()),
            file_table_);
      }
    }
    return absl::UnimplementedError(
        "Type inference version 2 is a work in progress and only supports "
        "invoking free functions in the same module so far.");
  }

  absl::Status DefaultHandler(const AstNode* node) override {
    for (AstNode* child : node->GetChildren(/*want_types=*/true)) {
      XLS_RETURN_IF_ERROR(child->Accept(this));
    }
    return absl::OkStatus();
  }

  const absl::flat_hash_set<const TypeAnnotation*>& auto_literal_annotations()
      const {
    return auto_literal_annotations_;
  }

 private:
  // Helper that handles invocation nodes calling non-parametric functions.
  absl::Status HandleNonParametricInvocation(const Invocation* node,
                                             const Function& fn) {
    // When we come in here with an example like:
    //   let x: u32 = foo(a, b);
    //
    // the table will look like this before descent into this function:
    //   Node               Annotation             Variable
    //   --------------------------------------------------
    //   x                  u32                    T0
    //   foo(a, b)                                 T0
    //
    // and this function will make it look like this:
    //   Node               Annotation             Variable
    //   --------------------------------------------------
    //   x                  u32                    T0
    //   foo(a, b)          formal ret type        T0
    //   a                  formal arg0 type       T1
    //   b                  formal arg1 type       T2
    //
    // Recursive descent will attach the types of the actual arg exprs to `T1`
    // and `T2`. Unification at the end will ensure that the LHS (`x` in this
    // example) agrees with the formal return type, and that each actual
    // argument type matches the formal argument type.

    // The argument count must be correct in order to do anything else.
    if (node->args().size() != fn.params().size()) {
      return ArgCountMismatchErrorStatus(
          node->span(),
          absl::Substitute("Expected $0 argument(s) but got $1.",
                           fn.params().size(), node->args().size()),
          file_table_);
    }

    const TypeAnnotation* return_type = GetReturnType(fn);
    XLS_RETURN_IF_ERROR(table_.SetTypeAnnotation(node, return_type));
    for (int i = 0; i < node->args().size(); i++) {
      const Expr* actual_param = node->args()[i];
      const Param* formal_param = fn.params()[i];
      XLS_ASSIGN_OR_RETURN(
          const NameRef* arg_type_variable,
          table_.DefineInternalVariable(
              InferenceVariableKind::kType, const_cast<Expr*>(actual_param),
              GenerateInternalTypeVariableName(formal_param, actual_param)));
      XLS_RETURN_IF_ERROR(
          table_.SetTypeVariable(actual_param, arg_type_variable));
      XLS_RETURN_IF_ERROR(table_.SetTypeAnnotation(
          actual_param, formal_param->type_annotation()));
      XLS_RETURN_IF_ERROR(actual_param->Accept(this));
    }
    return absl::OkStatus();
  }

  // Generates a name for an internal inference variable that will be used as
  // the type for the given node. The name is only relevant for traceability.
  template <typename T>
  std::string GenerateInternalTypeVariableName(const T* node) {
    return absl::Substitute("internal_type_$0_at_$1", node->identifier(),
                            node->span().ToString(file_table_));
  }
  // Specialization for `Expr` nodes, which do not have an identifier.
  template <>
  std::string GenerateInternalTypeVariableName(const Expr* node) {
    return absl::StrCat("internal_type_expr_at_",
                        node->span().ToString(file_table_));
  }
  // Specialization for `Array` nodes.
  template <>
  std::string GenerateInternalTypeVariableName(const Array* node) {
    return absl::StrCat("internal_type_array_element_at_",
                        node->span().ToString(file_table_));
  }
  // Variant for an actual function argument.
  std::string GenerateInternalTypeVariableName(const Param* formal_param,
                                               const Expr* actual_arg) {
    return absl::StrCat("internal_type_actual_arg_", formal_param->identifier(),
                        "_at_", actual_arg->span().ToString(file_table_));
  }

  // Propagates the type from the def for `ref`, to `ref` itself in the
  // inference table. This may result in a `TypeAnnotation` being added to the
  // table, but never a variable. If the type of the def is governed by a
  // variable, then `ref` will get a `TypeVariableTypeAnnotation`. This allows
  // the caller to assign a variable to `ref` which unifies it with its
  // context, while also carrying the type information over from its def.
  template <typename T>
  absl::Status PropagateDefToRef(const T* ref) {
    const AstNode* def;
    if constexpr (is_variant<decltype(ref->name_def())>::value) {
      def = ToAstNode(ref->name_def());
    } else {
      def = ref->name_def();
    }
    std::optional<const NameRef*> variable = table_.GetTypeVariable(def);
    if (variable.has_value()) {
      return table_.SetTypeAnnotation(
          ref, module_.Make<TypeVariableTypeAnnotation>(*variable));
    }
    std::optional<const TypeAnnotation*> annotation =
        table_.GetTypeAnnotation(def);
    if (annotation.has_value()) {
      return table_.SetTypeAnnotation(ref, *annotation);
    }
    return absl::OkStatus();
  }

  // Returns the explicit or implied return type of `fn`.
  const TypeAnnotation* GetReturnType(const Function& fn) {
    return fn.return_type() != nullptr
               ? fn.return_type()
               : CreateUnitTupleAnnotation(module_, fn.span());
  }

  Module& module_;
  InferenceTable& table_;
  const FileTable& file_table_;
  absl::flat_hash_set<const TypeAnnotation*> auto_literal_annotations_;
};

}  // namespace

absl::StatusOr<TypeInfo*> TypecheckModuleV2(Module* module,
                                            ImportData* import_data,
                                            WarningCollector* warnings) {
  std::unique_ptr<InferenceTable> table =
      InferenceTable::Create(*module, import_data->file_table());
  PopulateInferenceTableVisitor visitor(*module, *table,
                                        import_data->file_table());
  XLS_RETURN_IF_ERROR(module->Accept(&visitor));
  return InferenceTableToTypeInfo(*table, *module, *import_data, *warnings,
                                  import_data->file_table(),
                                  visitor.auto_literal_annotations());
}

}  // namespace xls::dslx
