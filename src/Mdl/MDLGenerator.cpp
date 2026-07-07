#include "MDLGenerator.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_set>

#include "../Function/Function.h"
#include "../Model.h"
#include "../Symbol/Equation.h"
#include "../Symbol/Expression.h"
#include "../Symbol/ExpressionList.h"
#include "../Symbol/LeftHandSide.h"
#include "../Symbol/Parse.h"
#include "../Symbol/SymbolList.h"
#include "../Symbol/SymbolNameSpace.h"
#include "../Symbol/UnitExpression.h"
#include "../Symbol/Variable.h"
#include "../Vensim/VensimView.h"
#include "../XMUtil.h"
#include "MDLFormat.h"

// The generated Vensim parser header declares vpyylval as YYSTYPE; the codebase
// convention (see VensimLex.cpp / Expression.cpp) is to alias YYSTYPE to the
// ParseUnion from Parse.h before including it. We only need the VPTT_* logical
// operator tokens, but the header is a single unit.
#define YYSTYPE ParseUnion
#include "../Vensim/VYacc.tab.hpp"

namespace {

// Follow ExpressionParen -> child until a non-paren node. The walker derives all
// parentheses from operator precedence, so parser-supplied paren nodes are
// dropped here and re-introduced only where precedence demands them.
Expression *Unwrap(Expression *e) {
  while (e && e->GetType() == EXPTYPE_Operator && e->GetBefore() && std::string(e->GetBefore()) == "(") {
    e = e->GetArg(0);
  }
  return e;
}

// Binary-operator precedence, or 100 for any node that is not a binary operator
// (so it never forces a child to be parenthesized).
//
// The values follow the *xmutil* Vensim grammar (src/Vensim/VYacc.y, low to
// high: + - < :OR: < comparison < :AND: < * / < ^), NOT simlin's writer.rs
// values. The walker re-parses through xmutil's own parser, so parentheses must
// be derived from the precedence that parser will use; simlin's AST came from a
// different parser with a different precedence lattice. The notable
// xmutil-specific orderings: comparison binds looser than + and -, :AND: binds
// tighter than comparison, and :OR: binds looser than comparison.
int MdlPrecedence(Expression *e) {
  e = Unwrap(e);
  if (!e)
    return 100;
  if (e->GetType() == EXPTYPE_Operator) {
    const char *op = e->GetOperator();
    if (op && *op) {  // binary arithmetic has an operator and an empty "before"
      switch (op[0]) {
      case '+':
      case '-':
        return 1;
      case '*':
      case '/':
        return 5;
      case '^':
        return 7;
      }
    }
    return 100;  // a paren or unary-minus node is not a binary parent
  }
  if (e->GetType() == EXPTYPE_Logical) {
    int o = static_cast<ExpressionLogical *>(e)->LogicalOperator();
    if (o == VPTT_or)
      return 2;
    if (o == '=' || o == '<' || o == '>' || o == VPTT_le || o == VPTT_ge || o == VPTT_ne)
      return 3;
    if (o == VPTT_and)
      return 4;
    return 100;  // VPTT_not is unary
  }
  return 100;
}

bool IsBinaryOp(Expression *e) {
  return MdlPrecedence(e) < 100;
}

// Grammar precedence of a unary node, or 100 for a non-unary node. The xmutil
// grammar (src/Vensim/VYacc.y:76-82, low to high) places unary minus at the
// LOWEST level (1, the `'-' exp` rule shares the level of binary +/-) and :NOT:
// at level 6. A unary node binds looser than the operators above it, so a unary
// left operand of a tighter binary parent must be parenthesized or it re-parses
// the wrong way (e.g. `-a ^ b` parses as -(a ^ b)).
int MdlUnaryPrecedence(Expression *e) {
  e = Unwrap(e);
  if (!e)
    return 100;
  if (e->GetType() == EXPTYPE_Operator) {
    const char *op = e->GetOperator();
    const char *before = e->GetBefore();
    if ((!op || !*op) && before && before[0] == '-')
      return 1;  // unary minus
  }
  if (e->GetType() == EXPTYPE_Logical && static_cast<ExpressionLogical *>(e)->LogicalOperator() == VPTT_not)
    return 6;  // :NOT:
  return 100;
}

// A unary operator is either arithmetic unary minus or a logical :NOT: --
// exactly the nodes MdlUnaryPrecedence classifies below 100, mirroring the
// IsBinaryOp/MdlPrecedence pair above.
bool IsUnaryOp(Expression *e) {
  return MdlUnaryPrecedence(e) < 100;
}

// Map a logical/comparison operator token to its Vensim spelling.
const char *LogicalSymbol(int oper) {
  switch (oper) {
  case '=':
    return "=";
  case '<':
    return "<";
  case '>':
    return ">";
  case VPTT_ne:
    return "<>";
  case VPTT_le:
    return "<=";
  case VPTT_ge:
    return ">=";
  case VPTT_and:
    return ":AND:";
  case VPTT_or:
    return ":OR:";
  case VPTT_not:
    return ":NOT:";
  default:
    return "?";
  }
}

// True if `name` is the synthetic-net-flow name MarkStockFlows mints for a stock
// named `stock` -- exactly "<stock> net flow" or "<stock> net flow_<n>" (the _n
// disambiguation suffix; see Variable::MarkStockFlows). This is the sole producer
// of that pattern, so a name match reliably identifies the reader's artifact.
bool IsSyntheticNetFlowName(const std::string &name, const std::string &stock) {
  const std::string base = stock + " net flow";
  if (name == base)
    return true;
  if (name.size() <= base.size() + 1 || name.compare(0, base.size(), base) != 0 || name[base.size()] != '_')
    return false;
  for (size_t i = base.size() + 1; i < name.size(); ++i) {
    if (!std::isdigit(static_cast<unsigned char>(name[i])))
      return false;
  }
  return true;
}

// If this stock's net flow is a reader-synthesized "<stock> net flow" variable
// (the non-clean-+/- case in Variable::MarkStockFlows: a single synthetic inflow,
// no outflows), return that synthetic flow variable; otherwise nullptr. The
// synthetic flow's i-th stored equation holds the ORIGINAL net-flow expression of
// the stock's i-th equation (MarkStockFlows builds them in lockstep), which is
// what we inline back into INTEG so the emitted .mdl matches the user's input.
Variable *SyntheticNetFlowFor(Variable *stock) {
  if (stock->Outflows().size() != 0 || stock->Inflows().size() != 1)
    return nullptr;
  Variable *flow = stock->Inflows()[0];
  if (!flow || !IsSyntheticNetFlowName(flow->GetName(), stock->GetName()))
    return nullptr;
  return flow;
}

// The reader-synthesized "<stock> net flow" variables in `vars`. EmitStockEntry
// inlines each one's expression back into its stock's INTEG, so any standalone
// emission of these would leak a reader artifact and (for per-element stocks)
// make the re-parse synthesize a duplicate. Both the main equation loop
// (GenerateEquations) and the macro-body loop (GenerateMacros) must suppress
// them, so the collection lives here as a shared helper rather than inline.
std::unordered_set<Variable *> CollectSyntheticNetFlows(const std::vector<Variable *> &vars) {
  std::unordered_set<Variable *> synth;
  for (Variable *v : vars) {
    if (v->VariableType() == XMILE_Type_STOCK) {
      if (Variable *flow = SyntheticNetFlowFor(v))
        synth.insert(flow);
    }
  }
  return synth;
}

}  // namespace

