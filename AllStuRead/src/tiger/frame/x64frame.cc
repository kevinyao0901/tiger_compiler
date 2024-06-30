#include "tiger/frame/x64frame.h"

#include <iostream>
#include <sstream>

//用于管理 x64 平台上的寄存器和栈帧

//声明一个外部的寄存器管理器指针
extern frame::RegManager *reg_manager;

namespace frame {

//管理 x64 平台上的寄存器
std::unordered_map<X64RegManager::Register, std::string> X64RegManager::reg_str = 
  std::unordered_map<X64RegManager::Register, std::string>{
    { RAX, "%rax" },
    { RBX, "%rbx" },
    { RCX, "%rcx" },
    { RDX, "%rdx" },
    { RSI, "%rsi" },
    { RDI, "%rdi" },
    { R8, "%r8" },
    { R9, "%r9" },
    { R10, "%r10" },
    { R11, "%r11" },
    { R12, "%r12" },
    { R13, "%r13" },
    { R14, "%r14" },
    { R15, "%r15" },
    { RBP, "%rbp" },
    { RSP, "%rsp" },
  };
  
//构造函数，初始化所有寄存器并将其存储在 regs_ 向量中，同时将每个寄存器映射到其字符串表示
X64RegManager::X64RegManager() {
  for (int i = 0; i < REG_COUNT; ++i) {
    temp::Temp *new_reg = temp::TempFactory::NewTemp();
    regs_.push_back(new_reg);
    temp_map_->Enter(new_reg, &reg_str.at(static_cast<Register>(i)));
  }
}

//返回一个包含所有寄存器的 TempList 对象
temp::TempList *X64RegManager::Registers() {
  temp::TempList *temps = new temp::TempList();
  for (int reg = 0; reg < REG_COUNT; ++reg) {
      temps->Append(regs_.at(reg));
  }
  return temps;
}

//返回一个包含函数参数寄存器的 TempList 对象
temp::TempList *X64RegManager::ArgRegs() {
  temp::TempList *temps = new temp::TempList({
    regs_.at(RDI),
    regs_.at(RSI),
    regs_.at(RDX),
    regs_.at(RCX),
    regs_.at(R8),
    regs_.at(R9),
  });
  return temps;
}

//CallerSaves()：返回一个包含调用者保存寄存器的 TempList 对象
temp::TempList *X64RegManager::CallerSaves() {
  temp::TempList *temps = new temp::TempList({
    regs_.at(RAX),
    regs_.at(RDI),
    regs_.at(RSI),
    regs_.at(RDX),
    regs_.at(RCX),
    regs_.at(R8),
    regs_.at(R9),
    regs_.at(R10),
    regs_.at(R11),
  });
  return temps;
}

//CalleeSaves()：返回一个包含被调用者保存寄存器的 TempList 对象
temp::TempList *X64RegManager::CalleeSaves() {
  temp::TempList *temps = new temp::TempList({
    regs_.at(RBX), 
    regs_.at(RBP), 
    regs_.at(R12),
    regs_.at(R13), 
    regs_.at(R14), 
    regs_.at(R15),
  });
  return temps;
}

//ReturnSink()：返回一个包含返回值寄存器和栈指针的 TempList 对象
temp::TempList *X64RegManager::ReturnSink() {
  temp::TempList *temps = CalleeSaves();
  temps->Append(regs_.at(RAX));
  temps->Append(regs_.at(RSP));
  return temps;
}


//返回相应的寄存器或常量值
temp::Temp *X64RegManager::FramePointer() {
  return regs_.at(RBP);
}

temp::Temp *X64RegManager::StackPointer() {
  return regs_.at(RSP);
}

temp::Temp *X64RegManager::ReturnValue() {
  return regs_.at(RAX);
}

temp::Temp *X64RegManager::ArithmeticAssistant() {
  return regs_.at(RDX);
}

int X64RegManager::WordSize() {
  return WORD_SIZE;
}

int X64RegManager::RegCount() {
  return REG_COUNT;
}

//表示在栈帧中的访问,继承自 Access(translate.h)，用于表示在栈帧中的访问
class InFrameAccess : public Access {
public:
  int offset;

