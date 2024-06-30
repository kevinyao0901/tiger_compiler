#include "tiger/translate/translate.h"

#include <tiger/absyn/absyn.h>

#include "tiger/env/env.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/frame/x64frame.h"
#include "tiger/frame/temp.h"
#include "tiger/frame/frame.h"

//外部变量，用于管理碎片（Frags）和寄存器（RegManager）
extern frame::Frags *frags;//p121
extern frame::RegManager *reg_manager;

namespace {
frame::ProcFrag *ProcEntryExit(tr::Level *level, tr::Exp *body);
}

//tr 命名空间包含了翻译过程中使用的类和函数
namespace tr {

//AllocLocal 方法在给定层级分配一个局部变量，如果 escape 为 true，则表示变量可能逃逸到当前函数之外
Access *Access::AllocLocal(Level *level, bool escape) {
  /* TODO: Put your lab5 code here */
  frame::Frame *frame = level->frame_;
  frame::Access *access = frame->AllocLocal(escape);
  return new Access(level, access);
}

//p108 表达式的种类Ex,Nx,Cx
//表示条件表达式的控制流，包含真标签、假标签和条件语句
class Cx {
public:
  temp::Label **trues_;
  temp::Label **falses_;
  tree::Stm *stm_;

  Cx(temp::Label **trues, temp::Label **falses, tree::Stm *stm)
      : trues_(trues), falses_(falses), stm_(stm) {}
};

//定义了三种不同的表达式转换方法
class Exp {
public:
  [[nodiscard]] virtual tree::Exp *UnEx() const = 0;// 转换为表达式树
  [[nodiscard]] virtual tree::Stm *UnNx() const = 0;//转换为语句树
  [[nodiscard]] virtual Cx UnCx(err::ErrorMsg *errormsg) const = 0;//转换为条件表达式
};

//包含表达式和类型，表示翻译结果
class ExpAndTy {
public:
  tr::Exp *exp_;//defined in tree.h
  type::Ty *ty_;

  ExpAndTy(tr::Exp *exp, type::Ty *ty) : exp_(exp), ty_(ty) {}
};

//ExExp 类继承自 Exp，表示普通的表达式。
//它重载了三个虚函数,UnEx: 返回表达式,UnNx: 返回表达式语句,UnCx: 返回条件表达式。
class ExExp : public Exp {
public:
  tree::Exp *exp_;

  explicit ExExp(tree::Exp *exp) : exp_(exp) {}

  [[nodiscard]] tree::Exp *UnEx() const override { 
    /* TODO: Put your lab5 code here */
    return exp_;
  }
  [[nodiscard]] tree::Stm *UnNx() const override {
    /* TODO: Put your lab5 code here */
    /*tree::ExpStm 是一个语句类，用于将表达式封装为语句。
    它的构造函数接受一个表达式作为参数，并将其存储在内部。*/
    return new tree::ExpStm(exp_);
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) const override {
    /* TODO: Put your lab5 code here */
    temp::Label *true_label = temp::LabelFactory::NewLabel();
    temp::Label *false_label = temp::LabelFactory::NewLabel();

    // Create a conditional jump statement
    tree::CjumpStm *cjump_stm = new tree::CjumpStm(tree::NE_OP, new tree::ConstExp(0), exp_, true_label, false_label);

    // Return a Cx with the true and false labels of the conditional jump statement
    return Cx(&cjump_stm->true_label_, &cjump_stm->false_label_, cjump_stm);
      }
};

//表示不产生值的语句
/*
重载了三个虚函数：
UnEx: 返回一个 EseqExp 表达式，表示语句序列。
UnNx: 返回语句本身。
UnCx: 抛出错误，因为没有结果不能转换为条件表达式。
*/
class NxExp : public Exp {
public:
  tree::Stm *stm_;

  explicit NxExp(tree::Stm *stm) : stm_(stm) {}

  [[nodiscard]] tree::Exp *UnEx() const override {
    /* TODO: Put your lab5 code here */
    return new tree::EseqExp(stm_, new tree::ConstExp(0));
  }
  [[nodiscard]] tree::Stm *UnNx() const override { 
    /* TODO: Put your lab5 code here */
     return stm_;
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) const override {
    /* TODO: Put your lab5 code here */
    errormsg->Error(errormsg->GetTokPos(), "No result cannot be cast to conditional.");
  }
};

//继承自 Exp，表示一个条件表达式
//含一个 Cx 类型的成员变量 cx_
class CxExp : public Exp {
public:
  Cx cx_;

  CxExp(temp::Label** trues, temp::Label** falses, tree::Stm *stm)
      : cx_(trues, falses, stm) {}
  
