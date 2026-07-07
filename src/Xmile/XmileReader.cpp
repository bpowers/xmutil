#include "XmileReader.h"

#include <tinyxml2.h>

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <set>
#include <stdexcept>

#include "../Model.h"
#include "../Symbol/Expression.h"
#include "../Symbol/ExpressionList.h"
#include "../Symbol/LeftHandSide.h"
#include "../Symbol/Symbol.h"
#include "../Symbol/SymbolList.h"
#include "../Symbol/SymbolNameSpace.h"
#include "../Symbol/UnitExpression.h"
#include "../Symbol/Units.h"
#include "../Symbol/Variable.h"
#include "../Vensim/VensimParse.h"
#include "../Vensim/VensimView.h"
#include "../XMUtil.h"
#include "XmileEqLex.h"
#include "XmileView.h"

// The bison-generated parser entry. The `-p xpyy` rename produces this name
// in XmileEqYacc.tab.cpp; declaring it here lets us avoid including the
// generated header (which would also export YYSTYPE / xpyylval globals that
// callers of XmileReader.h shouldn't see). The generated parser is compiled
// as C++, so no extern "C" wrapping.
int xpyyparse(void);

XmileReader *XPObject = nullptr;

namespace {

// Build the "<tag name="X">: " prefix that attributes an equation-level
// diagnostic to the variable element it came from.
std::string ElementContext(tinyxml2::XMLElement *el) {
  const char *tag = el ? el->Name() : nullptr;
  const char *nm = el ? el->Attribute("name") : nullptr;
  return std::string("<") + (tag ? tag : "?") + " name=\"" + (nm ? nm : "(?)") + "\">: ";
}

// Forward every diagnostic ParseEquation collected up to the document errs
// channel, tagged with the offending element, and report whether any was a hard
// error. Advisory diagnostics carry a "warning: " prefix (see the arity note in
// XmileParseFunctions and the non_negative advisory); anything else -- an
// unknown function lowered to a 0 placeholder, an unresolvable identifier -- is
// a wrongness that must fail the conversion even though ParseEquation returned a
// (placeholder) Expression. Without this classification those diagnostics were
// discarded whenever the placeholder-returning shims let the bison parse
// succeed, silently shipping a semantically wrong model.
bool ForwardEqnDiagnostics(const std::string &context, const std::vector<std::string> &eqnErrs,
                           std::vector<std::string> &errs) {
  bool hardError = false;
  for (const std::string &e : eqnErrs) {
    errs.push_back(context + e);
    if (e.compare(0, 9, "warning: ") != 0)
      hardError = true;
  }
  return hardError;
}

// Walk an expression tree, recording the names of ExpressionLookup targets --
// the `table` in `table(x)` -- whose Variable ended the parse with no defining
// equation. The tree shape mirrors Model::ResolveWildcardsInExpr. WITH LOOKUP
// nodes (an inline <gf> table, GetLookupVariable() == NULL) are always defined
// and skipped. Names accumulate into a set so a phantom referenced from several
// equations is reported once.
void CollectPhantomLookups(Expression *e, std::set<std::string> &phantoms) {
  if (!e)
    return;
  switch (e->GetType()) {
  case EXPTYPE_Lookup: {
    ExpressionLookup *lk = static_cast<ExpressionLookup *>(e);
    if (ExpressionVariable *ev = lk->GetLookupVariable()) {
      Variable *target = ev->GetVariable();
      if (target && target->GetAllEquations().empty() && target->GetAllInitEquations().empty())
        phantoms.insert(target->GetName());
    }
    CollectPhantomLookups(lk->GetInput(), phantoms);
    break;
  }
  case EXPTYPE_Operator:
    CollectPhantomLookups(e->GetArg(0), phantoms);
    CollectPhantomLookups(e->GetArg(1), phantoms);
    break;
  case EXPTYPE_Logical: {
    ExpressionLogical *lg = static_cast<ExpressionLogical *>(e);
    CollectPhantomLookups(lg->GetLeft(), phantoms);
    CollectPhantomLookups(lg->GetRight(), phantoms);
    break;
  }
  case EXPTYPE_Function:
  case EXPTYPE_FunctionMemory: {
    ExpressionList *args = static_cast<ExpressionFunction *>(e)->GetArgs();
    if (args)
      for (int i = 0; i < args->Length(); i++)
        CollectPhantomLookups(args->GetExp(i), phantoms);
    break;
  }
  default:
    // Number, literal, table, symlist, bare variable: no nested lookup target.
    break;
  }
}

}  // namespace

XmileReader::XmileReader(Model *model)
    : _model(model),
      pSymbolNameSpace(model->GetNameSpace()),
      _lastParsedExpr(nullptr),
      _currentLex(nullptr),
      _currentErrs(nullptr),
      _declaresPi(false) {
  // The bison action shims see the active reader only through this global, so
  // exactly one reader may be live. An `assert` alone compiled out under
  // NDEBUG -- which is the build an embedder parsing two documents from nested
  // scopes would be running -- and there the second reader silently took the
  // global over. Refuse at runtime instead: the extern-C entries catch this and
  // return NULL, which is a diagnosable failure rather than a wrong model.
  if (XPObject)
    throw std::runtime_error("XmileReader: another XMILE parse is already active in this process");
  XPObject = this;
  // ParseEquation and ProcessStock both look up xmutil Functions by name
  // (INTEG, IF THEN ELSE, ZIDZ, ...). Register the shared table here so the
  // reader is self-contained -- no need for the caller to construct a
  // transient VensimParse to seed the namespace. Guard against
  // double-registration in case the same namespace already has the table
  // (e.g. an in-process pipeline that ran VensimParse first).
  if (!pSymbolNameSpace->Find("INTEG"))
    RegisterXmutilFunctions(pSymbolNameSpace);
}

XmileReader::~XmileReader() {
  // Only the reader that claimed the global may clear it. Clearing it
  // unconditionally is what turned a stolen global into silent corruption: the
  // thief's destructor left the still-live owner pointing at nothing, and every
  // shim it called afterwards took its defensive null branch and returned a
  // 0-placeholder without a diagnostic.
  if (XPObject == this)
    XPObject = nullptr;
}

bool XmileReader::ProcessFile(const std::string &filename, const char *contents, size_t len,
                              std::vector<std::string> &errs) {
  tinyxml2::XMLDocument doc;
  // tinyxml2::XMLDocument::Parse accepts a length, so the buffer need not be
  // NUL-terminated -- the extern-C path passes a uint32_t length.
  tinyxml2::XMLError parseErr = doc.Parse(contents, len);
  if (parseErr != tinyxml2::XML_SUCCESS) {
    const char *detail = doc.ErrorStr();
    errs.push_back(filename + ": XML parse error: " + (detail ? detail : "(no detail)"));
    return false;
  }
  tinyxml2::XMLElement *root = doc.RootElement();
  if (!root) {
    errs.push_back(filename + ": empty XML document");
    return false;
  }
  // The bare local name "xmile" is the only accepted root. A vendor-prefixed
  // root like "isee:xmile" is not the XMILE envelope.
  const char *rootName = root->Name();
  if (!rootName || std::string(rootName) != "xmile") {
    errs.push_back(filename + ": root element is not <xmile> (got <" + (rootName ? rootName : "(null)") + ">)");
    return false;
  }
  // Envelope-level pre-pass: reject <macro> outright and refuse
  // documents that carry more than one <model> sibling. Counting <model>
  // siblings here -- rather than inside the dispatch loop below -- means a
  // multi-model document fails before any partial state is built on the
  // reader's Model, which keeps the error path symmetric with the empty-input
  // and malformed-XML returns above.
  int modelCount = 0;
  for (tinyxml2::XMLElement *child = root->FirstChildElement(); child; child = child->NextSiblingElement()) {
    const char *name = child->Name();
    if (!name)
      continue;
    if (IsForeignNamespace(name))
      continue;
    std::string tag(name);
    if (tag == "macro") {
      errs.push_back(filename + ": <macro> elements are not supported");
      return false;
    }
    if (tag == "model")
      ++modelCount;
  }
  if (modelCount > 1) {
    errs.push_back(filename + ": multiple <model> elements are not supported");
    return false;
  }
  bool ok = true;
  for (tinyxml2::XMLElement *child = root->FirstChildElement(); ok && child; child = child->NextSiblingElement()) {
    const char *name = child->Name();
    if (!name)
      continue;
    if (IsForeignNamespace(name))
      continue;
    std::string tag(name);
    if (tag == "header") {
      // The Model has no field that stores XMILE header metadata (name,
      // vendor, product); acknowledge the element so it doesn't fall into the
      // unknown bucket below.
      continue;
    } else if (tag == "sim_specs") {
      ok = ProcessSimSpecs(child, errs);
    } else if (tag == "model_units") {
      ok = ProcessModelUnits(child, errs);
    } else if (tag == "dimensions") {
      // XMILEGenerator emits the model's <dimensions> at root scope (sibling
      // to <model>), not inside <model>. The reader also accepts a
      // <dimensions> child of <model> (handled in ProcessModel) for fixtures
      // and writers that place it there; both forms feed the same
      // ProcessDimensions handler.
      ok = ProcessDimensions(child, errs);
    } else if (tag == "model") {
      ok = ProcessModel(child, errs);
    } else if (tag == "style") {
      // Styling sections carry no semantic content.
      continue;
    } else if (IsStellaUIWidget(name)) {
      continue;
    } else {
      // Unknown default-namespace envelope element. Drop silently to leave the
      // door open for future XMILE additions; tighten to a hard error only if
      // the corpus demands it.
      continue;
    }
  }
  // Whole-document validation deferred until every variable and <gf> has been
  // walked: a graphical-function target may be forward-referenced, so a lookup
  // application can only be judged phantom once the full document is in hand.
  if (ok && !ValidateLookupTargets(errs))
    ok = false;
  return ok;
}

