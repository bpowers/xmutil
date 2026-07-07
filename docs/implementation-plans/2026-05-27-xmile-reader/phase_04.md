# XMILE Reader Implementation Plan — Phase 4

**Goal:** Handle XMILE's stock / flow structure so the existing `MarkVariableTypes` reconstructs `INTEG` correctly and `logistic-growth` round-trips end-to-end.

**Architecture:** Replace the Phase 3 stub `ProcessStock` / `ProcessFlow` bodies. `<flow>` becomes an ordinary `Variable` with its equation attached (same shape as an aux). `<stock>` becomes a `Variable` whose stored equation is a **synthesized `INTEG(net_flow, init)` expression** built at read time: arg 0 is `inflow1 + inflow2 - outflow1 - outflow2 - ...`, arg 1 is the parsed initial-value expression. This mirrors what `VensimParse` produces for `s = INTEG(in - out, 100)`, so the existing `MarkTypes` and `MarkStockFlows` passes operate on it unchanged.

**Tech Stack:** Phase 2's equation parser, Phase 3's DOM walker, the `Expression*` / `Equation` / `ExpressionFunctionMemory` API.

**Scope:** Phase 4 of 8.

**Codebase verified:** 2026-05-28

---

## Acceptance Criteria Coverage

This phase implements and tests:

### xmile-reader.AC1: Model populates from XMILE via a stable API
- **xmile-reader.AC1.1 Success** (stock+flow slice): a Model with stocks and flows runs through `MarkVariableTypes` / `AdjustGroupNames` / `CheckGhostOwners` without error.

### xmile-reader.AC3: Round-trip preserves the model
- **xmile-reader.AC3.1 Success** (stock+flow slice): for `logistic-growth/model.xmile`, XMILE → Model → XMILE → Model' yields a `Model` equivalent to the first per `ModelComparator`. *(Scalar slice already covered in Phase 3; this phase adds stocks and flows.)*
- **xmile-reader.AC3.3 Success** (stock+flow slice): `logistic-growth/model.xmile` converts XMILE → Model → MDL → Model' yielding an equivalent `Model`.

---

## Codebase verification findings

- ✗ **Design's claim that the XMILE reader records inflow/outflow names on the Variable for `MarkStockFlows` to consume is INCORRECT.** `MarkStockFlows` (`src/Symbol/Variable.cpp:210-276`) does not consume a separate name list — it walks the stored INTEG expression via `TestMarkFlows` → `is_all_plus_minus`. The reader must **synthesize the full INTEG expression at read time**, matching the Vensim parser's output.
- ✓ `Model::MarkVariableTypes(ns)` (`src/Model.cpp:390-442`) runs two passes: Pass 1 calls `var->MarkTypes(ns)` to type each variable based on its expression shape (detects `EXPTYPE_FunctionMemory` + `IsIntegrator()` → `XMILE_Type_STOCK`); Pass 2 calls `var->MarkStockFlows(ns)` on each stock to discover its flows by walking the stored INTEG.
- ✓ `INTEG` is registered at `src/Function/Function.h:308-312` with arity 2 (arg 0 = active, arg 1 = init). `IsIntegrator()` returns true. The reader looks it up by name via `SymbolNameSpace::Find("INTEG")` and constructs `ExpressionFunctionMemory(sns, fn, args)`.
- ✓ The Vensim parser produces stocks of the shape:
  ```
  Variable(stock) — vEquations[0] = Equation { lhs = stock, rhs = ExpressionFunctionMemory(INTEG, [net_flow_expr, init_expr]) }
  ```
  The XMILE reader must produce the same shape.
