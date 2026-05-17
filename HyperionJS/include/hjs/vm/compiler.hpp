#pragma once

#include <hjs/parser/ast.hpp>
#include <hjs/vm/bytecode.hpp>
#include <cstdint>
#include <vector>
#include <memory>

namespace hjs::vm {

class Compiler : public hjs::ast::Visitor {
public:
    explicit Compiler(Chunk& chunk);

    void compile(hjs::ast::Expr& expr);
    void compile(hjs::ast::Stmt& stmt);

    // Expression visitors
    void visit_literal(hjs::ast::LiteralExpr& expr) override;
    void visit_binary(hjs::ast::BinaryExpr& expr) override;
    void visit_logical(hjs::ast::LogicalExpr& expr) override;
    void visit_grouping(hjs::ast::GroupingExpr& expr) override;
    void visit_unary(hjs::ast::UnaryExpr& expr) override;
    void visit_postfix(hjs::ast::PostfixExpr& expr) override;
    void visit_ternary(hjs::ast::TernaryExpr& expr) override;
    void visit_variable(hjs::ast::VariableExpr& expr) override;
    void visit_assign(hjs::ast::AssignExpr& expr) override;
    void visit_compound_assign(hjs::ast::CompoundAssignExpr& expr) override;
    void visit_call(hjs::ast::CallExpr& expr) override;
    void visit_new_expr(hjs::ast::NewExpr& expr) override;
    void visit_member(hjs::ast::MemberExpr& expr) override;
    void visit_set_expr(hjs::ast::SetExpr& expr) override;
    void visit_object_literal(hjs::ast::ObjectLiteralExpr& expr) override;
    void visit_array_literal(hjs::ast::ArrayLiteralExpr& expr) override;
    void visit_index_expr(hjs::ast::IndexExpr& expr) override;
    void visit_set_index_expr(hjs::ast::SetIndexExpr& expr) override;
    void visit_typeof_expr(hjs::ast::TypeofExpr& expr) override;
    void visit_arrow_function(hjs::ast::ArrowFunctionExpr& expr) override;
    void visit_template_string(hjs::ast::TemplateStringExpr& expr) override;

    // Statement visitors
    void visit_var_stmt(hjs::ast::VarStmt& stmt) override;
    void visit_expr_stmt(hjs::ast::ExprStmt& stmt) override;
    void visit_print_stmt(hjs::ast::PrintStmt& stmt) override;
    void visit_if_stmt(hjs::ast::IfStmt& stmt) override;
    void visit_while_stmt(hjs::ast::WhileStmt& stmt) override;
    void visit_block_stmt(hjs::ast::BlockStmt& stmt) override;
    void visit_function_stmt(hjs::ast::FunctionStmt& stmt) override;
    void visit_return_stmt(hjs::ast::ReturnStmt& stmt) override;
    void visit_break_stmt(hjs::ast::BreakStmt& stmt) override;
    void visit_continue_stmt(hjs::ast::ContinueStmt& stmt) override;
    void visit_throw_stmt(hjs::ast::ThrowStmt& stmt) override;
    void visit_try_stmt(hjs::ast::TryStmt& stmt) override;
    void visit_class_stmt(hjs::ast::ClassStmt& stmt) override;

    // Made public for sub-compiler access
    int m_scope_depth = 0;
    void add_local(const std::wstring& name);

private:
    void emit_byte(uint8_t b);
    void emit_op(OpCode op);
    void emit_constant(hjs::Value value);
    int  emit_jump(OpCode op);
    void patch_jump(int offset);
    void emit_loop(int loop_start);

    void begin_scope();
    void end_scope();
    int  resolve_local(const std::wstring& name);

    struct Local { std::wstring name; int depth; };

    Chunk& m_chunk;
    std::vector<Local> m_locals;
};

} // namespace hjs::vm