  //p111,将条件表达式转换为一个树表达式（tree::Exp），通过使用嵌套的 tree::EseqExp 来实现
  [[nodiscard]] tree::Exp *UnEx() const override {
    /* TODO: Put your lab5 code here */
    /*Unsure*/
    temp::Temp *reg = temp::TempFactory::NewTemp();
    temp::Label *t = temp::LabelFactory::NewLabel();
    temp::Label *f = temp::LabelFactory::NewLabel();

    // Update Cx to jump to the new true label and false label
    *cx_.trues_ = t;
    *cx_.falses_ = f;

    // Construct the nested EseqExp in a more readable and structured way
    tree::Stm *true_move = new tree::MoveStm(new tree::TempExp(reg), new tree::ConstExp(1));
    tree::Stm *false_move = new tree::MoveStm(new tree::TempExp(reg), new tree::ConstExp(0));
    tree::Stm *label_f = new tree::LabelStm(f);
    tree::Stm *label_t = new tree::LabelStm(t);

    tree::Exp *nested_eseq = new tree::EseqExp(true_move,
                                new tree::EseqExp(cx_.stm_,
                                  new tree::EseqExp(label_f,
                                    new tree::EseqExp(false_move,
                                      new tree::EseqExp(label_t,
                                        new tree::TempExp(reg))))));

    return nested_eseq;
  }
  //返回 Cx 的语句部分
  [[nodiscard]] tree::Stm *UnNx() const override {
    /* TODO: Put your lab5 code here */
    return cx_.stm_;
  }
  //接返回 Cx 对象 cx_
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) const override { 
    /* TODO: Put your lab5 code here */
    return cx_;
  }
};

//将 body 转换为一个 ProcFrag，并添加到全局的 frags 列表中
void ProcEntryExit(Level *level, Exp *body) {
  frame::ProcFrag *frag = new frame::ProcFrag(body->UnNx(), level->frame_);
  frags->PushBack(frag);
}


//Translate() 方法用于翻译整个程序
void ProgTr::Translate() {
  /* TODO: Put your lab5 code here */
  /*Unsure*/
  temp::Label *main_label = temp::LabelFactory::NamedLabel("tigermain");//主函数标签
  frame::Frame *main_frame = frame::NewFrame(main_label, std::vector<bool>());//主函数框架
  tr::Level *main_level = new tr::Level(main_frame, main_level_.get());//主函数级别

  //翻译抽象语法树
  tr::ExpAndTy *tree_expty = absyn_tree_->Translate(venv_.get(), tenv_.get(), main_level, nullptr, errormsg_.get());

  //调用 frame::ProcEntryExit1 创建主函数语句 main_stm
  tree::Stm *main_stm = frame::ProcEntryExit1(main_frame, tree_expty->exp_->UnNx());

  //调用 ProcEntryExit 创建并添加 ProcFrag
  ProcEntryExit(main_level, new tr::NxExp(main_stm));
}

} // namespace tr

// namespace {

// /**
//  * Wrapper for `ProcExitEntry1`, which deals with the return value of the
//  * function body
//  * @param level current level
//  * @param body function body
//  * @return statements after `ProcExitEntry1`
//  */
// frame::ProcFrag *ProcEntryExit(tr::Level *level, tr::Exp *body) {
//   /* TODO: Put your lab5 code here */
// }
// } // namespace

namespace absyn {

tr::ExpAndTy *AbsynTree::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  //调用了根节点的 Translate 方法。这个根节点代表整个抽象语法树的起始点。
  return root_->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *SimpleVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  /*Unsure*/
  //查找符号表，确定变量是否存在以及它是否是一个变量
  env::EnvEntry *ent = venv->Look(sym_);
  if (!ent || !dynamic_cast<env::VarEntry *>(ent)) {
    errormsg->Error(pos_, !ent ? "variable %s not exist" : "%s is not a variable", sym_->Name().data());
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance()); 
  }

  env::VarEntry *var_ent = static_cast<env::VarEntry *>(ent);
  tr::Access *dec_acc = var_ent->access_;
  tr::Level *dec_level = dec_acc->level_;
  tr::Level *cur_level = level;

  //如果当前级别与声明变量的级别相同，直接返回变量的表达式
  if (cur_level == dec_level) {
    tree::Exp *var_exp = frame::AccessCurrentExp(dec_acc->access_, dec_level->frame_);
    return new tr::ExpAndTy(new tr::ExExp(var_exp), var_ent->ty_);
  }
  
  // Follow the static links
  //否则，沿着静态链接找到声明变量的级别，然后返回变量的表达式
  tree::Exp *static_link = frame::AccessCurrentExp(cur_level->frame_->formal_access_.front(), cur_level->frame_);
  cur_level = cur_level->parent_;

  while (cur_level != dec_level) {
    static_link = frame::AccessExp(cur_level->frame_->formal_access_.front(), static_link);
    cur_level = cur_level->parent_;
  }

  // Now at declare level
  tree::Exp *var_exp = frame::AccessExp(dec_acc->access_, static_link);
  return new tr::ExpAndTy(new tr::ExExp(var_exp), var_ent->ty_);
}

tr::ExpAndTy *FieldVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  /*Unsure*/
  //翻译字段所在的变量，得到变量的表达式和类型
  tr::ExpAndTy *var_expty = var_->Translate(venv, tenv, level, label, errormsg);
  tree::Exp *var_exp = var_expty->exp_->UnEx();
  type::Ty *var_ty = var_expty->ty_;

  //检查变量的类型是否为记录类型
  if (auto rec_ty = dynamic_cast<type::RecordTy *>(var_ty->ActualTy())) {
    //如果是记录类型，查找字段的偏移量，计算字段的地址并返回相应的表达式和字段的类型
    int k = 0;
    for (auto field : rec_ty->fields_->GetList()) {
      if (field->name_->Name() == sym_->Name()) {
        tree::Exp *exp = new tree::MemExp(
          new tree::BinopExp(tree::PLUS_OP, var_exp, 
            new tree::ConstExp(k * level->frame_->WordSize())));
        return new tr::ExpAndTy(new tr::ExExp(exp), field->ty_->ActualTy());
      }
      k++;
    }
    errormsg->Error(pos_, "no field named %s", sym_->Name().data());
  } else {
    //如果不是记录类型，报告错误并返回一个默认的值
    errormsg->Error(var_->pos_, "not a record type");
  }

  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
}