- ✓ `MDLGenerator::EmitStockEntry` (`src/Mdl/MDLGenerator.cpp:626-701`) reads the INTEG expression directly from `vEquations`. The MDL writer **does not** re-synthesize INTEG from Inflows()/Outflows(); it requires the INTEG to already exist on the stock variable. This locks in the read-side contract.
- ✓ `XMILEGenerator` (`src/Xmile/XMILEGenerator.cpp:347-378`) emits `<inflow>` / `<outflow>` from `var->Inflows()` / `var->Outflows()`. Those lists are populated by `MarkStockFlows` after parsing — the reader does **not** set them manually.
- ✓ `Variable::Inflows()` and `Variable::Outflows()` exist (`src/Symbol/Variable.h`) and return `std::vector<Variable *> &`. These are output-only for the reader — read by the writers, populated by `MarkStockFlows`.
- ✓ `ExpressionFunctionMemory(SymbolNameSpace *, Function *, ExpressionList *)` is the correct constructor for `INTEG` calls (per Phase 2 finding). `ExpressionFunction` is used for memoryless functions; `ExpressionFunctionMemory` for stateful ones — `Function::IsMemoryless()` discriminates.
- ✓ The `INTEG` `Function*` returns `IsMemoryless() == false`. Constructing `ExpressionFunction` instead of `ExpressionFunctionMemory` would silently produce a wrong-shape tree that `is_all_plus_minus` rejects. The reader must construct the memory variant.
- ✓ Element ordering inside `<variables>`: the corpus puts `<stock>` first, then `<flow>`. The reader's `InsertVariable` lookup-or-create pattern handles forward references gracefully — when `ProcessStock` builds an `ExpressionVariable` for an inflow name, the flow's `Variable` is created (empty) if absent, and its actual equation gets attached later when `ProcessFlow` runs.
- ✓ `logistic-growth/model.xmile`:
  ```xml
  <stock name="population">
    <eqn>5</eqn>
    <inflow>net_birth_rate</inflow>
  </stock>
  <flow name="net_birth_rate">
    <eqn>fractional_growth_rate * population</eqn>
  </flow>
  ```
  One inflow, no outflows, scalar init `5`. This is the canonical Phase 4 fixture.
- ✗ The design references `MarkStockFlows` as the consumer of the inflow/outflow data — but as established, `MarkStockFlows` consumes the INTEG expression, not separate fields. The plan honors the design's intent (stocks-and-flows round-trip) while using the correct mechanism.

---

## Synthesizing INTEG: the read-side recipe

For each `<stock name="S"><eqn>INIT</eqn><inflow>I1</inflow><inflow>I2</inflow><outflow>O1</outflow></stock>`:

