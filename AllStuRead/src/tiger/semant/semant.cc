#include "tiger/absyn/absyn.h"
#include "tiger/semant/semant.h"

#include <unordered_map>

using namespace std;
namespace absyn {

//p82
//包含了对抽象语法树（AST）的语义分析功能(SemAnalyze in absyn.h)，定义了如何对各种表达式、变量和声明进行类型检查。

// 这是AbsynTree类的SemAnalyze方法，用于启动语义分析过程。
void AbsynTree::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                           err::ErrorMsg *errormsg) const {
  // root_是抽象语法树的根节点，它调用自身的SemAnalyze方法来对整个语法树进行语义分析。
  root_->SemAnalyze(venv, tenv, 0, errormsg);
}

//简单变量
type::Ty *SimpleVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  env::EnvEntry* entry = venv->Look(sym_);
  if (!entry || typeid(*entry) != typeid(env::VarEntry)) {
    errormsg->Error(pos_, "undefined variable %s", sym_->Name().data());
    return type::IntTy::Instance();
  }
  return (static_cast<env::VarEntry*>(entry))->ty_->ActualTy();
}

//记录字段变量
/*
let
    type Person = {name: string, age: int}
    var p := Person {name = "Alice", age = 30}
in
    p.name
end

分析记录变量的类型：
var_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy() 获取记录变量 var_ 的实际类型。如果 var_ 是 p，则应返回 Person 类型。
检查记录类型：
确认 varTy 是 RecordTy 类型。如果不是，则报告错误 "not a record type" 并返回默认类型 IntTy。
查找字段：
遍历 RecordTy 的字段列表，查找名称与 sym_ 匹配的字段。如果找到匹配字段，则返回字段的类型。
*/
type::Ty *FieldVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  type::Ty* varTy;

  varTy = var_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (typeid(*varTy) != typeid(type::RecordTy)) {
    errormsg->Error(var_->pos_, "not a record type");
    return type::IntTy::Instance();
  }

  type::FieldList* fieldList = static_cast<type::RecordTy*>(varTy)->fields_;
  for (type::Field* field : fieldList->GetList()) {
    if (field->name_->Name() == sym_->Name()) {
      return field->ty_->ActualTy();
    }
  }

  errormsg->Error(pos_, "field %s doesn't exist", sym_->Name().data());
  return type::IntTy::Instance();

}

//对数组元素的访问,比如 array[index]
type::Ty *SubscriptVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   int labelcount,
                                   err::ErrorMsg *errormsg) const {
  type::Ty* varTy, * subscriptTy;

  //检查变量类型
  varTy = var_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (typeid(*varTy) != typeid(type::ArrayTy)) {
    errormsg->Error(var_->pos_, "array type required");
    return type::IntTy::Instance();
  }

  //检查下标类型
  subscriptTy = subscript_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (typeid(*subscriptTy) != typeid(type::IntTy)) {
    errormsg->Error(subscript_->pos_, "ARRAY can only be subscripted by INT.");
    return type::IntTy::Instance();
  }

  //返回数组元素类型
  return static_cast<type::ArrayTy*>(varTy)->ty_;
}

type::Ty *VarExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  return var_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
}

type::Ty *NilExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  return type::NilTy::Instance();
}

type::Ty *IntExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  return type::IntTy::Instance();
}

type::Ty *StringExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  return type::StringTy::Instance();
}