bool XmileReader::IsForeignNamespace(const char *qualifiedName) {
  if (!qualifiedName)
    return false;
  return std::strchr(qualifiedName, ':') != nullptr;
}

bool XmileReader::IsStellaUIWidget(const char *unprefixedName) {
  if (!unprefixedName)
    return false;
  // Documented Stella UI widget tags plus the corpus-observed loop indicator.
  // Kept as an alphabetized static array for readability; the list is short
  // enough that a linear scan is faster than any hashed structure.
  static const char *const kWidgets[] = {
      "animation_object", "button",          "gauge",         "graph",  "knob",
      "loop_indicator",   "numeric_display", "numeric_input", "slider", "spatial_map",
  };
  for (const char *w : kWidgets) {
    if (std::strcmp(unprefixedName, w) == 0)
      return true;
  }
  return false;
}

std::string XmileReader::NormalizeName(const char *raw) {
  if (!raw)
    return std::string();
  std::string out;
  out.reserve(std::strlen(raw));
  bool inSpace = false;
  bool seenNonSpace = false;
  for (const char *p = raw; *p; ++p) {
    unsigned char c = static_cast<unsigned char>(*p);
    // Stella encodes visual line-breaks in display names as the literal 2-byte
    // sequence "\n" (backslash + n) -- and occasionally "\r" -- to instruct the
    // diagram renderer to wrap the label across multiple lines while keeping
    // the underlying identifier single-line. Vensim identifiers carry no such
    // marker (and the namespace's ToLowerSpace canonicalizer treats `_` and
    // real whitespace as equivalent but does NOT recognize literal `\n`), so a
    // model that declared an aux as `maximum_fishery_size` in <variables> and
    // then re-referenced it as `Maximum\nfishery size` in the <view> would
    // hash to two distinct names and produce a phantom duplicate variable.
    // Collapse the escape into a single space at the read boundary so all
    // downstream lookups see one canonical form. Mirrors the XMILE 1.0
    // convention; simlin's reference reader does the same.
    if (c == '\\' && (p[1] == 'n' || p[1] == 'r')) {
      ++p;
      inSpace = true;
      continue;
    }
    if (std::isspace(c)) {
      inSpace = true;
      continue;
    }
    if (inSpace && seenNonSpace)
      out.push_back(' ');
    out.push_back(static_cast<char>(c));
    seenNonSpace = true;
    inSpace = false;
  }
  return out;
}

std::string XmileReader::FoldNameKey(const std::string &normalizedName) {
  std::string key;
  key.reserve(normalizedName.size());
  for (char c : normalizedName) {
    unsigned char uc = static_cast<unsigned char>(c);
    if (uc == '_' || uc == ' ') {
      if (!key.empty() && key.back() != ' ')
        key.push_back(' ');
    } else {
      key.push_back(static_cast<char>(std::tolower(uc)));
    }
  }
  while (!key.empty() && key.back() == ' ')
    key.pop_back();
  return key;
}

const std::vector<std::string> *XmileReader::StocksForFlow(const std::string &foldedFlowName) const {
  auto it = _flowToStocks.find(foldedFlowName);
  if (it == _flowToStocks.end())
    return nullptr;
  return &it->second;
}

UnitExpression *XmileReader::ParseUnitsString(const std::string &text) {
  // A body that is empty once trimmed states no unit, which is not the same as
  // stating a malformed one -- it gets no UnitExpression rather than a bad one.
  size_t start = 0;
  while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])))
    ++start;
  size_t end = text.size();
  while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])))
    --end;
  if (start >= end)
    return nullptr;

  // Recursive descent parser for unit expressions. Grammar:
  //
  //   expr  := factor (('/' | '*') factor)*
  //   factor := '(' expr ')' | ident
  //
  // Precedence and associativity match VensimParse: left-associative, '/'
  // and '*' at the same level. The writer emits "a/(b*c)" for a unit with
  // numerator a and denominator [b, c]; the '(' group must be parsed atomically
  // so that "a/(b*c)" produces denominator b*c, not a mis-parsed "a/b * c".
  //
  // p is a reference into the string, advanced in place by each method. Both
  // productions skip leading whitespace before consuming. On malformed input
  // (unmatched parens, empty token) they return nullptr -- deleting any
  // partially-built operand first -- and the caller falls back to storing
  // only the raw text string.
  struct Parser {
    SymbolNameSpace *sns;
    const char *sEnd;

    void SkipSpace(const char *&p) const {
      while (p < sEnd && std::isspace(static_cast<unsigned char>(*p)))
        ++p;
    }

    // Find-or-create a Units symbol under the ">name" key the Vensim parser
    // uses for the units namespace.
    Units *InsertUnits(const std::string &name) const {
      std::string key = ">" + name;
      Symbol *sym = sns->Find(key);
      if (sym && sym->isType() == Symtype_Units)
        return static_cast<Units *>(sym);
      return new Units(sns, key);
    }

    UnitExpression *ParseFactor(const char *&p) const {
      SkipSpace(p);
      if (p >= sEnd)
        return nullptr;

      if (*p == '(') {
        ++p;
        UnitExpression *inner = ParseExpr(p);
        if (!inner)
          return nullptr;
        SkipSpace(p);
        if (p >= sEnd || *p != ')') {
          delete inner;
          return nullptr;
        }
        ++p;
        return inner;
      }

      // Identifier: everything up to the next operator, paren, or end.
      // Whitespace INSIDE the token is preserved ("kg of stuff" is one name),
      // but trailing whitespace is trimmed before returning.
      const char *tokStart = p;
      while (p < sEnd && *p != '/' && *p != '*' && *p != '(' && *p != ')')
        ++p;
      const char *tokEnd = p;
      while (tokEnd > tokStart && std::isspace(static_cast<unsigned char>(*(tokEnd - 1))))
        --tokEnd;
      if (tokEnd == tokStart)
        return nullptr;
      std::string name(tokStart, tokEnd);
      return new UnitExpression(sns, InsertUnits(name));
    }

    UnitExpression *ParseExpr(const char *&p) const {
      UnitExpression *acc = ParseFactor(p);
      if (!acc)
        return nullptr;
      for (;;) {
        SkipSpace(p);
        if (p >= sEnd || (*p != '/' && *p != '*'))
          break;
        bool isDiv = (*p == '/');
        ++p;
        UnitExpression *rhs = ParseFactor(p);
        if (!rhs) {
          delete acc;
          return nullptr;
        }
        acc = isDiv ? acc->Divide(rhs) : acc->Multiply(rhs);
      }
      return acc;
    }
  };

  const Parser parser{pSymbolNameSpace, text.c_str() + end};
  const char *pos = text.c_str() + start;
  UnitExpression *result = parser.ParseExpr(pos);
  // Reject if unconsumed input remains after the parse (e.g. an unmatched ')').
  parser.SkipSpace(pos);
  if (pos != parser.sEnd) {
    delete result;
    return nullptr;
  }
  return result;
}

void XmileReader::AttachVariableUnits(Variable *v, const char *unitsText) {
  if (!v || !unitsText)
    return;
  v->SetUnitsString(unitsText);
  UnitExpression *ue = ParseUnitsString(unitsText);
  if (ue) {
    // AddUnits returns false if a UnitExpression is already attached (a stock
    // re-emit can call back through here on the same Variable). In that case
    // the parsed expression is redundant; release it.
    if (!v->AddUnits(ue))
      delete ue;
  }
}

