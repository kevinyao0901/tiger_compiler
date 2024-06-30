#ifndef TIGER_CODEGEN_CODEGEN_H_
#define TIGER_CODEGEN_CODEGEN_H_

#include "tiger/canon/canon.h"
#include "tiger/codegen/assem.h"
#include "tiger/frame/x64frame.h"
#include "tiger/translate/tree.h"

// Forward Declarations
namespace frame {
class RegManager;
class Frame;
} // namespace frame

namespace assem {
class Instr;
class InstrList;
} // namespace assem

namespace canon {
class Traces;
} // namespace canon

namespace cg {

class AssemInstr {
public:
  AssemInstr() = delete;
  AssemInstr(nullptr_t) = delete;
  //显式构造函数，用于用 InstrList 初始化 AssemInstr 对象。
  explicit AssemInstr(assem::InstrList *instr_list) : instr_list_(instr_list) {}

  //打印汇编指令
  void Print(FILE *out, temp::Map *map) const;

  //返回指向 InstrList 对象的常量成员函数
  [[nodiscard]] assem::InstrList *GetInstrList() const { return instr_list_; }

private:
  //私有成员变量，用于存储指向 InstrList 对象的指针
  assem::InstrList *instr_list_;
};

class CodeGen {
public:
  //初始化成员变量 frame_ 和 traces_，并通过 frame->frame_size_->Name() 初始化 fs_
  CodeGen(frame::Frame *frame, std::unique_ptr<canon::Traces> traces)
      : frame_(frame), traces_(std::move(traces)) {
        fs_ = frame->frame_size_->Name();
      }

  //成员函数声明，用于生成代码
  void Codegen();
  //转移所有权并返回一个 AssemInstr 对象的唯一指针
  std::unique_ptr<AssemInstr> TransferAssemInstr() {
    return std::move(assem_instr_);
  }

private:
  frame::Frame *frame_; //存储指向 Frame 对象的指针
  std::string fs_; // Frame size label_
  std::unique_ptr<canon::Traces> traces_; //存储指向 Traces 对象的唯一指针
  std::unique_ptr<AssemInstr> assem_instr_; //存储指向 AssemInstr 对象的唯一指针
};

} // namespace cg
#endif