//函数调用表达式
type::Ty *CallExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  env::EnvEntry* entry;
  env::FunEntry* funcEnt;
  type::TyList* formalList;

  //在变量环境中查找函数
  entry = venv->Look(func_);
  if (!entry || typeid(*entry) != typeid(env::FunEntry)) {
    errormsg->Error(pos_, "undefined function %s", func_->Name().data());
    return type::VoidTy::Instance();
  }
  
  //获取形式参数：获取函数的形式参数列表
  funcEnt = static_cast<env::FunEntry*>(entry);
  formalList = funcEnt->formals_;

  // 检查参数类型：
  // 遍历实际参数和形式参数，检查每个参数的类型是否匹配。如果类型不匹配，报告错误并返回 VoidTy 类型。
  auto argIt = args_->GetList().begin();
  auto formalIt = formalList->GetList().begin();
  while (argIt != args_->GetList().end() && formalIt != formalList->GetList().end()) {
    type::Ty* argTy, * formalTy;
    formalTy = (*formalIt)->ActualTy();
    argTy = (*argIt)->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
    if (!argTy->IsSameType(formalTy)) {
      errormsg->Error((*argIt)->pos_, "para type mismatch");
      return type::VoidTy::Instance();
    }
    argIt++;
    formalIt++;
  }

  //检查参数数量：检查实际参数和形式参数的数量是否匹配。
  if (argIt != args_->GetList().end() || formalIt != formalList->GetList().end()) {
    int errorpos = argIt == args_->GetList().end() ? args_->GetList().back()->pos_ : (*argIt)->pos_;
    errormsg->Error(errorpos, "too many params in function %s", func_->Name().data());
    return type::VoidTy::Instance();
  }

  //返回函数返回类型：如果所有检查通过，返回函数的返回类型。
  if (!funcEnt->result_)
    return type::VoidTy::Instance();

  return funcEnt->result_->ActualTy();
}

//操作表达式，例如加法、减法、乘法和除法。
type::Ty *OpExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  //分析左右操作数类型
  type::Ty *leftTy = left_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  type::Ty *rightTy = right_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();

  //检查操作数类型
  // 对于加法、减法、乘法和除法操作，检查左右操作数是否都是整数类型。
  // 对于其他操作（如比较操作），检查左右操作数的类型是否相同
  if (oper_ == absyn::PLUS_OP || oper_ == absyn::MINUS_OP 
    || oper_ == absyn::TIMES_OP || oper_==absyn::DIVIDE_OP) {
      if (typeid(*leftTy) != typeid(type::IntTy)) 
        errormsg->Error(left_->pos_, "integer required");
      if (typeid(*rightTy) != typeid(type::IntTy)) 
        errormsg->Error(left_->pos_, "integer required");
      return type::IntTy::Instance();
  } else {
    if (!leftTy->IsSameType(rightTy)) {
      errormsg->Error(left_->pos_, "same type required");
      return type::IntTy::Instance();
    }
    return type::IntTy::Instance();
  }
}

//表示记录类型的表达式
type::Ty *RecordExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  //查找类型定义
  type::Ty* ty = tenv->Look(typ_);
  if (!ty) {
    errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
    return type::VoidTy::Instance();
  }
  return ty;
}

//顺序表达式，即一系列表达式按顺序执行。
type::Ty *SeqExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  //分析每个表达式：依次分析 seq_ 列表中的每个表达式，并获取它们的类型。
  type::Ty* ty;
  for (Exp* exp : seq_->GetList())
    ty = exp->SemAnalyze(venv, tenv, labelcount, errormsg);
  //返回最后一个表达式的类型：
  return ty;
}

//赋值表达式
type::Ty *AssignExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  //分析变量和表达式的类型
  type::Ty* varTy = var_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  type::Ty* expTy = exp_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();

  //检查类型匹配
  if (!varTy->IsSameType(expTy)) {
    errormsg->Error(exp_->pos_, "unmatched assign exp");
    return type::VoidTy::Instance();
  }

  // 检查是否为只读变量
  if (typeid(*var_) == typeid(SimpleVar)) {
    SimpleVar* simpVar = static_cast<SimpleVar*>(var_);
    if (venv->Look(simpVar->sym_)->readonly_) {
      errormsg->Error(var_->pos_, "loop variable can't be assigned");
      return type::VoidTy::Instance();
    }
  }

  return type::VoidTy::Instance();
}

//if 表达式
type::Ty *IfExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  type::Ty* testTy, * thenTy, * elseTy;

  //对 if 条件进行语义分析，确保其类型为 IntTy（整数类型）
  testTy = test_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (typeid(*testTy) != typeid(type::IntTy)) {
    errormsg->Error(test_->pos_, "integer required for if test condition");
    return type::VoidTy::Instance();
  }

  //对 then 分支进行语义分析。如果没有 else 分支且 then 分支的类型不是 VoidTy，则报告错误。
  thenTy = then_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();

  if (!elsee_ && typeid(*thenTy) != typeid(type::VoidTy)) {
    errormsg->Error(then_->pos_, "if-then exp's body must produce no value");
    return type::VoidTy::Instance();
  }

  //如果存在 else 分支，对其进行语义分析并检查 then 和 else 分支的类型是否一致。
  if (elsee_) {
    elseTy = elsee_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
    if (!thenTy->IsSameType(elseTy)) {
      errormsg->Error(elsee_->pos_, "then exp and else exp type mismatch");
      return thenTy;
    }
  }

  return thenTy;
}