bool XmileReader::ProcessSimSpecs(tinyxml2::XMLElement *simSpecs, std::vector<std::string> &errs) {
  // method= is case-insensitive across the corpus (Stella emits "euler",
  // xmutil emits "Euler"). Missing or unrecognized values fall back to Euler;
  // RK4/RK2 are the only other XMILE-defined integrators.
  Integration_Type integration = Integration_Type_EULER;
  if (const char *method = simSpecs->Attribute("method")) {
    if (StringMatch(method, "rk4"))
      integration = Integration_Type_RK4;
    else if (StringMatch(method, "rk2"))
      integration = Integration_Type_RK2;
  }
  _model->SetIntegrationType(integration);

  // The XMILE writer falls back to 0/100/1 defaults when the corresponding
  // control variables are absent (XMILEGenerator::generateSimSpecs), so the
  // reader mirrors those defaults for missing child elements. SAVEPER
  // defaults to dt per the XMILE spec.
  double startVal = 0.0;
  double stopVal = 100.0;
  double dtVal = 1.0;

  if (tinyxml2::XMLElement *e = simSpecs->FirstChildElement("start"))
    startVal = e->DoubleText(0.0);
  if (tinyxml2::XMLElement *e = simSpecs->FirstChildElement("stop"))
    stopVal = e->DoubleText(100.0);
  if (tinyxml2::XMLElement *e = simSpecs->FirstChildElement("dt")) {
    dtVal = e->DoubleText(1.0);
    // dt reciprocal="true" means the body is N and the actual dt is 1/N. The
    // xmutil writer never emits reciprocal form (its dt is always the resolved
    // double), so a self-round-trip never sees it -- but Stella XMILE does,
    // and the reader must interoperate. Resolve at read time so all downstream
    // code sees a plain dt.
    if (const char *recip = e->Attribute("reciprocal")) {
      std::string r(recip);
      if (r == "true" || r == "1") {
        if (dtVal != 0.0)
          dtVal = 1.0 / dtVal;
        else
          errs.push_back("<dt reciprocal=\"true\"> with zero body; ignoring reciprocal");
      }
    }
  }
  double saveStepVal = dtVal;
  if (tinyxml2::XMLElement *e = simSpecs->FirstChildElement("save_step"))
    saveStepVal = e->DoubleText(dtVal);

  // Populate both the control Variables (for the writer's GetConstanValue
  // path) and the Model setters (for the engine's compiled-in fast path).
  // Mirrors how VensimParse drops out of its Control section.
  SetControlVariable("INITIAL TIME", startVal);
  SetControlVariable("FINAL TIME", stopVal);
  SetControlVariable("TIME STEP", dtVal);
  SetControlVariable("SAVEPER", saveStepVal);
  _model->set_initial_time(startVal);
  _model->set_finall_time(stopVal);  // misspelled in Model.h; mirrors header
  _model->set_dt(dtVal);

  // time_units attribute attaches to TIME STEP as a raw units string; the
  // XMILE writer reads this back via GetUnits("TIME STEP") with FINAL TIME
  // and INITIAL TIME as fallbacks. Only the raw string is stored -- no
  // UnitExpression is parsed for control variables.
  if (const char *tu = simSpecs->Attribute("time_units")) {
    if (Variable *ts = FindVariable("TIME STEP"))
      ts->SetUnitsString(tu);
  }

  return true;
}

bool XmileReader::ProcessModelUnits(tinyxml2::XMLElement *units, std::vector<std::string> &errs) {
  (void)errs;
  // Model::UnitEquivs() stores comma-separated raw strings whose first field
  // is the canonical unit name, the second is the eqn body, and any trailing
  // fields are aliases. The XMILE writer reverses this in generateModelUnits;
  // the reader mirrors the same shape so unit definitions survive round-trip.
  for (tinyxml2::XMLElement *u = units->FirstChildElement("unit"); u; u = u->NextSiblingElement("unit")) {
    const char *uname = u->Attribute("name");
    if (!uname)
      continue;
    std::string composed = uname;
    if (tinyxml2::XMLElement *eqnEl = u->FirstChildElement("eqn")) {
      if (const char *txt = eqnEl->GetText()) {
        composed += ",";
        composed += txt;
      }
    }
    for (tinyxml2::XMLElement *aliasEl = u->FirstChildElement("alias"); aliasEl;
         aliasEl = aliasEl->NextSiblingElement("alias")) {
      if (const char *txt = aliasEl->GetText()) {
        composed += ",";
        composed += txt;
      }
    }
    _model->UnitEquivs().push_back(composed);
  }
  return true;
}

ModelGroup *XmileReader::ProcessGroup(tinyxml2::XMLElement *groupEl, std::vector<std::string> &errs) {
  const char *name = groupEl->Attribute("name");
  if (!name) {
    errs.push_back("<group> with no name attribute");
    return nullptr;
  }
  std::string normName = NormalizeName(name);

  // The XMILE owner="..." attribute references another group by name. The
  // walker may visit groups in any order, so an unresolved owner here just
  // means "no owner" -- the view walk preserves the original order, and any
  // forward reference becomes a name-only inference downstream.
  ModelGroup *owner = nullptr;
  if (const char *ownerName = groupEl->Attribute("owner"))
    owner = FindGroupByName(NormalizeName(ownerName));

  // The same group can appear under multiple <view> elements in a multi-view
  // file; reusing keeps Model::Groups() unique by name.
  ModelGroup *group = FindGroupByName(normName);
  if (!group) {
    group = new ModelGroup(normName, owner);
    _model->Groups().push_back(group);
  }

  for (tinyxml2::XMLElement *vEl = groupEl->FirstChildElement("var"); vEl; vEl = vEl->NextSiblingElement("var")) {
    const char *vname = vEl->GetText();
    if (!vname)
      continue;
    std::string n = NormalizeName(vname);
    Variable *v = FindVariable(n);
    if (!v) {
      // Forward reference to a variable declared later in the file (or never
      // declared explicitly). InsertVariable creates a placeholder so the
      // group membership round-trips; MarkVariableTypes classifies it.
      v = InsertVariable(n);
      if (!v)
        continue;
    }
    v->SetGroup(group);
    group->vVariables.push_back(v);
  }
  return group;
}

ModelGroup *XmileReader::FindGroupByName(const std::string &norm) {
  for (ModelGroup *g : _model->Groups()) {
    if (g->sName == norm)
      return g;
  }
  return nullptr;
}

// True if a <view> element has any sketch-geometry children (stocks, auxes,
// flows, aliases, or connectors). A <view> whose only children are <group>
// elements carries no sketch geometry; in that case the reader processes the
// groups in-place via ProcessGroup and does NOT allocate a VensimView, so the
// writer's empty-views branch (which is the only path that emits groups under
// <views>) keeps firing on the next round-trip. Before view-geometry support
// the reader never allocated views at all, so the writer always took that
// empty-views path; the geometry-only criterion preserves that contract for
// group-only views.
static bool ViewHasGeometry(tinyxml2::XMLElement *viewEl) {
  for (tinyxml2::XMLElement *c = viewEl->FirstChildElement(); c; c = c->NextSiblingElement()) {
    const char *name = c->Name();
    if (!name)
      continue;
    if (XmileReader::IsForeignNamespace(name))
      continue;
    std::string tag(name);
    if (tag == "stock" || tag == "aux" || tag == "flow" || tag == "alias" || tag == "connector")
      return true;
  }
  return false;
}

