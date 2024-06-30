#include "tiger/codegen/codegen.h"

#include <cassert>
#include <sstream>
#include <iostream>

//实现了代码生成的主要逻辑，负责将抽象语法树（AST）转换为汇编指令列表
extern frame::RegManager *reg_manager;

namespace {

constexpr int maxlen = 1024;
constexpr int wordsize = 8;

} // namespace

namespace cg {

//将语法树中的语句转换为汇编指令，并将结果存储在 assem_instr_ 中
void CodeGen::Codegen() {
  /* TODO: Put your lab5 code here */
  /*Unsure*/
  auto *stm_list = traces_->GetStmList();//获取traces_对象中的语句列表stm_list
  auto *instr_list = new assem::InstrList();//创建一个新的汇编指令列表

  for (auto *stm : stm_list->GetList()) {
    stm->Munch(*instr_list, fs_);
  }

  // 为instr_list添加伪指令，处理返回寄存器
  instr_list = frame::ProcEntryExit2(instr_list);
  assem_instr_ = std::make_unique<AssemInstr>(instr_list);
  /* End for lab5 code */
}

//将生成的汇编指令打印到指定的输出文件
void AssemInstr::Print(FILE *out, temp::Map *map) const {
  for (auto instr : instr_list_->GetList())
    instr->Print(out, map);
  fprintf(out, "\n");
}
} // namespace cg

