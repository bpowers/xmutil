#ifndef _XMUTIL_SYMBOL_VARIABLE_H
#define _XMUTIL_SYMBOL_VARIABLE_H
#include "Symbol.h"
#include "Equation.h"
#include "UnitExpression.h"
#include "../Function/State.h"
#include "../Function/Function.h"
#include <vector>

/* varibale content - the true stuff for variables without the associated symbol
  the indirect subclassing is necessary because the variable type is not necessarily
  known when it is entered into the symbol table - contrast this with Function which
  is all just a class

  It might also be possible to incorporate State directly into VariableContent, but
  right now it is convenient to completely clear states when analyzing a model

  VariableContent is an abstract class 
  */
enum XMILE_Type { XMILE_Type_UNKNOWN, XMILE_Type_AUX, XMILE_Type_STOCK, XMILE_Type_FLOW, XMILE_Type_ARRAY, XMILE_Type_ARRAY_ELM };
class VariableContent
{
public :
   VariableContent(void);
   virtual ~VariableContent(void) = 0 ;
   friend class Variable ;
   friend class VariableContentVar ;
   inline double Eval(ContextInfo *info) { return pState->GetValue(0) ; } // todo subscritps
   inline void SetInitialValue(int off,double val) { pState->SetInitialValue(off,val) ; }
   inline void SetActiveValue(int off,double val) { pState->SetActiveValue(off,val) ; }

   virtual bool CheckComputed(Symbol *parent,ContextInfo *info,bool first) { return true ; }
   virtual void Clear(void) ;
   virtual void AddEq(Equation *eq) { }
   virtual Equation *GetEquation(int pos) { return '\0' ; }
   virtual std::vector<Equation*> GetAllEquations() { return std::vector<Equation*>(); }
   virtual bool AddUnits(UnitExpression *un) { return false; }
   virtual void OutputComputable(ContextInfo *info) { assert(0) ; }
   virtual void CheckPlaceholderVars(Model *m) {}
   virtual void SetupState(ContextInfo *info) {} // returns number of entries in state vector required (states can also claim thier own storage)
   virtual void SetAlternateName(const std::string &altname) { }
   virtual const std::string &GetAlternateName(void) { assert(0); std::string *s = new std::string;return *s; }
   virtual int SubscriptCount(std::vector<Symbol *> &elmlist) { return 0 ; }
   virtual XMILE_Type MarkFlows(SymbolNameSpace* sns, Variable *parent, XMILE_Type intype) { return XMILE_Type_UNKNOWN; }

protected :
   State *pState ; // different kinds of states, also compute flags are here along with var type

};
class VariableContentSub : public VariableContent
{
public :
   VariableContentSub(Variable *v)  {pFamily = v ; }
   ~VariableContentSub(void) {}
private:
   Variable *pFamily ;
   std::vector<Variable *>vElements ; // includes count 
} ;
class VariableContentElm : public VariableContent
{
public :
   VariableContentElm(Variable *v)  {pFamily = v ; }
   ~VariableContentElm(void) {}
private:
   Variable *pFamily ;
   int iValue ; /* 1 based value */
} ;
class VariableContentVar : public VariableContent
{
public :
   VariableContentVar(void)  { pUnits = '\0' ; }
   ~VariableContentVar(void) {}
   //friend class Variable ;



   bool CheckComputed(Symbol *parent,ContextInfo *info,bool first) ;
   void Clear(void) ;
   void AddEq(Equation *eq) { vEquations.push_back(eq) ; }
   virtual Equation *GetEquation(int pos) { return vEquations[pos] ; }
   virtual std::vector<Equation*> GetAllEquations() { return vEquations; }
   bool AddUnits(UnitExpression *un) {if(!pUnits) { pUnits=un;return true;} return false ; }
   void OutputComputable(ContextInfo *info) { *info << sAlternateName ; }
   void CheckPlaceholderVars(Model *m) ;
   void SetupState(ContextInfo *info) ; // returns number of entries in state vector required (states can also claim thier own storage)
   void SetAlternateName(const std::string &altname) { sAlternateName = altname ; }
   const std::string &GetAlternateName(void) { return sAlternateName ; }
   int SubscriptCount(std::vector<Symbol *> &elmlist) ;


protected :
   std::vector<Variable*>vSubscripts ; // for regular variables the family's for subscripts the family (possibly self) follwed by elements
   std::vector<Equation*>vEquations ;
   std::string Comment ; // arbitrary UTF8 string
   std::string sAlternateName ; // for writing out equations as computer code
   UnitExpression *pUnits ; // units could be attached to equations
};

class Variable :
   public Symbol
{
public:
   Variable(SymbolNameSpace *sns,const std::string &name);
   ~Variable(void);
   // virtual functions
   inline SYMTYPE isType(void) {return Symtype_Variable ; }
   // virtuals passed on to content - need to keep pVariableContent populated before calling
   bool CheckComputed(ContextInfo *info,bool first) {if(pVariableContent)return pVariableContent->CheckComputed(this,info,first) ; return false ; }  
   void CheckPlaceholderVars(Model *m) {if(pVariableContent)pVariableContent->CheckPlaceholderVars(m) ;}
   void SetupState(ContextInfo *info) {if(pVariableContent)pVariableContent->SetupState(info) ;} 
   int SubscriptCount(std::vector<Symbol *> &elmlist) { return pVariableContent?pVariableContent->SubscriptCount(elmlist):0; }
   // passthrough calls - many of these are virtual in VariableContent or passed through to yet another class
   void AddEq(Equation *eq) ;
   inline Equation *GetEquation(int pos) { return pVariableContent->GetEquation(pos) ; }
   std::vector<Equation*> GetAllEquations() { return pVariableContent->GetAllEquations(); }
   inline bool AddUnits(UnitExpression *un) { return pVariableContent->AddUnits(un); }
   inline void OutputComputable(ContextInfo *info) { pVariableContent->OutputComputable(info) ; }
   inline double Eval(ContextInfo *info) { return pVariableContent->Eval(info) ;  }
   inline void SetInitialValue(int off,double val) { pVariableContent->SetInitialValue(off,val) ; }
   inline void SetActiveValue(int off,double val) { pVariableContent->SetActiveValue(off,val) ; }
   inline void SetAlternateName(const std::string &altname) { pVariableContent->SetAlternateName(altname) ; }
   inline const std::string &GetAlternateName(void) { return pVariableContent->GetAlternateName() ; }

   XMILE_Type MarkFlows(SymbolNameSpace* sns); // mark the variableType of inflows/outflows
   XMILE_Type VariableType() { return mVariableType; }
   void SetVariableType(XMILE_Type t) { mVariableType = t; }

   // for other function calles
   inline VariableContent *Content(void) { return pVariableContent ; }
   void SetContent(VariableContent *v) { pVariableContent = v ; } 
   // virtual 
private :
   VariableContent *pVariableContent ; // dependent on variable type which is not known on instantiation
   XMILE_Type mVariableType;
} ;



#endif