//p113,翻译数组下标访问的抽象语法树节点。生成一个表示数组下标访问的中间表达式
tr::ExpAndTy *SubscriptVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                      tr::Level *level, temp::Label *label,
                                      err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  /*Unsure*/
  tr::ExpAndTy *var_expty = var_->Translate(venv, tenv, level, label, errormsg);//调用翻译数组变量
  tree::Exp *var_exp = var_expty->exp_->UnEx();
  type::Ty *var_ty = var_expty->ty_;

  //检查变量的实际类型是否为数组类型
  if (auto array_ty = dynamic_cast<type::ArrayTy *>(var_ty->ActualTy())) {
    tr::ExpAndTy *subscript_expty = subscript_->Translate(venv, tenv, level, label, errormsg);//调用翻译下标表达式
    tree::Exp *subscript_exp = subscript_expty->exp_->UnEx();

    //查下标表达式的实际类型是否为整数类型
    if (dynamic_cast<type::IntTy *>(subscript_expty->ty_->ActualTy())) {
      //创建一个内存访问表达式
      tree::Exp *exp = new tree::MemExp(
        new tree::BinopExp(tree::PLUS_OP, var_exp, //将结果加上数组基地址
          new tree::BinopExp(tree::MUL_OP, subscript_exp, //计算下标乘以数组元素大小
            new tree::ConstExp(level->frame_->WordSize()))));
      return new tr::ExpAndTy(new tr::ExExp(exp), array_ty->ty_);
    } else {
      errormsg->Error(subscript_->pos_, "require integer array subscript");
    }
  } else {
    errormsg->Error(var_->pos_, "not an array type");
  }

  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
}

tr::ExpAndTy *VarExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return var_->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *NilExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::NilTy::Instance());
}

tr::ExpAndTy *IntExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(val_)), type::IntTy::Instance());
}

tr::ExpAndTy *StringExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  temp::Label *str_label = temp::LabelFactory::NewLabel();//创建一个新的标签用于字符串常量
  frags->PushBack(new frame::StringFrag(str_label, str_));//将字符串常量存储在一个片段中
  return new tr::ExpAndTy(new tr::ExExp(new tree::NameExp(str_label)), type::StringTy::Instance());
}

//翻译函数调用的抽象语法树节点
tr::ExpAndTy *CallExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  /*Unsure*/
  //检查条目是否存在且是 env::FunEntry 类型
  env::EnvEntry *ent = venv->Look(func_);
  if (!ent || typeid(*ent) != typeid(env::FunEntry)) {
    errormsg->Error(pos_, "undefined function %s", func_->Name().data());
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
  }

  //准备参数列表
  env::FunEntry *func_ent = static_cast<env::FunEntry *>(ent);
  auto args = std::make_unique<tree::ExpList>();
  tree::Exp *func_exp;

  //处理静态链接
  //检查函数条目是否有标签,如果有标签，表示是内部函数调用，需要处理静态链接
  if (func_ent->label_) {
    // Pass the static link
    //如果调用者的层级是函数的直接父层级，将当前层级的帧地址添加到参数列表中
    if (func_ent->level_->parent_ == level) {
      args->Append(level->frame_->FrameAddress());
    } else {
      //如果调用者的层级不是函数的直接父层级，需要沿静态链向上查找，直到找到共同祖先层级，生成静态链接表达式并添加到参数列表中
      tree::Exp *static_link = frame::AccessCurrentExp(level->frame_->formal_access_.front(), level->frame_);
      tr::Level *cur_level = level->parent_;  // Owns the frame which static_link points to

      // Follow the static link upwards
      while (cur_level && cur_level != func_ent->level_->parent_) {
        static_link = frame::AccessExp(cur_level->parent_->frame_->formal_access_.front(), static_link);
        cur_level = cur_level->parent_;
      }

      if (cur_level) {
        args->Append(static_link);
      } else {
        errormsg->Error(pos_, "%s cannot call %s", level->frame_->GetLabel().data(), func_ent->level_->frame_->GetLabel().data());
        return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
      }
    }
    func_exp = new tree::NameExp(func_ent->label_);
  } else {  // External call
    func_exp = new tree::NameExp(func_);
  }

  //翻译参数表达式,遍历参数列表 args_->GetList()，翻译每个参数表达式并添加到参数列表中
  for (Exp *arg : args_->GetList()) {
    tree::Exp *arg_exp = arg->Translate(venv, tenv, level, label, errormsg)->exp_->UnEx();
    args->Append(arg_exp);
  }

  //更新当前层级的帧的最大传出参数数目
  level->frame_->max_outgoing_args_ = std::max(level->frame_->max_outgoing_args_,
                                               static_cast<int>(args->GetList().size()) - static_cast<int>(reg_manager->ArgRegs()->GetList().size()));

  tree::Exp *call_exp = new tree::CallExp(func_exp, args.release());
  return new tr::ExpAndTy(new tr::ExExp(call_exp), func_ent->result_);
  /* End for lab5 code */
}