namespace tree {

/* TODO: Put your lab5 code here */
/**
 * Generate code for passing arguments
 * @param args argument list
 * @param instr_holder instruction holder
 * @return temp list to hold arguments
 */

/*Unsure*/
//处理内存访问表达式，并生成相应的汇编指令
static assem::MemFetch *MunchMem(tree::Exp* mem_exp, int ordinal, 
                                 assem::InstrList &instr_list, std::string_view fs) {
  //将mem_exp转换为tree::MemExp类型
  auto *mem = static_cast<tree::MemExp *>(mem_exp);
  auto *exp = mem->exp_;//获取内存访问表达式exp
  std::stringstream mem_ss;

  //检查exp是否为PLUS_OP的二元操作表达式bin_exp
  if (auto *bin_exp = dynamic_cast<tree::BinopExp *>(exp); bin_exp && bin_exp->op_ == tree::PLUS_OP) {
    tree::ConstExp *offset_exp = nullptr;
    temp::Temp *base_reg = nullptr;

    //进一步检查bin_exp的左操作数和右操作数是否为常量表达式ConstExp
    ////根据常量表达式的位置，生成基础寄存器base_reg,根据常量表达式的值生成内存访问字符串
    if ((offset_exp = dynamic_cast<tree::ConstExp *>(bin_exp->right_))) {
      base_reg = bin_exp->left_->Munch(instr_list, fs);
    } else if ((offset_exp = dynamic_cast<tree::ConstExp *>(bin_exp->left_))) {
      base_reg = bin_exp->right_->Munch(instr_list, fs);
    } else {
      base_reg = exp->Munch(instr_list, fs);
      mem_ss << "(`s" << ordinal << ")";
      return new assem::MemFetch(mem_ss.str(), new temp::TempList(base_reg));
    }
    //如果exp不是二元操作表达式，直接生成内存寄存器mem_reg
    if (offset_exp->consti_ != 0) {
      mem_ss << offset_exp->consti_;
    }
    mem_ss << "(`s" << ordinal << ")";
    return new assem::MemFetch(mem_ss.str(), new temp::TempList(base_reg));

  } else {
    temp::Temp *mem_reg = exp->Munch(instr_list, fs);
    mem_ss << "(`s" << ordinal << ")";
    return new assem::MemFetch(mem_ss.str(), new temp::TempList(mem_reg));
  }
}

//处理函数调用中的参数传递，将参数表达式转换为相应的汇编指令
temp::TempList *ExpList::MunchArgs(assem::InstrList &instr_list, std::string_view fs) {
  
  /* End for lab5 code */
  /*Unsure*/
  //创建一个新的temp::TempList对象arg_list用于保存参数寄存器
  temp::TempList *arg_list = new temp::TempList();
  //获取可以用于传递参数的寄存器数量arg_reg_count
  int arg_reg_count = reg_manager->ArgRegs()->GetList().size();
  int i = 0;

  //遍历参数列表中的每个参数
  for (tree::Exp *arg : this->GetList()) {  // static link is included
    //如果参数索引i小于可用寄存器数量
    if (i < arg_reg_count) {
      //获取目标寄存器dst_reg
      temp::Temp *dst_reg = reg_manager->ArgRegs()->NthTemp(i);
      //根据参数类型（常量或表达式）生成对应的汇编指令，并添加到instr_list中
      if (auto const_exp = dynamic_cast<tree::ConstExp *>(arg)) {
        instr_list.Append(new assem::OperInstr(
            "movq $" + std::to_string(const_exp->consti_) + ", `d0", 
            new temp::TempList(dst_reg), nullptr, nullptr));
      } else {
        temp::Temp *src_reg = arg->Munch(instr_list, fs);
        instr_list.Append(new assem::MoveInstr(
            "movq `s0, `d0", 
            new temp::TempList(dst_reg), 
            new temp::TempList(src_reg)));
      }
      //将目标寄存器添加到arg_list中
      arg_list->Append(dst_reg);
    } else {  // the first one goes to (%rsp)
      //参数索引i大于或等于可用寄存器数量
      
      //计算参数在栈中的偏移量offset
      std::string offset = (i != arg_reg_count) ? std::to_string((i - arg_reg_count) * wordsize) : "";
      //获取栈指针字符串stack_ptr_str
      std::string stack_ptr_str = *reg_manager->temp_map_->Look(reg_manager->StackPointer());

      //根据参数类型（常量或表达式）生成对应的汇编指令，并添加到instr_list中
      if (auto const_exp = dynamic_cast<tree::ConstExp *>(arg)) {
        instr_list.Append(new assem::OperInstr(
            "movq $" + std::to_string(const_exp->consti_) + ", " + offset + "(" + stack_ptr_str + ")", 
            nullptr, new temp::TempList(reg_manager->StackPointer()), nullptr));
      } else {
        temp::Temp *src_reg = arg->Munch(instr_list, fs);
        instr_list.Append(new assem::OperInstr(
            "movq `s0, " + offset + "(" + stack_ptr_str + ")", 
            nullptr, new temp::TempList({src_reg, reg_manager->StackPointer()}), nullptr));
      }
    }
    ++i;
  }

  //参数数量大于寄存器数量，将栈指针添加到arg_list中
  if (i > arg_reg_count) {
    arg_list->Append(reg_manager->StackPointer());
  }

  return arg_list;
}

//生成顺序语句的汇编指令
void SeqStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  // SeqStm should not exist in codegen phase
  left_->Munch(instr_list, fs);
  right_->Munch(instr_list, fs);
  /* End for lab5 code */
}

//生成标签语句的汇编指令
void LabelStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  instr_list.Append(new assem::LabelInstr(label_->Name(), label_));
  /* End for lab5 code */
}

void JumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  std::string instr_str = "jmp " + exp_->name_->Name();
  instr_list.Append(new assem::OperInstr(instr_str, nullptr, nullptr, 
                      new assem::Targets(jumps_)));
  /* End for lab5 code */
}