bool XmileReader::ProcessViews(tinyxml2::XMLElement *views, std::vector<std::string> &errs) {
  // Two shapes of children may appear at this level:
  //
  //   <group ...>          - direct under <views>. The writer emits this from
  //                          the empty-views-but-has-groups branch in
  //                          XMILEGenerator::generateSectorViews when the
  //                          model has sectors but no sketch.
  //   <view ...>           - a sketch view with stocks, flows, auxes,
  //                          connectors, aliases, and possibly nested
  //                          <group> children.
  //
  // For <group> direct children, harvest them eagerly so they survive a
  // round-trip through the no-sketch path. For <view> children with real
  // sketch geometry, instantiate an XmileView to populate a VensimView; v1
  // processes only the first such <view>, additional ones produce a soft
  // warning. A <view> whose only children are <group> elements is treated as
  // an alias for <views><group>: extract the groups in place and skip the
  // VensimView allocation (see ViewHasGeometry's rationale).
  int viewCount = 0;
  for (tinyxml2::XMLElement *child = views->FirstChildElement(); child; child = child->NextSiblingElement()) {
    const char *cname = child->Name();
    if (!cname)
      continue;
    if (IsForeignNamespace(cname))
      continue;
    std::string tag(cname);
    if (tag == "group") {
      if (!ProcessGroup(child, errs))
        return false;
    } else if (tag == "view") {
      if (!ViewHasGeometry(child)) {
        for (tinyxml2::XMLElement *vc = child->FirstChildElement("group"); vc; vc = vc->NextSiblingElement("group")) {
          if (!ProcessGroup(vc, errs))
            return false;
        }
        // Geometry-less views are group-only containers; they carry no
        // drawable elements and do not occupy a numbered view slot, so
        // viewCount is intentionally not incremented here.
        continue;
      }
      if (viewCount == 0) {
        VensimView *view = new VensimView();
        // XMILE <view> has no required title attribute; fall back to "main"
        // so the MDL sketch header has a non-empty title (the writer emits
        // the title verbatim into the \\\---/// header line).
        if (const char *vn = child->Attribute("name"))
          view->SetTitle(vn);
        else
          view->SetTitle("main");
        _model->AddView(view);
        XmileView xv(this, _model, view);
        if (!xv.ProcessView(child, errs))
          return false;
      } else {
        errs.push_back(std::string("multi-view XMILE: skipping view #") + std::to_string(viewCount + 1) +
                       " (v1 supports only the first view)");
      }
      ++viewCount;
    }
  }
  return true;
}

bool XmileReader::ProcessDimensions(tinyxml2::XMLElement *dimsEl, std::vector<std::string> &errs) {
  // Parity target: Vensim's `Dim: a, b, c` (VYacc.y:117) produces an Equation
  // with iEqType=':' whose RHS is an ExpressionSymbolList wrapping the element
  // SymbolList. MarkTypes (Variable.cpp:134-149) keys off the Symlist shape to
  // flip Dim to XMILE_Type_ARRAY and each element to XMILE_Type_ARRAY_ELM, so
  // matching that shape from the XMILE side is what makes the downstream
  // typing pass work without special-casing the source format.
  for (tinyxml2::XMLElement *dim = dimsEl->FirstChildElement("dim"); dim; dim = dim->NextSiblingElement("dim")) {
    const char *dimName = dim->Attribute("name");
    if (!dimName) {
      errs.push_back("<dim> with no name attribute");
      return false;
    }
    std::string normName = NormalizeName(dimName);
    Variable *dimVar = InsertVariable(normName);
    if (!dimVar) {
      errs.push_back(std::string("<dim name=\"") + dimName + "\">: name collides with a non-variable symbol");
      return false;
    }
    EnsureCanonicalName(dimVar, normName);

    SymbolList *symList = nullptr;
    bool hasSize = (dim->Attribute("size") != nullptr);
    if (hasSize) {
      int n = dim->IntAttribute("size", 0);
      if (n <= 0) {
        errs.push_back(std::string("<dim name=\"") + dimName + "\"> has invalid size attribute");
        return false;
      }
      symList = BuildIndexedElementList(n, errs);
    } else {
      symList = BuildElementList(dim, errs);
    }
    // Checked for BOTH branches: an ExpressionSymbolList carrying a null
    // SymList() is dereferenced unconditionally by Variable::MarkTypes,
    // XMILEGenerator::generateDimensions and MDLGenerator::EmitDimensionEntry,
    // so a dimension that could not be built has to fail the parse rather than
    // travel as one that looks built.
    if (!symList)
      return false;

    ExpressionSymbolList *rhs = new ExpressionSymbolList(pSymbolNameSpace, symList, NULL);
    AddEquationFor(dimVar, nullptr, rhs, ':');
  }
  return true;
}

SymbolList *XmileReader::BuildElementList(tinyxml2::XMLElement *dimEl, std::vector<std::string> &errs) {
  SymbolList *list = nullptr;
  for (tinyxml2::XMLElement *elem = dimEl->FirstChildElement("elem"); elem; elem = elem->NextSiblingElement("elem")) {
    const char *elemName = elem->Attribute("name");
    if (!elemName) {
      errs.push_back("<elem> with no name attribute");
      return nullptr;
    }
    std::string normName = NormalizeName(elemName);
    Variable *elemVar = InsertVariable(normName);
    if (!elemVar) {
      errs.push_back(std::string("<elem name=\"") + elemName + "\">: name collides with a non-variable symbol");
      return nullptr;
    }
    EnsureCanonicalName(elemVar, normName);
    if (!list)
      list = new SymbolList(pSymbolNameSpace, elemVar, false);
    else
      list->Append(elemVar, false);
  }
  if (!list) {
    const char *dimName = dimEl->Attribute("name");
    errs.push_back(std::string("<dim name=\"") + (dimName ? dimName : "(?)") +
                   "\"> has no <elem> children and no size attribute");
  }
  return list;
}

SymbolList *XmileReader::BuildIndexedElementList(int n, std::vector<std::string> &errs) {
  // XMILE permits numeric element names ("1","2",...) and the writer emits
  // them as <elem name="1"/> rather than reconstructing the size="N" shorthand,
  // so eagerly materializing the elements here is round-trip-safe -- a
  // self-round-trip lands in the explicit-elem path on the second read.
  //
  // Limitation: the synthesized
  // element Variable names are bare integer strings ("1", "2", ...).  The
  // equation lexer treats bare digit sequences as numeric literals, so an
  // equation referencing inflow[1] would parse the "1" as a number token rather
  // than resolving to the element Variable named "1".  In practice no model in
  // the current corpus uses indexed-dim subscript references in equation text
  // directly, so the collision is harmless today.  Supporting such references
  // would require either a non-numeric element-name shape (e.g. "_1", "e1")
  // or special-cased bracketed-index parsing in the lexer.
  SymbolList *list = nullptr;
  for (int i = 1; i <= n; ++i) {
    std::string name = std::to_string(i);
    Variable *elemVar = InsertVariable(name);
    if (!elemVar) {
      // size="n" states the dimension's extent exactly, so materializing n-1 of
      // the n elements is not a partial success -- it is a dimension of the
      // wrong size, which every consumer downstream would then trust. Report it
      // the way BuildElementList reports the same collision and give up.
      errs.push_back(std::string("<dim size=\"") + std::to_string(n) + "\">: element name \"" + name +
                     "\" collides with a non-variable symbol");
      return nullptr;
    }
    if (!list)
      list = new SymbolList(pSymbolNameSpace, elemVar, false);
    else
      list->Append(elemVar, false);
  }
  return list;
}

Variable *XmileReader::DeclareVariable(tinyxml2::XMLElement *el, std::vector<std::string> &errs) {
  const char *name = el->Attribute("name");
  if (!name) {
    errs.push_back(std::string("<") + el->Name() + "> with no name attribute");
    return nullptr;
  }
  // XMILE allows literal newlines inside name attributes for visual line
  // wrapping (the simlin/Stella corpus uses names like "fractional \ngrowth
  // rate"). Vensim identifiers are single-line, so the reader collapses to a
  // single-space separator at the read boundary -- matching what MDLGenerator
  // emits on the way out.
  std::string normName = NormalizeName(name);
  Variable *v = InsertVariable(normName);
  if (!v) {
    errs.push_back(std::string("<") + el->Name() + " name=\"" + name + "\">: name collides with a non-variable symbol");
    return nullptr;
  }
  EnsureCanonicalName(v, normName);
  return v;
}

Expression *XmileReader::ParseEqnFor(tinyxml2::XMLElement *ctxEl, const char *eqnText, std::vector<std::string> &errs) {
  std::vector<std::string> eqnErrs;
  Expression *rhs = ParseEquation(eqnText ? eqnText : "", eqnErrs);
  const bool hardErr = ForwardEqnDiagnostics(ElementContext(ctxEl), eqnErrs, errs);
  if (!rhs || hardErr)
    return nullptr;
  return rhs;
}

void XmileReader::AddEquationFor(Variable *v, SymbolList *lhsSubs, Expression *rhs, int token) {
  ExpressionVariable *lhsVar = new ExpressionVariable(pSymbolNameSpace, v, lhsSubs);
  LeftHandSide *lhs = new LeftHandSide(pSymbolNameSpace, lhsVar, NULL, NULL, 0);
  Equation *eq = new Equation(pSymbolNameSpace, lhs, rhs, token);
  v->AddEq(eq);
}

