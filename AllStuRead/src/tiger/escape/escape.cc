#include "tiger/escape/escape.h"
#include "tiger/absyn/absyn.h"

namespace esc {
/*
使用抽象语法树（absyn_tree_）的 Traverse 方法来进行逃逸分析。
env_ 是一个指向逃逸环境的指针，传递给 Traverse 方法。
*/
void EscFinder::FindEscape() { absyn_tree_->Traverse(env_.get()); }
} // namespace esc

namespace absyn {

/*All class below were defined in absyn.h, 
mainly discuss in 6.2.3 in the textbook,
we need to complete the Traverse func defined in each class*/
/*递归遍历抽象语法树的方法，依次调用各节点的 Traverse 方法*/
void AbsynTree::Traverse(esc::EscEnvPtr env) {
  /* TODO: Put your lab5 code here */
  /*调用根节点的 Traverse 方法，并传递深度为 1*/
  if (!root_ || !env)
    throw std::invalid_argument("NULL pointer argument");
  root_->Traverse(env, 1);
  /* End for lab5 code */
}

void SimpleVar::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  /*查找变量在逃逸环境中的条目，如果条目存在且其深度小于当前深度，则标记该变量为逃逸。*/
  esc::EscapeEntry * entry = env->Look(sym_);
  if (entry && entry->depth_ < depth) {
    *(entry->escape_) = true;
  }
  /* End for lab5 code */
}

/*FieldVar::Traverse 和 SubscriptVar::Traverse,递归遍历变量和下标*/
/*对记录字段的访问*/
void FieldVar::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  var_->Traverse(env, depth);
  /* End for lab5 code */
}

void SubscriptVar::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  var_->Traverse(env, depth);
  subscript_->Traverse(env, depth);
  /* End for lab5 code */
}

void VarExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  var_->Traverse(env, depth);
  /* End for lab5 code */
}

void NilExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  /* End for lab5 code */
}

void IntExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  /* End for lab5 code */
}

void StringExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  /* End for lab5 code */
}

/*遍历所有参数*/
void CallExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  if (!args_)
    return;
  for (Exp *arg : args_->GetList())
    arg->Traverse(env, depth);
  /* End for lab5 code */
}

/*遍历操作符两边的表达式*/
void OpExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  left_->Traverse(env, depth);
  right_->Traverse(env, depth);
  /* End for lab5 code */
}

/*记录类型的实例化,遍历记录中的每个字段*/
/*
假设有一个表示学生记录的 RecordExp 表达式：

tiger
Copy code
let
  type student = {name: string, age: int}
in
  student{name = "Alice", age = 20}
end
在这个 RecordExp 中：
  type_ 表示记录类型 student。
  fields_ 包含两个 EField：name = "Alice" 和 age = 20。
  在 Traverse 函数中，遍历 fields_ 列表，并对每个 EField 的初始化表达式（如 "Alice" 和 20）进行遍历和处理。
*/
void RecordExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  for (EField *efield : fields_->GetList())
    efield->exp_->Traverse(env, depth);
  /* End for lab5 code */
}

/*遍历序列中的每个表达式*/
void SeqExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  for (Exp *seq_exp : seq_->GetList())
    seq_exp->Traverse(env, depth);
  /* End for lab5 code */
}

/*遍历变量和赋值表达式*/
void AssignExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  var_->Traverse(env, depth);
  exp_->Traverse(env, depth);
  /* End for lab5 code */
}

/*遍历if表达式、then表达式和可选的else表达式*/
void IfExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  test_->Traverse(env, depth);
  then_->Traverse(env, depth);
  if (elsee_)
    elsee_->Traverse(env, depth);
  /* End for lab5 code */
}

/*遍历测试表达式和循环体*/
void WhileExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  test_->Traverse(env, depth);
  assert(body_);
  body_->Traverse(env, depth);
  /* End for lab5 code */
}

/*遍历循环的开始和结束表达式，并在循环体内增加作用域*/
/*
在 for 循环中，循环变量（例如 i）的作用域应该局限于循环体内部。
在每次循环迭代开始之前，循环变量都会被重新赋值。
因此，需要在循环体内引入新的作用域，以确保变量不会影响到外部的代码。

ForExp 和 WhileExp 的主要区别在于 ForExp 需要引入一个新的循环变量，而 WhileExp 不需要。
具体来说，ForExp 定义了一个新的循环变量，其作用域应该仅限于循环体内。
而 WhileExp 则没有引入新的变量，只是评估一个条件表达式，因此不需要在循环体内引入新的作用域。
*/
void ForExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  lo_->Traverse(env, depth);
  hi_->Traverse(env, depth);

  env->BeginScope();
  escape_ = false;
  env->Enter(var_, new esc::EscapeEntry(depth, &escape_));//在当前作用域内注册循环变量
  assert(body_);
  body_->Traverse(env, depth);
  env->EndScope();
  /* End for lab5 code */
}

void BreakExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  /* End for lab5 code */
}

/*遍历所有声明和主体表达式，并在过程中增加和减少作用域*/
/*
eg.
假设有一个 let 表达式如下：

tiger
Copy code
let
  var a := 10
  function f(x: int): int = x + a
in
  f(5) + a
end
在这个 let 表达式中：

声明了一个变量 a 和一个函数 f。
主体表达式是 f(5) + a。
在 Traverse 函数中：
  env->BeginScope() 开始一个新的作用域。
  遍历变量声明 var a := 10 并将其添加到新的作用域中。
  遍历函数声明 function f(x: int): int = x + a 并将其添加到新的作用域中。
  遍历主体表达式 f(5) + a。
  env->EndScope() 结束当前作用域，移除变量 a 和函数 f。
*/
void LetExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  env->BeginScope();
  for (Dec *dec : decs_->GetList())
    dec->Traverse(env, depth);

  assert(body_ != nullptr);
  body_->Traverse(env, depth);
  env->EndScope();
  /* End for lab5 code */
}

/*遍历数组大小和初始化表达式*/
void ArrayExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  size_->Traverse(env, depth);
  init_->Traverse(env, depth);
  /* End for lab5 code */
}

void VoidExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  /* End for lab5 code */
}

/*遍历所有函数声明，并在每个函数体内增加作用域*/
void FunctionDec::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  for (FunDec *func_dec : functions_->GetList()) {
    env->BeginScope();
    if (func_dec->params_) {
      for (absyn::Field *param : func_dec->params_->GetList()) {
        param->escape_ = false;
        env->Enter(param->name_,
                   new esc::EscapeEntry(depth + 1, &param->escape_));
      }
    }
    func_dec->body_->Traverse(env, depth + 1);
    env->EndScope();
  }
  /* End for lab5 code */
}

/*遍历变量初始化表达式，并在环境中注册变量*/
void VarDec::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  escape_ = false;
  env->Enter(var_, new esc::EscapeEntry(depth, &escape_));
  init_->Traverse(env, depth);
  /* End for lab5 code */
}

void TypeDec::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  /* End for lab5 code */
}

} // namespace absyn