//翻译二元操作表达式
tr::ExpAndTy *OpExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  /*Unsure*/
  //分别翻译左右操作数，检查翻译结果是否有效
  tr::ExpAndTy *left_expty = left_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *right_expty = right_->Translate(venv, tenv, level, label, errormsg);

  if (!left_expty || !right_expty) {
    errormsg->Error(pos_, "invalid operands in binary operation");
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
  }

  //从翻译结果中提取左右操作数的中间表达式
  tree::Exp *left_exp = left_expty->exp_->UnEx();
  tree::Exp *right_exp = right_expty->exp_->UnEx();

  //检查左右操作数的类型是否一致
  if (!left_expty->ty_->IsSameType(right_expty->ty_)) {
    errormsg->Error(pos_, "binary operation type mismatch");
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
  }

  /*
  处理字符串操作：
      如果操作数是字符串类型，处理字符串比较操作。
      对于 EQ_OP 和 NEQ_OP 操作，调用外部函数 string_equal，并生成相应的表达式。
      如果遇到不支持的字符串操作，报告错误并返回默认值。
  */
  tr::Exp *exp = nullptr;
  if (typeid(*left_expty->ty_->ActualTy()) == typeid(type::StringTy)) {
    tree::ExpList *args = new tree::ExpList({left_exp, right_exp});
    const char *func_name = nullptr;

    if (oper_ == absyn::EQ_OP) {
      func_name = "string_equal";
      exp = new tr::ExExp(frame::ExternalCall(func_name, args));
    } else if (oper_ == absyn::NEQ_OP) {
      func_name = "string_equal";
      exp = new tr::ExExp(new tree::BinopExp(tree::MINUS_OP, new tree::ConstExp(1), frame::ExternalCall(func_name, args)));
    } else {
      errormsg->Error(pos_, "unexpected binary token for string");
      return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
    }
  } else {
    //对于其他类型的操作，生成相应的二元操作表达式或条件跳转表达式
    tree::CjumpStm *cjump = nullptr;
    tree::BinopExp *binop = nullptr;

    /*
    根据操作符类型（如加法、减法、乘法、除法等）生成对应的二元操作表达式 BinopExp。
    对于比较操作（如相等、不等、大于、小于等），生成条件跳转表达式 CjumpStm。
    */
    switch (oper_) {
      case absyn::PLUS_OP:
        binop = new tree::BinopExp(tree::PLUS_OP, left_exp, right_exp);
        break;
      case absyn::MINUS_OP:
        binop = new tree::BinopExp(tree::MINUS_OP, left_exp, right_exp);
        break;
      case absyn::TIMES_OP:
        binop = new tree::BinopExp(tree::MUL_OP, left_exp, right_exp);
        break;
      case absyn::DIVIDE_OP:
        binop = new tree::BinopExp(tree::DIV_OP, left_exp, right_exp);
        break;
      case absyn::EQ_OP:
        cjump = new tree::CjumpStm(tree::EQ_OP, left_exp, right_exp, nullptr, nullptr);
        break;
      case absyn::NEQ_OP:
        cjump = new tree::CjumpStm(tree::NE_OP, left_exp, right_exp, nullptr, nullptr);
        break;
      case absyn::GT_OP:
        cjump = new tree::CjumpStm(tree::GT_OP, left_exp, right_exp, nullptr, nullptr);
        break;
      case absyn::GE_OP:
        cjump = new tree::CjumpStm(tree::GE_OP, left_exp, right_exp, nullptr, nullptr);
        break;
      case absyn::LT_OP:
        cjump = new tree::CjumpStm(tree::LT_OP, left_exp, right_exp, nullptr, nullptr);
        break;
      case absyn::LE_OP:
        cjump = new tree::CjumpStm(tree::LE_OP, left_exp, right_exp, nullptr, nullptr);
        break;
      default:
        errormsg->Error(pos_, "unexpected binary token %d", oper_);
        return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
    }

    //如果生成的表达式有效，返回包含该表达式和整数类型的 ExpAndTy 对象
    if (binop) {
      exp = new tr::ExExp(binop);
    } else if (cjump) {
      exp = new tr::CxExp(&cjump->true_label_, &cjump->false_label_, cjump);
    }
  }

  if (!exp) {
    errormsg->Error(pos_, "unexpected binary operation");
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
  }

  return new tr::ExpAndTy(exp, type::IntTy::Instance());
}

//p118,记录表达式
tr::ExpAndTy *RecordExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,      
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  /*Unsure*/
  //类型查找
  type::Ty* ty = tenv->Look(typ_);
  if (!ty || typeid(*ty->ActualTy()) != typeid(type::RecordTy)) {
    errormsg->Error(pos_, !ty ? "undefined type %s" : "type %s is not a record", typ_->Name().data());
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
  }

  //从 rec_ty 中获取字段类型，从 fields_ 中获取字段表达式。如果字段数量不匹配，则报告错误。
  type::RecordTy *rec_ty = static_cast<type::RecordTy *>(ty->ActualTy());
  const auto &fields = rec_ty->fields_->GetList();
  const auto &efields = fields_->GetList();
  int n = fields.size();

  if (fields.empty() || efields.empty() || fields.size() != efields.size()) {
    errormsg->Error(pos_, fields.empty() && efields.empty() ? "record has no fields" : "field type mismatch");
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
  }

  //使用外部调用 alloc_record 分配记录的内存
  auto *reg_exp = new tree::TempExp(temp::TempFactory::NewTemp());
  auto *call_args = new tree::ExpList({new tree::ConstExp(n * level->frame_->WordSize())});
  auto *malloc_stm = new tree::MoveStm(reg_exp, frame::ExternalCall("alloc_record", call_args));

  //字段初始化
  tree::Stm *stm = nullptr;
  auto field_it = fields.begin();
  auto efield_it = efields.begin();
  for (int i = 0; i < n; ++i, ++field_it, ++efield_it) {
    auto *efield_expty = (*efield_it)->exp_->Translate(venv, tenv, level, label, errormsg);//将字段表达式翻译成中间代码
    //检查字段类型是否匹配
    if (!(*field_it)->ty_->IsSameType(efield_expty->ty_)) {
      errormsg->Error(pos_, "field type mismatch");
      return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
    }

    //使用偏移量表达式设置字段的内存位置，并生成 MoveStm 语句
    tree::Exp *offset_exp = new tree::BinopExp(tree::PLUS_OP, reg_exp, new tree::ConstExp(i * level->frame_->WordSize()));
    tree::Stm *field_stm = new tree::MoveStm(new tree::MemExp(offset_exp), efield_expty->exp_->UnEx());
    stm = stm ? new tree::SeqStm(stm, field_stm) : field_stm;
  }

  //生成包含分配和字段初始化的中间代码表达式，并返回
  tree::Exp *res_exp = new tree::EseqExp(new tree::SeqStm(malloc_stm, stm), reg_exp);
  return new tr::ExpAndTy(new tr::ExExp(res_exp), rec_ty);
}

