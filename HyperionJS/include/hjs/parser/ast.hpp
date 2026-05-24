#pragma once

#include <memory>
#include <vector>
#include <string>
#include <hjs/core/value.hpp>

namespace hjs::ast {

class Expr;
class Stmt;

class Visitor {
public:
    virtual ~Visitor() = default;
    // Expressions
    virtual void visit_literal(class LiteralExpr& expr) = 0;
    virtual void visit_binary(class BinaryExpr& expr) = 0;
    virtual void visit_logical(class LogicalExpr& expr) = 0;
    virtual void visit_grouping(class GroupingExpr& expr) = 0;
    virtual void visit_unary(class UnaryExpr& expr) = 0;
    virtual void visit_postfix(class PostfixExpr& expr) = 0;
    virtual void visit_ternary(class TernaryExpr& expr) = 0;
    virtual void visit_variable(class VariableExpr& expr) = 0;
    virtual void visit_assign(class AssignExpr& expr) = 0;
    virtual void visit_compound_assign(class CompoundAssignExpr& expr) = 0;
    virtual void visit_call(class CallExpr& expr) = 0;
    virtual void visit_new_expr(class NewExpr& expr) = 0;
    virtual void visit_member(class MemberExpr& expr) = 0;
    virtual void visit_set_expr(class SetExpr& expr) = 0;
    virtual void visit_object_literal(class ObjectLiteralExpr& expr) = 0;
    virtual void visit_array_literal(class ArrayLiteralExpr& expr) = 0;
    virtual void visit_index_expr(class IndexExpr& expr) = 0;
    virtual void visit_set_index_expr(class SetIndexExpr& expr) = 0;
    virtual void visit_typeof_expr(class TypeofExpr& expr) = 0;
    virtual void visit_arrow_function(class ArrowFunctionExpr& expr) = 0;
    virtual void visit_template_string(class TemplateStringExpr& expr) = 0;
    // Statements
    virtual void visit_var_stmt(class VarStmt& stmt) = 0;
    virtual void visit_expr_stmt(class ExprStmt& stmt) = 0;
    virtual void visit_print_stmt(class PrintStmt& stmt) = 0;
    virtual void visit_if_stmt(class IfStmt& stmt) = 0;
    virtual void visit_while_stmt(class WhileStmt& stmt) = 0;
    virtual void visit_block_stmt(class BlockStmt& stmt) = 0;
    virtual void visit_function_stmt(class FunctionStmt& stmt) = 0;
    virtual void visit_return_stmt(class ReturnStmt& stmt) = 0;
    virtual void visit_break_stmt(class BreakStmt& stmt) = 0;
    virtual void visit_continue_stmt(class ContinueStmt& stmt) = 0;
    virtual void visit_throw_stmt(class ThrowStmt& stmt) = 0;
    virtual void visit_try_stmt(class TryStmt& stmt) = 0;
    virtual void visit_class_stmt(class ClassStmt& stmt) = 0;
    // Expressions
    virtual void visit_await(class AwaitExpr& expr) = 0;
};

class Node { public: virtual ~Node() = default; };
class Stmt : public Node { public: virtual void accept(Visitor& v) = 0; };
class Expr : public Node { public: virtual void accept(Visitor& v) = 0; };

// ---- Expressions ----

class LiteralExpr : public Expr {
public:
    explicit LiteralExpr(hjs::Value v) : value(std::move(v)) {}
    void accept(Visitor& v) override { v.visit_literal(*this); }
    hjs::Value value;
};

class BinaryExpr : public Expr {
public:
    BinaryExpr(std::unique_ptr<Expr> l, std::wstring op, std::unique_ptr<Expr> r)
        : left(std::move(l)), op(std::move(op)), right(std::move(r)) {}
    void accept(Visitor& v) override { v.visit_binary(*this); }
    std::unique_ptr<Expr> left;
    std::wstring op;
    std::unique_ptr<Expr> right;
};

// Logical expressions use short-circuit evaluation (different from binary)
class LogicalExpr : public Expr {
public:
    LogicalExpr(std::unique_ptr<Expr> l, std::wstring op, std::unique_ptr<Expr> r)
        : left(std::move(l)), op(std::move(op)), right(std::move(r)) {}
    void accept(Visitor& v) override { v.visit_logical(*this); }
    std::unique_ptr<Expr> left;
    std::wstring op; // "&&" or "||"
    std::unique_ptr<Expr> right;
};

class GroupingExpr : public Expr {
public:
    explicit GroupingExpr(std::unique_ptr<Expr> e) : expression(std::move(e)) {}
    void accept(Visitor& v) override { v.visit_grouping(*this); }
    std::unique_ptr<Expr> expression;
};

class UnaryExpr : public Expr {
public:
    UnaryExpr(std::wstring op, std::unique_ptr<Expr> r, bool prefix = true)
        : op(std::move(op)), right(std::move(r)), prefix(prefix) {}
    void accept(Visitor& v) override { v.visit_unary(*this); }
    std::wstring op;
    std::unique_ptr<Expr> right;
    bool prefix;
};

// Post-increment / post-decrement
class PostfixExpr : public Expr {
public:
    PostfixExpr(std::unique_ptr<Expr> operand, std::wstring op)
        : operand(std::move(operand)), op(std::move(op)) {}
    void accept(Visitor& v) override { v.visit_postfix(*this); }
    std::unique_ptr<Expr> operand;
    std::wstring op; // "++" or "--"
};

// condition ? then : else
class TernaryExpr : public Expr {
public:
    TernaryExpr(std::unique_ptr<Expr> cond, std::unique_ptr<Expr> then_, std::unique_ptr<Expr> else_)
        : condition(std::move(cond)), then_expr(std::move(then_)), else_expr(std::move(else_)) {}
    void accept(Visitor& v) override { v.visit_ternary(*this); }
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Expr> then_expr;
    std::unique_ptr<Expr> else_expr;
};

class VariableExpr : public Expr {
public:
    explicit VariableExpr(std::wstring n) : name(std::move(n)) {}
    void accept(Visitor& v) override { v.visit_variable(*this); }
    std::wstring name;
};

class AssignExpr : public Expr {
public:
    AssignExpr(std::wstring n, std::unique_ptr<Expr> val)
        : name(std::move(n)), value(std::move(val)) {}
    void accept(Visitor& v) override { v.visit_assign(*this); }
    std::wstring name;
    std::unique_ptr<Expr> value;
};

// +=, -=, *=, /=, %=
class CompoundAssignExpr : public Expr {
public:
    CompoundAssignExpr(std::wstring n, std::wstring op, std::unique_ptr<Expr> val)
        : name(std::move(n)), op(std::move(op)), value(std::move(val)) {}
    void accept(Visitor& v) override { v.visit_compound_assign(*this); }
    std::wstring name;
    std::wstring op;
    std::unique_ptr<Expr> value;
};

class SetExpr : public Expr {
public:
    SetExpr(std::unique_ptr<Expr> obj, std::wstring prop, std::unique_ptr<Expr> val)
        : object(std::move(obj)), property(std::move(prop)), value(std::move(val)) {}
    void accept(Visitor& v) override { v.visit_set_expr(*this); }
    std::unique_ptr<Expr> object;
    std::wstring property;
    std::unique_ptr<Expr> value;
};

struct ObjectProperty {
    std::wstring key;
    std::unique_ptr<Expr> value;
};

class ObjectLiteralExpr : public Expr {
public:
    explicit ObjectLiteralExpr(std::vector<ObjectProperty> props) : properties(std::move(props)) {}
    void accept(Visitor& v) override { v.visit_object_literal(*this); }
    std::vector<ObjectProperty> properties;
};

class ArrayLiteralExpr : public Expr {
public:
    explicit ArrayLiteralExpr(std::vector<std::unique_ptr<Expr>> elems) : elements(std::move(elems)) {}
    void accept(Visitor& v) override { v.visit_array_literal(*this); }
    std::vector<std::unique_ptr<Expr>> elements;
};

class IndexExpr : public Expr {
public:
    IndexExpr(std::unique_ptr<Expr> obj, std::unique_ptr<Expr> idx)
        : object(std::move(obj)), index(std::move(idx)) {}
    void accept(Visitor& v) override { v.visit_index_expr(*this); }
    std::unique_ptr<Expr> object;
    std::unique_ptr<Expr> index;
};

class SetIndexExpr : public Expr {
public:
    SetIndexExpr(std::unique_ptr<Expr> obj, std::unique_ptr<Expr> idx, std::unique_ptr<Expr> val)
        : object(std::move(obj)), index(std::move(idx)), value(std::move(val)) {}
    void accept(Visitor& v) override { v.visit_set_index_expr(*this); }
    std::unique_ptr<Expr> object;
    std::unique_ptr<Expr> index;
    std::unique_ptr<Expr> value;
};

class AwaitExpr : public Expr {
public:
    explicit AwaitExpr(std::unique_ptr<Expr> expr) : expression(std::move(expr)) {}
    void accept(Visitor& v) override { v.visit_await(*this); }
    std::unique_ptr<Expr> expression;
};

class CallExpr : public Expr {
public:
    CallExpr(std::unique_ptr<Expr> callee, std::vector<std::unique_ptr<Expr>> args)
        : callee(std::move(callee)), arguments(std::move(args)) {}
    void accept(Visitor& v) override { v.visit_call(*this); }
    std::unique_ptr<Expr> callee;
    std::vector<std::unique_ptr<Expr>> arguments;
};

class NewExpr : public Expr {
public:
    NewExpr(std::unique_ptr<Expr> callee, std::vector<std::unique_ptr<Expr>> args)
        : callee(std::move(callee)), arguments(std::move(args)) {}
    void accept(Visitor& v) override { v.visit_new_expr(*this); }
    std::unique_ptr<Expr> callee;
    std::vector<std::unique_ptr<Expr>> arguments;
};

class MemberExpr : public Expr {
public:
    MemberExpr(std::unique_ptr<Expr> obj, std::wstring prop)
        : object(std::move(obj)), property(std::move(prop)) {}
    void accept(Visitor& v) override { v.visit_member(*this); }
    std::unique_ptr<Expr> object;
    std::wstring property;
};

class TypeofExpr : public Expr {
public:
    explicit TypeofExpr(std::unique_ptr<Expr> e) : expression(std::move(e)) {}
    void accept(Visitor& v) override { v.visit_typeof_expr(*this); }
    std::unique_ptr<Expr> expression;
};

class ArrowFunctionExpr : public Expr {
public:
    ArrowFunctionExpr(std::vector<std::wstring> params, std::unique_ptr<Expr> body_expr, std::vector<std::unique_ptr<Stmt>> body_stmts)
        : params(std::move(params)), body_expr(std::move(body_expr)), body_stmts(std::move(body_stmts)) {}
    void accept(Visitor& v) override { v.visit_arrow_function(*this); }
    std::vector<std::wstring> params;
    std::unique_ptr<Expr> body_expr;         // for: (x) => expr
    std::vector<std::unique_ptr<Stmt>> body_stmts; // for: (x) => { stmts }
};

class TemplateStringExpr : public Expr {
public:
    explicit TemplateStringExpr(std::wstring raw) : raw(std::move(raw)) {}
    void accept(Visitor& v) override { v.visit_template_string(*this); }
    std::wstring raw;
};

// ---- Statements ----

class VarStmt : public Stmt {
public:
    VarStmt(std::wstring n, std::unique_ptr<Expr> init, bool is_const = false)
        : name(std::move(n)), initializer(std::move(init)), is_const(is_const) {}
    void accept(Visitor& v) override { v.visit_var_stmt(*this); }
    std::wstring name;
    std::unique_ptr<Expr> initializer;
    bool is_const;
};

class ExprStmt : public Stmt {
public:
    explicit ExprStmt(std::unique_ptr<Expr> e) : expression(std::move(e)) {}
    void accept(Visitor& v) override { v.visit_expr_stmt(*this); }
    std::unique_ptr<Expr> expression;
};

class PrintStmt : public Stmt {
public:
    explicit PrintStmt(std::unique_ptr<Expr> e) : expression(std::move(e)) {}
    void accept(Visitor& v) override { v.visit_print_stmt(*this); }
    std::unique_ptr<Expr> expression;
};

class IfStmt : public Stmt {
public:
    IfStmt(std::unique_ptr<Expr> cond, std::unique_ptr<Stmt> then_, std::unique_ptr<Stmt> else_)
        : condition(std::move(cond)), then_branch(std::move(then_)), else_branch(std::move(else_)) {}
    void accept(Visitor& v) override { v.visit_if_stmt(*this); }
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> then_branch;
    std::unique_ptr<Stmt> else_branch;
};

class WhileStmt : public Stmt {
public:
    WhileStmt(std::unique_ptr<Expr> cond, std::unique_ptr<Stmt> body)
        : condition(std::move(cond)), body(std::move(body)) {}
    void accept(Visitor& v) override { v.visit_while_stmt(*this); }
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> body;
};

class BlockStmt : public Stmt {
public:
    explicit BlockStmt(std::vector<std::unique_ptr<Stmt>> stmts) : statements(std::move(stmts)) {}
    void accept(Visitor& v) override { v.visit_block_stmt(*this); }
    std::vector<std::unique_ptr<Stmt>> statements;
};

class FunctionStmt : public Stmt {
public:
    FunctionStmt(std::wstring n, std::vector<std::wstring> params, std::vector<std::unique_ptr<Stmt>> body)
        : name(std::move(n)), params(std::move(params)), body(std::move(body)) {}
    void accept(Visitor& v) override { v.visit_function_stmt(*this); }
    std::wstring name;
    std::vector<std::wstring> params;
    std::vector<std::unique_ptr<Stmt>> body;
};

class ReturnStmt : public Stmt {
public:
    explicit ReturnStmt(std::unique_ptr<Expr> val) : value(std::move(val)) {}
    void accept(Visitor& v) override { v.visit_return_stmt(*this); }
    std::unique_ptr<Expr> value; // may be null
};

class BreakStmt : public Stmt {
public:
    void accept(Visitor& v) override { v.visit_break_stmt(*this); }
};

class ContinueStmt : public Stmt {
public:
    void accept(Visitor& v) override { v.visit_continue_stmt(*this); }
};

class ThrowStmt : public Stmt {
public:
    explicit ThrowStmt(std::unique_ptr<Expr> val) : value(std::move(val)) {}
    void accept(Visitor& v) override { v.visit_throw_stmt(*this); }
    std::unique_ptr<Expr> value;
};

class TryStmt : public Stmt {
public:
    TryStmt(std::unique_ptr<Stmt> body, std::wstring catch_var,
            std::unique_ptr<Stmt> catch_body, std::unique_ptr<Stmt> finally_body)
        : body(std::move(body)), catch_var(std::move(catch_var)),
          catch_body(std::move(catch_body)), finally_body(std::move(finally_body)) {}
    void accept(Visitor& v) override { v.visit_try_stmt(*this); }
    std::unique_ptr<Stmt> body;
    std::wstring catch_var;
    std::unique_ptr<Stmt> catch_body;   // may be null
    std::unique_ptr<Stmt> finally_body; // may be null
};

struct ClassMethod {
    std::wstring name;
    std::vector<std::wstring> params;
    std::vector<std::unique_ptr<Stmt>> body;
    bool is_static = false;
};

class ClassStmt : public Stmt {
public:
    ClassStmt(std::wstring name, std::wstring superclass, std::vector<ClassMethod> methods)
        : name(std::move(name)), superclass(std::move(superclass)), methods(std::move(methods)) {}
    void accept(Visitor& v) override { v.visit_class_stmt(*this); }
    std::wstring name;
    std::wstring superclass;
    std::vector<ClassMethod> methods;
};

} // namespace hjs::ast