1. `Variable *stock = InsertVariable("S")` — lookup-or-create.
2. Parse `INIT` via `ParseEquation` → `Expression *init`.
3. For each `<inflow>` and `<outflow>`:
   - `Variable *f = InsertVariable(flowName)` (creates an empty Variable if `<flow>` hasn't been processed yet — its equation will be attached later when we hit the `<flow>` element).
   - `ExpressionVariable *ef = new ExpressionVariable(sns, f, NULL)`.
4. Build the net-flow expression:
   - With inflows `[I1, I2]` and outflows `[O1]`: build `(I1 + I2) - O1`. The first inflow seeds an `ExpressionVariable`; each subsequent inflow is folded in via `new ExpressionAdd(sns, prev, next)`; each outflow via `new ExpressionSubtract(sns, prev, next)`. If there are no inflows, start with `0` (`new ExpressionNumber(sns, 0)`) and subtract outflows from it. If there are no inflows or outflows (a "stand-alone" stock — rare; clouds at both ends), the net-flow is the literal `0`.
5. Build `ExpressionList *args` containing `[net_flow, init]` (in that order — INTEG's arity is 2, arg 0 active, arg 1 init).
6. Look up `Function *integ = static_cast<Function*>(sns->Find("INTEG"))`. Construct `ExpressionFunctionMemory *integExpr = new ExpressionFunctionMemory(sns, integ, args)`.
7. Build `LeftHandSide *lhs = new LeftHandSide(sns, new ExpressionVariable(sns, stock, NULL), NULL, NULL, 0)`.
8. `Equation *eq = new Equation(sns, lhs, integExpr, '=')` and `stock->AddEq(eq)`.

After all variables have been processed, the post-parse pipeline runs:
- `MarkVariableTypes` Pass 1 → `MarkTypes` finds the INTEG → sets `stock` to `XMILE_Type_STOCK`. Flows without INTEGs → `XMILE_Type_AUX` initially.
- `MarkVariableTypes` Pass 2 → `MarkStockFlows` walks the INTEG → finds `I1`, `I2`, `O1` as the `+`/`-` operands → sets each to `XMILE_Type_FLOW`, populates `stock->Inflows()` and `stock->Outflows()`.

The synthesized expression must be in the **clean** shape `is_all_plus_minus` recognizes: a left-leaning tree of `+`/`-` operations on bare `ExpressionVariable` leaves. Specifically:
- `ExpressionAdd(L, R)` where `L` is either an `ExpressionVariable` or another `+`/`-` tree, `R` is an `ExpressionVariable`.
- `ExpressionSubtract(L, R)` where `R` is an `ExpressionVariable`. Verify this constraint is satisfied for the multi-flow case by reading `is_all_plus_minus` (`src/Symbol/Expression.cpp:86-131`).

For the corner case **no inflows + no outflows** (a freestanding stock with init only — likely never appears in our corpus but could appear in third-party XMILE), the net-flow is `ExpressionNumber(0)`. `MarkStockFlows`'s `is_all_plus_minus` will accept this (a numeric `0` is the trivial sum), and the resulting `stock->Inflows()` / `Outflows()` are empty.

---

<!-- START_SUBCOMPONENT_A (tasks 1-2) -->

<!-- START_TASK_1 -->
### Task 1: ProcessFlow — `<flow>` element handler

**Verifies:** AC1.1, AC3.1, AC3.3 (flow slice).

**Files:**
- Modify: `src/Xmile/XmileReader.cpp` (`ProcessFlow` body — replace Phase 3 stub)

**Implementation notes:**

`<flow>` is structurally identical to `<aux>` from the reader's perspective — both attach a single equation to a Variable. The difference is post-parse classification: a `<flow>` that's referenced from a stock's `<inflow>` / `<outflow>` will get `XMILE_Type_FLOW` from `MarkStockFlows`; a `<flow>` that isn't referenced will get `XMILE_Type_AUX` (since `MarkTypes` only knows about INTEG and AsFlow).

```cpp
bool XmileReader::ProcessFlow(tinyxml2::XMLElement *flow, std::vector<std::string> &errs) {
  const char *name = flow->Attribute("name");
  if (!name) {
    errs.push_back("<flow> with no name attribute");
    return false;
  }
  std::string normName = NormalizeName(name);
  Variable *v = InsertVariable(normName);

  tinyxml2::XMLElement *eqnEl = flow->FirstChildElement("eqn");
  if (!eqnEl) {
    errs.push_back(std::string("<flow name=\"") + name + "\"> has no <eqn>");
    return false;
  }
  const char *eqnText = eqnEl->GetText();
  if (!eqnText) eqnText = "";
  std::vector<std::string> eqnErrs;
  Expression *rhs = ParseEquation(eqnText, eqnErrs);
  if (!rhs) {
    for (const std::string &e : eqnErrs) {
      errs.push_back(std::string("<flow name=\"") + name + "\">: " + e);
    }
    return false;
  }

  ExpressionVariable *lhsVar = new ExpressionVariable(pSymbolNameSpace, v, NULL);
  LeftHandSide *lhs = new LeftHandSide(pSymbolNameSpace, lhsVar, NULL, NULL, 0);
  Equation *eq = new Equation(pSymbolNameSpace, lhs, rhs, '=');
  v->AddEq(eq);

  if (tinyxml2::XMLElement *u = flow->FirstChildElement("units")) {
    if (const char *ut = u->GetText()) v->SetUnitsString(ut);
  }
  if (tinyxml2::XMLElement *d = flow->FirstChildElement("doc")) {
    if (const char *dt = d->GetText()) v->SetComment(dt);
  }
  // <non_negative/> attribute / element: XMILE marker for clamped flows. xmutil
  // has no equivalent representation in v1; silently drop. The writer also
  // doesn't emit it (no codebase support), so this is round-trip-safe.
  return true;
}
```

Note: this is essentially `ProcessAux` with a different tag name. **Do not refactor them into a shared helper yet** — Phase 5 will introduce dimension handling that diverges between aux and flow shape (apply-to-all vs per-element). Keep them parallel for now; a clean refactor can come in Phase 5 if natural.

**Verification:**
- `ninja -C out/Debug xmutil_test` succeeds.
- Existing tests pass.

**Commit:** `feat(xmile): <flow> reader`
<!-- END_TASK_1 -->

<!-- START_TASK_2 -->
### Task 2: ProcessStock — synthesize INTEG from `<eqn>` + `<inflow>` + `<outflow>`

**Verifies:** AC1.1, AC3.1, AC3.3 (stock slice).

**Files:**
- Modify: `src/Xmile/XmileReader.cpp` (`ProcessStock` body — replace Phase 3 stub; add a private helper for building the net-flow expression)
- Modify: `src/Xmile/XmileReader.h` (declare `BuildNetFlow` helper)

**Implementation notes:**

```cpp
// XmileReader.h, private section:
Expression *BuildNetFlowExpression(
    const std::vector<std::string> &inflowNames,
    const std::vector<std::string> &outflowNames);

// XmileReader.cpp:

// Build INTEG arg 0 from named in/outflows. The shape must satisfy
// is_all_plus_minus (src/Symbol/Expression.cpp:86-131) so MarkStockFlows can
// extract the flow Variables: a left-leaning tree of +/- ops on bare
// ExpressionVariable leaves, with subtraction restricted to RHS-leaf form.
Expression *XmileReader::BuildNetFlowExpression(
    const std::vector<std::string> &inflowNames,
    const std::vector<std::string> &outflowNames) {
  SymbolNameSpace *sns = pSymbolNameSpace;
  Expression *acc = nullptr;

  for (const std::string &name : inflowNames) {
    Variable *v = InsertVariable(name);
    Expression *ev = new ExpressionVariable(sns, v, NULL);
    if (!acc) {
      acc = ev;
    } else {
      acc = new ExpressionAdd(sns, acc, ev);
    }
  }
  // If no inflows, start with literal 0 so the subtraction case still produces
  // a well-formed expression.
  if (!acc) {
    acc = new ExpressionNumber(sns, 0.0);
  }
  for (const std::string &name : outflowNames) {
    Variable *v = InsertVariable(name);
    Expression *ev = new ExpressionVariable(sns, v, NULL);
    acc = new ExpressionSubtract(sns, acc, ev);
  }
  return acc;
}

bool XmileReader::ProcessStock(tinyxml2::XMLElement *stock, std::vector<std::string> &errs) {
  const char *name = stock->Attribute("name");
  if (!name) {
    errs.push_back("<stock> with no name attribute");
    return false;
  }
  std::string normName = NormalizeName(name);
  Variable *v = InsertVariable(normName);

  // Initial value: <eqn> body. For Phase 4 this is required (Phase 5 will
  // handle per-element subscripted stocks; Phase 6 standalone-gf-on-stock).
  tinyxml2::XMLElement *eqnEl = stock->FirstChildElement("eqn");
  if (!eqnEl) {
    errs.push_back(std::string("<stock name=\"") + name + "\"> has no <eqn>");
    return false;
  }
  const char *eqnText = eqnEl->GetText();
  if (!eqnText) eqnText = "";
  std::vector<std::string> eqnErrs;
  Expression *init = ParseEquation(eqnText, eqnErrs);
  if (!init) {
    for (const std::string &e : eqnErrs) {
      errs.push_back(std::string("<stock name=\"") + name + "\">: " + e);
    }
    return false;
  }

  // Collect <inflow> / <outflow> names.
  std::vector<std::string> inflowNames, outflowNames;
  for (tinyxml2::XMLElement *child = stock->FirstChildElement("inflow"); child;
       child = child->NextSiblingElement("inflow")) {
    if (const char *t = child->GetText()) inflowNames.push_back(NormalizeName(t));
  }
  for (tinyxml2::XMLElement *child = stock->FirstChildElement("outflow"); child;
       child = child->NextSiblingElement("outflow")) {
    if (const char *t = child->GetText()) outflowNames.push_back(NormalizeName(t));
  }

  // Synthesize INTEG(net_flow, init). Look up INTEG; constructing it via
  // SymbolNameSpace::Find rather than VensimParse means we need the function
  // table to be already registered. Phase 1's XmileReader ctor does NOT call
  // ReadyFunctions; we rely on the caller (convert_xmile_to_*, the test harness,
  // or Model::ParseXMILE drivers) to have a VensimParse construct-and-destruct
  // run first OR to register functions explicitly. See the "Function
  // registration timing" note below.
  Function *integ = static_cast<Function *>(pSymbolNameSpace->Find("INTEG"));
  if (!integ) {
    errs.push_back(std::string("<stock name=\"") + name +
                   "\">: INTEG function not registered (xmutil function table is empty)");
    return false;
  }

  Expression *netFlow = BuildNetFlowExpression(inflowNames, outflowNames);

  // Build the ExpressionList for INTEG([netFlow, init]).
  // ExpressionList(SymbolNameSpace*) is the ctor signature in
  // src/Symbol/ExpressionList.h — there is no default ctor; pass the namespace.
  ExpressionList *args = new ExpressionList(pSymbolNameSpace);
  args->Append(netFlow);
  args->Append(init);

  ExpressionFunctionMemory *integExpr =
      new ExpressionFunctionMemory(pSymbolNameSpace, integ, args);

  ExpressionVariable *lhsVar = new ExpressionVariable(pSymbolNameSpace, v, NULL);
  LeftHandSide *lhs = new LeftHandSide(pSymbolNameSpace, lhsVar, NULL, NULL, 0);
  Equation *eq = new Equation(pSymbolNameSpace, lhs, integExpr, '=');
  v->AddEq(eq);

  if (tinyxml2::XMLElement *u = stock->FirstChildElement("units")) {
    if (const char *ut = u->GetText()) v->SetUnitsString(ut);
  }
  if (tinyxml2::XMLElement *d = stock->FirstChildElement("doc")) {
    if (const char *dt = d->GetText()) v->SetComment(dt);
  }

  return true;
}
```

**Function registration timing.** The Vensim parser registers functions in its constructor via `ReadyFunctions()` (`VensimParse.cpp:35`). The XMILE reader needs the same set registered before `ProcessStock` (and Phase 2's `xpyy_call`) can resolve `INTEG`, `IF THEN ELSE`, etc. There are two viable approaches:

1. **Add `XmileReader::ReadyFunctions()`** that calls the same private `ReadyFunctions` body as `VensimParse`. Refactor the body out of `VensimParse` into a shared static helper (e.g., `XmutilRegisterFunctions(SymbolNameSpace*)`) and have both parsers call it.

2. **Have the XMILE reader's caller (the extern-C entries `convert_xmile_to_xmile` / `convert_xmile_to_mdl`) construct a transient `VensimParse vp{&m}` to register functions, then destruct it before constructing `XmileReader`.**

Approach 1 is cleaner. Add a free function:

```cpp
// src/Vensim/VensimParse.h:
extern void RegisterXmutilFunctions(SymbolNameSpace *sns);
```

Move the body of `VensimParse::ReadyFunctions()` (`src/Vensim/VensimParse.cpp:~50-200` — all the `new FunctionXxx(sns)` calls) into this free function. Update `VensimParse::ReadyFunctions()` to delegate. Update Phase 1's `XmileReader` constructor to also call `RegisterXmutilFunctions(pSymbolNameSpace)` (provided the namespace doesn't already have INTEG — guard by a `Find("INTEG") == nullptr` check to avoid double-registration when a VensimParse already ran).

Apply this refactor **in Task 2** — the cleanest moment, when we first need it.

**Side benefit — Phase 2 test simplification.** Once `XmileReader`'s ctor calls `RegisterXmutilFunctions` directly (with a `Find("INTEG") == nullptr` guard against double-registration), the boilerplate in Phase 2 Task 5's tests (`Model m; { VensimParse vp(&m); } XmileReader reader(&m);`) reduces to `Model m; XmileReader reader(&m);`. After this task lands, update the `Fixture` struct in `test/xmile/EquationParseTest.cpp` accordingly — drop the transient-VensimParse dance. Include this simplification in this task's commit.

**Verification:**
- `ninja -C out/Debug xmutil_test` succeeds.
- Existing Vensim tests pass (the function-table refactor must not regress them).
- `out/Debug/XMUtil third_party/simlin/default_projects/logistic-growth/model.xmile` produces a non-empty `.xmile` output.

**Commit:** `feat(xmile): <stock> reader synthesizing INTEG from inflow/outflow names`
<!-- END_TASK_2 -->

<!-- END_SUBCOMPONENT_A -->

<!-- START_SUBCOMPONENT_B (tasks 3-3) -->

<!-- START_TASK_3 -->
### Task 3: StockFlowRoundTripTest — logistic-growth corpus + synthetic fixtures

**Verifies:** AC1.1, AC3.1, AC3.3 (stock+flow slice).

**Files:**
- Create: `test/xmile/StockFlowRoundTripTest.cpp`
- Modify: `XMUtil.gyp` (`xmutil_test` `sources`: add the new test file)

**Implementation notes:**

The headline test loads the corpus fixture from disk and round-trips it. Use the `XMUTIL_SRC_ROOT` define (already set by `XMUtil.gyp:151`) to find the file path — the same mechanism `test/mdl/CorpusRoundTripTest.cpp` uses.

```cpp
#include <fstream>
#include <sstream>
#include <string>
#include "../../src/Model.h"
#include "../../src/Symbol/Variable.h"
#include "../TestHarness.h"
#include "RoundTrip.h"

namespace {

std::string ReadFile(const std::string &path) {
  std::ifstream in(path, std::ios::in | std::ios::binary);
  if (!in.is_open()) return std::string();
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

// Hand-written fixtures that exercise specific stock shapes the corpus may not.
const char *kSingleInflowNoOutflow = R"(<?xml version="1.0" encoding="utf-8"?>
<xmile version="1.0" xmlns="http://docs.oasis-open.org/xmile/ns/XMILE/v1.0">
  <sim_specs method="euler"><start>0</start><stop>10</stop><dt>1</dt></sim_specs>
  <model><variables>
    <stock name="s"><eqn>100</eqn><inflow>in</inflow></stock>
    <flow name="in"><eqn>5</eqn></flow>
  </variables></model>
</xmile>
)";

const char *kMultipleInflowsAndOutflows = R"(<?xml version="1.0" encoding="utf-8"?>
<xmile version="1.0" xmlns="http://docs.oasis-open.org/xmile/ns/XMILE/v1.0">
  <sim_specs method="euler"><start>0</start><stop>10</stop><dt>1</dt></sim_specs>
  <model><variables>
    <stock name="s">
      <eqn>0</eqn>
      <inflow>in1</inflow>
      <inflow>in2</inflow>
      <outflow>out1</outflow>
    </stock>
    <flow name="in1"><eqn>1</eqn></flow>
    <flow name="in2"><eqn>2</eqn></flow>
    <flow name="out1"><eqn>3</eqn></flow>
  </variables></model>
</xmile>
)";

const char *kStockReferencedBeforeFlowDeclared = R"(<?xml version="1.0" encoding="utf-8"?>
<xmile version="1.0" xmlns="http://docs.oasis-open.org/xmile/ns/XMILE/v1.0">
  <sim_specs method="euler"><start>0</start><stop>10</stop><dt>1</dt></sim_specs>
  <model><variables>
    <stock name="s"><eqn>10</eqn><inflow>in</inflow></stock>
    <aux name="rate"><eqn>2</eqn></aux>
    <flow name="in"><eqn>rate</eqn></flow>
  </variables></model>
</xmile>
)";

}  // namespace

TEST(StockFlow_corpus_logistic_growth_round_trip) {
  std::string xmile = ReadFile(std::string(XMUTIL_SRC_ROOT) +
                               "/third_party/simlin/default_projects/logistic-growth/model.xmile");
  CHECK(!xmile.empty());
  if (xmile.empty()) return;
  std::vector<std::string> diffs = xmileroundtrip::RoundTripDiffs(xmile);
  for (const std::string &d : diffs) printf("  diff: %s\n", d.c_str());
  CHECK(diffs.empty());
}

TEST(StockFlow_corpus_logistic_growth_to_mdl) {
  std::string xmile = ReadFile(std::string(XMUTIL_SRC_ROOT) +
                               "/third_party/simlin/default_projects/logistic-growth/model.xmile");
  CHECK(!xmile.empty());
  if (xmile.empty()) return;
  std::vector<std::string> diffs = xmileroundtrip::XmileToMdlDiffs(xmile);
  for (const std::string &d : diffs) printf("  diff: %s\n", d.c_str());
  CHECK(diffs.empty());
}

TEST(StockFlow_single_inflow_no_outflow_round_trip) {
  std::vector<std::string> diffs = xmileroundtrip::RoundTripDiffs(kSingleInflowNoOutflow);
  for (const std::string &d : diffs) printf("  diff: %s\n", d.c_str());
  CHECK(diffs.empty());
}

TEST(StockFlow_multiple_in_and_out_round_trip) {
  std::vector<std::string> diffs = xmileroundtrip::RoundTripDiffs(kMultipleInflowsAndOutflows);
  for (const std::string &d : diffs) printf("  diff: %s\n", d.c_str());
  CHECK(diffs.empty());
}

TEST(StockFlow_forward_reference_to_flow_ok) {
  std::vector<std::string> diffs = xmileroundtrip::RoundTripDiffs(kStockReferencedBeforeFlowDeclared);
  for (const std::string &d : diffs) printf("  diff: %s\n", d.c_str());
  CHECK(diffs.empty());
}

TEST(StockFlow_stock_classified_as_STOCK_after_pipeline) {
  // Direct shape check: after parsing + pipeline, the stock variable has
  // XMILE_Type_STOCK and Inflows()/Outflows() are populated by MarkStockFlows.
  std::vector<std::string> errs;
  Model *m = xmileroundtrip::ParseXMILE(kMultipleInflowsAndOutflows, errs);
  CHECK(m != nullptr); CHECK(errs.empty());
  if (!m) return;
  Variable *stock = static_cast<Variable *>(m->GetNameSpace()->Find("s"));
  CHECK(stock != nullptr);
  if (stock) {
    CHECK(stock->VariableType() == XMILE_Type_STOCK);
    CHECK(stock->Inflows().size() == 2);
    CHECK(stock->Outflows().size() == 1);
  }
  delete m;
}
```

**Verification:**
- `ninja -C out/Debug xmutil_test` succeeds.
- All six new tests pass.
- Existing tests still pass.

**Commit:** `test(xmile): stock+flow round-trip (logistic-growth corpus)`
<!-- END_TASK_3 -->

<!-- END_SUBCOMPONENT_B -->

---

## Phase 4 done when

- `logistic-growth/model.xmile` round-trips XMILE → XMILE with no `ModelComparator` diffs.
- `logistic-growth/model.xmile` converts XMILE → MDL with an equivalent re-parsed `Model`.
- The six new tests in `StockFlowRoundTripTest.cpp` pass.
- All Phase 3 and earlier tests still pass.
- `XmileReader` (or a refactored shared helper) registers the same `Function*` set `VensimParse` does, so `INTEG`, `IF THEN ELSE`, etc. resolve from the equation grammar's `xpyy_call`.

## Out of scope for Phase 4

- Dimensions / arrays / subscripted stocks (Phase 5).
- Standalone graphical-function-as-stock (rare; Phase 6 if needed).
- Views / sketch (Phase 7) — the test verifies the structural model, not the diagram.
- Corpus `fishbanks` and `reliability` round-trips (Phase 7/8) — those require view geometry.
- Cloud handling via `_hasUpstream` / `_hasDownstream` (Phase 7 — sketch synthesis).
