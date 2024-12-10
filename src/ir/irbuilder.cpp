#include "ir/irbuilder.hpp"

void ir::IrBuilder::visit(ast::compunit_syntax &node)
{
    /*
        init global variable
    */

    this->compunit = std::make_shared<ir::ir_module>();
    /*
        register lib functions
    */

    /*
        visit children
    */
    for(auto & i : node.global_defs)
        i->accept(*this);
}

void ir::IrBuilder::visit(ast::func_def_syntax &node){
    auto rettype = node.rettype;
    string name = node.name;
    
    this->cur_block = nullptr;
    this->cur_func = nullptr;
    this->cur_func_name = "";
    this->found_main = false;
    this->return_bb = nullptr;
    this->return_value.clear();
    /*
        first we should register this function in symbol table
    */
    cur_func = compunit->new_func(name);
    this->functions.push_func(name,cur_func);
    /*
        next we should deal with entry module
        in this module, we should insert instructions which is responsible for args.  
    */
    cur_func->entry = cur_func->new_block();
    cur_block = cur_func->entry;
    if(name == "main"){
        this->found_main = true;
    }
    return_bb = cur_func->new_block();
    /*
        deal with block
    */
    node.body->accept(*this);
    /*
        deal with ret
    */
    switch (rettype)
    {
    case vartype::VOID:
        {
            cur_block->push_back(std::make_shared<ir::jump>(return_bb));
            auto ret_value = std::make_shared<ir::ir_constant>(0);
            return_bb->push_back(std::make_shared<ir::ret>(ret_value,false));
            //false express non return
        }
        break;
    default:
        {
            auto ret_val = std::make_shared<ir::ir_constant>(0);
            return_value.emplace_back(ret_val,cur_block);
            cur_block->push_back(std::make_shared<ir::jump>(return_bb));
            /*
                set a default value to last block
            */
            /*
                add a phi inst
            */
            auto ret_dst = cur_func->new_reg(vartype::INT);
            auto phi_inst = std::make_shared<ir::phi>(ret_dst);
            phi_inst->uses = return_value;
            return_bb->push_back(phi_inst);
            return_bb->push_back(std::make_shared<ir::ret>(ret_dst,true));
        }
        break;
    }
}

void ir::IrBuilder::visit(ast::rel_cond_syntax &node)
{
    // 访问左右操作数
    node.lhs->accept(*this);
    auto lhs = pass_value;
    node.rhs->accept(*this);
    auto rhs = pass_value;
    
    // 创建新的寄存器存储比较结果
    auto dst = cur_func->new_reg(vartype::INT);
    
    // 生成比较指令
    cur_block->push_back(std::make_shared<ir::cmp_ins>(dst, lhs, rhs, node.op));
    
    // 传递结果
    this->pass_value = dst;
}

void ir::IrBuilder::visit(ast::logic_cond_syntax &node)
{
    // 访问左右操作数
    node.lhs->accept(*this);
    auto lhs = pass_value;
    node.rhs->accept(*this);
    auto rhs = pass_value;
    
    // 创建新的寄存器存储逻辑运算结果
    auto dst = cur_func->new_reg(vartype::INT);
    
    // 生成逻辑运算指令
    cur_block->push_back(std::make_shared<ir::logic_ins>(dst, lhs, rhs, node.op));
    
    // 传递结果
    this->pass_value = dst;
}

void ir::IrBuilder::visit(ast::binop_expr_syntax &node)
{
    /*
        the output of binary expression is a value, not a condition
    */
    node.lhs->accept(*this);
    auto exp1=pass_value;
    node.rhs->accept(*this);
    auto exp2=pass_value;
    auto dst = cur_func->new_reg(vartype::INT);
    cur_block->push_back(std::make_shared<ir::binary_op_ins>(dst,exp1,exp2,node.op));
    this->pass_value=dst;
}

void ir::IrBuilder::visit(ast::unaryop_expr_syntax &node)
{
    // 访问操作数
    node.rhs->accept(*this);
    auto rhs = pass_value;
    
    // 创建新的寄存器存储结果
    auto dst = cur_func->new_reg(vartype::INT);
    
    // 对于负号运算符，生成 0 - operand 的指令
    if (node.op == unaryop::minus) {
        auto zero = std::make_shared<ir::ir_constant>(0);
        cur_block->push_back(std::make_shared<ir::binary_op_ins>(
            dst,
            zero,
            rhs,
            binop::minus
        ));
    }
    // 其他一元运算符的处理...
    else {
        cur_block->push_back(std::make_shared<ir::unary_op_ins>(dst, rhs, node.op));
    }
    
    // 传递结果
    this->pass_value = dst;
}