MDLGenerator::MDLGenerator(Model *model) : _model(model) {
}

bool MDLGenerator::IsControlVar(const std::string &name) {
  // Compare under Vensim's own identifier equivalence: ToLowerSpace lowercases
  // and treats '_' / whitespace runs as a single space, so a control variable
  // written with underscores (FINAL_TIME, the SDEverywhere/PySD convention) is
  // recognized as the same name GenerateControl looks up via the namespace
  // (ns->Find("FINAL TIME")). A plain case-fold compare would miss the
  // underscore form, leaving the control to leak into the regular equation
  // section AND be re-emitted in .Control -- a duplicate definition on re-parse.
  std::string *canon = SymbolNameSpace::ToLowerSpace(name);
  bool is_control = *canon == "initial time" || *canon == "final time" || *canon == "time step" || *canon == "saveper";
  delete canon;
  return is_control;
}

std::string MDLGenerator::Print() {
  // The body is assembled with bare '\n' and converted to CRLF once at the end
  // (matching simlin writer.rs:2760); emitting CRLF inline would require every
  // helper to know the line ending. Order: header, macro blocks, grouped +
  // ungrouped non-control equations, the .Control group, the sketch frame, then
  // the trailing :L settings section (which the parser only reads after the
  // sketch terminator ///---\\\). Macros come first because the Vensim parser
  // (VensimParse.cpp:650-665) defines a :MACRO: block's name in the main
  // namespace as it reads it, so the macro must be declared before the main
  // equations that call it -- matching simlin writer.rs and the XMILE generator.
  std::string out;
  out += "{UTF-8}\n";
  GenerateMacros(out);
  GenerateEquations(out);
  GenerateControl(out);
  GenerateSketch(out);
  GenerateSettings(out);

  std::string crlf;
  crlf.reserve(out.size() + out.size() / 16);
  for (char c : out) {
    if (c == '\n')
      crlf += "\r\n";
    else
      crlf += c;
  }
  return crlf;
}

void MDLGenerator::EmitVariableRecord(std::string &out, int uid, VensimVariableElement *e) {
  // 10,uid,name,x,y,w,h,shape,bits,... (VensimView.cpp:6-49). The reader takes
  // _attached from shape bit5 and _ghost from bits bit0 INVERTED (bit0 set =>
  // not a ghost), and resolves the Variable* by name via FindVariable, so the
  // name must match the equation-section spelling (FormatMDLIdent).
  int shape = 3;
  if (e->Attached())
    shape |= (1 << 5);
  int bits = e->Ghost(nullptr, false) ? 2 : 3;
  out += "10," + std::to_string(uid) + "," + mdl::FormatMDLIdent(e->GetVariable()->GetName()) + ",";
  out += std::to_string(e->X()) + "," + std::to_string(e->Y()) + "," + std::to_string(e->Width()) + "," +
         std::to_string(e->Height()) + ",";
  out += std::to_string(shape) + "," + std::to_string(bits) + ",0,0,0,0,0,0\n";
}

void MDLGenerator::EmitValveRecord(std::string &out, int uid, VensimValveElement *e) {
  // 11,uid,name,x,y,w,h,shape (VensimView.cpp:84-98). The name token is read and
  // discarded; only shape bit5 (attached) is meaningful. 34 = 32|2 sets bit5.
  int shape = e->Attached() ? 34 : 2;
  out += "11," + std::to_string(uid) + ",0," + std::to_string(e->X()) + "," + std::to_string(e->Y()) + "," +
         std::to_string(e->Width()) + "," + std::to_string(e->Height()) + "," + std::to_string(shape) + "\n";
}

void MDLGenerator::EmitCommentRecord(std::string &out, int uid, VensimCommentElement *e) {
  // 12,uid,name,x,y,w,h,shape,bits (VensimView.cpp:64-82). The reader consumes
  // the following line as scratch text iff bits bit2 is set, so bits is emitted
  // as 0 (bit2 clear) and no trailing text line follows.
  out += "12," + std::to_string(uid) + ",0," + std::to_string(e->X()) + "," + std::to_string(e->Y()) + "," +
         std::to_string(e->Width()) + "," + std::to_string(e->Height()) + ",8,0\n";
}