bool XmileReader::ProcessAuxOrFlow(tinyxml2::XMLElement *el, std::vector<std::string> &errs) {
  // Handles <aux> and <flow> alike: a flow is structurally identical to an aux
  // from the reader's perspective (one or more equations attached to a
  // Variable). Post-parse, MarkStockFlows reclassifies a flow that is
  // referenced from a stock's <inflow>/<outflow> as XMILE_Type_FLOW; an
  // orphan <flow> stays XMILE_Type_AUX (MarkTypes only knows INTEG / AsFlow).
  //
  // <non_negative/> is an XMILE clamp marker; Vensim .mdl has no equivalent
  // representation, so it is dropped on output -- but not silently: an advisory
  // (WarnIfNonNegative, below) tells the user the converted model can go
  // negative where the source could not.
  Variable *v = DeclareVariable(el, errs);
  if (!v)
    return false;
  WarnIfNonNegative(el, v, errs);

  // Per-element form is signaled by any <element subscript="..."> child; the
  // <dimensions> sibling is then informational only. Apply-to-all (with or
  // without a <dimensions> child) uses a single Equation -- the scalar case
  // is the no-<dimensions> sub-case of apply-to-all.
  if (el->FirstChildElement("element"))
    return ProcessPerElementEquations(el, v, errs);
  return ProcessAppliesToAllEquation(el, v, el->FirstChildElement("dimensions"), errs);
}

bool XmileReader::ProcessStandaloneGf(tinyxml2::XMLElement *el, std::vector<std::string> &errs) {
  Variable *v = DeclareVariable(el, errs);
  if (!v)
    return false;
  // Same shape as the "no eqn + gf" branch of ProcessAppliesToAllEquation: the
  // equation's RHS is the ExpressionTable itself with the '(' token, matching
  // VensimParse::AddTable's standalone lookup form.
  ExpressionTable *table = ProcessGf(el, errs);
  if (!table)
    return false;
  AddEquationFor(v, nullptr, table, '(');
  AttachUnitsAndDoc(el, v);
  return true;
}

bool XmileReader::ProcessAppliesToAllEquation(tinyxml2::XMLElement *varEl, Variable *v, tinyxml2::XMLElement *dimsChild,
                                              std::vector<std::string> &errs) {
  const char *vname = varEl->Attribute("name");
  tinyxml2::XMLElement *eqnEl = varEl->FirstChildElement("eqn");
  tinyxml2::XMLElement *gfEl = varEl->FirstChildElement("gf");
  const char *eqnText = (eqnEl && eqnEl->GetText()) ? eqnEl->GetText() : "";
  // A whitespace-only or newline-only <eqn> body (e.g. `<eqn>\n</eqn>`) must
  // not be treated as a real equation: it would route to the WITH LOOKUP branch
  // and then fail to parse, silently dropping a standalone-GF variable. The
  // canonical "no eqn" state is either a null GetText() or a body whose only
  // characters are whitespace.
  const bool hasEqn =
      std::any_of(eqnText, eqnText + std::strlen(eqnText), [](unsigned char c) { return !std::isspace(c); });

  // Four combinations of <eqn> / <gf>:
  //   (1) eqn + no gf    -> regular aux/flow equation (scalar or a2a)
  //   (2) eqn + gf       -> WITH LOOKUP: rhs = ExpressionLookup(input, table)
  //   (3) no eqn + gf    -> standalone graphical function, token '(' a la
  //                         VensimParse::AddTable (VensimParse.cpp:173)
  //   (4) no eqn + no gf -> soft-warn and leave the Variable equation-free;
  //                         MarkVariableTypes will catch the dangling symbol
  //                         downstream if it matters.
  //
  // The LHS allocation is deferred into each success branch so case (4) never
  // produces orphan unconfirmed allocations -- the namespace would still tear
  // them down at destruction, but skipping the allocation entirely keeps
  // case (4) allocation-free.

  if (!hasEqn && !gfEl) {
    errs.push_back(std::string("<") + varEl->Name() + " name=\"" + (vname ? vname : "(?)") +
                   "\"> has neither <eqn> nor <gf>");
    return true;
  }

  Expression *rhs = nullptr;
  int eqToken = '=';

  if (hasEqn && !gfEl) {
    rhs = ParseEqnFor(varEl, eqnText, errs);
    if (!rhs)
      return false;
  } else if (hasEqn && gfEl) {
    Expression *input = ParseEqnFor(varEl, eqnText, errs);
    if (!input)
      return false;
    ExpressionTable *table = ProcessGf(gfEl, errs);
    if (!table) {
      delete input;
      return false;
    }
    rhs = new ExpressionLookup(pSymbolNameSpace, input, table);
  } else {
    // Standalone graphical function: the Equation's expression is the
    // ExpressionTable itself with token '(' (matches the standalone shape
    // VensimParse::AddTable produces for `var( [(0,0)(1,1)], ... )`).
    ExpressionTable *table = ProcessGf(gfEl, errs);
    if (!table)
      return false;
    rhs = table;
    eqToken = '(';
  }

  SymbolList *lhsSubs = dimsChild ? BuildAppliesToAllSubs(dimsChild, errs) : nullptr;
  AddEquationFor(v, lhsSubs, rhs, eqToken);

  AttachUnitsAndDoc(varEl, v);
  return true;
}

void XmileReader::AttachUnitsAndDoc(tinyxml2::XMLElement *varEl, Variable *v) {
  // <units> / <doc> are stored both as raw strings (the writer's fallback
  // path) and as a parsed UnitExpression (the writer's primary path, and the
  // shape the comparator reads via Variable::Units()). Without the parsed
  // expression, an XMILE -> MDL -> Vensim re-parse round trip would compare
  // the raw "a/b/c" text against the canonical "a/(b*c)" that the Vensim
  // parser produces and flag a spurious "units differ" diff.
  if (tinyxml2::XMLElement *u = varEl->FirstChildElement("units")) {
    if (const char *ut = u->GetText())
      AttachVariableUnits(v, ut);
  }
  if (tinyxml2::XMLElement *d = varEl->FirstChildElement("doc")) {
    if (const char *dt = d->GetText())
      v->SetComment(dt);
  }
}

bool XmileReader::ProcessPerElementEquations(tinyxml2::XMLElement *varEl, Variable *v, std::vector<std::string> &errs) {
  // Per-element + <gf> (variable-level or per-element) is rare and
  // intentionally unsupported: the per-element equations always take
  // precedence here. Surface a warning so the corpus tells us if it ever
  // appears.
  if (varEl->FirstChildElement("gf")) {
    errs.push_back(ElementContext(varEl) +
                   "<gf> on a per-element subscripted variable is not supported (skipping <gf>)");
  }
  for (tinyxml2::XMLElement *elemEl = varEl->FirstChildElement("element"); elemEl;
       elemEl = elemEl->NextSiblingElement("element")) {
    if (elemEl->FirstChildElement("gf")) {
      const char *subs = elemEl->Attribute("subscript");
      errs.push_back(ElementContext(varEl) + "<gf> on <element subscript=\"" + (subs ? subs : "(?)") +
                     "\"> is not supported (skipping <gf>)");
    }
    const char *subs = elemEl->Attribute("subscript");
    if (!subs) {
      errs.push_back(ElementContext(varEl) + "<element> with no subscript attribute");
      return false;
    }
    SymbolList *lhsSubs = ParseSubscriptList(subs);
    if (!lhsSubs) {
      errs.push_back(ElementContext(varEl) + "empty or malformed subscript=\"" + subs + "\"");
      return false;
    }

    tinyxml2::XMLElement *eqnEl = elemEl->FirstChildElement("eqn");
    if (!eqnEl) {
      errs.push_back(ElementContext(varEl) + "<element subscript=\"" + subs + "\"> has no <eqn>");
      return false;
    }
    Expression *rhs = ParseEqnFor(varEl, eqnEl->GetText(), errs);
    if (!rhs)
      return false;
    AddEquationFor(v, lhsSubs, rhs, '=');
  }
  // Variable-level <units> and <doc> apply to all per-element equations -- the
  // writer reads back one units string per variable, not one per element.
  AttachUnitsAndDoc(varEl, v);
  return true;
}

SymbolList *XmileReader::ParseSubscriptList(const std::string &subscriptAttr) {
  // XMILEGenerator emits comma-space ("a, b") but Stella and some hand-edited
  // files emit a bare comma. Splitting on ',' and trimming whitespace handles
  // both shapes uniformly. Empty fragments (trailing or doubled commas) are
  // dropped silently; a fully-empty result is signalled by returning nullptr
  // so the caller can attach context to the diagnostic.
  SymbolList *list = nullptr;
  size_t i = 0;
  const size_t n = subscriptAttr.size();
  while (i < n) {
    size_t comma = subscriptAttr.find(',', i);
    size_t end = (comma == std::string::npos) ? n : comma;
    size_t start = i;
    while (start < end && std::isspace(static_cast<unsigned char>(subscriptAttr[start])))
      ++start;
    size_t finish = end;
    while (finish > start && std::isspace(static_cast<unsigned char>(subscriptAttr[finish - 1])))
      --finish;
    if (finish > start) {
      std::string token = subscriptAttr.substr(start, finish - start);
      Variable *elemVar = InsertVariable(NormalizeName(token.c_str()));
      if (elemVar) {
        if (!list)
          list = new SymbolList(pSymbolNameSpace, elemVar, false);
        else
          list->Append(elemVar, false);
      }
    }
    if (comma == std::string::npos)
      break;
    i = comma + 1;
  }
  return list;
}