//生成条件跳转语句的汇编指令
void CjumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  /*Unsure*/
  //生成左操作数和右操作数的汇编指令，并获取对应的临时寄存器
  std::stringstream instr_ss;
  temp::Temp *left_reg = left_->Munch(instr_list, fs);
  temp::Temp *right_reg = nullptr;
  
  //生成比较指令，并将其添加到instr_list中
  if (typeid(*right_) == typeid(tree::ConstExp)) {
    tree::ConstExp *right_const = static_cast<tree::ConstExp *>(right_);
    instr_ss << "cmpq $" << right_const->consti_ << ", `s0";
    instr_list.Append(new assem::OperInstr(instr_ss.str(), nullptr, 
                        new temp::TempList(left_reg), nullptr));
  } else {
    right_reg = right_->Munch(instr_list, fs);
    instr_list.Append(new assem::OperInstr("cmpq `s0, `s1", nullptr,
                        new temp::TempList({right_reg, left_reg}), nullptr));
  }

  //根据条件操作符op_确定对应的汇编跳转指令字符串
  instr_ss.str("");
  const char *jump_type = nullptr;
  switch (op_) {
    case EQ_OP: jump_type = "je"; break;
    case NE_OP: jump_type = "jne"; break;
    case LT_OP: jump_type = "jl"; break;
    case GT_OP: jump_type = "jg"; break;
    case LE_OP: jump_type = "jle"; break;
    case GE_OP: jump_type = "jge"; break;
    default: return; // error
  }

  //生成条件跳转指令，并将其添加到instr_list中
  instr_ss << jump_type << " " << true_label_->Name();
  instr_list.Append(new assem::OperInstr(instr_ss.str(), nullptr, nullptr,
                      new assem::Targets(new std::vector<temp::Label *>{true_label_})));

  //if (right_reg) temp::Temp::ReleaseTemp(right_reg); // Release temporary right_reg if allocated
  /* End for lab5 code */
}

//赋值语句
void MoveStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  /*Unsure*/
  std::stringstream instr_ss;

  //检查目标表达式dst_是否为MemExp类型
  if (auto dst_mem = dynamic_cast<tree::MemExp*>(dst_)) {  // movq r, M,生成源寄存器和目标寄存器的汇编指令
    temp::Temp *src_reg = src_->Munch(instr_list, fs);
    assem::MemFetch *fetch = MunchMem(dst_mem, 1, instr_list, fs);
    instr_ss << "movq `s0, " << fetch->fetch_;
    temp::TempList *src_regs = new temp::TempList(src_reg);
    src_regs->CatList(fetch->regs_);
    instr_list.Append(new assem::OperInstr(instr_ss.str(), 
                        nullptr, src_regs, nullptr));

  } else if (auto src_mem = dynamic_cast<tree::MemExp*>(src_)) {  // movq M, r,生成源寄存器和目标寄存器的汇编指令
    //如果是，进一步检查源表达式src_是否为MemExp类型
    assem::MemFetch *fetch = MunchMem(src_mem, 0, instr_list, fs);
    temp::Temp *dst_reg = dst_->Munch(instr_list, fs);
    instr_ss << "movq " << fetch->fetch_ << ", `d0";
    instr_list.Append(new assem::OperInstr(instr_ss.str(),
                        new temp::TempList(dst_reg), fetch->regs_, nullptr));

  } else if (auto src_const = dynamic_cast<tree::ConstExp*>(src_)) {  // movq $imm, r,生成源寄存器和目标内存访问的汇编指令
    temp::Temp *dst_reg = dst_->Munch(instr_list, fs);
    instr_ss << "movq $" << src_const->consti_ << ", `d0";
    instr_list.Append(new assem::OperInstr(instr_ss.str(), 
                        new temp::TempList(dst_reg), nullptr, nullptr));

  } else {  // movq r1, r2,源寄存器和目标寄存器的汇编指令
    temp::Temp *src_reg = src_->Munch(instr_list, fs);
    temp::Temp *dst_reg = dst_->Munch(instr_list, fs);
    instr_list.Append(new assem::MoveInstr("movq `s0, `d0",
                        new temp::TempList(dst_reg), new temp::TempList(src_reg)));
  }
  /* End for lab5 code */
}

void ExpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  exp_->Munch(instr_list, fs);
  /* End for lab5 code */
}

