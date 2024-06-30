#ifndef TIGER_ENV_ENV_H_
#define TIGER_ENV_ENV_H_

#include "tiger/frame/temp.h"
#include "tiger/semant/types.h"
#include "tiger/symbol/symbol.h"

// Forward Declarations
/*tr::Access和tr::Level是用于IR树翻译的类，在这里进行前向声明以避免包含实际定义。*/
namespace tr {
class Access;
class Level;
} // namespace tr

namespace env {
/*EnvEntry是一个基类，表示环境中的条目，具有一个readonly_成员变量，表示该条目是否是只读的。*/
class EnvEntry {
public:
  bool readonly_;

  
  explicit EnvEntry(bool readonly = true) : readonly_(readonly) {}
  virtual ~EnvEntry() = default;
};

class VarEntry : public EnvEntry {
public:
  tr::Access *access_;
  type::Ty *ty_;

  // For lab4(semantic analysis) only
  //VarEntry继承自EnvEntry，用于表示变量条目。
  explicit VarEntry(type::Ty *ty, bool readonly = false)
      : EnvEntry(readonly), ty_(ty), access_(nullptr){};

  // For lab5(translate IR tree)
  //ty_成员变量存储变量的类型。access_成员变量在IR树翻译时使用，表示变量的访问链接。
  VarEntry(tr::Access *access, type::Ty *ty, bool readonly = false)
      : EnvEntry(readonly), ty_(ty), access_(access){};
};

class FunEntry : public EnvEntry {
public:
  //formals_成员变量存储函数的参数类型列表。
  //result_成员变量存储函数的返回类型。
  //level_和label_成员变量在IR树翻译时使用，分别表示函数的嵌套层次和标签。
  tr::Level *level_;
  temp::Label *label_;
  type::TyList *formals_;
  type::Ty *result_;

  // FunEntry继承自EnvEntry，用于表示函数条目。

  //提供了两个构造函数：一个用于语义分析阶段（只需参数类型和返回类型），
  //另一个用于IR树翻译阶段（需要嵌套层次、标签、参数类型和返回类型）。
  // For lab4(semantic analysis) only


  FunEntry(type::TyList *formals, type::Ty *result)
      : formals_(formals), result_(result), level_(nullptr), label_(nullptr) {}

  // For lab5(translate IR tree)
  FunEntry(tr::Level *level, temp::Label *label, type::TyList *formals,
           type::Ty *result)
      : formals_(formals), result_(result), level_(level), label_(label) {}
};

/*
符号表类型定义：

VEnv和TEnv分别是用于存储变量和类型的符号表，使用sym::Table模板类实现。
VEnvPtr和TEnvPtr是指向变量和类型符号表的指针类型。
*/
using VEnv = sym::Table<env::EnvEntry>;
using TEnv = sym::Table<type::Ty>;
using VEnvPtr = sym::Table<env::EnvEntry> *;
using TEnvPtr = sym::Table<type::Ty> *;
} // namespace env

#endif // TIGER_ENV_ENV_H_