SymbolList *XmileReader::BuildAppliesToAllSubs(tinyxml2::XMLElement *dimsEl, std::vector<std::string> &errs) {
  SymbolList *list = nullptr;
  for (tinyxml2::XMLElement *dim = dimsEl->FirstChildElement("dim"); dim; dim = dim->NextSiblingElement("dim")) {
    const char *dname = dim->Attribute("name");
    if (!dname) {
      // A <dim> inside a variable's <dimensions> block with no name attribute
      // is malformed. Report it to match the diagnostic emitted by
      // ProcessDimensions for the same malformed input, then continue building
      // as much of the subscript list as possible so downstream passes can
      // still diagnose further errors in the same variable.
      errs.push_back("<dim> with no name attribute");
      continue;
    }
    Variable *dimVar = InsertVariable(NormalizeName(dname));
    if (!dimVar)
      continue;
    if (!list)
      list = new SymbolList(pSymbolNameSpace, dimVar, false);
    else
      list->Append(dimVar, false);
  }
  return list;
}

// Parse a comma-separated list of doubles into a vector. Whitespace around
// tokens is tolerated (XMILE permits line-wrapped <ypts>/<xpts> bodies).
// strtod stops at the first non-numeric character so " 5.0 " parses as 5.0
// with no trailing-garbage check needed. When errs is non-null and any tokens
// are skipped, a single diagnostic naming the context (e.g. "<gf><ypts>") is
// appended so the caller does not have to guess why xpts/ypts lengths diverge.
static std::vector<double> ParseDoubleList(const char *text, std::vector<std::string> *errs, const char *context,
                                           char sep) {
  std::vector<double> out;
  if (!text)
    return out;
  const char *p = text;
  int skipped = 0;
  bool in_bad_token = false;
  while (*p) {
    const char *sep_start = p;
    while (*p && (std::isspace(static_cast<unsigned char>(*p)) || *p == sep))
      ++p;
    // Crossing any separator characters means we entered a new token region,
    // so a subsequent bad run counts as a fresh distinct bad token.
    if (p != sep_start)
      in_bad_token = false;
    if (!*p)
      break;
    char *endp = nullptr;
    double v = std::strtod(p, &endp);
    if (endp == p) {
      // No digits consumed -- skip one byte and resume so a stray non-numeric
      // token doesn't trap the loop. Count the transition into a bad run once
      // rather than once per byte so `skipped` reflects distinct token count.
      if (!in_bad_token) {
        ++skipped;
        in_bad_token = true;
      }
      ++p;
      continue;
    }
    in_bad_token = false;
    out.push_back(v);
    p = endp;
  }
  if (skipped > 0 && errs) {
    std::string msg = std::to_string(skipped) + " malformed token(s) skipped while parsing";
    if (context) {
      msg += " ";
      msg += context;
    }
    errs->push_back(msg);
  }
  return out;
}

ExpressionTable *XmileReader::ProcessGf(tinyxml2::XMLElement *gf, std::vector<std::string> &errs) {
  ExpressionTable *table = new ExpressionTable(pSymbolNameSpace);

  // XMILE 1.0 allows type="continuous" (default), "extrapolate", or "discrete".
  // Only "extrapolate" has a direct xmutil representation; "discrete" maps onto
  // continuous with a warning so the rest of the model still loads.
  if (const char *gtype = gf->Attribute("type")) {
    std::string t(gtype);
    if (t == "extrapolate") {
      table->SetExtrapolate(true);
    } else if (t == "discrete") {
      errs.push_back("<gf type=\"discrete\"> treated as continuous (xmutil has no discrete representation)");
    }
  }

  tinyxml2::XMLElement *yptsEl = gf->FirstChildElement("ypts");
  if (!yptsEl || !yptsEl->GetText()) {
    errs.push_back("<gf> has no <ypts>");
    delete table;
    return nullptr;
  }
  // XMILE lets <xpts>/<ypts> override the default comma item-separator via a sep
  // attribute (e.g. sep=";" for "0;5;10"); we split on that character instead.
  auto listSep = [](tinyxml2::XMLElement *el) -> char {
    const char *s = el->Attribute("sep");
    return (s && s[0]) ? s[0] : ',';
  };
  std::vector<double> ys = ParseDoubleList(yptsEl->GetText(), &errs, "<gf><ypts>", listSep(yptsEl));
  if (ys.empty()) {
    errs.push_back("<gf><ypts> is empty");
    delete table;
    return nullptr;
  }

  // Explicit <xpts> wins; otherwise derive even-spaced xs from <xscale>. The
  // fishbanks corpus uses the xscale-only shape, so the derived path is on the
  // round-trip critical path.
  std::vector<double> xs;
  tinyxml2::XMLElement *xptsEl = gf->FirstChildElement("xpts");
  if (xptsEl && xptsEl->GetText()) {
    xs = ParseDoubleList(xptsEl->GetText(), &errs, "<gf><xpts>", listSep(xptsEl));
  } else if (tinyxml2::XMLElement *xscaleEl = gf->FirstChildElement("xscale")) {
    double xmin = xscaleEl->DoubleAttribute("min", 0.0);
    double xmax = xscaleEl->DoubleAttribute("max", 1.0);
    if (xmax <= xmin) {
      errs.push_back(std::string("<xscale min=") + std::to_string(xmin) + " max=" + std::to_string(xmax) +
                     "> not strictly increasing; graphical function may produce constant or reversed output");
    }
    int n = static_cast<int>(ys.size());
    if (n == 1) {
      xs.push_back(xmin);
    } else {
      double step = (xmax - xmin) / (n - 1);
      for (int i = 0; i < n; ++i)
        xs.push_back(xmin + i * step);
    }
  } else {
    errs.push_back("<gf> has neither <xpts> nor <xscale>");
    delete table;
    return nullptr;
  }

  // Length mismatch is permissive -- silently truncate to the shorter side
  // after warning. Mirrors MDLFormat::WriteLookupBody (MDLFormat.cpp:280).
  size_t npairs = std::min(xs.size(), ys.size());
  if (xs.size() != ys.size()) {
    errs.push_back("<gf> has mismatched <xpts>/<ypts> lengths; truncating to " + std::to_string(npairs));
  }
  for (size_t i = 0; i < npairs; ++i)
    table->AddPair(xs[i], ys[i]);
  return table;
}

Expression *XmileReader::BuildNetFlowSubscripted(const std::vector<std::string> &inflowNames,
                                                 const std::vector<std::string> &outflowNames, SymbolList *lhsSubs,
                                                 std::vector<std::string> &errs) {
  // Shape constraint enforced by is_all_plus_minus (Expression.cpp:86): a
  // left-leaning tree of binary +/- operators with bare ExpressionVariable
  // leaves on the right of each subtraction. Anything else (a unary minus,
  // a parenthesized subgroup, a constant on the right) makes MarkStockFlows
  // mark the flow list invalid and the writer falls back to emitting the raw
  // INTEG, losing the per-flow round-trip.
  //
  // When lhsSubs is non-null each flow reference receives an independent
  // SymbolList::Clone so the per-ExpressionVariable destructor can free its
  // own pSubList without crashing the sibling references at teardown.
  //
  // A flow name held by a non-Variable symbol (a registered builtin such as
  // STEP) makes InsertVariable answer NULL. There is no partial expression
  // worth keeping: the tree built here IS the stock's equation, and an
  // ExpressionVariable with a NULL Variable is dereferenced unconditionally by
  // both writers. So the builder fails, freeing what it has built -- matching
  // what DeclareVariable already does for a <flow> DECLARED under that name.
  auto makeRef = [&](const std::string &name, const char *role) -> Expression * {
    Variable *v = InsertVariable(name);
    if (!v) {
      errs.push_back(std::string("<") + role + ">" + name + "</" + role +
                     ">: name collides with a non-variable symbol");
      return nullptr;
    }
    SymbolList *subsCopy = lhsSubs ? lhsSubs->Clone() : nullptr;
    return new ExpressionVariable(pSymbolNameSpace, v, subsCopy);
  };

  Expression *acc = nullptr;
  for (const std::string &name : inflowNames) {
    Expression *ev = makeRef(name, "inflow");
    if (!ev) {
      delete acc;
      return nullptr;
    }
    if (!acc)
      acc = ev;
    else
      acc = new ExpressionAdd(pSymbolNameSpace, acc, ev);
  }
  // No inflows but at least one outflow: lead with a unary minus on the first
  // outflow ("-O1 - O2 ...") rather than seeding "0 - O1 - O2 ...". A leading
  // literal 0 makes is_all_plus_minus (Expression.cpp) reject the whole chain,
  // leaving the outflows unclassified as flows; the unary-minus form is exactly
  // what Vensim writes for an outflow-only stock and what the classifier
  // accepts, so the flow types survive the round trip.
  size_t outflowStart = 0;
  if (!acc && !outflowNames.empty()) {
    Expression *ev = makeRef(outflowNames[0], "outflow");
    if (!ev)
      return nullptr;
    acc = new ExpressionUnaryMinus(pSymbolNameSpace, ev, nullptr);
    outflowStart = 1;
  }
  // No inflows AND no outflows: a literal 0. is_all_plus_minus rejects pure
  // numbers, but a stock with neither inflow nor outflow is a degenerate edge
  // case the writer wouldn't round-trip anyway.
  if (!acc)
    acc = new ExpressionNumber(pSymbolNameSpace, 0.0);
  for (size_t i = outflowStart; i < outflowNames.size(); i++) {
    Expression *ev = makeRef(outflowNames[i], "outflow");
    if (!ev) {
      delete acc;
      return nullptr;
    }
    acc = new ExpressionSubtract(pSymbolNameSpace, acc, ev);
  }
  return acc;
}

