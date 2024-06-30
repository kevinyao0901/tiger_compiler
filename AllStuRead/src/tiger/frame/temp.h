#ifndef TIGER_FRAME_TEMP_H_
#define TIGER_FRAME_TEMP_H_

#include "tiger/symbol/symbol.h"

#include <list>

//p100,用于管理临时变量和标签的类和函数
namespace temp {

//Label 是 sym::Symbol 的别名，用于表示标签。LabelFactory 类用于创建和管理标签
using Label = sym::Symbol;

class LabelFactory {
public:
  static Label *NewLabel();//创建一个新的标签
  static Label *NamedLabel(std::string_view name);//创建一个带有指定名称的标签
  static std::string LabelString(Label *s);//返回标签的字符串表示

private:
  int label_id_ = 0;
  static LabelFactory label_factory;
};


/*Temp 类表示一个临时变量，包含一个整数成员变量 num_ 表示临时变量的编号。TempFactory 类用于创建和管理临时变量。*/
class Temp {
  friend class TempFactory;

public:
  [[nodiscard]] int Int() const;

private:
  int num_;
  explicit Temp(int num) : num_(num) {}
};

class TempFactory {
public:
  static Temp *NewTemp();

private:
  int temp_id_ = 100;
  static TempFactory temp_factory;
};

/*Map 类用于管理临时变量到字符串的映射*/
class Map {
public:
  void Enter(Temp *t, std::string *s);
  std::string *Look(Temp *t);
  void DumpMap(FILE *out);

  static Map *Empty();
  static Map *Name();
  static Map *LayerMap(Map *over, Map *under);

private:
  tab::Table<Temp, std::string> *tab_;
  Map *under_;

  Map() : tab_(new tab::Table<Temp, std::string>()), under_(nullptr) {}
  Map(tab::Table<Temp, std::string> *tab, Map *under)
      : tab_(tab), under_(under) {}
};

/*表示一个临时变量列表*/
class TempList {
public:
  explicit TempList(Temp *t) : temp_list_({t}) {}
  TempList(std::initializer_list<Temp *> list) : temp_list_(list) {}
  TempList() = default;
  void Append(Temp *t) { temp_list_.push_back(t); }
  [[nodiscard]] Temp *NthTemp(int i) const;
  [[nodiscard]] const std::list<Temp *> &GetList() const { return temp_list_; }
  void CatList(const TempList *tl)
  {
    if (!tl || tl->temp_list_.empty()) 
      return;
    temp_list_.insert(temp_list_.end(), tl->temp_list_.begin(),tl->temp_list_.end());
  }

  bool Contain(Temp *t) const;
  //void CatList(const TempList *tl);
  TempList *Union(const TempList *tl) const;
  TempList *Diff(const TempList *tl) const;
  bool IdentitalTo(const TempList *tl) const;
  std::list<Temp *>::const_iterator Replace(
    std::list<Temp *>::const_iterator pos, Temp *temp);

private:
  std::list<Temp *> temp_list_;
};


} // namespace temp

#endif