void ir::IrBuilder::visit(ast::lval_syntax &node)
{
    // 从符号表中查找变量
    auto var = scope.find_var(node.name);
    if (!var) {
        // 错误处理：变量未定义
        return;
    }
    
    // 创建新的寄存器存储加载的值
    auto dst = cur_func->new_reg(vartype::INT);
    
    // 生成load指令
    cur_block->push_back(std::make_shared<ir::load>(dst, var->get_addr()));
    
    // 传递结果
    this->pass_value = dst;
}



void ir::IrBuilder::visit(ast::literal_syntax &node)
{
    auto constant = std::make_shared<ir::ir_constant>(node.intConst);
    pass_value = constant;
}

void ir::IrBuilder::visit(ast::var_def_stmt_syntax &node)
{
    // 为变量分配内存空间
    auto var = cur_func->new_obj(node.name);
    
    // 在当前作用域中注册这个变量
    this->scope.push_var(node.name, var);
    
    // 生成alloc指令
    cur_block->push_back(std::make_shared<ir::alloc>(var));

    // 如果有初始值,需要生成store指令存储初始值
    if(node.initializer) {
        // 访问初始值表达式,结果会存在pass_value中
        node.initializer->accept(*this);
        
        // 生成store指令,将初始值存入分配的内存
        cur_block->push_back(std::make_shared<ir::store>(var->get_addr(), pass_value));
    }
}

void ir::IrBuilder::visit(ast::assign_stmt_syntax &node)
{
    // 获取左值的地址
    auto var = scope.find_var(node.target->name);
    if (!var) {
        // 错误处理：变量未定义
        return;
    }
    
    // 计算右值表达式
    node.value->accept(*this);
    auto value = pass_value;
    
    // 生成store指令
    cur_block->push_back(std::make_shared<ir::store>(var->get_addr(), value));
}

void ir::IrBuilder::visit(ast::block_syntax &node)
{
    //由于我们的函数没有参数，所以，直接让block来处理scope的事情
    this->scope.enter();
    for(auto i :node.body)
    {
        i->accept(*this);
    }
    this->scope.exit();
}

void ir::IrBuilder::visit(ast::if_stmt_syntax &node)
{
    // 访问条件表达式
    node.pred->accept(*this);
    auto cond_value = pass_value;
    
    // 创建 then 和 merge 基本块
    auto then_bb = cur_func->new_block();
    auto merge_bb = cur_func->new_block();
    
    // 创建 else 基本块（如果有else分支）
    auto else_bb = node.else_body ? cur_func->new_block() : merge_bb;
    
    // 生成条件跳转指令
    cur_block->push_back(std::make_shared<ir::br>(cond_value, then_bb, else_bb));
    
    // 处理 then 分支
    cur_block = then_bb;
    node.then_body->accept(*this);
    cur_block->push_back(std::make_shared<ir::jump>(merge_bb));
    
    // 处理 else 分支（如果存在）
    if (node.else_body) {
        cur_block = else_bb;
        node.else_body->accept(*this);
        cur_block->push_back(std::make_shared<ir::jump>(merge_bb));
    }
    
    // 设置当前基本块为合并块
    cur_block = merge_bb;
}

void ir::IrBuilder::visit(ast::return_stmt_syntax &node)
{

    if(cur_func->rettype != vartype::VOID)//有返回值
    {
        node.exp->accept(*this);
        //放置好返回值和来源BB后跳转到return_bb
        return_value.emplace_back(pass_value,cur_block);
        cur_block->push_back(std::make_shared<ir::jump>(return_bb));
    }
    else//没有返回值
    {
        //直接跳转到return的BB
        auto jmp_inst = std::make_shared<ir::jump>(return_bb);
        cur_block->push_back(jmp_inst);
    }
    //对BB进行划分，这个BB用于防止return后面的部分（这部分永远不会被执行到）
    this->cur_block = this->cur_func->new_block();
}

void ir::IrBuilder::visit(ast::var_decl_stmt_syntax &node)
{
    for(auto i :node.var_def_list)
    {
        i->accept(*this);
    }
}