//顺序表达式
tr::ExpAndTy *SeqExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  /*Unsure*/
  //初始化序列表达式列表
  std::vector<tree::Exp*> seq_exps;
  tr::ExpAndTy *expty = nullptr;

  //将每个子表达式翻译成中间代码并添加到列表中
  for (auto exp : seq_->GetList()) {
    expty = exp->Translate(venv, tenv, level, label, errormsg);
    seq_exps.push_back(expty->exp_->UnEx());
  }

  //如果列表为空，则返回默认值
  if (seq_exps.empty()) {
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
  }

  //生成包含所有子表达式的中间代码表达式，并返回
  auto it = seq_exps.rbegin();
  tree::Exp *res_exp = *it++;
  while (it != seq_exps.rend()) {
    res_exp = new tree::EseqExp(new tree::ExpStm(*it++), res_exp);
  }

  return new tr::ExpAndTy(new tr::ExExp(res_exp), expty->ty_);
}

//p113,赋值表达式
tr::ExpAndTy *AssignExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,                       
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  /*Unsure*/
  //翻译变量和赋值表达式
  tr::ExpAndTy *var_expty = var_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *exp_expty = exp_->Translate(venv, tenv, level, label, errormsg);

  //检查类型是否匹配
  if (!var_expty->ty_->IsSameType(exp_expty->ty_)) {
    errormsg->Error(exp_->pos_, "unmatched assign exp");
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
  }

  //生成 MoveStm 语句以执行赋值操作
  tree::Stm *assign_stm = new tree::MoveStm(var_expty->exp_->UnEx(), exp_expty->exp_->UnEx());

  return new tr::ExpAndTy(new tr::NxExp(assign_stm), type::VoidTy::Instance());
}

tr::ExpAndTy *IfExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  /*Unsure*/
  // 翻译条件表达式 & then 分支
  tr::ExpAndTy *test_expty = test_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *then_expty = then_->Translate(venv, tenv, level, label, errormsg);
  //获取条件表达式的控制流
  tr::Cx test_cx = test_expty->exp_->UnCx(errormsg);

  // 为条件表达式的真和假分支生成新的标签
  if (!*test_cx.trues_) *test_cx.trues_ = temp::LabelFactory::NewLabel();
  if (!*test_cx.falses_) *test_cx.falses_ = temp::LabelFactory::NewLabel();


  //// 处理没有 else 分支的 if 语句
  if (!elsee_) {
    // 检查 then 分支是否返回非 void 类型
    if (typeid(*then_expty->ty_) != typeid(type::VoidTy)) {
      errormsg->Error(then_->pos_, "if with no else must return no value");
      return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
    }

    //// 生成对应的语句序列
    tree::Stm *stm = new tree::SeqStm(test_cx.stm_,
                      new tree::SeqStm(new tree::LabelStm(*test_cx.trues_),
                        new tree::SeqStm(then_expty->exp_->UnNx(),
                          new tree::LabelStm(*test_cx.falses_))));
    return new tr::ExpAndTy(new tr::NxExp(stm), type::VoidTy::Instance());
  }

  // 翻译 else 分支
  tr::ExpAndTy *else_expty = elsee_->Translate(venv, tenv, level, label, errormsg);

  // 检查 then 和 else 分支的返回类型是否一致
  if (!then_expty->ty_->IsSameType(else_expty->ty_)) {
    errormsg->Error(elsee_->pos_, "then exp and else exp type mismatch");
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
  }

  // 生成汇合点的标签
  temp::Label *converge_label = temp::LabelFactory::NewLabel();
  auto converge_jumps = std::make_unique<std::vector<temp::Label *>>(1, converge_label);

  if (typeid(*then_expty->ty_) == typeid(type::VoidTy)) {
    // 处理 then 和 else 分支都为 void 类型的情况
    //意味着 if 表达式本身不会返回一个值
    tree::Stm *stm = new tree::SeqStm(test_cx.stm_,
                      new tree::SeqStm(new tree::LabelStm(*test_cx.trues_),
                        new tree::SeqStm(then_expty->exp_->UnNx(),
                          new tree::SeqStm(new tree::JumpStm(new tree::NameExp(converge_label), converge_jumps.get()),
                            new tree::SeqStm(new tree::LabelStm(*test_cx.falses_),
                              new tree::SeqStm(else_expty->exp_->UnNx(),
                                new tree::LabelStm(converge_label)))))));
    return new tr::ExpAndTy(new tr::NxExp(stm), type::VoidTy::Instance());
  } else {
    // 处理 then 和 else 分支返回非 void 类型的情况
    //意味着 if 表达式本身会返回一个值。处理这种情况时，需要一个临时寄存器来存储返回值
    temp::Temp *reg = temp::TempFactory::NewTemp();
    tree::Exp *reg_exp = new tree::TempExp(reg);
    tree::Exp *exp = new tree::EseqExp(test_cx.stm_,
                      new tree::EseqExp(new tree::LabelStm(*test_cx.trues_),
                        new tree::EseqExp(new tree::MoveStm(reg_exp, then_expty->exp_->UnEx()),
                          new tree::EseqExp(new tree::JumpStm(new tree::NameExp(converge_label), converge_jumps.get()),
                            new tree::EseqExp(new tree::LabelStm(*test_cx.falses_),
                              new tree::EseqExp(new tree::MoveStm(reg_exp, else_expty->exp_->UnEx()),
                                new tree::EseqExp(new tree::LabelStm(converge_label), reg_exp)))))));
    return new tr::ExpAndTy(new tr::ExExp(exp), then_expty->ty_);
  }
}