// while 表达式
type::Ty *WhileExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  //为 while 循环创建新的作用域。
  venv->BeginScope();
  tenv->BeginScope();

  type::Ty* testTy, * bodyTy;

  //对 while 条件进行语义分析，确保其类型为 IntTy（整数类型）。
  testTy = test_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (typeid(*testTy) != typeid(type::IntTy)) {
    errormsg->Error(test_->pos_, "while test condition must produce INT value.");
    return type::VoidTy::Instance();
  }

  //对循环体进行语义分析，确保其类型为 VoidTy（无返回值）。
  bodyTy = body_->SemAnalyze(venv, tenv, -1, errormsg)->ActualTy();
  if (typeid(*bodyTy) != typeid(type::VoidTy)) {
    errormsg->Error(body_->pos_, "while body must produce no value");
    return type::VoidTy::Instance();
  }

  //结束作用域
  venv->EndScope();
  tenv->EndScope();

  return type::VoidTy::Instance();
}

//for 表达式
type::Ty *ForExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  type::Ty* loTy, * hiTy, * bodyTy;

  //开始新的作用域
  venv->BeginScope();
  tenv->BeginScope();

  //将 for 循环变量绑定为整数类型，并标记为只读。因为在 for 循环中，循环变量不能被重新赋值。
  venv->Enter(var_, new env::VarEntry(type::IntTy::Instance(), true));

  //对 for 循环的起始值下界lo和结束值上界hi进行语义分析，确保它们的类型为 IntTy（整数类型）。
  loTy = lo_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (typeid(*loTy) != typeid(type::IntTy)) {
    errormsg->Error(lo_->pos_, "for exp's range type is not integer");
    // venv->EndScope();
    // return type::VoidTy::Instance();
  }

  hiTy = hi_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  if (typeid(*hiTy) != typeid(type::IntTy)) {
    errormsg->Error(hi_->pos_, "for exp's range type is not integer");
    // venv->EndScope();
    // return type::VoidTy::Instance();
  }

  //对 for 循环体进行语义分析，确保其类型为 VoidTy（无返回值）
  bodyTy = body_->SemAnalyze(venv, tenv, -1, errormsg)->ActualTy();
  if (typeid(*bodyTy) != typeid(type::VoidTy)) {
    errormsg->Error(body_->pos_, "for body must produce no value.");
    venv->EndScope();
    return type::VoidTy::Instance();
  }

  // /结束作用域
  venv->EndScope();
  tenv->EndScope();

  return type::VoidTy::Instance();
}

type::Ty *BreakExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  //检查 break 表达式是否位于循环内部。
  if (labelcount != -1)
    errormsg->Error(pos_, "break is not inside any loop");
  return type::VoidTy::Instance();
}

//let 表达式
/*
eg.
let
  var x := 10
  var y := 20
in
  x + y
end

为 let 表达式创建新的作用域。这意味着在 let 表达式中的变量和声明只在 let 表达式内部可见。

语义分析将检查 x 的类型和初始值是否匹配。这里，x 被声明为整数并赋值为 10。
语义分析将检查 y 的类型和初始值是否匹配。这里，y 被声明为整数并赋值为 20。

对 let 表达式的主体部分进行语义分析。
在示例中，主体部分是 x + y。语义分析将检查 x 和 y 的类型，并确保它们可以进行加法运算。
*/
type::Ty *LetExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  //开始新的作用域
  venv->BeginScope();
  tenv->BeginScope();

  //对 let 表达式中的声明部分进行语义分析。
  for (Dec* dec : decs_->GetList())
    dec->SemAnalyze(venv, tenv, labelcount, errormsg);

  //主体部分分析
  type::Ty *result = body_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();

  //结束作用域
  venv->EndScope();
  tenv->EndScope();

  return result;
}