temp::Temp *BinopExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  /*Unsure*/
  std::string assem_instr;
  std::stringstream instr_ss;

  //生成左操作数和右操作数的汇编指令，并获取对应的临时寄存器
  /*
  根据操作符op_生成对应的汇编指令：
  加法：生成addq指令。
  减法：生成subq指令。
  乘法：生成imulq指令。
  除法：生成cqto指令（扩展到64位），idivq指令（有符号除法），以及将结果移动到目标寄存器的指令。
  返回生成的目标寄存器dst
  */
  if (op_ == PLUS_OP && dynamic_cast<tree::NameExp*>(right_) &&
      dynamic_cast<tree::NameExp*>(right_)->name_->Name() == fs) {
    temp::Temp *left_reg = left_->Munch(instr_list, fs);
    temp::Temp *res_reg = temp::TempFactory::NewTemp();
    instr_ss << "leaq " << fs << "(`s0), `d0";
    instr_list.Append(new assem::OperInstr(instr_ss.str(), 
                        new temp::TempList(res_reg), 
                        new temp::TempList(left_reg), 
                        nullptr));
    return res_reg;
  }

  if (op_ == PLUS_OP || op_ == MINUS_OP) {
    assem_instr = (op_ == PLUS_OP) ? "addq" : "subq";
    temp::Temp *left_reg = left_->Munch(instr_list, fs);
    temp::Temp *res_reg = temp::TempFactory::NewTemp();
    instr_list.Append(new assem::MoveInstr("movq `s0, `d0", 
                        new temp::TempList(res_reg),
                        new temp::TempList(left_reg)));

    if (auto right_const = dynamic_cast<tree::ConstExp*>(right_)) {
      instr_ss << assem_instr << " $" << right_const->consti_ << ", `d0";
      instr_list.Append(new assem::OperInstr(instr_ss.str(),
                          new temp::TempList({res_reg}),
                          new temp::TempList(res_reg),
                          nullptr));
    } else {
      temp::Temp *right_reg = right_->Munch(instr_list, fs);
      instr_ss << assem_instr << " `s1, `d0";
      instr_list.Append(new assem::OperInstr(instr_ss.str(),
                          new temp::TempList({res_reg}),
                          new temp::TempList({res_reg, right_reg}),
                          nullptr));
    }

    return res_reg;
  }

  if (op_ == MUL_OP || op_ == DIV_OP) {
    assem_instr = (op_ == MUL_OP) ? "imulq" : "idivq";
    temp::Temp *rax = reg_manager->ReturnValue();
    temp::Temp *rdx = reg_manager->ArithmeticAssistant();
    temp::Temp *rax_saver = temp::TempFactory::NewTemp();
    temp::Temp *rdx_saver = temp::TempFactory::NewTemp();

    instr_list.Append(new assem::MoveInstr("movq `s0, `d0", 
                        new temp::TempList(rax_saver), 
                        new temp::TempList(rax)));
    instr_list.Append(new assem::MoveInstr("movq `s0, `d0", 
                        new temp::TempList(rdx_saver), 
                        new temp::TempList(rdx)));

    if (auto left_const = dynamic_cast<tree::ConstExp*>(left_)) {
      instr_ss << "movq $" << left_const->consti_ << ", `d0";
      instr_list.Append(new assem::OperInstr(instr_ss.str(),
                          new temp::TempList(rax), 
                          nullptr, nullptr));
    } else if (auto left_mem = dynamic_cast<tree::MemExp*>(left_)) {
      assem::MemFetch *fetch = MunchMem(left_mem, 0, instr_list, fs);
      instr_ss << "movq " << fetch->fetch_ << ", `d0";
      instr_list.Append(new assem::OperInstr(instr_ss.str(),
                          new temp::TempList(rax),
                          fetch->regs_, nullptr));
    } else {
      temp::Temp *left_reg = left_->Munch(instr_list, fs);
      instr_list.Append(new assem::MoveInstr("movq `s0, `d0",
                          new temp::TempList(rax), 
                          new temp::TempList(left_reg)));
    }

    instr_ss.str("");

    if (op_ == DIV_OP)
      instr_list.Append(new assem::OperInstr("cqto", 
                          new temp::TempList({rdx, rax, rax_saver, rdx_saver}),
                          new temp::TempList(rax),
                          nullptr));
    
    temp::Temp *right_reg = right_->Munch(instr_list, fs);
    instr_ss << assem_instr << " `s2";
    instr_list.Append(new assem::OperInstr(instr_ss.str(),
                        new temp::TempList({rdx, rax, rax_saver, rdx_saver}),
                        new temp::TempList({rdx, rax, right_reg}),
                        nullptr));

    temp::Temp *res_reg = temp::TempFactory::NewTemp();
    instr_list.Append(new assem::MoveInstr("movq `s0, `d0", 
                        new temp::TempList(res_reg), 
                        new temp::TempList(rax)));

    instr_list.Append(new assem::MoveInstr("movq `s0, `d0", 
                        new temp::TempList(rax), 
                        new temp::TempList(rax_saver)));
    instr_list.Append(new assem::MoveInstr("movq `s0, `d0", 
                        new temp::TempList(rdx), 
                        new temp::TempList(rdx_saver)));

    return res_reg;
  }

  return temp::TempFactory::NewTemp();  // error
  /* End for lab5 code */
}

