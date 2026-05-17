#include <hjs/vm/compiler.hpp>
#include <hjs/runtime/object.hpp>
#include <stdexcept>

namespace hjs::vm {

Compiler::Compiler(Chunk& chunk) : m_chunk(chunk) {}

void Compiler::compile(hjs::ast::Expr& expr) { expr.accept(*this); }
void Compiler::compile(hjs::ast::Stmt& stmt) { stmt.accept(*this); }

// ---- Emit helpers -------------------------------------------------------

void Compiler::emit_byte(uint8_t b)      { m_chunk.write(b); }
void Compiler::emit_op(OpCode op)        { m_chunk.write((uint8_t)op); }

void Compiler::emit_constant(hjs::Value v) {
    int idx = m_chunk.add_constant(std::move(v));
    emit_op(OpCode::Constant);
    emit_byte((uint8_t)idx);
}

// Emit a 2-byte jump instruction, return position of the offset bytes for patching
int Compiler::emit_jump(OpCode op) {
    emit_op(op);
    m_chunk.write(0xFF); m_chunk.write(0xFF); // placeholder
    return m_chunk.current_offset() - 2;
}

void Compiler::patch_jump(int offset) {
    int jump = m_chunk.current_offset() - offset - 2;
    if (jump > 65535) throw std::runtime_error("Jump offset overflow.");
    m_chunk.patch16(offset, (uint16_t)jump);
}

void Compiler::emit_loop(int loop_start) {
    emit_op(OpCode::Loop);
    int offset = m_chunk.current_offset() - loop_start + 2;
    if (offset > 65535) throw std::runtime_error("Loop offset overflow.");
    m_chunk.write16((uint16_t)offset);
}

// ---- Scope helpers ------------------------------------------------------

void Compiler::begin_scope() { m_scope_depth++; }

void Compiler::end_scope() {
    m_scope_depth--;
    while (!m_locals.empty() && m_locals.back().depth > m_scope_depth) {
        emit_op(OpCode::Pop);
        m_locals.pop_back();
    }
}

void Compiler::add_local(const std::wstring& name) {
    m_locals.push_back({name, m_scope_depth});
}

int Compiler::resolve_local(const std::wstring& name) {
    for (int i = (int)m_locals.size() - 1; i >= 0; --i)
        if (m_locals[i].name == name) return i;
    return -1;
}

// ---- Expression visitors -----------------------------------------------

void Compiler::visit_literal(hjs::ast::LiteralExpr& expr) {
    emit_constant(expr.value);
}

void Compiler::visit_grouping(hjs::ast::GroupingExpr& expr) {
    expr.expression->accept(*this);
}

void Compiler::visit_unary(hjs::ast::UnaryExpr& expr) {
    expr.right->accept(*this);
    if (expr.op == L"-") emit_op(OpCode::Negate);
    else if (expr.op == L"!") emit_op(OpCode::Not);
    else if (expr.op == L"++") { emit_constant(Value(1.0)); emit_op(OpCode::Add); }
    else if (expr.op == L"--") { emit_constant(Value(1.0)); emit_op(OpCode::Subtract); }
}

void Compiler::visit_postfix(hjs::ast::PostfixExpr& expr) {
    // Evaluate operand, duplicate on stack, then modify the variable
    expr.operand->accept(*this);
    emit_op(OpCode::Dup); // keep original value as expression result
    emit_constant(Value(1.0));
    if (expr.op == L"++") emit_op(OpCode::Add);
    else                   emit_op(OpCode::Subtract);
    // Write back
    if (auto* ve = dynamic_cast<hjs::ast::VariableExpr*>(expr.operand.get())) {
        int arg = resolve_local(ve->name);
        if (arg != -1) {
            emit_op(OpCode::SetLocal); emit_byte((uint8_t)arg);
        } else {
            int idx = m_chunk.add_constant(Value(ve->name));
            emit_op(OpCode::SetGlobal); emit_byte((uint8_t)idx);
        }
        emit_op(OpCode::Pop); // discard written value; original is on stack
    }
}

void Compiler::visit_binary(hjs::ast::BinaryExpr& expr) {
    expr.left->accept(*this);
    expr.right->accept(*this);

    if      (expr.op == L"+")   emit_op(OpCode::Add);
    else if (expr.op == L"-")   emit_op(OpCode::Subtract);
    else if (expr.op == L"*")   emit_op(OpCode::Multiply);
    else if (expr.op == L"/")   emit_op(OpCode::Divide);
    else if (expr.op == L"%")   emit_op(OpCode::Modulo);
    else if (expr.op == L"<")   emit_op(OpCode::Less);
    else if (expr.op == L"<=")  emit_op(OpCode::LessEqual);
    else if (expr.op == L">")   emit_op(OpCode::Greater);
    else if (expr.op == L">=")  emit_op(OpCode::GreaterEqual);
    else if (expr.op == L"==")  emit_op(OpCode::Equal);
    else if (expr.op == L"===") emit_op(OpCode::StrictEqual);
    else if (expr.op == L"!=")  emit_op(OpCode::NotEqual);
    else if (expr.op == L"!==") emit_op(OpCode::StrictNotEqual);
}

void Compiler::visit_logical(hjs::ast::LogicalExpr& expr) {
    expr.left->accept(*this);
    if (expr.op == L"&&") {
        int jump = emit_jump(OpCode::JumpIfFalse);
        emit_op(OpCode::Pop);
        expr.right->accept(*this);
        patch_jump(jump);
    } else { // ||
        int jump = emit_jump(OpCode::JumpIfTrue);
        emit_op(OpCode::Pop);
        expr.right->accept(*this);
        patch_jump(jump);
    }
}

void Compiler::visit_ternary(hjs::ast::TernaryExpr& expr) {
    expr.condition->accept(*this);
    int else_jump = emit_jump(OpCode::JumpIfFalse);
    emit_op(OpCode::Pop);
    expr.then_expr->accept(*this);
    int end_jump = emit_jump(OpCode::Jump);
    patch_jump(else_jump);
    emit_op(OpCode::Pop);
    expr.else_expr->accept(*this);
    patch_jump(end_jump);
}

void Compiler::visit_variable(hjs::ast::VariableExpr& expr) {
    int arg = resolve_local(expr.name);
    if (arg != -1) {
        emit_op(OpCode::GetLocal); emit_byte((uint8_t)arg);
    } else {
        int idx = m_chunk.add_constant(Value(expr.name));
        emit_op(OpCode::GetGlobal); emit_byte((uint8_t)idx);
    }
}

void Compiler::visit_assign(hjs::ast::AssignExpr& expr) {
    expr.value->accept(*this);
    int arg = resolve_local(expr.name);
    if (arg != -1) {
        emit_op(OpCode::SetLocal); emit_byte((uint8_t)arg);
    } else {
        int idx = m_chunk.add_constant(Value(expr.name));
        emit_op(OpCode::SetGlobal); emit_byte((uint8_t)idx);
    }
}

void Compiler::visit_compound_assign(hjs::ast::CompoundAssignExpr& expr) {
    // Load current value
    int arg = resolve_local(expr.name);
    if (arg != -1) {
        emit_op(OpCode::GetLocal); emit_byte((uint8_t)arg);
    } else {
        int idx = m_chunk.add_constant(Value(expr.name));
        emit_op(OpCode::GetGlobal); emit_byte((uint8_t)idx);
    }
    expr.value->accept(*this);
    // Apply operator
    if      (expr.op == L"+=") emit_op(OpCode::Add);
    else if (expr.op == L"-=") emit_op(OpCode::Subtract);
    else if (expr.op == L"*=") emit_op(OpCode::Multiply);
    else if (expr.op == L"/=") emit_op(OpCode::Divide);
    else if (expr.op == L"%=") emit_op(OpCode::Modulo);
    // Store back
    if (arg != -1) {
        emit_op(OpCode::SetLocal); emit_byte((uint8_t)arg);
    } else {
        int idx = m_chunk.add_constant(Value(expr.name));
        emit_op(OpCode::SetGlobal); emit_byte((uint8_t)idx);
    }
}

void Compiler::visit_typeof_expr(hjs::ast::TypeofExpr& expr) {
    expr.expression->accept(*this);
    emit_op(OpCode::Typeof);
}

void Compiler::visit_template_string(hjs::ast::TemplateStringExpr& expr) {
    // Emit the raw string as a constant (interpolation is future work)
    emit_constant(Value(expr.raw));
}

void Compiler::visit_call(hjs::ast::CallExpr& expr) {
    expr.callee->accept(*this);
    for (auto& arg : expr.arguments) arg->accept(*this);
    emit_op(OpCode::Call); emit_byte((uint8_t)expr.arguments.size());
}

void Compiler::visit_new_expr(hjs::ast::NewExpr& expr) {
    expr.callee->accept(*this);
    for (auto& arg : expr.arguments) arg->accept(*this);
    emit_op(OpCode::CallNew); emit_byte((uint8_t)expr.arguments.size());
}

void Compiler::visit_member(hjs::ast::MemberExpr& expr) {
    expr.object->accept(*this);
    int idx = m_chunk.add_constant(Value(expr.property));
    emit_op(OpCode::GetProperty); emit_byte((uint8_t)idx);
}

void Compiler::visit_set_expr(hjs::ast::SetExpr& expr) {
    expr.object->accept(*this);
    expr.value->accept(*this);
    int idx = m_chunk.add_constant(Value(expr.property));
    emit_op(OpCode::SetProperty); emit_byte((uint8_t)idx);
}

void Compiler::visit_object_literal(hjs::ast::ObjectLiteralExpr& expr) {
    for (auto& prop : expr.properties) prop.value->accept(*this);
    emit_op(OpCode::CreateObject);
    emit_byte((uint8_t)expr.properties.size());
    for (int i = (int)expr.properties.size() - 1; i >= 0; --i) {
        int idx = m_chunk.add_constant(Value(expr.properties[i].key));
        emit_byte((uint8_t)idx);
    }
}

void Compiler::visit_array_literal(hjs::ast::ArrayLiteralExpr& expr) {
    for (auto& e : expr.elements) e->accept(*this);
    emit_op(OpCode::CreateArray);
    emit_byte((uint8_t)expr.elements.size());
}

void Compiler::visit_index_expr(hjs::ast::IndexExpr& expr) {
    expr.object->accept(*this);
    expr.index->accept(*this);
    emit_op(OpCode::GetIndex);
}

void Compiler::visit_set_index_expr(hjs::ast::SetIndexExpr& expr) {
    expr.object->accept(*this);
    expr.index->accept(*this);
    expr.value->accept(*this);
    emit_op(OpCode::SetIndex);
}

void Compiler::visit_arrow_function(hjs::ast::ArrowFunctionExpr& expr) {
    // Compile arrow function as an anonymous JSFunction
    auto fn_chunk = std::make_unique<Chunk>();
    Compiler sub(*fn_chunk);
    sub.m_scope_depth = 1;
    sub.add_local(L"this");
    for (auto& p : expr.params) sub.add_local(p);

    if (expr.body_expr) {
        expr.body_expr->accept(sub);
        sub.emit_op(OpCode::Return);
    } else {
        for (auto& s : expr.body_stmts) sub.compile(*s);
        sub.emit_op(OpCode::Return);
    }

    static std::vector<std::unique_ptr<Chunk>> g_fn_chunks;
    g_fn_chunks.push_back(std::move(fn_chunk));
    auto fn = std::make_shared<JSFunction>(L"<arrow>", (int)expr.params.size(), g_fn_chunks.back().get());
    emit_constant(Value(fn));
}

// ---- Statement visitors -------------------------------------------------

void Compiler::visit_var_stmt(hjs::ast::VarStmt& stmt) {
    if (stmt.initializer) stmt.initializer->accept(*this);
    else                  emit_op(OpCode::Nil);

    if (m_scope_depth > 0) {
        add_local(stmt.name);
    } else {
        int idx = m_chunk.add_constant(Value(stmt.name));
        emit_op(OpCode::DefineGlobal); emit_byte((uint8_t)idx);
    }
}

void Compiler::visit_expr_stmt(hjs::ast::ExprStmt& stmt) {
    stmt.expression->accept(*this);
    emit_op(OpCode::Pop);
}

void Compiler::visit_print_stmt(hjs::ast::PrintStmt& stmt) {
    stmt.expression->accept(*this);
    emit_op(OpCode::Print);
}

void Compiler::visit_if_stmt(hjs::ast::IfStmt& stmt) {
    stmt.condition->accept(*this);
    int then_jump = emit_jump(OpCode::JumpIfFalse);
    emit_op(OpCode::Pop);
    stmt.then_branch->accept(*this);
    int else_jump = emit_jump(OpCode::Jump);
    patch_jump(then_jump);
    emit_op(OpCode::Pop);
    if (stmt.else_branch) stmt.else_branch->accept(*this);
    patch_jump(else_jump);
}

void Compiler::visit_while_stmt(hjs::ast::WhileStmt& stmt) {
    int loop_start = m_chunk.current_offset();
    stmt.condition->accept(*this);
    int exit_jump = emit_jump(OpCode::JumpIfFalse);
    emit_op(OpCode::Pop);
    stmt.body->accept(*this);
    emit_loop(loop_start);
    patch_jump(exit_jump);
    emit_op(OpCode::Pop);
}

void Compiler::visit_block_stmt(hjs::ast::BlockStmt& stmt) {
    begin_scope();
    for (auto& s : stmt.statements) if (s) s->accept(*this);
    end_scope();
}

void Compiler::visit_function_stmt(hjs::ast::FunctionStmt& stmt) {
    auto fn_chunk = std::make_unique<Chunk>();
    Compiler sub(*fn_chunk);
    sub.m_scope_depth = 1;
    sub.add_local(L"this");
    for (auto& p : stmt.params) sub.add_local(p);
    for (auto& s : stmt.body)   sub.compile(*s);
    fn_chunk->write((uint8_t)OpCode::Nil);
    fn_chunk->write((uint8_t)OpCode::Return);

    static std::vector<std::unique_ptr<Chunk>> g_fn_chunks;
    g_fn_chunks.push_back(std::move(fn_chunk));
    auto fn = std::make_shared<JSFunction>(stmt.name, (int)stmt.params.size(), g_fn_chunks.back().get());

    if (m_scope_depth > 0) {
        emit_constant(Value(fn));
        add_local(stmt.name);
    } else {
        int idx = m_chunk.add_constant(Value(stmt.name));
        emit_constant(Value(fn));
        emit_op(OpCode::DefineGlobal); emit_byte((uint8_t)idx);
    }
}

void Compiler::visit_return_stmt(hjs::ast::ReturnStmt& stmt) {
    if (stmt.value) stmt.value->accept(*this);
    else            emit_op(OpCode::Nil);
    emit_op(OpCode::Return);
}

void Compiler::visit_break_stmt(hjs::ast::BreakStmt&) {
    // Placeholder: full break/continue needs loop context tracking
    emit_op(OpCode::Nil);
}

void Compiler::visit_continue_stmt(hjs::ast::ContinueStmt&) {
    emit_op(OpCode::Nil);
}

void Compiler::visit_throw_stmt(hjs::ast::ThrowStmt& stmt) {
    stmt.value->accept(*this);
    emit_op(OpCode::Throw);
}

void Compiler::visit_try_stmt(hjs::ast::TryStmt& stmt) {
    int try_jump = emit_jump(OpCode::TryStart);
    
    // Body
    stmt.body->accept(*this);
    emit_op(OpCode::TryEnd);
    
    int catch_jump = emit_jump(OpCode::Jump); // skip catch if no error

    // Catch block
    patch_jump(try_jump);
    if (stmt.catch_body) {
        begin_scope();
        if (!stmt.catch_var.empty()) {
            add_local(stmt.catch_var);
        } else {
            emit_op(OpCode::Pop); // Discard exception if unnamed catch
        }
        stmt.catch_body->accept(*this);
        end_scope();
    } else {
        // No catch, just rethrow (pop caught exception and throw again)
        emit_op(OpCode::Throw);
    }
    
    patch_jump(catch_jump);

    if (stmt.finally_body) {
        stmt.finally_body->accept(*this);
    }
}

void Compiler::visit_class_stmt(hjs::ast::ClassStmt& stmt) {
    // Create the class object
    auto class_obj = std::make_shared<JSObject>();

    // Compile each method as a JSFunction and set it on the prototype
    static std::vector<std::unique_ptr<Chunk>> g_class_chunks;

    auto proto = std::make_shared<JSObject>();
    for (auto& method : stmt.methods) {
        auto fn_chunk = std::make_unique<Chunk>();
        Compiler sub(*fn_chunk);
        sub.m_scope_depth = 1;
        sub.add_local(L"this");
        for (auto& p : method.params) sub.add_local(p);
        for (auto& s : method.body)   sub.compile(*s);
        fn_chunk->write((uint8_t)OpCode::Nil);
        fn_chunk->write((uint8_t)OpCode::Return);
        g_class_chunks.push_back(std::move(fn_chunk));
        auto fn = std::make_shared<JSFunction>(method.name, (int)method.params.size(), g_class_chunks.back().get());
        if (method.is_static)
            class_obj->set_property(method.name, Value(std::static_pointer_cast<JSObject>(fn)));
        else
            proto->set_property(method.name, Value(std::static_pointer_cast<JSObject>(fn)));
    }
    class_obj->set_property(L"prototype", Value(proto));

    // Emit the class value
    if (m_scope_depth > 0) {
        emit_constant(Value(class_obj));
        add_local(stmt.name);
    } else {
        int idx = m_chunk.add_constant(Value(stmt.name));
        emit_constant(Value(class_obj));
        emit_op(OpCode::DefineGlobal); emit_byte((uint8_t)idx);
    }
}

} // namespace hjs::vm