type::Ty *ArrayExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  type::Ty* arrayTy, * sizeTy, * initTy;

  //检查获取到的数组类型是否存在，并且确保它是数组类型。
  arrayTy = tenv->Look(typ_);
  if (!arrayTy || typeid(*arrayTy->ActualTy()) != typeid(type::ArrayTy)) {
    errormsg->Error(pos_, "no array type %s", typ_->Name().data());
    return type::IntTy::Instance();
  }

  //对数组大小表达式进行语义分析，并检查其是否为整数类型。
  sizeTy = size_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typeid(*sizeTy) != typeid(type::IntTy)) {
    errormsg->Error(size_->pos_, "integer required for array size");
    return arrayTy;
  }

  //对数组初始化表达式进行语义分析，并检查其类型是否与数组元素类型匹配。
  initTy = init_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (initTy->ActualTy() != static_cast<type::ArrayTy*>(arrayTy->ActualTy())->ty_->ActualTy()) {
    errormsg->Error(init_->pos_, "type mismatch");
    return arrayTy;
  }

  return arrayTy;
  
}

//空表达式
type::Ty *VoidExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  return type::VoidTy::Instance();
}

//p85,函数声明
void FunctionDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  type::Ty* resultTy, * bodyTy;
  type::TyList* formals;
  unordered_map<string, int> functionRecord;

  //  参数类型是否合法
  // 第一次遍历函数列表，检查重复定义并记录函数签名，确保没有重复的函数名，并将函数签名添加到变量环境 venv
  for (FunDec* function : functions_->GetList()) {
    if (functionRecord.count(function->name_->Name())) {
      errormsg->Error(function->pos_, "two functions have the same name");
      return;
    }
    
    //使用 functionRecord 记录每个函数的名称。
    functionRecord[function->name_->Name()] = 1;

    /*通过 tenv->Look 获取函数返回类型，
    并通过 function->params_->MakeFormalTyList 获取函数参数列表类型。
    然后将函数名称及其签名（参数类型列表和返回类型）添加到变量环境 venv 中。*/
    resultTy = function->result_ ? tenv->Look(function->result_) : nullptr;
    formals = function->params_->MakeFormalTyList(tenv, errormsg);
    venv->Enter(function->name_, new env::FunEntry(formals, resultTy));
  }

  for (FunDec* function : functions_->GetList()) {
    resultTy = function->result_ ? tenv->Look(function->result_) : nullptr;
    formals = function->params_->MakeFormalTyList(tenv, errormsg);
    
    //进入新的作用域：
    venv->BeginScope();

    //将函数参数添加到变量环境 venv 中，使其在函数体中可见。
    auto paramIt = function->params_->GetList().begin();
    auto formalIt = formals->GetList().begin();
    for (; formalIt != formals->GetList().end(); paramIt++, formalIt++) {
      venv->Enter((*paramIt)->name_, new env::VarEntry(*formalIt));
    }

    //  返回值类型是否合法
    //  函数体进行语义分析，并检查函数体返回类型是否与函数声明的返回类型匹配。
    // 如果函数没有声明返回类型（即过程），确保函数体返回 void 类型。
    // 如果函数声明了返回类型，确保函数体返回类型与声明的返回类型匹配。
    bodyTy = function->body_->SemAnalyze(venv, tenv, labelcount, errormsg);
    if (!function->result_ && typeid(*bodyTy) != typeid(type::VoidTy)) {
      errormsg->Error(function->body_->pos_, "procedure returns value");
    } else if (function->result_ && !bodyTy->IsSameType(resultTy)) {
      errormsg->Error(function->body_->pos_, "wrong function return type");
    }

    //结束作用域
    venv->EndScope();
  }
}

//变量声明
void VarDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                        err::ErrorMsg *errormsg) const {
  type::Ty* ty, * initTy;

  //处理显式指定的变量类型
  //如果显式指定了变量类型 (typ_)，则在类型环境 (tenv) 中查找该类型。
  if (typ_) {
    ty = tenv->Look(typ_);
    if (!ty) {
      errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
      return;
    }
  }

  //分析初始化表达式的类型
  /*
  如果初始化表达式的类型为 Nil，且未显式指定变量类型或指定的变量类型不是 Record 类型，报告错误并返回。
  如果显式指定了变量类型但初始化表达式的类型与其不匹配，报告错误并返回。
  */
  initTy = init_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typeid(*initTy) == typeid(type::NilTy) 
    && (!typ_ || typeid(*ty->ActualTy()) != typeid(type::RecordTy))) {
      errormsg->Error(init_->pos_, "init should not be nil without type specified");
      return;
  } else if (typ_ && !(initTy->IsSameType(ty))) {
    errormsg->Error(init_->pos_, "type mismatch");
    return;
  }

  //添加变量到变量环境
  venv->Enter(var_, new env::VarEntry(initTy));
}