//内存访问表达式的汇编指令
temp::Temp *MemExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  /*Unsure*/
  //创建一个新的临时寄存器dst
  temp::Temp* reg = temp::TempFactory::NewTemp();
  assem::MemFetch* fetch = MunchMem(this, 0, instr_list, fs);//调用MunchMem函数生成内存访问指令

  //生成将内存内容移动到目标寄存器的汇编指令，并添加到instr_list中
  std::stringstream instr_ss;
  instr_ss << "movq " << fetch->fetch_ << ", `d0";
  instr_list.Append(new assem::OperInstr(instr_ss.str(), 
                                           new temp::TempList(reg), 
                                           fetch->regs_, nullptr));
  return reg;
  /* End for lab5 code */
}

temp::Temp *TempExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  return temp_;
  /* End for lab5 code */
}

temp::Temp *EseqExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  // EseqExp should not exist in codegen phase
  stm_->Munch(instr_list, fs);
  return exp_->Munch(instr_list, fs);
  /* End for lab5 code */
}

//标签地址表达式
temp::Temp *NameExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  /*Unsure*/
  temp::Temp* reg = temp::TempFactory::NewTemp();
  std::stringstream instr_ss;
  instr_ss << "leaq " << name_->Name() << "(%rip), `d0";
  instr_list.Append(new assem::OperInstr(instr_ss.str(), 
                                            new temp::TempList(reg), 
                                            nullptr, nullptr));
  return reg;
  /* End for lab5 code */
}

//常量表达式
temp::Temp *ConstExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  /*Unsure*/
  temp::Temp* reg = temp::TempFactory::NewTemp();
  std::string instr_str = "movq $" + std::to_string(consti_) + ", `d0";
  instr_list.Append(new assem::OperInstr(instr_str, 
                                           new temp::TempList(reg), 
                                           nullptr, nullptr));
  return reg;
  /* End for lab5 code */
}

temp::Temp *CallExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  //创建一个新的临时寄存器rax用于保存返回值
  temp::Temp* rax = reg_manager->ReturnValue();
    
  //检查函数表达式的类型是否为NameExp，如果不是则返回rax
  if (typeid(*fun_) != typeid(tree::NameExp))  // error
      return rax; 
    
  // 生成函数参数的汇编指令，并获取参数列表
  auto* arg_list = args_->MunchArgs(instr_list, fs);
  // 获取需要保存的寄存器列表
  auto* calldefs = reg_manager->CallerSaves();
  calldefs->Append(rax);

  // 获取函数名
  std::string fun_name = static_cast<tree::NameExp*>(fun_)->name_->Name();
  // 生成调用指令，并添加到instr_list中
  instr_list.Append(new assem::OperInstr("callq " + fun_name, 
                                           calldefs, arg_list, nullptr));
  return rax;
  /* End for lab5 code */
}

} // namespace tree