//p119 while 循环
tr::ExpAndTy *WhileExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,            
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  /*Unsure*/
  // 翻译 while 循环的条件表达式
  tr::ExpAndTy *test_expty = test_->Translate(venv, tenv, level, label, errormsg);
  // 获取条件表达式的控制流
  tr::Cx test_cx = test_expty->exp_->UnCx(errormsg);

  // 生成测试条件、循环体和结束的标签
  temp::Label *test_label = temp::LabelFactory::NewLabel();
  temp::Label *body_label = temp::LabelFactory::NewLabel();
  temp::Label *done_label = temp::LabelFactory::NewLabel();

  // 翻译循环体
  tr::ExpAndTy *body_expty = body_->Translate(venv, tenv, level, done_label, errormsg);
  // 检查循环体是否返回非 void 类型
  if (typeid(*body_expty->ty_) != typeid(type::VoidTy)) {
    errormsg->Error(body_->pos_, "while body must produce no value");
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
  }

  // 设置条件表达式的真和假分支
  *test_cx.trues_ = body_label;
  *test_cx.falses_ = done_label;

  // 创建一个跳转回测试条件的跳转语句
  auto test_jumps = std::make_unique<std::vector<temp::Label *>>(1, test_label);
  tree::Stm *test_jump_stm = new tree::JumpStm(new tree::NameExp(test_label), test_jumps.get());

  // 生成 while 循环的语句序列
  tree::Stm *while_stm = new tree::SeqStm(new tree::LabelStm(test_label),
                          new tree::SeqStm(test_cx.stm_,
                            new tree::SeqStm(new tree::LabelStm(body_label),
                              new tree::SeqStm(body_expty->exp_->UnNx(),
                                new tree::SeqStm(test_jump_stm, new tree::LabelStm(done_label))))));
  
  return new tr::ExpAndTy(new tr::NxExp(while_stm), type::VoidTy::Instance());
}

//p119
tr::ExpAndTy *ForExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  /*Unsure*/
  //创建循环上限符号
  sym::Symbol *limit_sym = sym::Symbol::UniqueSymbol("limit");

  //声明循环变量和上限变量
  DecList *decs = new DecList();
  VarDec *lo_dec = new VarDec(lo_->pos_, var_, nullptr, lo_);
  lo_dec->escape_ = escape_;
  decs->Prepend(lo_dec);

  VarDec *hi_dec = new VarDec(hi_->pos_, limit_sym, nullptr, hi_);
  hi_dec->escape_ = false;
  decs->Prepend(hi_dec);

  auto createSimpleVar = [this](sym::Symbol *sym) {
      return new SimpleVar(pos_, sym);
  };

  auto createVarExp = [this](SimpleVar *var) {
      return new VarExp(pos_, var);
  };

  SimpleVar *it_var = createSimpleVar(var_);
  SimpleVar *limit_var = createSimpleVar(limit_sym);
  VarExp *it_exp = createVarExp(it_var);
  VarExp *limit_exp = createVarExp(limit_var);

  //创建简单变量表示
  OpExp *test_exp = new OpExp(pos_, LE_OP, it_exp, limit_exp);

  //创建循环体表达式
  ExpList *body_exps = new ExpList();
  body_exps->Prepend(new AssignExp(pos_, it_var, new OpExp(pos_, PLUS_OP, it_exp, new IntExp(pos_, 1))));
  body_exps->Prepend(body_);

  SeqExp *seq_exp = new SeqExp(pos_, body_exps);
  WhileExp *while_exp = new WhileExp(pos_, test_exp, seq_exp);
  LetExp *let_exp = new LetExp(while_exp->pos_, decs, while_exp);

  //创建 WhileExp 表达式
  return let_exp->Translate(venv, tenv, level, label, errormsg);
}

//break 表达式
tr::ExpAndTy *BreakExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  //生成一个跳转语句跳到循环结束的标签
  std::vector<temp::Label *> *jumps = new std::vector<temp::Label *>{label};
  tree::Stm *jump_stm = new tree::JumpStm(new tree::NameExp(label), jumps);
  return new tr::ExpAndTy(new tr::NxExp(jump_stm), type::VoidTy::Instance());
}