void MDLGenerator::EmitConnectorRecord(std::string &out, int uid, VensimConnectorElement *e) {
  // 1,uid,from,to,<ign>,<ign>,POL,<6 ign>,N|(x,y)| (VensimView.cpp:115-144).
  // Invalidated connectors carry from==to==0 (Invalidate()) and are dropped.
  // POL is the polarity char's ASCII code as a decimal int: 43='+', 45='-',
  // 0=none. The six ignored fields after POL match the observed Vensim defaults
  // (the empty field before the point blob is required so the blob lands in the
  // final GetString slot). N is forced to 1 point by the reader.
  if (e->From() == 0 && e->To() == 0)
    return;
  int pol = 0;
  char p = e->Polarity();
  if (p == '+')
    pol = 43;
  else if (p == '-')
    pol = 45;
  out += "1," + std::to_string(uid) + "," + std::to_string(e->From()) + "," + std::to_string(e->To()) + ",1,0," +
         std::to_string(pol) + ",0,0,64,0,-1--1--1,,1|(" + std::to_string(e->X()) + "," + std::to_string(e->Y()) +
         ")|\n";
}

void MDLGenerator::GenerateSketch(std::string &out) {
  // The opener marker (VensimParse.cpp:285), a V300 version line (checked at
  // :288), a *Title line (SetTitle(buf+1) at :298), the Vensim default
  // font/cosmetic line, then per-element records, then a single terminator
  // marker (:327) the settings reader requires before it will read the :L
  // block. The opener marker is the 9-char sequence backslash-backslash-
  // backslash-dash-dash-dash-slash-slash-slash; the terminator is its mirror
  // image. (In the C++ string literals below each backslash is doubled.)
  const char *kVersion = "V300  Do not put anything below this section - it will be ignored";
  const char *kFontLine = "$192-192-192,0,Helvetica|10|B|0-0-0|0-0-0|-1--1--1|-1--1--1|96,96,100,0";

  // xmutil only ever creates VensimView (VensimParse.cpp:293); Dynamo creates
  // none. Anything else would not carry Vensim records, so it is filtered out.
  std::vector<VensimView *> views;
  for (View *v : _model->Views()) {
    VensimView *vv = dynamic_cast<VensimView *>(v);
    if (vv)
      views.push_back(vv);
  }

  // One frame header per view: the opener marker, version line, *Title, and
  // the default font/cosmetic line.
  auto emitFrameHeader = [&](const std::string &title) {
    out += "\\\\\\---///\n";
    out += std::string(kVersion) + "\n";
    out += "*" + title + "\n";
    out += std::string(kFontLine) + "\n";
  };

  if (views.empty()) {
    // No geometry to serialize: emit a single empty frame so the terminator
    // marker and the settings section re-parse. Fabricates no coordinates
    // (zero element records).
    emitFrameHeader("View 1");
    out += "///---\\\\\\\n";
    return;
  }

  for (VensimView *vv : views) {
    emitFrameHeader(vv->Title().empty() ? "View 1" : vv->Title());

    // The array index is the on-wire UID. NULL slots (a sparse view) are skipped
    // but their index is still consumed, so a connector's From()/To() UIDs --
    // which are these indices -- remain valid after re-parse.
    VensimViewElements &elems = vv->Elements();
    for (size_t uid = 0; uid < elems.size(); uid++) {
      VensimViewElement *e = elems[uid];
      if (!e)
        continue;
      switch (e->Type()) {
      case VensimViewElement::ElementTypeVARIABLE:
        EmitVariableRecord(out, (int)uid, static_cast<VensimVariableElement *>(e));
        break;
      case VensimViewElement::ElementTypeVALVE:
        EmitValveRecord(out, (int)uid, static_cast<VensimValveElement *>(e));
        break;
      case VensimViewElement::ElementTypeCOMMENT:
        EmitCommentRecord(out, (int)uid, static_cast<VensimCommentElement *>(e));
        break;
      case VensimViewElement::ElementTypeCONNECTOR:
        EmitConnectorRecord(out, (int)uid, static_cast<VensimConnectorElement *>(e));
        break;
      }
    }
  }

  out += "///---\\\\\\\n";
}

void MDLGenerator::GenerateSettings(std::string &out) {
  // Port of simlin write_settings_section (writer.rs:3217-3282). The marker line
  // begins with the literal DEL byte 0x7F that VensimParse.cpp:324 scans for; the
  // only lines xmutil's parser consumes are 22: (unit equivalences) and 15:
  // (integration method). The remaining cosmetic lines are emitted verbatim so a
  // real Vensim can open the file; xmutil ignores them.
  out += ":L";
  out += '\x7F';
  out += "<%^E!@\n";

  for (const std::string &eq : _model->UnitEquivs())
    out += "22:" + eq + "\n";

  // Vensim 15: integration code: Euler->0, RK4->1, RK2->3 (the inverse of
  // VensimParse.cpp:334-348, which also accepts 2->Euler/5->RK4/4->RK2).
  int code = 0;
  if (_model->IntegrationType() == Integration_Type_RK4)
    code = 1;
  else if (_model->IntegrationType() == Integration_Type_RK2)
    code = 3;
  out += "15:0,0,0," + std::to_string(code) + ",0,0\n";

  out += "19:100,0\n27:0,\n34:0,\n4:Time\n35:Date\n36:YYYY-MM-DD\n";
  out += "37:2000\n38:1\n39:1\n40:2\n41:0\n42:0\n";

  // 24/25/26: the display time range (start, stop, stop).
  double start = _model->GetConstanValue("INITIAL TIME", _model->initial_time());
  double stop = _model->GetConstanValue("FINAL TIME", _model->final_time());
  out += "24:" + mdl::FormatMDLNumber(start) + "\n";
  out += "25:" + mdl::FormatMDLNumber(stop) + "\n";
  out += "26:" + mdl::FormatMDLNumber(stop) + "\n";
}