//类型声明
/*
type
  List = array of Integer;
  Node = { value: Integer, next: ^Node }
  Tree = { data: Integer, left: ^Tree, right: ^Tree }

*/
void TypeDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                         err::ErrorMsg *errormsg) const {
  type::Ty* ty;
  type::NameTy* tenvTy;
  unordered_map<string, int> typeRecord;

  //第一次遍历类型声明列表,使用 typeRecord 记录每个类型的名称。如果发现重复定义，报告错误并返回。
  //List、Node和Tree是唯一的，没有重复定义
  //将类型名称及其初始类型（NULL）添加到类型环境 (tenv) 中。
  //将这些类型名称（List、Node、Tree）及其初始类型（NULL）添加到类型环境中。
  for (NameAndTy* nameAndTy : types_->GetList()) {
    if (typeRecord.count(nameAndTy->name_->Name())) {
      errormsg->Error(nameAndTy->ty_->pos_, "two types have the same name");
      return;
    }
    typeRecord[nameAndTy->name_->Name()] = 1;
    tenv->Enter(nameAndTy->name_, new type::NameTy(nameAndTy->name_, NULL));
  }

  //第二次遍历类型声明列表,通过类型声明的 SemAnalyze 方法分析类型定义，
  //查找类型环境中的类型名称（List、Node、Tree），并对其进行语义分析。
  //每个类型的定义将被解析为相应的类型结构，并将其更新到类型环境中,并将其更新到类型环境中。
  for (NameAndTy* nameAndTy : types_->GetList()) {
    ty = tenv->Look(nameAndTy->name_);
    if (!ty) {
      errormsg->Error(nameAndTy->ty_->pos_, "undefined type %s", nameAndTy->name_->Name().data());
      return;
    }
    tenvTy = static_cast<type::NameTy*>(ty);
    tenvTy->ty_ = nameAndTy->ty_->SemAnalyze(tenv, errormsg);
    tenv->Set(nameAndTy->name_, tenvTy);
  }

  //处理类型别名链,遍历类型别名链，检查是否存在循环定义。如果发现类型循环，报告错误并返回。
  //在例子中，Node类型包含指向Node类型的指针，Tree类型也包含指向Tree类型的指针。这种情况下，我们会报告类型循环错误。
  for (NameAndTy* nameAndTy : types_->GetList()) {
    ty = static_cast<type::NameTy*>(tenv->Look(nameAndTy->name_))->ty_;
    while (typeid(*ty) == typeid(type::NameTy)) {
      tenvTy = static_cast<type::NameTy*>(ty);
      if (tenvTy->sym_->Name() == nameAndTy->name_->Name()) {
        errormsg->Error(nameAndTy->ty_->pos_, "illegal type cycle");
        return;
      }
      ty = tenvTy->ty_;
    }
  }
}

//对另一个类型的别名或引用
type::Ty *NameTy::SemAnalyze(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  //在类型环境（tenv）中查找给定名称关联的类型来进行分析
  type::Ty* ty = tenv->Look(name_);
  return ty;
}

//记录类型
type::Ty *RecordTy::SemAnalyze(env::TEnvPtr tenv,
                               err::ErrorMsg *errormsg) const {
  //创建记录类型的字段列表，分析记录定义中的字段类型和名称。
  type::FieldList* fieldList = record_->MakeFieldList(tenv, errormsg);
  return new type::RecordTy(fieldList);
}

//数组类型
type::Ty *ArrayTy::SemAnalyze(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  //在类型环境中查找与数组元素类型关联的类型
  return new type::ArrayTy(tenv->Look(array_));
}

} // namespace absyn

namespace sem {

void ProgSem::SemAnalyze() {
  FillBaseVEnv();
  FillBaseTEnv();
  absyn_tree_->SemAnalyze(venv_.get(), tenv_.get(), errormsg_.get());
}

} // namespace tr