//let 表达式
tr::ExpAndTy *LetExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  /*Unusre*/
  //开启一个新的作用域
  venv->BeginScope();
  tenv->BeginScope();

  tree::Stm *dec_stm = nullptr;

  //翻译声明列表和主体表达式
  // Translate declarations
  for (auto dec : decs_->GetList()) {
    tree::Stm *current_dec_stm = dec->Translate(venv, tenv, level, label, errormsg)->UnNx();
    dec_stm = dec_stm ? new tree::SeqStm(dec_stm, current_dec_stm) : current_dec_stm;
  }

  // Translate body
  tr::ExpAndTy *body_expty = body_->Translate(venv, tenv, level, label, errormsg);
  tr::Exp *result_exp;

  //根据主体表达式的类型，我们生成不同类型的中间表达式
  if (typeid(*body_expty->exp_) == typeid(tr::NxExp)) {
    tree::Stm *seq_stm = dec_stm ? new tree::SeqStm(dec_stm, body_expty->exp_->UnNx()) : body_expty->exp_->UnNx();
    result_exp = new tr::NxExp(seq_stm);
  } else {
    tree::Exp *eseq_exp = dec_stm ? new tree::EseqExp(dec_stm, body_expty->exp_->UnEx()) : body_expty->exp_->UnEx();
    result_exp = new tr::ExExp(eseq_exp);
  }

  //关闭作用域并返回结果
  venv->EndScope();
  tenv->EndScope();

  return new tr::ExpAndTy(result_exp, typeid(*body_expty->exp_) == typeid(tr::NxExp) ? type::VoidTy::Instance() : body_expty->ty_);
}

//p118
tr::ExpAndTy *ArrayExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,                    
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  /*Unsure*/
  //查找类型并检查是否定义
  type::Ty* ty = tenv->Look(typ_);
  if (!ty) {
    errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
  }

  //检查类型是否为数组类型
  type::Ty* actual_ty = ty->ActualTy();
  if (typeid(*actual_ty) != typeid(type::ArrayTy)) {
    errormsg->Error(pos_, "type %s is not an array", typ_->Name().data());
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
  }

  //获取数组类型并检查大小表达式的类型是否为整数
  type::ArrayTy *arr_ty = static_cast<type::ArrayTy *>(actual_ty);

  tr::ExpAndTy *size_expty = size_->Translate(venv, tenv, level, label, errormsg);
  if (typeid(*size_expty->ty_->ActualTy()) != typeid(type::IntTy)) {
    errormsg->Error(size_->pos_, "integer required for array size");
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
  }

  //检查初始化表达式的类型是否与数组元素类型匹配
  tr::ExpAndTy *init_expty = init_->Translate(venv, tenv, level, label, errormsg);
  if (!init_expty->ty_->IsSameType(arr_ty->ty_)) {
    errormsg->Error(init_->pos_, "type mismatch");
    return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
  }

  //生成数组初始化的中间代码
  tree::Exp *reg_exp = new tree::TempExp(temp::TempFactory::NewTemp());
  tree::ExpList *call_args = new tree::ExpList({size_expty->exp_->UnEx(), init_expty->exp_->UnEx()});
  tree::Stm *init_stm = new tree::MoveStm(reg_exp, frame::ExternalCall("init_array", call_args));
  tree::Exp *res_exp = new tree::EseqExp(init_stm, reg_exp);

  return new tr::ExpAndTy(new tr::ExExp(res_exp), arr_ty);
}

tr::ExpAndTy *VoidExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::VoidTy::Instance());
}

tr::Exp *FunctionDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  /*Unsure*/
  //收集函数签名
  //创建函数记录表
  std::unordered_map<std::string, temp::Label *> function_record;

  // First pass: Collect function signatures
  //遍历函数列表，收集每个函数的签名
  for (FunDec *function : functions_->GetList()) {
    const std::string &name = function->name_->Name();
    //如果发现两个函数的名字相同，抛出错误信息，并返回一个占位的表达式
    if (function_record.count(name)) {
      errormsg->Error(function->pos_, "two functions have the same name");
      return new tr::ExExp(new tree::ConstExp(0));
    }

    //为每个函数创建一个新的标签，并在环境中插入一个 FunEntry 条目
    temp::Label *fun_label = temp::LabelFactory::NewLabel();
    function_record[name] = fun_label;

    type::Ty* result_ty = function->result_ ? tenv->Look(function->result_) : type::VoidTy::Instance();
    type::TyList* formal_tys = function->params_->MakeFormalTyList(tenv, errormsg);

    std::vector<bool> formal_escapes{true};
    for (auto param : function->params_->GetList()) 
      formal_escapes.push_back(param->escape_);

    frame::Frame* new_frame = frame::NewFrame(fun_label, formal_escapes);
    tr::Level *new_level = new tr::Level(new_frame, level);

    venv->Enter(function->name_, new env::FunEntry(new_level, fun_label, formal_tys, result_ty));
  }

  // Second pass: Translate function bodies
  //翻译函数体
  for (FunDec* function : functions_->GetList()) {
    env::FunEntry *func_ent = static_cast<env::FunEntry *>(venv->Look(function->name_));
    tr::Level *new_level = func_ent->level_;
    frame::Frame* new_frame = new_level->frame_;
    const auto &formal_access = new_frame->formal_access_;
    type::Ty* result_ty = func_ent->result_;
    type::TyList* formal_tys = func_ent->formals_;

    venv->BeginScope();

    //将函数的参数加入环境中，以便在函数体中能够访问它们
    auto param_it = function->params_->GetList().cbegin();
    auto formal_ty_it = formal_tys->GetList().cbegin();
    auto acc_it = formal_access.cbegin() + 1;  // Skip the static link
    for (; formal_ty_it != formal_tys->GetList().cend(); param_it++, formal_ty_it++, acc_it++) {
      venv->Enter((*param_it)->name_, new env::VarEntry(new tr::Access(new_level, *acc_it), *formal_ty_it));
    }

    // 翻译函数体
    tr::ExpAndTy* body_expty = function->body_->Translate(venv, tenv, new_level, label, errormsg);
    if (!function->result_ && typeid(*body_expty->ty_) != typeid(type::VoidTy)) {
      errormsg->Error(function->body_->pos_, "procedure returns value");
    } else if (function->result_ && !body_expty->ty_->IsSameType(result_ty)) {
      errormsg->Error(function->body_->pos_, "return type of function %s mismatch", function->name_->Name().data());
    }

    //处理完函数体后，退出作用域，清理局部变量
    tree::Stm *body_stm = frame::ProcEntryExit1(new_frame, 
                            new tree::MoveStm(new tree::TempExp(reg_manager->ReturnValue()), 
                                              body_expty->exp_->UnEx()));
    tr::ProcEntryExit(new_level, new tr::NxExp(body_stm));

    venv->EndScope();
  }

  return new tr::NxExp(new tree::ExpStm(new tree::ConstExp(0)));
}