void MDLGenerator::GenerateMacros(std::string &out) {
  // Emit each Vensim macro as a ":MACRO: name(args) ... :END OF MACRO:" block.
  // The body is the macro's own local namespace (mf->NameSpace()), iterated and
  // emitted with the SAME GenerateVariableEntry the main model uses, so a
  // macro-body stock emits as INTEG(...) and an arrayed body var keeps its
  // subscripts. The control-var / group / banner handling of GenerateEquations is
  // deliberately NOT applied here: a macro has no .Control group and no banners,
  // so calling GenerateVariableEntry directly is correct and avoids that logic.
  for (MacroFunction *mf : _model->MacroFunctions()) {
    out += ":MACRO: " + mdl::FormatMDLIdent(mf->GetName()) + "(";
    ExpressionList *args = mf->Args();
    int n = args ? args->Length() : 0;
    for (int i = 0; i < n; i++) {
      if (i)
        out += ", ";
      // Formal parameters parse as ExpressionVariable; RenderExpression emits
      // the bare identifier (no subscripts), which is the macro arg-list spelling.
      out += RenderExpression(args->GetExp(i));
    }
    out += ")\n";

    // Already name-ordered: Model::GetVariables sorts by SymbolNameLess, which
    // is this same by-name compare. That is where the byte-stability invariant
    // lives -- a second sort here would only make two places claim it.
    std::vector<Variable *> body = _model->GetVariables(mf->NameSpace());

    // A macro-body stock whose net flow did not decompose into a clean +/- of
    // named flows gets a reader-synthesized "<stock> net flow" variable in the
    // macro namespace (MarkStockFlows runs per-macro via MarkVariableTypes). That
    // synthetic flow's expression is inlined back into the stock's INTEG by
    // EmitStockEntry, so emitting it standalone here would double-emit it (and on
    // re-parse re-synthesize a duplicate) -- exactly as in the main model, so the
    // same suppression applies.
    std::unordered_set<Variable *> synthFlows = CollectSyntheticNetFlows(body);

    for (Variable *v : body) {
      if (v->VariableType() == XMILE_Type_ARRAY_ELM)
        continue;
      if (synthFlows.count(v))
        continue;
      GenerateVariableEntry(out, v);
    }
    out += ":END OF MACRO:\n\n";
  }
}

void MDLGenerator::GenerateEquations(std::string &out) {
  // Name-ordered by Model::GetVariables, which is what makes a model emit
  // byte-stably regardless of hash-table order.
  std::vector<Variable *> vars = _model->GetVariables(nullptr);

  // Collect the reader-synthesized "<stock> net flow" variables so they are
  // suppressed below: EmitStockEntry inlines each one's expression back into its
  // stock's INTEG, so emitting the synthetic flow as a standalone variable would
  // both leak a reader artifact and (for per-element stocks) make the re-parse
  // synthesize a duplicate. See EmitStockEntry for the full rationale.
  std::unordered_set<Variable *> synthFlows = CollectSyntheticNetFlows(vars);

  // A variable is emitted in the main equation section unless it is suppressed
  // (Unwanted), an array element (emitted through its parent), a synthetic net
  // flow (inlined into its stock's INTEG), or a control variable (emitted only
  // by GenerateControl). Control vars are NOT Unwanted on the .mdl path, so the
  // explicit IsControlVar check is required in addition to the others.
  auto emittable = [&](Variable *v) {
    return !v->Unwanted() && v->VariableType() != XMILE_Type_ARRAY_ELM && !IsControlVar(v->GetName()) &&
           !synthFlows.count(v);
  };

  // Index every variable's owning group so an ungrouped variable can be
  // identified. A variable in no group must be emitted BEFORE the first group
  // banner: Vensim assigns a variable to the most recent banner above it, so
  // emitting ungrouped variables after the banners would absorb them into the
  // last group on re-parse (a real grouping-fidelity bug -- e.g. top-level
  // dimension definitions ending up inside the final user group).
  std::unordered_set<Variable *> grouped;
  for (ModelGroup *group : _model->Groups()) {
    if (StringMatch(group->sName, ".Control") || StringMatch(group->sName, "Control"))
      continue;
    for (Variable *v : group->vVariables) {
      if (v && emittable(v))
        grouped.insert(v);
    }
  }

  // Ungrouped, non-control variables first, so they precede every banner and
  // stay group-less across the round trip.
  for (Variable *v : vars) {
    if (!emittable(v))
      continue;
    if (grouped.count(v))
      continue;
    GenerateVariableEntry(out, v);
  }

  // Then each group's members under its banner (groups are visited in parse
  // order; members are sorted by name for byte-stable output). The .Control
  // group, if the source had one, is skipped here and re-emitted by
  // GenerateControl so the four control vars are always grouped consistently.
  for (ModelGroup *group : _model->Groups()) {
    if (StringMatch(group->sName, ".Control") || StringMatch(group->sName, "Control"))
      continue;

    std::vector<Variable *> members;
    for (Variable *v : group->vVariables) {
      if (v && emittable(v))
        members.push_back(v);
    }
    if (members.empty())
      continue;  // a banner that held only control vars contributes no entries
    std::sort(members.begin(), members.end(), [](Variable *a, Variable *b) { return a->GetName() < b->GetName(); });

    EmitGroupBanner(out, GroupBannerPath(group), "");
    for (Variable *v : members)
      GenerateVariableEntry(out, v);
  }
}

std::string MDLGenerator::GroupBannerPath(ModelGroup *group) const {
  // A Vensim group's identity IS its dotted path, and VensimParse recovers an
  // owner link by looking the parent path back up -- so a banner carrying only
  // the group's own name states no nesting at all, and the forest was lost on
  // every write.
  //
  // xmutil's ModelGroup name is the flattened path ('.' -> '-'), so for a model
  // that came from a .mdl the child's name already begins with its owner's name
  // plus a separator; stripping that prefix recovers the leaf and the emission
  // is byte-stable across round trips. For a model from XMILE -- where the name
  // and the nesting are independent claims -- there is no prefix to strip and
  // the leaf is the whole name, so the group is re-read under its path
  // (`Outer-Inner`). That rename is the .mdl format's, not a choice: a Vensim
  // group has no identity apart from where it sits.
  std::vector<ModelGroup *> chain;
  // Bounded by the group count rather than written as a plain walk: nothing
  // re-validates a Model on the way into a writer, and a pOwner cycle would
  // otherwise hang. Same reasoning as XMILEGenerator's owner prepass.
  const size_t limit = _model->Groups().size() + 1;
  for (ModelGroup *g = group; g && chain.size() < limit; g = g->pOwner)
    chain.push_back(g);

  std::string path;
  for (size_t i = chain.size(); i-- > 0;) {
    ModelGroup *g = chain[i];
    std::string leaf = g->sName;
    if (g->pOwner) {
      const std::string prefix = g->pOwner->sName + "-";
      if (leaf.size() > prefix.size() && leaf.compare(0, prefix.size(), prefix) == 0)
        leaf = leaf.substr(prefix.size());
    }
    // A '.' inside the leaf would read back as one more level of nesting than
    // the model has -- an invented parent and a renamed group. Fold it to the
    // same '-' the reader would have produced, so what is written and what is
    // read back are the same string.
    std::replace(leaf.begin(), leaf.end(), '.', '-');
    if (!path.empty())
      path += ".";
    path += leaf;
  }
  return path;
}