  explicit InFrameAccess(int offset) : offset(offset) {}
  
  //生成访问该栈帧中变量的字符串表示
  std::string MunchAccess(Frame *frame) override {
    std::stringstream ss;
    ss << "(" << frame->frame_size_->Name() << "-" << offset << ")("
       << *reg_manager->temp_map_->Look(reg_manager->StackPointer()) << ")";
    return ss.str();
  }
};

//表示在寄存器中的访问,继承自 Access，用于表示在寄存器中的访问
class InRegAccess : public Access {
public:
  temp::Temp *reg;

  explicit InRegAccess(temp::Temp *reg) : reg(reg) {}

  //生成访问该寄存器中变量的字符串表示
  std::string MunchAccess(Frame *frame) override {
    return *temp::Map::Name()->Look(reg);
  }
};

// x64 平台上的栈帧,继承自 Frame，用于表示 x64 平台上的栈帧
class X64Frame : public Frame {
public:
  X64Frame(temp::Label *name) : Frame(name) {
    word_size_ = X64_WORD_SIZE;
  }

  Access *AllocLocal(bool escape) override;
  tree::Exp *FrameAddress() const override;
  int WordSize() const override;
  std::string GetLabel() const override;
  tree::Exp *StackOffset(int frame_offset) const override;

};

//分配一个本地变量，如果 escape 为 true，则分配在栈帧中，否则分配在寄存器中。
Access *X64Frame::AllocLocal(bool escape) {
  Access *access;
  if (escape) {
    local_count_++;
    access = new InFrameAccess(local_count_ * word_size_);
  } else {
    access = new InRegAccess(temp::TempFactory::NewTemp());
  }
  local_access_.push_back(access);
  return access;
}

//返回帧地址的表达式
tree::Exp *X64Frame::FrameAddress() const {
  return new tree::BinopExp(tree::PLUS_OP, 
          new tree::TempExp(reg_manager->StackPointer()), new tree::NameExp(frame_size_));
}

int X64Frame::WordSize() const {
  return word_size_;
}

std::string X64Frame::GetLabel() const {
  return name_->Name();
}

//返回栈偏移量的表达式
tree::Exp *X64Frame::StackOffset(int frame_offset) const {
  return new tree::BinopExp(tree::MINUS_OP, 
          new tree::NameExp(frame_size_), new tree::ConstExp(frame_offset));
}

//用于创建一个新的 X64Frame 对象
Frame *NewFrame(temp::Label *name, std::vector<bool> formals) {
  
  Frame* frame = new X64Frame(name);//创建一个 X64Frame 对象。
  int frame_offset = frame->WordSize();//初始化 frame_offset 为 WordSize
  frame->frame_size_ = temp::LabelFactory::NamedLabel(name->Name() + "_framesize");//创建一个帧大小的标签

  //创建一个新的临时寄存器来存储帧指针
  tree::TempExp *fp_exp = new tree::TempExp(temp::TempFactory::NewTemp());
  //创建视图转换语句，将帧指针存储到该临时寄存器
  frame->view_shift = new tree::MoveStm(fp_exp, frame->FrameAddress());

  tree::Exp *dst_exp;
  tree::Stm *single_view_shift;
  //处理形参
  int arg_reg_count = reg_manager->ArgRegs()->GetList().size();
  tree::Exp *fp_exp_copy; 

  // Formals
  //如果形参数量大于寄存器数量，则创建一个新的临时寄存器来存储帧地址
  if (formals.size() > arg_reg_count) {
    fp_exp_copy = new tree::TempExp(temp::TempFactory::NewTemp());
    frame->view_shift = new tree::SeqStm(frame->view_shift, new tree::MoveStm(fp_exp_copy, frame->FrameAddress()));
  }

  //遍历形参列表，根据是否需要逃逸分配内存
  for (int i = 0; i < formals.size(); ++i) {
    if (formals.at(i)) {  // escape
      //逃逸的参数在栈帧中分配，并更新 frame_offset
      frame->formal_access_.push_back(new InFrameAccess(frame_offset));
      dst_exp = new tree::MemExp(new tree::BinopExp(tree::MINUS_OP, 
                  fp_exp, new tree::ConstExp((i + 1) * frame->WordSize())));
      frame_offset += frame->WordSize();
      frame->local_count_++;
    } else {
      //不逃逸的参数在寄存器中分配
      temp::Temp *reg = temp::TempFactory::NewTemp();
      frame->formal_access_.push_back(new InRegAccess(reg));
      dst_exp = new tree::TempExp(reg);
    }
    
    //根据参数位置，创建相应的视图转换语句
    if (i < arg_reg_count) {
      single_view_shift = new tree::MoveStm(dst_exp, new tree::TempExp(reg_manager->ArgRegs()->NthTemp(i)));
    } else {
      // *fp is return address
      single_view_shift = new tree::MoveStm(dst_exp, new tree::MemExp(
                            new tree::BinopExp(tree::PLUS_OP, fp_exp_copy, 
                              new tree::ConstExp((i - arg_reg_count + 1) * frame->WordSize()))));
    }
    frame->view_shift = new tree::SeqStm(frame->view_shift, single_view_shift);
  }

  // Save and restore callee-save registers 
  // Now save in temporary registers (spilled by register allocator)
  // 获取所有被调用者保存的寄存器列表
  temp::TempList *callee_saves = reg_manager->CalleeSaves();
  temp::Temp *store_reg;
  tree::Stm *single_save;
  tree::Stm *single_restore;


  store_reg = temp::TempFactory::NewTemp();
  frame->save_callee_saves = new tree::MoveStm(new tree::TempExp(store_reg), 
                              new tree::TempExp(callee_saves->GetList().front()));
  frame->restore_callee_saves = new tree::MoveStm(new tree::TempExp(callee_saves->GetList().front()),
                                  new tree::TempExp(store_reg));

  //为每个寄存器创建保存和恢复语句，存储到临时寄存器
  for (auto reg_it = ++callee_saves->GetList().begin(); reg_it != callee_saves->GetList().end(); ++reg_it) {
    store_reg = temp::TempFactory::NewTemp();
    single_save = new tree::MoveStm(new tree::TempExp(store_reg), new tree::TempExp(*reg_it));
    single_restore = new tree::MoveStm(new tree::TempExp(*reg_it), new tree::TempExp(store_reg));
    frame->save_callee_saves = new tree::SeqStm(single_save, frame->save_callee_saves);
    frame->restore_callee_saves = new tree::SeqStm(single_restore, frame->restore_callee_saves);
  }

  return frame;
}

/* Work iff frame is the current frame */
//用于生成一个表达式，用来访问当前栈帧中的一个变量或寄存器
/*
InFrameAccess（变量在栈帧中）：

首先，将 acc 类型转换为 InFrameAccess。
使用 frame->FrameAddress() 获取当前帧地址。
通过一个减法表达式 
tree::BinopExp(tree::MINUS_OP, frame->FrameAddress(), new tree::ConstExp(frame_acc->offset)) 
计算变量在栈中的位置。
将该地址封装在一个内存表达式 tree::MemExp 中。


InRegAccess（变量在寄存器中）：

将 acc 类型转换为 InRegAccess。
使用 reg_acc->reg 直接创建一个寄存器表达式 tree::TempExp
*/


/*如果访问是通过帧内访问（InFrameAccess），它根据帧指针和偏移量计算内存地址。
否则，如果是寄存器内访问（InRegAccess），则返回一个临时寄存器表达式*/
tree::Exp *AccessCurrentExp(Access *acc, Frame *frame) {
  if (typeid(*acc) == typeid(InFrameAccess)) {
    InFrameAccess *frame_acc = static_cast<InFrameAccess *>(acc);

    // access via temporary frame pointer
    return new tree::MemExp(new tree::BinopExp(tree::MINUS_OP, 
            frame->FrameAddress(), new tree::ConstExp(frame_acc->offset)));

  } else {
    InRegAccess *reg_acc = static_cast<InRegAccess *>(acc);
    return new tree::TempExp(reg_acc->reg);
  }
}

/*根据 acc 的类型判断是帧内访问还是寄存器内访问。
如果是帧内访问，则使用 fp 表达式和偏移量计算内存地址，并返回一个 tree::MemExp 表达式。
如果是寄存器内访问，则直接返回一个临时寄存器表达式 tree::TempExp*/
tree::Exp *AccessExp(Access *acc, tree::Exp *fp) {
  if (typeid(*acc) == typeid(InFrameAccess)) {
    InFrameAccess *frame_acc = static_cast<InFrameAccess *>(acc);
    return new tree::MemExp(new tree::BinopExp(tree::MINUS_OP, 
            fp, new tree::ConstExp(frame_acc->offset)));
  } else {
    InRegAccess *reg_acc = static_cast<InRegAccess *>(acc);
    return new tree::TempExp(reg_acc->reg);
  }
}

/*temp::LabelFactory::NamedLabel 获取函数名对应的标签，并构建一个 tree::NameExp 表达式。
将参数列表和函数名表达式传入 tree::CallExp 构造函数，返回一个函数调用表达式。*/
tree::Exp *ExternalCall(std::string s, tree::ExpList *args) {
  return new tree::CallExp(new tree::NameExp(temp::LabelFactory::NamedLabel(s)), args);
}

//返回修改后的函数体 (tree::Stm *)，包含序言和结尾代码
//为了在函数执行过程中正确保存和恢复寄存器的值，以及进行必要的视图转换
tree::Stm *ProcEntryExit1(Frame *frame, tree::Stm *stm) {

  // Concatenate view shift statements as well as save and restore of callee-save registers to 
  // function body.
  /*将保存调用者保存寄存器和视图转换相关的代码 (frame->save_callee_saves, frame->view_shift, frame->restore_callee_saves)
   添加到函数体中，通过 tree::SeqStm 连接这些语句*/
  stm = new tree::SeqStm(frame->save_callee_saves, stm);
  stm = new tree::SeqStm(frame->view_shift, stm);
  stm = new tree::SeqStm(stm, frame->restore_callee_saves);

  return stm;
}

//返回修改后的指令列表 (assem::InstrList *)，通常用于添加返回指令或清理操作
//确保函数执行后能够正确返回或者进行必要的资源释放
assem::InstrList *ProcEntryExit2(assem::InstrList *body) {
  assem::Instr *return_sink = new assem::OperInstr("", nullptr, reg_manager->ReturnSink(), nullptr);
  body->Append(return_sink);
  return body;
}

//返回一个完整的汇编过程 (assem::Proc)，包括序言、函数体和结尾
//计算函数的帧大小，并生成相应的汇编指令，包括设置帧大小标签、函数标签以及分配和释放栈空间的指令
assem::Proc *ProcEntryExit3(Frame *frame, assem::InstrList *body) {
  std::stringstream prologue_ss;
  std::stringstream epilogue_ss;

  // 根据帧的大小计算栈空间大小，并生成序言代码，包括设置帧大小标签、函数标签和分配栈空间
  int fs = (frame->local_count_ + frame->max_outgoing_args_) * frame->WordSize();
  prologue_ss << ".set " << frame->frame_size_->Name() << ", " << fs << "\n"; 

  // 生成结尾代码，包括释放栈空间和返回指令
  prologue_ss << frame->GetLabel() << ":\n";

  // 构造并返回一个 assem::Proc 对象，包含序言、函数体和结尾代码
  if (fs != 0)
    prologue_ss << "subq $" << fs << ", " 
                << *reg_manager->temp_map_->Look(reg_manager->StackPointer())
                << "\n";
  
  // Reset stack pointer (deallocate the frame)
  if (fs != 0)
    epilogue_ss << "addq $" << fs << ", " 
                << *reg_manager->temp_map_->Look(reg_manager->StackPointer())
                << "\n";

  // Return instruction
  epilogue_ss << "retq\n";

  return new assem::Proc(prologue_ss.str(), body, epilogue_ss.str());
}

} // namespace frame