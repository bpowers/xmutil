#ifndef _XMUTIL_VENSIM_VENSIMPARSE_H
#define _XMUTIL_VENSIM_VENSIMPARSE_H
#include <boost/iostreams/device/mapped_file.hpp>
#include <iostream>
#include <string>
#include "../Function/Function.h"
#include "../Symbol/Equation.h"
#include "../Symbol/Parse.h"
#include "../Symbol/Symbol.h"
#include "../Symbol/Units.h"
#include "VensimLex.h"

#define BUFLEN 4096  // for reading sketch info

class VensimView;

class VensimParseSyntaxError {
public:
  std::string str;
};

class VensimParse {
public:
  VensimParse(Model *model);
  ~VensimParse(void);
  void ReadyFunctions();
  bool ProcessFile(const std::string &filename);
  inline int yylex(void) {
    return mVensimLex.yylex();
  }
  int yyerror(const char *str);
  Equation *AddEq(LeftHandSide *lhs, Expression *ex, ExpressionList *exl, int tok);
  Equation *AddTable(LeftHandSide *lhs, Expression *ex, ExpressionTable *table, bool legacy);
  inline SymbolNameSpace *GetSymbolNameSpace(void) {
    return pSymbolNameSpace;
  }
  Variable *InsertVariable(const std::string &name);
  Variable *FindVariable(const std::string &name);
  Units *InsertUnits(const std::string &name);
  UnitExpression *InsertUnitExpression(Units *u);
  void AddFullEq(Equation *eq, UnitExpression *un);
  LeftHandSide *AddExceptInterp(ExpressionVariable *var, SymbolListList *except, int interpmode);
  SymbolList *SymList(SymbolList *in, Variable *add, bool bang, Variable *end);
  SymbolList *MapSymList(SymbolList *in, Variable *range, SymbolList *list);
  UnitExpression *UnitsDiv(UnitExpression *num, UnitExpression *denom);
  UnitExpression *UnitsMult(UnitExpression *f, UnitExpression *s);
  UnitExpression *UnitsRange(UnitExpression *e, double minval, double maxval, double increment);
  SymbolListList *ChainSublist(SymbolListList *sll, SymbolList *nsl);
  ExpressionList *ChainExpressionList(ExpressionList *el, Expression *e);
  Expression *NumExpression(double num);
  Expression *LiteralExpression(const char *lit);
  ExpressionVariable *VarExpression(Variable *var, SymbolList *subs);
  ExpressionSymbolList *SymlistExpression(SymbolList *sym, SymbolList *map);
  Expression *OperatorExpression(int oper, Expression *exp1, Expression *exp2);
  Expression *FunctionExpression(Function *func, ExpressionList *eargs);
  Expression *LookupExpression(ExpressionVariable *var, Expression *exp);
  ExpressionTable *TablePairs(ExpressionTable *table, double x, double y);
  ExpressionTable *XYTableVec(ExpressionTable *table, double val);
  ExpressionTable *TableRange(ExpressionTable *table, double x1, double y1, double x2, double y2);
  void MacroStart();
  void MacroExpression(Variable *macro, ExpressionList *margs);
  void MacroEnd();

  VensimLex &Lexer() {
    return mVensimLex;
  }
  char *GetInt(char *buf, int &val);
  char *GetString(char *buf, std::string &s);

private:
  bool FindNextEq(bool want_comment);

  Model *_model;
  std::string sFilename;
  boost::iostreams::mapped_file_source mfSource;
  VensimLex mVensimLex;
  VensimParseSyntaxError mSyntaxError;
  SymbolNameSpace *pSymbolNameSpace;
  SymbolNameSpace *pMainSymbolNameSpace;
  Variable *pActiveVar;
  bool mInMacro;
  std::vector<MacroFunction *> mMacroFunctions;
};

extern VensimParse *VPObject;

#endif