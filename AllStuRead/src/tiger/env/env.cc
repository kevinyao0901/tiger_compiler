#include "tiger/env/env.h"
#include "tiger/translate/translate.h"
#include "tiger/semant/semant.h"

/*定义了两个命名空间 sem 和 tr，分别用于语义分析和翻译过程中的环境填充函数。每个命名空间都包含两个函数
sem::ProgSem 类：

FillBaseTEnv：填充基本类型环境，将基本类型 int 和 string 添加到类型环境 tenv_ 中。
FillBaseVEnv：填充基本变量环境，将一系列预定义的函数（如 flush、exit、chr 等）添加到变量环境 venv_ 中。

tr::ProgTr 类：

FillBaseTEnv：同样填充基本类型环境，将基本类型 int 和 string 添加到类型环境 tenv_ 中。
FillBaseVEnv：填充基本变量环境，将一系列预定义的函数（如 flush、exit、chr 等）添加到变量环境 venv_ 中，并且在 FunEntry 的构造函数中多传入了 level 和 label 参数，用于处理翻译过程中函数的层次和标签信息。
*/
namespace sem {
void ProgSem::FillBaseTEnv() {
    /*p79,p81,tiger has two basic types(int & string)*/
  tenv_->Enter(sym::Symbol::UniqueSymbol("int"), type::IntTy::Instance());
  tenv_->Enter(sym::Symbol::UniqueSymbol("string"), type::StringTy::Instance());
}

void ProgSem::FillBaseVEnv() {
  type::Ty *result;
  type::TyList *formals;

  venv_->Enter(sym::Symbol::UniqueSymbol("flush"),
               new env::FunEntry(new type::TyList(), type::VoidTy::Instance()));

  formals = new type::TyList(type::IntTy::Instance());

  venv_->Enter(
      sym::Symbol::UniqueSymbol("exit"),
      new env::FunEntry(formals, type::VoidTy::Instance()));

  result = type::StringTy::Instance();

  venv_->Enter(sym::Symbol::UniqueSymbol("chr"),
               new env::FunEntry(formals, result));

  venv_->Enter(sym::Symbol::UniqueSymbol("getchar"),
               new env::FunEntry(new type::TyList(), result));

  formals = new type::TyList(type::StringTy::Instance());

  venv_->Enter(
      sym::Symbol::UniqueSymbol("print"),
      new env::FunEntry(formals, type::VoidTy::Instance()));
  venv_->Enter(sym::Symbol::UniqueSymbol("printi"),
               new env::FunEntry(new type::TyList(type::IntTy::Instance()),
                                 type::VoidTy::Instance()));

  result = type::IntTy::Instance();
  venv_->Enter(sym::Symbol::UniqueSymbol("ord"),
               new env::FunEntry(formals, result));

  venv_->Enter(sym::Symbol::UniqueSymbol("size"),
               new env::FunEntry(formals, result));

  result = type::StringTy::Instance();
  formals = new type::TyList(
      {type::StringTy::Instance(), type::StringTy::Instance()});
  venv_->Enter(sym::Symbol::UniqueSymbol("concat"),
               new env::FunEntry(formals, result));

  formals =
      new type::TyList({type::StringTy::Instance(), type::IntTy::Instance(),
                        type::IntTy::Instance()});
  venv_->Enter(sym::Symbol::UniqueSymbol("substring"),
               new env::FunEntry(formals, result));

}

} // namespace sem

namespace tr {

void ProgTr::FillBaseTEnv() {
  tenv_->Enter(sym::Symbol::UniqueSymbol("int"), type::IntTy::Instance());
  tenv_->Enter(sym::Symbol::UniqueSymbol("string"), type::StringTy::Instance());
}

void ProgTr::FillBaseVEnv() {
  type::Ty *result;
  type::TyList *formals;

  temp::Label *label = nullptr;
  tr::Level *level = main_level_.get();

  venv_->Enter(sym::Symbol::UniqueSymbol("flush"),
               new env::FunEntry(level, label, new type::TyList(),
                                 type::VoidTy::Instance()));

  formals = new type::TyList(type::IntTy::Instance());

  venv_->Enter(
      sym::Symbol::UniqueSymbol("exit"),
      new env::FunEntry(level, label, formals, type::VoidTy::Instance()));

  result = type::StringTy::Instance();

  venv_->Enter(sym::Symbol::UniqueSymbol("chr"),
               new env::FunEntry(level, label, formals, result));

  venv_->Enter(sym::Symbol::UniqueSymbol("getchar"),
               new env::FunEntry(level, label, new type::TyList(), result));

  formals = new type::TyList(type::StringTy::Instance());

  venv_->Enter(
      sym::Symbol::UniqueSymbol("print"),
      new env::FunEntry(level, label, formals, type::VoidTy::Instance()));
  venv_->Enter(sym::Symbol::UniqueSymbol("printi"),
               new env::FunEntry(level, label,
                                 new type::TyList(type::IntTy::Instance()),
                                 type::VoidTy::Instance()));

  result = type::IntTy::Instance();
  venv_->Enter(sym::Symbol::UniqueSymbol("ord"),
               new env::FunEntry(level, label, formals, result));

  venv_->Enter(sym::Symbol::UniqueSymbol("size"),
               new env::FunEntry(level, label, formals, result));

  result = type::StringTy::Instance();
  formals = new type::TyList(
      {type::StringTy::Instance(), type::StringTy::Instance()});
  venv_->Enter(sym::Symbol::UniqueSymbol("concat"),
               new env::FunEntry(level, label, formals, result));

  formals =
      new type::TyList({type::StringTy::Instance(), type::IntTy::Instance(),
                        type::IntTy::Instance()});
  venv_->Enter(sym::Symbol::UniqueSymbol("substring"),
               new env::FunEntry(level, label, formals, result));

}

} // namespace tr
