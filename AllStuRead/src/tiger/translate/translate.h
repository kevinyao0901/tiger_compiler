/*
定义了翻译过程中用于管理变量作用域、层级结构和错误处理的基本类。ProgTr 类是翻译过程的核心，
通过初始化环境并调用 Translate 方法将 AST 翻译为 IR。Access 和 Level 类用于管理变量的访问和层级关系，
确保在嵌套函数和变量作用域中的正确性
*/

#ifndef TIGER_TRANSLATE_TRANSLATE_H_
#define TIGER_TRANSLATE_TRANSLATE_H_

#include <list>
#include <memory>

#include "tiger/absyn/absyn.h"
#include "tiger/env/env.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/frame/frame.h"
#include "tiger/semant/types.h"

namespace tr {

class Exp;
class ExpAndTy;
class Level;

class Access {
/*表示对变量或参数的访问*/
public:
  Level *level_;/*指向变量或参数所属的层级*/
  frame::Access *access_;/*指向框架层的访问*/

  Access(Level *level, frame::Access *access)
      : level_(level), access_(access) {}
  static Access *AllocLocal(Level *level, bool escape);/*分配局部变量，escape 指示变量是否需要逃逸。*/
};

class Level {
/*嵌套函数的层级结构，用于管理变量作用域*/
public:
  frame::Frame *frame_;/*当前层级的框架*/
  Level *parent_;/*父层级，用于表示嵌套关系*/

  Level() : frame_(nullptr), parent_(nullptr) {}
  explicit Level(frame::Frame *frame) : frame_(frame), parent_(nullptr) {}
  Level(frame::Frame *frame, Level *parent) : frame_(frame), parent_(parent) {}
};

/*管理整个翻译过程，包括初始化符号表和翻译 AST*/
class ProgTr {
public:
  // TODO: Put your lab5 code here */
  /*初始化 absyn_tree_, errormsg_, tenv_, venv_ 和 main_level_，并填充基础环境。*/
  ProgTr(std::unique_ptr<absyn::AbsynTree> absyn_tree, 
    std::unique_ptr<err::ErrorMsg> errormsg) {
      absyn_tree_ = std::move(absyn_tree);
      errormsg_ = std::move(errormsg);
      tenv_ = std::make_unique<env::TEnv>();
      venv_ = std::make_unique<env::VEnv>();
      main_level_ = std::make_unique<Level>();
      FillBaseVEnv();
      FillBaseTEnv();
    }

  /**
   * Translate IR tree 翻译 IR 树
   */
  void Translate();

  /**
   * Transfer the ownership of errormsg to outer scope
   * 转移 errormsg_ 的所有权
   * @return unique pointer to errormsg
   */
  std::unique_ptr<err::ErrorMsg> TransferErrormsg() {
    return std::move(errormsg_);
  }


private:
  std::unique_ptr<absyn::AbsynTree> absyn_tree_;//抽象语法树
  std::unique_ptr<err::ErrorMsg> errormsg_;//错误信息
  std::unique_ptr<Level> main_level_;//主层级
  std::unique_ptr<env::TEnv> tenv_;//类型环境
  std::unique_ptr<env::VEnv> venv_;//变量环境

  // Fill base symbol for var env and type env 填充基础变量环&基础类型环境
  void FillBaseVEnv();
  void FillBaseTEnv();
};

//处理过程入口和退出的操作，用于在翻译过程中管理函数的进入和退出
void ProcEntryExit(Level *level, Exp *body);

} // namespace tr

#endif