void MDLGenerator::EmitGroupBanner(std::string &out, const std::string &name, const std::string &doc) {
  // Port of simlin writer.rs:2934-2941. The 56 '*' bars are immediately followed
  // by '~' on the closing line, matching what the Vensim lexer scans for a group
  // banner (VensimLex.cpp:161-186).
  //
  // The group name is modeler-authored free text on a single banner line, so a
  // raw '|' or line break in it would corrupt the banner; sanitize it (#849).
  // The doc is only ever "" or a fixed safe literal at the two call sites
  // (GenerateEquations passes "", GenerateControl the control-group caption), so
  // it carries no modeler text and needs no sanitizing.
  const std::string bar(56, '*');
  std::string safeName = mdl::SanitizeFreeText(name, mdl::FreeTextLineMode::SingleLine, "");
  out += "\n" + bar + "\n\t" + safeName + "\n" + bar + "~\n\t\t" + doc + "\n\t|\n";
}

void MDLGenerator::GenerateControl(std::string &out) {
  // The four control variables are emitted as a dedicated .Control group so the
  // sim specs survive a round trip. Each is emitted from its actual Variable when
  // present (so SAVEPER = TIME STEP, and any real units/comments, round-trip);
  // only an absent control var is synthesized from its numeric value.
  EmitGroupBanner(out, ".Control", "Simulation Control Parameters");

  struct Control {
    const char *name;
    double fallback;
  };
  const Control controls[] = {
      {"INITIAL TIME", _model->initial_time()},
      {"FINAL TIME", _model->final_time()},
      {"TIME STEP", _model->dt()},
      {"SAVEPER", _model->dt()},
  };

  SymbolNameSpace *ns = _model->GetNameSpace();
  for (const Control &c : controls) {
    Symbol *sym = ns->Find(c.name);
    if (sym && sym->isType() == Symtype_Variable) {
      GenerateVariableEntry(out, static_cast<Variable *>(sym));
    } else {
      // The control var is not in the namespace; synthesize a numeric entry from
      // its value so the .Control group is always complete.
      double value = _model->GetConstanValue(c.name, c.fallback);
      out += std::string(c.name) + " = " + mdl::FormatMDLNumber(value);
      out += "\n\t~~|\n";
    }
  }
}

void MDLGenerator::GenerateVariableEntry(std::string &out, Variable *v) {
  switch (v->VariableType()) {
  case XMILE_Type_AUX:
  case XMILE_Type_DELAYAUX:
  case XMILE_Type_FLOW: {
    // One entry per stored equation: a scalar yields one, an apply-to-all
    // yields one with a dimension subscript, and a per-element variable yields
    // N (each carrying its own LHS element subscript).
    for (Equation *eq : v->GetAllEquations()) {
      std::string lhs = mdl::FormatMDLIdent(v->GetName());
      SymbolList *subs = eq->GetLeft()->GetSubs();
      if (subs && subs->Length() > 0)
        lhs += RenderSubscripts(subs);

      Expression *rhsExpr = eq->GetExpression();
      // A standalone graphical function (parsed from "name(<points>)") stores the
      // ExpressionTable directly as its RHS. It is emitted as "name(<body>)" with
      // NO "=" -- the distinguishing Vensim syntax for a table variable. A
      // top-level WITH LOOKUP is an ExpressionLookup, not an ExpressionTable, so
      // it falls through to the normal "name = WITH LOOKUP(...)" path below.
      if (rhsExpr && rhsExpr->GetType() == EXPTYPE_Table) {
        // "name(<body>)" is juxtaposition, not an assignment: no "=".
        std::string body = mdl::WriteLookupBody(static_cast<ExpressionTable *>(rhsExpr));
        EmitWrappedEntry(out, lhs + "(" + body + ")", v);
        continue;
      }

      // :EXCEPT: is out of scope for v1: we do not read eq->GetLeft()'s
      // pExceptList, so a model using the [a]:EXCEPT:[b] form is filtered from
      // the corpus rather than emitted with a silently-dropped clause.
      std::string rhs = RenderExpression(rhsExpr);
      EmitWrappedEntry(out, lhs + " = " + rhs, v);
    }
    break;
  }
  case XMILE_Type_STOCK:
    EmitStockEntry(out, v);
    break;
  case XMILE_Type_ARRAY:
    EmitDimensionEntry(out, v);
    break;
  case XMILE_Type_UNKNOWN:
  default:
    // Untyped or unsupported: skip silently, matching XMILEGenerator.
    break;
  }
}

