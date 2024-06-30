#ifndef TIGER_ESCAPE_ESCAPE_H_
#define TIGER_ESCAPE_ESCAPE_H_

#include <memory>

#include "tiger/symbol/symbol.h"

// Forward Declarations
namespace absyn {
class AbsynTree;
} // namespace absyn

namespace esc {

class EscapeEntry {
public:
  int depth_;//当前作用域的深度
  bool *escape_;//一个指向布尔值的指针，表示变量是否逃逸

  EscapeEntry(int depth, bool *escape) : depth_(depth), escape_(escape) {}
};

//存储逃逸分析条目
using EscEnv = sym::Table<esc::EscapeEntry>;
using EscEnvPtr = sym::Table<esc::EscapeEntry> *;

class EscFinder {
public:
  EscFinder() = delete;
  //用抽象语法树初始化 EscFinder 对象，同时创建一个新的逃逸分析环境。
  explicit EscFinder(std::unique_ptr<absyn::AbsynTree> absyn_tree)
      : absyn_tree_(std::move(absyn_tree)), env_(std::make_unique<EscEnv>()) {}

  /**
   * Escape analysis
   */
  //执行逃逸分析的主要方法
  void FindEscape();

  /**
   * Transfer the ownership of absyn tree to outer scope
   * @return unique pointer to the absyn tree
   */
  //将抽象语法树的所有权转移到外部作用域。
  std::unique_ptr<absyn::AbsynTree> TransferAbsynTree() {
    return std::move(absyn_tree_);
  }

private:
  //指向抽象语法树的智能指针
  std::unique_ptr<absyn::AbsynTree> absyn_tree_;
  //指向逃逸分析环境的智能指针
  std::unique_ptr<esc::EscEnv> env_;
};
} // namespace esc

#endif