tr::Exp *VarDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                           tr::Level *level, temp::Label *label,
                           err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  /*Unsure*/
  type::Ty* ty = nullptr;

  //在类型环境中查找该类型
  if (typ_) {
    ty = tenv->Look(typ_);
    if (!ty) {
      errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
      return new tr::ExExp(new tree::ConstExp(0));
    }
  }

  //调用初始值表达式的 Translate 方法，得到初始值的类型和表达式
  tr::ExpAndTy *init_expty = init_->Translate(venv, tenv, level, label, errormsg);

  //如果变量声明了类型，检查初始值的类型是否与声明的类型匹配
  if (typ_ && !init_expty->ty_->IsSameType(ty)) {
    errormsg->Error(pos_, "type mismatch");
    return new tr::ExExp(new tree::ConstExp(0));
  }

  //为变量分配存储位置，并将其添加到符号表中
  tr::Access *var_acc = tr::Access::AllocLocal(level, escape_);
  venv->Enter(var_, new env::VarEntry(var_acc, init_expty->ty_));

  //生成变量声明的中间代码，表示将初始值存储到变量的存储位置
  tree::Exp *acc_exp = frame::AccessCurrentExp(var_acc->access_, level->frame_);
  tree::Stm *dec_stm = new tree::MoveStm(acc_exp, init_expty->exp_->UnEx());

  return new tr::NxExp(dec_stm);
}

//类型声明
tr::Exp *TypeDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                            tr::Level *level, temp::Label *label,
                            err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  /*Unsure*/
  //创建类型记录表
  std::unordered_map<std::string, int> typeRecord;

  // First pass: check for duplicate type names and enter incomplete types into the environment
  //检查重复类型名并在环境中插入不完全类型
  for (NameAndTy* nameAndTy : types_->GetList()) {
    const std::string &typeName = nameAndTy->name_->Name();
    if (typeRecord.count(typeName)) {
      errormsg->Error(nameAndTy->ty_->pos_, "two types have the same name");
      return new tr::ExExp(new tree::ConstExp(0));
    }
    typeRecord[typeName] = 1;
    tenv->Enter(nameAndTy->name_, new type::NameTy(nameAndTy->name_, nullptr));
  }

  // Second pass: resolve type definitions
  //遍历类型列表，解析每个类型的定义，并更新不完全类型
  for (NameAndTy* nameAndTy : types_->GetList()) {
    auto* tenv_ty = static_cast<type::NameTy*>(tenv->Look(nameAndTy->name_));
    tenv_ty->ty_ = nameAndTy->ty_->Translate(tenv, errormsg);
  }

  // Third pass: detect cycles
  //遍历类型列表，检测类型定义中的循环引用
  for (NameAndTy* nameAndTy : types_->GetList()) {
    type::Ty* ty = static_cast<type::NameTy*>(tenv->Look(nameAndTy->name_))->ty_;
    while (typeid(*ty) == typeid(type::NameTy)) {
      auto* tenv_ty = static_cast<type::NameTy*>(ty);
      if (tenv_ty->sym_->Name() == nameAndTy->name_->Name()) {
        errormsg->Error(nameAndTy->ty_->pos_, "illegal type cycle");
        return new tr::ExExp(new tree::ConstExp(0));
      }
      ty = tenv_ty->ty_;
    }
  }

  return new tr::NxExp(new tree::ExpStm(new tree::ConstExp(0)));
}

type::Ty *NameTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return tenv->Look(name_);
}

type::Ty *RecordTy::Translate(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  //生成记录类型的字段列表
  type::FieldList* fieldList = record_->MakeFieldList(tenv, errormsg);
  return new type::RecordTy(fieldList);
}

type::Ty *ArrayTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new type::ArrayTy(tenv->Look(array_));
}

} // namespace absyn