void MDLGenerator::EmitStockEntry(std::string &out, Variable *v) {
  // One entry per stored equation: a scalar stock yields one, an apply-to-all
  // one with a dimension subscript, and a per-element stock N (each with its own
  // net flow and initial value).
  //
  // The init value (INTEG arg 1) always comes from THIS equation's stored INTEG
  // node, rendered through the walker so a per-element stock keeps its own
  // per-element initial.
  //
  // The net flow (INTEG arg 0) is where the reader's stock/flow synthesis has to
  // be undone. MarkStockFlows leaves the stored arg 0 in one of two shapes:
  //   - a clean +/- of named flows (e.g. `inflow[Dim] - outflow[Dim]`): arg 0 is
  //     the user's own expression and is emitted verbatim, and
  //   - a reference to a reader-synthesized "<stock> net flow" variable (the
  //     non-clean case, AND -- because of the subscript bug noted in
  //     MarkStockFlows -- EVERY per-element stock, even one with clean per-element
  //     flows): the original net-flow expression was moved onto that synthetic
  //     variable's own equation, and arg 0 was rewritten to reference it.
  // For the second shape we inline the synthetic variable's matching stored
  // equation (its i-th equation is the i-th stock equation's original net flow,
  // built in lockstep by MarkStockFlows) rather than emitting the synthetic-flow
  // reference. Inlining reproduces the user's input exactly, so re-parsing
  // re-synthesizes the SAME single "<stock> net flow". Emitting the reference
  // instead would, for a per-element stock, make the re-parse hit the subscript
  // bug again and synthesize a DUPLICATE "<stock> net flow_1" (the arrayed-stock
  // review finding). The synthetic flow itself is suppressed from the equation
  // section (GenerateEquations) since its expression now lives inline; it is a
  // reader artifact, never user-authored, so the emitted .mdl stays clean.
  Variable *synth = SyntheticNetFlowFor(v);
  std::vector<Equation *> stockEqs = v->GetAllEquations();
  std::vector<Equation *> synthEqs = synth ? synth->GetAllEquations() : std::vector<Equation *>();
  // A Vensim stock carries its initial value as INTEG's second argument and has
  // no separate init equations. A Dynamo level instead becomes a one-argument
  // INTEGRATE (its net flow) plus a separate `N` init equation (the Dynamo
  // convention, mirrored by XMILEGenerator's init_eqns handling). The init
  // equations align by index with the stock equations, so the i-th init equation
  // supplies the i-th stock entry's initial value when INTEG itself has none.
  std::vector<Equation *> initEqs = v->GetAllInitEquations();
  for (size_t i = 0; i < stockEqs.size(); ++i) {
    Equation *eq = stockEqs[i];
    std::string lhs = mdl::FormatMDLIdent(v->GetName());
    SymbolList *subs = eq->GetLeft()->GetSubs();
    if (subs && subs->Length() > 0)
      lhs += RenderSubscripts(subs);

    std::string net = "0";
    std::string init = "0";
    Expression *rhs = eq->GetExpression();
    if (rhs && (rhs->GetType() == EXPTYPE_FunctionMemory || rhs->GetType() == EXPTYPE_Function)) {
      ExpressionFunction *fn = static_cast<ExpressionFunction *>(rhs);
      Function *f = fn->GetFunction();
      ExpressionList *args = fn->GetArgs();
      // The Vensim INTEG has two args (active, init); the Dynamo INTEGRATE has
      // one (the net flow). Accept either: the net flow is always available, and
      // the init comes from arg 1 when present, otherwise from the stock's init
      // equation.
      if (f && f->IsIntegrator() && args && args->Length() >= 1) {
        // Inline the synthetic flow's original net-flow expression when this
        // stock's flow was synthesized; otherwise emit arg 0 (the user's own
        // clean +/- of named flows) verbatim.
        if (synth && i < synthEqs.size())
          net = RenderExpression(synthEqs[i]->GetExpression());
        else
          net = RenderExpression(args->GetExp(0));
        if (net.empty())
          net = "0";

        if (args->Length() >= 2)
          init = RenderExpression(args->GetExp(1));
        else if (i < initEqs.size())
          init = RenderExpression(initEqs[i]->GetExpression());
      }
    }

    EmitWrappedEntry(out, lhs + " = INTEG(" + net + ", " + init + ")", v);
  }
}

void MDLGenerator::EmitDimensionEntry(std::string &out, Variable *v) {
  // A dimension/subrange is defined by a single equation whose RHS is an
  // ExpressionSymbolList: SymList() holds the element symbols (subranges are
  // already expanded to their constituent elements by the reader, so emitting
  // the element list re-parses to the same set), and Map() optionally holds a
  // subscript-mapping clause.
  Equation *eq = v->GetEquation(0);
  if (!eq)
    return;
  Expression *exp = eq->GetExpression();
  if (!exp || exp->GetType() != EXPTYPE_Symlist)
    return;
  ExpressionSymbolList *esl = static_cast<ExpressionSymbolList *>(exp);

  std::string entry = mdl::FormatMDLIdent(v->GetName()) + ": " + RenderDimensionElements(esl->SymList());
  if (SymbolList *map = esl->Map())
    entry += " -> " + RenderDimensionMap(map);

  // A dimension can carry units and a comment just like any other variable
  // (e.g. "Region: R1, R2, R3 ~ dmnl ~ a small dimension |"), so emit the same
  // units/comment trailer the equation entries use rather than the empty "~~|"
  // shorthand -- otherwise a dimension's units and documentation are silently
  // dropped on a round trip. When both are empty the trailer re-parses to the
  // same empty units/comment that "~~|" would.
  out += entry;
  out += UnitsCommentTrailer(v);
  out += "\n";
}

std::string MDLGenerator::RenderDimensionElements(SymbolList *sl) {
  // A flat, comma-separated element list (no surrounding brackets), used for the
  // left side of a dimension definition and for the elements of a subdimension
  // map. A nested LIST entry recurses, which is how a subrange that the reader
  // left unexpanded would still emit its elements.
  std::string out;
  int n = sl->Length();
  for (int i = 0; i < n; i++) {
    if (i)
      out += ", ";
    const SymbolList::SymbolListEntry &e = (*sl)[i];
    if (e.eType == SymbolList::EntryType_LIST)
      out += RenderDimensionElements(e.u.pSymbolList);
    else
      out += mdl::FormatMDLIdent(e.u.pSymbol->GetName());
  }
  return out;
}