Expression *XmileReader::BuildIntegExpression(Expression *netFlow, Expression *init, std::vector<std::string> &errs) {
  Function *integ = static_cast<Function *>(pSymbolNameSpace->Find("INTEG"));
  if (!integ) {
    // The XmileReader ctor seeds this; reaching this branch means the
    // namespace was tampered with after construction.
    errs.push_back("INTEG function not registered (xmutil function table is empty)");
    return nullptr;
  }
  ExpressionList *args = new ExpressionList(pSymbolNameSpace);
  args->Append(netFlow);
  args->Append(init);
  return new ExpressionFunctionMemory(pSymbolNameSpace, integ, args);
}

bool XmileReader::ProcessStock(tinyxml2::XMLElement *stock, std::vector<std::string> &errs) {
  // Synthesizes an INTEG expression at read time so the existing post-parse
  // pipeline (Model::MarkVariableTypes -> MarkTypes -> MarkStockFlows) sees
  // the same shape VensimParse produces for `s = INTEG(in - out, init)`.
  // MarkStockFlows walks the INTEG arg-0 expression to populate
  // stock->Inflows() / Outflows(); the writers (XMILE and MDL) read from those
  // lists. There is no separate "record the flow names" channel.
  //
  // Subscripted stocks come in two shapes:
  //   * apply-to-all: a single top-level <eqn> + <dimensions> child. One
  //     Equation, LHS subscripted with the dim list, net-flow references
  //     mirror the LHS subscripts (so MarkStockFlows still sees in - out).
  //   * per-element: one <element subscript="..."><eqn>...</eqn></element>
  //     per element. One Equation per element, each with its own LHS subscript
  //     and a net-flow expression whose flow references carry that same
  //     subscript -- which is why every reference needs an independent
  //     SymbolList::Clone (sharing would double-free at teardown).
  Variable *v = DeclareVariable(stock, errs);
  if (!v)
    return false;
  // DeclareVariable verified the name attribute; EnsureCanonicalName made the
  // Variable's stored name the normalized form.
  const char *name = stock->Attribute("name");
  const std::string &normName = v->GetName();
  WarnIfNonNegative(stock, v, errs);

  // <gf> has no valid semantics on a stock -- the XMILE writer asserts it
  // (the XMILE_Type_AUX || XMILE_Type_FLOW assert in XMILEGenerator.cpp's
  // <gf> emission) and no engine consumes it. Surface a warning so a malformed
  // corpus file is visible, then ignore the element.
  if (stock->FirstChildElement("gf")) {
    errs.push_back(ElementContext(stock) + "<gf> on a stock is not supported (skipping <gf>)");
  }

  std::vector<std::string> inflowNames;
  std::vector<std::string> outflowNames;
  for (tinyxml2::XMLElement *child = stock->FirstChildElement("inflow"); child;
       child = child->NextSiblingElement("inflow")) {
    if (const char *t = child->GetText())
      inflowNames.push_back(NormalizeName(t));
  }
  for (tinyxml2::XMLElement *child = stock->FirstChildElement("outflow"); child;
       child = child->NextSiblingElement("outflow")) {
    if (const char *t = child->GetText())
      outflowNames.push_back(NormalizeName(t));
  }

  // Record the structural stock<->flow association for the view pass: a
  // flow's pipe endpoint may only anchor on a stock that lists it here (see
  // StocksForFlow).
  for (const std::string &f : inflowNames)
    _flowToStocks[FoldNameKey(f)].push_back(FoldNameKey(normName));
  for (const std::string &f : outflowNames)
    _flowToStocks[FoldNameKey(f)].push_back(FoldNameKey(normName));

  if (stock->FirstChildElement("element")) {
    // Per-element subscripted stock.
    for (tinyxml2::XMLElement *elemEl = stock->FirstChildElement("element"); elemEl;
         elemEl = elemEl->NextSiblingElement("element")) {
      const char *subs = elemEl->Attribute("subscript");
      if (!subs) {
        errs.push_back(ElementContext(stock) + "<element> with no subscript attribute");
        return false;
      }
      SymbolList *elementSubList = ParseSubscriptList(subs);
      if (!elementSubList) {
        errs.push_back(ElementContext(stock) + "empty subscript=\"" + subs + "\"");
        return false;
      }

      tinyxml2::XMLElement *eqnEl = elemEl->FirstChildElement("eqn");
      if (!eqnEl) {
        errs.push_back(ElementContext(stock) + "<element subscript=\"" + subs + "\"> has no <eqn>");
        return false;
      }
      Expression *init = ParseEqnFor(stock, eqnEl->GetText(), errs);
      if (!init)
        return false;

      Expression *netFlow = BuildNetFlowSubscripted(inflowNames, outflowNames, elementSubList, errs);
      if (!netFlow)
        return false;
      Expression *integExpr = BuildIntegExpression(netFlow, init, errs);
      if (!integExpr)
        return false;

      AddEquationFor(v, elementSubList, integExpr, '=');
    }
  } else {
    // Apply-to-all (subscripted or scalar). Scalar is just the no-<dimensions>
    // sub-case (BuildNetFlowSubscripted with a null lhsSubs).
    tinyxml2::XMLElement *eqnEl = stock->FirstChildElement("eqn");
    if (!eqnEl) {
      errs.push_back(std::string("<stock name=\"") + name + "\"> has no <eqn>");
      return false;
    }
    Expression *init = ParseEqnFor(stock, eqnEl->GetText(), errs);
    if (!init)
      return false;

    tinyxml2::XMLElement *dimsChild = stock->FirstChildElement("dimensions");
    SymbolList *lhsSubs = dimsChild ? BuildAppliesToAllSubs(dimsChild, errs) : nullptr;
    Expression *netFlow = BuildNetFlowSubscripted(inflowNames, outflowNames, lhsSubs, errs);
    if (!netFlow)
      return false;
    Expression *integExpr = BuildIntegExpression(netFlow, init, errs);
    if (!integExpr)
      return false;

    AddEquationFor(v, lhsSubs, integExpr, '=');
  }

  AttachUnitsAndDoc(stock, v);
  return true;
}

bool XmileReader::ValidateLookupTargets(std::vector<std::string> &errs) {
  std::set<std::string> phantoms;
  for (Variable *v : _model->GetVariables(nullptr)) {
    for (Equation *eq : v->GetAllEquations())
      CollectPhantomLookups(eq->GetExpression(), phantoms);
    for (Equation *eq : v->GetAllInitEquations())
      CollectPhantomLookups(eq->GetExpression(), phantoms);
  }
  for (const std::string &name : phantoms) {
    errs.push_back("'" + name + "' is applied as a function or lookup (" + name +
                   "(...)) but is never defined; it may be an unsupported or misspelled function name");
  }
  return phantoms.empty();
}

void XmileReader::WarnIfNonNegative(tinyxml2::XMLElement *varEl, Variable *v, std::vector<std::string> &errs) {
  tinyxml2::XMLElement *nn = varEl->FirstChildElement("non_negative");
  if (!nn)
    return;
  // A self-closing <non_negative/> or an empty body means the clamp is on; an
  // explicit body is a case-insensitive, whitespace-trimmed boolean, so
  // <non_negative>false</non_negative> turns it off (no advisory).
  bool clamp = true;
  if (const char *body = nn->GetText()) {
    std::string t(body);
    size_t a = t.find_first_not_of(" \t\r\n");
    if (a != std::string::npos) {
      size_t b = t.find_last_not_of(" \t\r\n");
      std::string val = t.substr(a, b - a + 1);
      clamp = !StringMatch(val, "false") && val != "0";
    }
  }
  if (!clamp)
    return;
  errs.push_back("warning: <non_negative> clamp on '" + v->GetName() +
                 "' is not representable in Vensim .mdl output; the converted model can go negative");
}

void XmileReader::ScanForShadowedKeywords(tinyxml2::XMLElement *model) {
  // Only <variables> children can declare a variable a bare identifier in an
  // equation would resolve to. <dimensions> declares dimension and element
  // Variables, but those are reachable only from subscript positions, which
  // the grammar routes away from xpyy_resolve_symbol's keyword check.
  tinyxml2::XMLElement *variables = model->FirstChildElement("variables");
  if (!variables)
    return;
  for (tinyxml2::XMLElement *child = variables->FirstChildElement(); child; child = child->NextSiblingElement()) {
    const char *tag = child->Name();
    if (!tag || IsForeignNamespace(tag))
      continue;
    const char *name = child->Attribute("name");
    if (name && StringMatch(NormalizeName(name), "pi"))
      _declaresPi = true;
  }
}

bool XmileReader::ProcessModel(tinyxml2::XMLElement *model, std::vector<std::string> &errs) {
  // Multi-<model> rejection is handled by ProcessFile's envelope pre-pass;
  // here we only walk the <variables> children of the one <model> we're
  // given.
  ScanForShadowedKeywords(model);
  tinyxml2::XMLElement *variables = model->FirstChildElement("variables");
  if (!variables)
    return true;
  for (tinyxml2::XMLElement *child = variables->FirstChildElement(); child; child = child->NextSiblingElement()) {
    const char *name = child->Name();
    if (!name)
      continue;
    if (IsForeignNamespace(name))
      continue;
    std::string tag(name);
    if (tag == "module") {
      // <module> elements signal a submodel, which is not supported. The
      // error message names the offending
      // module when an attribute is present so the diagnostic points the
      // human at the failing element.
      const char *modname = child->Attribute("name");
      std::string msg = "<module";
      if (modname) {
        msg += " name=\"";
        msg += modname;
        msg += "\"";
      }
      msg += ">: modules are not supported";
      errs.push_back(msg);
      return false;
    }
    if (tag == "aux" || tag == "flow") {
      if (!ProcessAuxOrFlow(child, errs))
        return false;
    } else if (tag == "gf") {
      // A top-level <gf name="..."> under <variables> is a standalone named
      // graphical function -- a lookup variable referenced elsewhere as
      // name(input). It is a sibling of <aux>, not a child of one, so it needs
      // its own handler; without it the table data (and the variable itself)
      // would be silently dropped and any name(input) reference left dangling.
      if (!ProcessStandaloneGf(child, errs))
        return false;
    } else if (tag == "stock") {
      if (!ProcessStock(child, errs))
        return false;
    } else if (tag == "dimensions") {
      // The <dimensions> child of <variables> is informational on subscripted
      // variables and is handled inside ProcessAuxOrFlow / ProcessStock.
      // A stray <dimensions> at <variables> scope (legal in some XMILE
      // dialects but never emitted by xmutil) is silently dropped here.
      continue;
    } else if (IsStellaUIWidget(name)) {
      continue;
    } else {
      // Unknown element inside <variables>. Quiet for now -- if a corpus run
      // surfaces something noteworthy, we can tighten to an error.
      continue;
    }
  }
  // The top-level <dimensions> element is a sibling of <variables> under
  // <model>. Walk it after the variables so any subscripted variable that
  // referenced the dim by name during equation parsing already has its
  // placeholder Variable in the namespace -- ProcessDimensions then attaches
  // the dim Variable's defining equation. MarkTypes (post-parse) flips both
  // sides to XMILE_Type_ARRAY / XMILE_Type_ARRAY_ELM.
  if (tinyxml2::XMLElement *dims = model->FirstChildElement("dimensions")) {
    if (!ProcessDimensions(dims, errs))
      return false;
  }
  // <views> contains layout geometry and (in some writer paths) the only
  // surviving record of group membership; ProcessViews reads both.
  if (tinyxml2::XMLElement *views = model->FirstChildElement("views")) {
    if (!ProcessViews(views, errs))
      return false;
  }
  return true;
}

void XmileReader::SetControlVariable(const std::string &name, double value) {
  // Mirror the shape VensimParse produces for control variables: a Variable
  // in the namespace whose first equation is a constant-numeric expression.
  // The writer's GetConstanValue path reads from this equation; the engine
  // also has parallel _initial_time / _final_time / _dt fields the caller
  // updates separately.
  //
  // ProcessSimSpecs is dispatched before ProcessModel in the envelope loop,
  // so these variables are freshly created and equation-free here. However,
  // a <group><var>INITIAL TIME</var></group> placeholder created by
  // ProcessGroup can arrive before sim_specs has run (if the <views> walk
  // precedes <sim_specs> in the document). Guard against appending a second
  // equation onto an already-populated variable.
  Variable *v = InsertVariable(name);
  if (!v)
    return;
  if (!v->GetAllEquations().empty())
    return;
  AddEquationFor(v, nullptr, new ExpressionNumber(pSymbolNameSpace, value), '=');
}

Variable *XmileReader::FindVariable(const std::string &name) {
  Symbol *sym = pSymbolNameSpace->Find(name);
  if (sym && sym->isType() == Symtype_Variable)
    return static_cast<Variable *>(sym);
  return nullptr;
}

Expression *XmileReader::ParseEquation(const std::string &text, std::vector<std::string> &errs) {
  // The bison shims (xpyy_set_result, xpyylex, xpyyerror) reach into the
  // reader through XPObject. We stash the lexer, errs sink, and result slot
  // for the duration of the parse, then clear them so subsequent calls or
  // unrelated reader use can't see stale pointers. The lex instance is
  // stack-local because XMILE has no global per-document equation state --
  // each <eqn> stands alone.
  XmileEqLex lex;
  lex.Initialize(text.c_str(), text.size());

  // No save/restore of prior slot values: ParseEquation is never re-entered
  // (the bison actions only build Expression nodes; none of them parses), so
  // the slots are always null on entry and clearing on exit is sufficient.
  _currentLex = &lex;
  _currentErrs = &errs;
  _lastParsedExpr = nullptr;

  const int rc = xpyyparse();

  Expression *result = _lastParsedExpr;

  _currentLex = nullptr;
  _currentErrs = nullptr;
  _lastParsedExpr = nullptr;

  if (rc != 0) {
    if (errs.empty())
      errs.push_back("parse error in equation: " + text);
    // Free any partial Expression we may have stashed via xpyy_set_result
    // before the abort -- xpyyerror is allowed to set it before YYABORT
    // (the apostrophe rule, for one, does not).
    delete result;
    return nullptr;
  }
  return result;
}

Variable *XmileReader::InsertVariable(const std::string &name) {
  Symbol *sym = pSymbolNameSpace->Find(name);
  if (sym) {
    // A non-Variable entry under this name is a type collision (e.g. a
    // Function registered under "INTEG"); surfacing it as a soft failure
    // (nullptr) lets the equation parser tag the error rather than crashing.
    // Non-Variable symbols return nullptr; the caller may surface a typed
    // error if needed.
    if (sym->isType() == Symtype_Variable)
      return static_cast<Variable *>(sym);
    return nullptr;
  }
  // The Variable ctor registers itself in pSymbolNameSpace; the next Find for
  // the same name will hit.
  return new Variable(pSymbolNameSpace, name);
}

void XmileReader::EnsureCanonicalName(Variable *v, const std::string &declaredName) {
  if (!v || v->GetName() == declaredName)
    return;
  // The hashtable key uses ToLowerSpace, which folds `_` and ` ` together; the
  // pre- and post-rename keys are therefore the same entry, so a direct SetName
  // (rather than going through SymbolNameSpace::Rename, which refuses to
  // re-key onto a colliding slot) is safe and leaves the hash consistent. The
  // alternate-name field is set by Variable::AddEq on the first equation; we
  // mutate sName before equations are attached at the declaration site, so the
  // later AddEq picks up the canonical form.
  v->SetName(declaredName);
}