std::string MDLGenerator::RenderDimensionMap(SymbolList *map) {
  // The mapping clause after "->". Two attested forms (verified against
  // test/fixtures/sdeverywhere/models/mapping/mapping.mdl):
  //   - a plain target dimension:  "DimD"     (Map() has SYMBOL entries, no MapRange)
  //   - a subdimension map:        "(DimA: SubA, A3)"  (a list with MapRange set)
  // A list whose MapRange is set is the "(Range: elements)" form; otherwise the
  // top-level entries are rendered comma-separated, recursing into any nested
  // "(Range: ...)" list.
  if (map->IsMapList())
    return "(" + mdl::FormatMDLIdent(map->MapRange()->GetName()) + ": " + RenderDimensionElements(map) + ")";

  std::string out;
  int n = map->Length();
  for (int i = 0; i < n; i++) {
    if (i)
      out += ", ";
    const SymbolList::SymbolListEntry &e = (*map)[i];
    if (e.eType == SymbolList::EntryType_LIST)
      out += RenderDimensionMap(e.u.pSymbolList);
    else
      out += mdl::FormatMDLIdent(e.u.pSymbol->GetName());
  }
  return out;
}

std::string MDLGenerator::UnitsCommentTrailer(Variable *v) {
  // Units come from the parsed UnitExpression when available (it normalizes the
  // numerator/denominator spelling) and fall back to the raw units string
  // otherwise. Mirrors simlin write_units_and_comment (writer.rs:1354-1362).
  std::string units;
  if (UnitExpression *ue = v->Units())
    units = ue->GetEquationString();
  else
    units = v->GetUnitsString();

  // Sanitize both free-text fields so a raw structural character cannot
  // terminate the entry early (which drops the following variable on re-import,
  // #849). Units is a single-line field and forbids a bare '~' (its own field
  // separator); the comment is multi-line and forbids nothing beyond the shared
  // '|' -> '/' and section-terminator substitutions. SanitizeFreeText also
  // normalizes '\r\n'/'\r' losslessly to '\n', which subsumes the manual '\r'
  // erase the previous code did here -- so a CRLF-sourced field is a fixpoint
  // rather than accreting a carriage return each write, and Print's single final
  // LF->CRLF pass restores CRLF.
  std::string comment = mdl::SanitizeFreeText(v->Comment(), mdl::FreeTextLineMode::Multiline, "");
  units = mdl::SanitizeFreeText(units, mdl::FreeTextLineMode::SingleLine, "~");

  // The Vensim lexer (VensimLex::GetComment) captures the separator whitespace
  // between the second '~' and the comment text as part of the stored comment,
  // then strips trailing whitespace. We re-supply that separator as the "\t"
  // below, so we drop the captured leading whitespace here; otherwise it would
  // accrete another tab on every emit/re-parse cycle. This is a stable
  // normalization, not a content change (the comment text is preserved).
  size_t start = comment.find_first_not_of(" \t\n");
  if (start == std::string::npos)
    comment.clear();
  else
    comment.erase(0, start);

  return "\n\t~\t" + units + "\n\t~\t" + comment + "\n\t|";
}

void MDLGenerator::EmitWrappedEntry(std::string &out, const std::string &entry, Variable *v) {
  out += mdl::WrapEquation(entry, 80);
  out += UnitsCommentTrailer(v);
  out += "\n";
}

std::string MDLGenerator::RenderExpression(Expression *e) {
  e = Unwrap(e);
  if (!e)
    return "";
  switch (e->GetType()) {
  case EXPTYPE_Number:
    return mdl::FormatMDLNumber(static_cast<ExpressionNumber *>(e)->GetValue());
  case EXPTYPE_Literal:
    return static_cast<ExpressionLiteral *>(e)->GetValue();
  case EXPTYPE_Variable:
    return RenderVariableRef(static_cast<ExpressionVariable *>(e));
  case EXPTYPE_Operator:
    return RenderOperator(e);
  case EXPTYPE_Function:
  case EXPTYPE_FunctionMemory:
    return RenderFunction(static_cast<ExpressionFunction *>(e));
  case EXPTYPE_Logical:
    return RenderLogical(static_cast<ExpressionLogical *>(e));
  case EXPTYPE_Lookup:
  case EXPTYPE_Table:
  case EXPTYPE_NumberTable:
  case EXPTYPE_Symlist:
    return RenderTableLike(e);
  default:
    return "";
  }
}

std::string MDLGenerator::RenderOperator(Expression *e) {
  // e is unwrapped, so it is a real arithmetic operator, not a paren node.
  const char *op = e->GetOperator();
  if (op && *op) {
    std::string l = ParenIfNecessary(e, e->GetArg(0), false, RenderExpression(e->GetArg(0)));
    std::string r = ParenIfNecessary(e, e->GetArg(1), true, RenderExpression(e->GetArg(1)));
    return l + " " + op + " " + r;
  }
  // Unary minus: operand is in GetArg(0); emit with no space after the sign.
  std::string c = ParenIfNecessary(e, e->GetArg(0), false, RenderExpression(e->GetArg(0)));
  return "-" + c;
}

std::string MDLGenerator::RenderLogical(ExpressionLogical *lg) {
  int oper = lg->LogicalOperator();
  const char *sym = LogicalSymbol(oper);
  if (oper == VPTT_not) {
    // Unary :NOT:; the operand lives in GetRight() (GetLeft() is NULL).
    std::string c = ParenIfNecessary(lg, lg->GetRight(), false, RenderExpression(lg->GetRight()));
    return std::string(sym) + " " + c;
  }
  std::string l = ParenIfNecessary(lg, lg->GetLeft(), false, RenderExpression(lg->GetLeft()));
  std::string r = ParenIfNecessary(lg, lg->GetRight(), true, RenderExpression(lg->GetRight()));
  return l + " " + sym + " " + r;
}

std::string MDLGenerator::RenderFunction(ExpressionFunction *fn) {
  Function *f = fn->GetFunction();
  ExpressionList *args = fn->GetArgs();
  // GetName() is the Vensim name; we never call Function::OutputComputable, so
  // none of the XMILE structural rewrites (PULSE->IF, etc.) fire.
  std::string name = mdl::FormatMDLIdent(f->GetName());
  int n = args ? args->Length() : 0;
  // Vensim's only zero-argument production is `VPTT_function '(' ')'` (VYacc.y)
  // -- a function token followed by anything else is a syntax error, and the
  // parser discards just the offending equation, so a bare `RANDOM 0 1` dropped
  // the whole variable on re-import while the conversion still reported success.
  if (n == 0)
    return name + "()";
  std::string out = name + "(";
  for (int i = 0; i < n; i++) {
    if (i)
      out += ", ";
    out += RenderExpression(args->GetExp(i));
  }
  out += ")";
  return out;
}

std::string MDLGenerator::RenderVariableRef(ExpressionVariable *v) {
  std::string out = mdl::FormatMDLIdent(v->GetVariable()->GetName());
  SymbolList *subs = v->GetSubs();
  if (subs && subs->Length() > 0)
    out += RenderSubscripts(subs);
  return out;
}

std::string MDLGenerator::RenderSubscripts(SymbolList *subs) {
  std::string out = "[";
  for (int i = 0; i < subs->Length(); i++) {
    if (i)
      out += ", ";
    const SymbolList::SymbolListEntry &e = (*subs)[i];
    if (e.eType == SymbolList::EntryType_LIST) {
      out += RenderSubscripts(e.u.pSymbolList);  // nested subscript list (rare)
    } else if (e.u.pSymbol == nullptr) {
      // An unbound `*` wildcard (null bang symbol) should have been resolved by
      // XmileReader::ResolveWildcardSubscripts before reaching the writer; guard
      // against a null deref so a stray one degrades to a marker rather than
      // crashing. Vensim has no bare-`*` subscript form.
      out += "*";
    } else {
      out += mdl::FormatMDLIdent(e.u.pSymbol->GetName());
      if (e.eType == SymbolList::EntryType_BANG_SYMBOL)
        out += "!";  // Vensim "bang" subscript marks a vector-iteration dimension
    }
  }
  out += "]";
  return out;
}

std::string MDLGenerator::RenderTableLike(Expression *e) {
  switch (e->GetType()) {
  case EXPTYPE_Lookup: {
    ExpressionLookup *lk = static_cast<ExpressionLookup *>(e);
    if (lk->GetTable()) {
      // Embedded "WITH LOOKUP(input, (<body>))": an inline table applied to an
      // input expression. This is how the reader represents both a top-level
      // "y = WITH LOOKUP(...)" and a nested one.
      return "WITH LOOKUP(" + RenderExpression(lk->GetInput()) + ", (" + mdl::WriteLookupBody(lk->GetTable()) + "))";
    }
    // A lookup call "table(input)": a reference to a named lookup variable
    // applied to an input. GetLookupVariable() carries no subscripts in this
    // form, so the bare identifier is sufficient.
    return mdl::FormatMDLIdent(lk->GetLookupVariable()->GetVariable()->GetName()) + "(" +
           RenderExpression(lk->GetInput()) + ")";
  }
  case EXPTYPE_Table:
    // A bare table inside an expression (rare; the standalone graphical-function
    // case is handled at the equation level). Emit just the body.
    return mdl::WriteLookupBody(static_cast<ExpressionTable *>(e));
  case EXPTYPE_NumberTable: {
    // Arrayed constant data "1, 2, 3" that survived MarkVariableTypes (the usual
    // case blows it out into per-element ExpressionNumber equations). The
    // multi-row ";" form is already lost at parse time -- AddValue drops the row
    // index -- so only a single comma-joined row is recoverable.
    const std::vector<double> &vals = static_cast<ExpressionNumberTable *>(e)->GetVals();
    std::string out;
    for (size_t i = 0; i < vals.size(); i++) {
      if (i)
        out += ",";
      out += mdl::FormatMDLNumber(vals[i]);
    }
    return out;
  }
  case EXPTYPE_Symlist:
    // A symbol list appearing inside an expression (rare; a dimension definition
    // is emitted at the equation level). Emit the bracketed element list.
    return "[" + RenderDimensionElements(static_cast<ExpressionSymbolList *>(e)->SymList()) + "]";
  default:
    return "";
  }
}

std::string MDLGenerator::ParenIfNecessary(Expression *parent, Expression *child, bool isRightChild,
                                           const std::string &childStr) {
  bool needs = false;
  if (IsBinaryOp(parent) && IsBinaryOp(child)) {
    int pp = MdlPrecedence(parent);
    int cp = MdlPrecedence(child);
    if (pp > cp) {
      needs = true;
    } else if (pp == cp) {
      // Equal precedence: parenthesize the operand that the parent's
      // associativity would otherwise re-group the wrong way.
      Expression *p = Unwrap(parent);
      const char *po = p ? p->GetOperator() : nullptr;
      if (po && po[0] == '^') {
        // ^ is right-associative (a ^ b ^ c == a ^ (b ^ c)), so an explicit
        // left grouping (a ^ b) ^ c must keep its parentheses.
        needs = !isRightChild;
      } else if (po && (po[0] == '-' || po[0] == '/')) {
        // - and / are left-associative and non-associative in value, so the
        // right operand needs parens: a - (b - c) != (a - b) - c. xmutil has no
        // MOD operator (MODULO is a function), so only - and / apply here.
        needs = isRightChild;
      }
    }
  } else if (IsBinaryOp(parent) && IsUnaryOp(child)) {
    // A unary child binds looser than every operator above its grammar level.
    // As a LEFT operand of a tighter binary parent it would re-bind the wrong
    // way (`-a ^ b` -> -(a ^ b), `:NOT: a ^ b` -> :NOT: (a ^ b)), so it needs
    // parens. As a RIGHT operand it extends rightward to the end of the
    // sub-expression and re-parses correctly without them (`a * -b`), so we
    // leave those bare. At equal precedence (unary minus under binary +/-) the
    // left operand also re-parses correctly, hence the strict comparison.
    needs = !isRightChild && MdlUnaryPrecedence(child) < MdlPrecedence(parent);
  } else if (IsUnaryOp(parent) && IsBinaryOp(child)) {
    needs = true;
  } else if (IsUnaryOp(parent) && IsUnaryOp(child)) {
    // Nested unary minus: emit `-(-a)` rather than the fragile `--a` that some
    // Vensim consumers reject (it still re-parses, but is poor output). :NOT:
    // never directly nests another unary without an intervening operator.
    needs = true;
  }
  return needs ? "(" + childStr + ")" : childStr;
}
