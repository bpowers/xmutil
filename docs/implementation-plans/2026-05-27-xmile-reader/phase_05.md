# XMILE Reader Implementation Plan — Phase 5

**Goal:** XMILE dimension definitions and subscripted variables populate xmutil's `XMILE_Type_ARRAY` Variables and apply-to-all / per-element equation shapes the existing pipeline already handles.

**Architecture:** A new `ProcessDimensions` method walks `<dimensions>/<dim>/<elem>` inside `<model>` (top-level) and creates a `Variable` per dimension with an `ExpressionSymbolList` RHS. Subscripted `<aux>` / `<flow>` / `<stock>` get extended logic: a top-level `<eqn>` + `<dimensions>` sibling becomes an apply-to-all equation (single `Equation` with a LHS subscript list). `<element subscript="...">` children become per-element equations (one `Equation` per `<element>`, each with its own LHS subscript). The post-parse `MarkVariableTypes` flips the dim's type to `XMILE_Type_ARRAY` and its elements to `XMILE_Type_ARRAY_ELM` — the reader does not set those types directly.

**Tech Stack:** Phase 2's equation parser (handles subscript syntax in RHS expressions), Phase 3's DOM walker, Phase 4's stock handler (extended for subscripted stocks). `SymbolList` / `ExpressionSymbolList` API in `src/Symbol/`.

**Scope:** Phase 5 of 8.

**Codebase verified:** 2026-05-28

---

## Acceptance Criteria Coverage

This phase implements and tests:

### xmile-reader.AC2: XMILE equations parse to xmutil Expression trees
- **xmile-reader.AC2.5 Success:** Subscripts (`x[a, b]`, `x[*]`, `x[*:Dim]`, `x[l:r]`, `x[@1]`) parse to `ExpressionVariable` with the corresponding subscript representation. *(Phase 2 implemented the grammar; Phase 5 verifies subscripted equations end-to-end now that dimensions exist.)*

### xmile-reader.AC3: Round-trip preserves the model
- **xmile-reader.AC3.1 Success** (array slice): for an arrays-bearing model, XMILE → Model → XMILE → Model' yields a `Model` equivalent to the first per `ModelComparator`.
- **xmile-reader.AC3.2 Success** (dimensions slice): dimension definitions and subscripted variables are equivalent across the XMILE round-trip.

---

## Codebase verification findings

- ✓ Vensim's `Dim: a, b, c` produces an `Equation` with `iEqType = ':'`, LHS = `ExpressionVariable(Dim, NULL)`, RHS = `ExpressionSymbolList(sns, SymList[a,b,c], NULL)` (`src/Vensim/VYacc.y:117`). The XMILE reader produces the same shape from `<dim name="Dim"><elem name="a"/><elem name="b"/><elem name="c"/></dim>`.
- ✓ **`XMILE_Type_ARRAY` is set by `MarkTypes`, not the parser** (`src/Symbol/Variable.cpp:134-141`). When the first equation's RHS is `EXPTYPE_Symlist`, `MarkTypes`:
  1. Sets `mVariableType = XMILE_Type_ARRAY` on the dim Variable.
  2. Calls `symlist->SetOwner(this)` — this walks the symlist and sets `Owner()` on each element Variable.
  3. Flips each element's `mVariableType` from `XMILE_Type_UNKNOWN` to `XMILE_Type_ARRAY_ELM` (lines 143-149).
  
  The reader just builds the `ExpressionSymbolList`; the pipeline does the typing.
- ✓ `ExpressionSymbolList(SymbolNameSpace *, SymbolList *subs, SymbolList *map)` — the third arg is the subscript-map (used by Vensim's `->` syntax). XMILE's writer discards maps; the reader has no map to provide → always pass `NULL`.
- ✓ `SymbolList::Append(Symbol *, bool bang)` — used to build element lists. The `bang` flag (`!`) is for "bang index" semantics (the dimension as a single bound). For element lists in dimension definitions, `bang = false`.
- ✓ `XMILEGenerator::generateDimensions` (`src/Xmile/XMILEGenerator.cpp:245-280`) emits `<dim name="X"><elem name="a"/>...<elem name="c"/></dim>` for each `XMILE_Type_ARRAY` Variable, expanding any nested sub-dimension references via `Equation::GetSubscriptElements`. **The writer never emits `size="N"`** — that's an input-only form. The reader expands `<dim name="X" size="N">` to synthesized elements `"1"`, `"2"`, ..., `"N"`.
- ✓ Subscripted variable equations:
  - **Apply-to-all** (e.g., `inflow[Dim] = 5`): single `Equation` on the Variable, LHS has a `SymbolList` containing the dim Variable. Writer emits a single `<eqn>` + `<dimensions>` sibling.
  - **Per-element** (e.g., `inflow[a] = 1` / `inflow[b] = 2`): two `Equation`s on the same Variable, each with its own LHS subscript referencing an element Variable. Writer emits two `<element subscript="a">` / `<element subscript="b">` children + `<dimensions>` sibling.
- ✓ `<element subscript="a, b">` uses **comma-space-separated** element names for multi-dimensional subscripts (`XMILEGenerator.cpp:414-418`). The reader splits on `", "` (with trimming for robustness against single-comma writers).
- ✓ `<dimensions>` child element on a variable is emitted **after** any `<element>` children (`XMILEGenerator.cpp:519-555`). The reader processes them in any order.
- ✓ None of the three corpus models (`fishbanks`, `logistic-growth`, `reliability`) use dimensions. All Phase 5 tests use hand-written fixtures.
- ✓ Forward references work: dimension `Dim` can be defined before or after a subscripted variable references it. `InsertVariable` is create-if-absent (Phase 1 helper). `MarkTypes` runs post-parse, after every Variable has its equations.
- ✓ Stocks can also be subscripted: a `<stock>` with `<dimensions>` is the same shape as a subscripted aux, but the equation is an `INTEG(...)` synthesized per the Phase 4 recipe. The net-flow + init expressions inherit the subscript from the LHS — the reader builds the LHS SymbolList from `<dimensions>` (apply-to-all) or `<element subscript="...">` (per-element) and threads it through to `LeftHandSide`.
- ✓ The XMILE equation grammar (Phase 2) already supports subscripted RHS variables (`x[a, b]`, `x[*]`, `x[*:Dim]`, `x[l:r]`, `x[@1]`). Phase 5 only needs to handle LHS subscripts at the DOM walker level.

---

## Dimension-handling design

```
<dimensions>                    — top-level (under <model>), separate from per-var <dimensions>
  <dim name="Dim">
    <elem name="a"/>
    <elem name="b"/>
    <elem name="c"/>
  </dim>
  <dim name="OtherDim" size="3"/>   — indexed; reader expands to elements "1","2","3"
</dimensions>
```

Mapping to xmutil:
- Each `<dim>` → `Variable(sns, "Dim")` with one `Equation` (`iEqType = ':'`):
  - LHS: `ExpressionVariable(Dim, NULL)` wrapped in `LeftHandSide(sns, lhsVar, NULL, NULL, 0)`.
  - RHS: `ExpressionSymbolList(sns, SymList, NULL)` where SymList contains the element Variables.
- Each `<elem name="a"/>` → `Variable(sns, "a")`. Created via `InsertVariable("a")` so multiple dims sharing element names are handled correctly. The element Variable's owner backlink is set later by `MarkTypes`.
- `<dim name="X" size="N"/>` → `Variable(sns, "X")` with synthesized element names `"1"` ... `"N"`, each created via `InsertVariable`.

For subscripted variable equations:
- Apply-to-all `<aux name="inflow"><eqn>5</eqn><dimensions><dim name="Dim"/></dimensions></aux>`:
  - Build a `SymbolList` containing the `Dim` Variable (one entry per `<dim>` child).
  - LHS: `ExpressionVariable(inflow, dimSymList)` → `LeftHandSide(sns, lhsVar, NULL, NULL, 0)`.
  - RHS: parse `<eqn>` via `ParseEquation`.
  - Single `Equation`, `iEqType = '='`.
- Per-element `<aux name="inflow"><element subscript="a"><eqn>1</eqn></element><element subscript="b"><eqn>2</eqn></element><dimensions><dim name="Dim"/></dimensions></aux>`:
  - For each `<element>`: split `subscript` attribute on `", "` (or `,` with whitespace trim), look up each element name via `InsertVariable`, build a `SymbolList`. LHS = `ExpressionVariable(inflow, elemSymList)`. RHS = parsed `<eqn>` body. One `Equation` per element.
  - The variable's `<dimensions>` child is informational for the writer's round-trip; the reader doesn't strictly need it for the per-element case (the LHS subscripts already tell the writer what to do). But Phase 5 parses it to validate consistency: every per-element subscript must be an element of one of the declared dims. Inconsistencies are logged as errors but parsing continues.

---

<!-- START_SUBCOMPONENT_A (tasks 1-2) -->

<!-- START_TASK_1 -->
### Task 1: ProcessDimensions — top-level `<dimensions>/<dim>/<elem>` handler

**Verifies:** AC3.2 (dimensions equivalent across round-trip).

**Files:**
- Modify: `src/Xmile/XmileReader.h` (declare `ProcessDimensions` and a `BuildElementList` helper)
- Modify: `src/Xmile/XmileReader.cpp` (implement)

**Implementation notes:**

`ProcessModel` (Phase 3 Task 1) already iterates `<variables>` children. Add a parallel walk for the top-level `<dimensions>` element, which is a *sibling* of `<variables>` under `<model>`:

```cpp
// In ProcessModel, after the <variables> walk and before the <views> walk:
if (tinyxml2::XMLElement *dims = model->FirstChildElement("dimensions")) {
  if (!ProcessDimensions(dims, errs)) return false;
}
```

`ProcessDimensions` body:

```cpp
bool XmileReader::ProcessDimensions(tinyxml2::XMLElement *dimsEl,
                                    std::vector<std::string> &errs) {
  for (tinyxml2::XMLElement *dim = dimsEl->FirstChildElement("dim"); dim;
       dim = dim->NextSiblingElement("dim")) {
    const char *dimName = dim->Attribute("name");
    if (!dimName) {
      errs.push_back("<dim> with no name attribute");
      return false;
    }
    std::string normName = NormalizeName(dimName);
    Variable *dimVar = InsertVariable(normName);

    // Build the SymbolList of element Variables.
    SymbolList *symList = nullptr;
    bool hasIndexed = (dim->Attribute("size") != nullptr);
    if (hasIndexed) {
      int n = dim->IntAttribute("size", 0);
      if (n <= 0) {
        errs.push_back(std::string("<dim name=\"") + dimName +
                       "\"> has invalid size attribute");
        return false;
      }
      symList = BuildIndexedElementList(n);
    } else {
      symList = BuildElementList(dim, errs);
      if (!symList) return false;  // already pushed error
    }

    // Build ExpressionSymbolList RHS + LeftHandSide LHS + Equation.
    ExpressionSymbolList *rhs =
        new ExpressionSymbolList(pSymbolNameSpace, symList, /*map=*/NULL);
    ExpressionVariable *lhsVar =
        new ExpressionVariable(pSymbolNameSpace, dimVar, NULL);
    LeftHandSide *lhs =
        new LeftHandSide(pSymbolNameSpace, lhsVar, NULL, NULL, 0);
    Equation *eq = new Equation(pSymbolNameSpace, lhs, rhs, ':');
    dimVar->AddEq(eq);
  }
  return true;
}

// Build a SymbolList from <elem name="..."/> children. Each name -> Variable
// (lookup-or-create). Returns nullptr (and pushes an error) on malformed input.
SymbolList *XmileReader::BuildElementList(tinyxml2::XMLElement *dimEl,
                                          std::vector<std::string> &errs) {
  SymbolList *list = nullptr;
  for (tinyxml2::XMLElement *elem = dimEl->FirstChildElement("elem"); elem;
       elem = elem->NextSiblingElement("elem")) {
    const char *elemName = elem->Attribute("name");
    if (!elemName) {
      errs.push_back("<elem> with no name attribute");
      delete list;
      return nullptr;
    }
    std::string normName = NormalizeName(elemName);
    Variable *elemVar = InsertVariable(normName);
    if (!list) {
      list = new SymbolList(pSymbolNameSpace, elemVar, /*bang=*/false);
    } else {
      list->Append(elemVar, /*bang=*/false);
    }
  }
  if (!list) {
    errs.push_back("<dim> with no <elem> children and no size attribute");
  }
  return list;
}

// For <dim name="X" size="N"/>, synthesize element Variables named "1".."N".
// These are valid XMILE element names (numeric strings are permitted).
SymbolList *XmileReader::BuildIndexedElementList(int n) {
  SymbolList *list = nullptr;
  for (int i = 1; i <= n; ++i) {
    std::string name = std::to_string(i);
    Variable *elemVar = InsertVariable(name);
    if (!list) {
      list = new SymbolList(pSymbolNameSpace, elemVar, false);
    } else {
      list->Append(elemVar, false);
    }
  }
  return list;
}
```

Confirm `SymbolList`'s constructors and `Append` method signatures from `src/Symbol/SymbolList.h` during implementation. The codebase investigator's report indicates an `Append(Symbol *, bool bang)` method exists; if the constructor signature differs (e.g., empty constructor + `Append` everywhere), adjust accordingly. Match the shape `VensimParse::SymList` uses internally.

**Verification:**
- `ninja -C out/Debug xmutil_test` succeeds.
- Existing tests still pass.

**Commit:** `feat(xmile): <dimensions>/<dim>/<elem> reader`
<!-- END_TASK_1 -->

<!-- START_TASK_2 -->
### Task 2: Subscripted variable equations — apply-to-all and per-element

**Verifies:** AC2.5 (subscripted variables), AC3.1, AC3.2 (array slice).

**Files:**
- Modify: `src/Xmile/XmileReader.cpp` (extend `ProcessAux`, `ProcessFlow`, `ProcessStock` to handle subscripts; add a `BuildLHSSubscripts` helper)
- Modify: `src/Xmile/XmileReader.h` (declare the helper)

**Implementation notes:**

Introduce a helper that converts a comma-space-separated subscript attribute (e.g., `"a, b"`) into a `SymbolList` for an LHS:

```cpp
// Parse a subscript attribute value into a SymbolList. Element names are
// looked up via InsertVariable (lookup-or-create). The "," / ", " separator
// matches what XMILEGenerator emits (XMILEGenerator.cpp:408-413).
SymbolList *XmileReader::ParseSubscriptList(const std::string &subscriptAttr);

// Build a SymbolList from a per-var <dimensions><dim name="X"/>...</dimensions>
// child — used for apply-to-all equations.
SymbolList *XmileReader::BuildAppliesToAllSubs(tinyxml2::XMLElement *dimsEl);
```

`ParseSubscriptList` splits on `,`, trims whitespace, and `InsertVariable`s each name. Skip empty fragments to handle trailing/extra commas defensively.

`BuildAppliesToAllSubs` walks `<dim name="X"/>` children and `InsertVariable`s each dim name.

Now extend `ProcessAux` to support subscripted forms. Replace the Phase 3 scalar-only body:

```cpp
bool XmileReader::ProcessAux(tinyxml2::XMLElement *aux,
                             std::vector<std::string> &errs) {
  const char *name = aux->Attribute("name");
  if (!name) {
    errs.push_back("<aux> with no name attribute");
    return false;
  }
  std::string normName = NormalizeName(name);
  Variable *v = InsertVariable(normName);

  // Determine subscripted-ness from the <dimensions> child.
  tinyxml2::XMLElement *dimsChild = aux->FirstChildElement("dimensions");
  // Check for per-element children. The presence of any <element subscript="..."/>
  // signals per-element form regardless of the top-level <eqn>.
  tinyxml2::XMLElement *firstElement = aux->FirstChildElement("element");

  if (firstElement) {
    // Per-element form. The <dimensions> sibling validates consistency but is
    // not strictly required for equation construction (the LHS subscripts on
    // each element carry the structure).
    return ProcessPerElementEquations(aux, v, errs);
  }

  // Apply-to-all form (with or without <dimensions>) — the path Phase 3 already
  // handled for the no-dimensions case.
  return ProcessAppliesToAllEquation(aux, v, dimsChild, errs);
}

// Helper: apply-to-all (scalar = no dimsChild; arrayed = dimsChild present)
bool XmileReader::ProcessAppliesToAllEquation(
    tinyxml2::XMLElement *varEl, Variable *v,
    tinyxml2::XMLElement *dimsChild, std::vector<std::string> &errs) {
  tinyxml2::XMLElement *eqnEl = varEl->FirstChildElement("eqn");
  if (!eqnEl) {
    // Phase 6 covers the standalone-gf-on-aux form. Phase 5: log + skip.
    errs.push_back(std::string("<aux/flow name=\"") +
                   (varEl->Attribute("name") ? varEl->Attribute("name") : "(?)") +
                   "\"> has no <eqn> (graphical-function form deferred to Phase 6)");
    return true;
  }
  const char *eqnText = eqnEl->GetText() ? eqnEl->GetText() : "";
  std::vector<std::string> eqnErrs;
  Expression *rhs = ParseEquation(eqnText, eqnErrs);
  if (!rhs) {
    for (const std::string &e : eqnErrs) errs.push_back(e);
    return false;
  }

  SymbolList *lhsSubs = nullptr;
  if (dimsChild) {
    lhsSubs = BuildAppliesToAllSubs(dimsChild);
  }
  ExpressionVariable *lhsVar = new ExpressionVariable(pSymbolNameSpace, v, lhsSubs);
  LeftHandSide *lhs = new LeftHandSide(pSymbolNameSpace, lhsVar, NULL, NULL, 0);
  Equation *eq = new Equation(pSymbolNameSpace, lhs, rhs, '=');
  v->AddEq(eq);

  if (tinyxml2::XMLElement *u = varEl->FirstChildElement("units")) {
    if (const char *ut = u->GetText()) v->SetUnitsString(ut);
  }
  if (tinyxml2::XMLElement *d = varEl->FirstChildElement("doc")) {
    if (const char *dt = d->GetText()) v->SetComment(dt);
  }
  return true;
}

// Helper: per-element — one Equation per <element> child.
bool XmileReader::ProcessPerElementEquations(tinyxml2::XMLElement *varEl,
                                             Variable *v,
                                             std::vector<std::string> &errs) {
  for (tinyxml2::XMLElement *elemEl = varEl->FirstChildElement("element"); elemEl;
       elemEl = elemEl->NextSiblingElement("element")) {
    const char *subs = elemEl->Attribute("subscript");
    if (!subs) {
      errs.push_back("<element> with no subscript attribute");
      return false;
    }
    SymbolList *lhsSubs = ParseSubscriptList(subs);
    if (!lhsSubs) return false;

    tinyxml2::XMLElement *eqnEl = elemEl->FirstChildElement("eqn");
    if (!eqnEl) {
      errs.push_back(std::string("<element subscript=\"") + subs +
                     "\"> has no <eqn>");
      return false;
    }
    const char *eqnText = eqnEl->GetText() ? eqnEl->GetText() : "";
    std::vector<std::string> eqnErrs;
    Expression *rhs = ParseEquation(eqnText, eqnErrs);
    if (!rhs) {
      for (const std::string &e : eqnErrs) errs.push_back(e);
      return false;
    }
    ExpressionVariable *lhsVar = new ExpressionVariable(pSymbolNameSpace, v, lhsSubs);
    LeftHandSide *lhs = new LeftHandSide(pSymbolNameSpace, lhsVar, NULL, NULL, 0);
    Equation *eq = new Equation(pSymbolNameSpace, lhs, rhs, '=');
    v->AddEq(eq);
  }
  // Variable-level <units> and <doc> apply to all elements.
  if (tinyxml2::XMLElement *u = varEl->FirstChildElement("units")) {
    if (const char *ut = u->GetText()) v->SetUnitsString(ut);
  }
  if (tinyxml2::XMLElement *d = varEl->FirstChildElement("doc")) {
    if (const char *dt = d->GetText()) v->SetComment(dt);
  }
  return true;
}
```

`ProcessFlow` gets the same restructure — flows can also be subscripted, exactly like auxes. Refactor `ProcessFlow` to call the shared `ProcessAppliesToAllEquation` / `ProcessPerElementEquations` helpers as well.

`ProcessStock` needs more involved handling: each per-element stock equation needs its own synthesized INTEG with subscripted inflow/outflow references. For the apply-to-all subscripted stock case, the LHS subscript propagates; the inflows/outflows reference the same dimension. Extend `ProcessStock`:

```cpp
bool XmileReader::ProcessStock(tinyxml2::XMLElement *stock,
                               std::vector<std::string> &errs) {
  const char *name = stock->Attribute("name");
  if (!name) { errs.push_back("<stock> with no name attribute"); return false; }
  std::string normName = NormalizeName(name);
  Variable *v = InsertVariable(normName);

  tinyxml2::XMLElement *dimsChild = stock->FirstChildElement("dimensions");
  tinyxml2::XMLElement *firstElement = stock->FirstChildElement("element");

  // Collect inflow/outflow names (apply to all per-element equations).
  std::vector<std::string> inflowNames, outflowNames;
  for (auto *e = stock->FirstChildElement("inflow"); e; e = e->NextSiblingElement("inflow"))
    if (const char *t = e->GetText()) inflowNames.push_back(NormalizeName(t));
  for (auto *e = stock->FirstChildElement("outflow"); e; e = e->NextSiblingElement("outflow"))
    if (const char *t = e->GetText()) outflowNames.push_back(NormalizeName(t));

  if (firstElement) {
    // Per-element subscripted stock: one INTEG-bearing equation per <element>.
    // The net-flow expression for element [a] references inflow[a] - outflow[a].
    for (auto *elemEl = stock->FirstChildElement("element"); elemEl;
         elemEl = elemEl->NextSiblingElement("element")) {
      const char *subs = elemEl->Attribute("subscript");
      if (!subs) { errs.push_back("<element> with no subscript"); return false; }
      SymbolList *elementSubList = ParseSubscriptList(subs);
      if (!elementSubList) return false;

      auto *eqnEl = elemEl->FirstChildElement("eqn");
      if (!eqnEl) { errs.push_back("<element> on stock has no <eqn>"); return false; }
      std::vector<std::string> eqnErrs;
      Expression *init = ParseEquation(eqnEl->GetText() ? eqnEl->GetText() : "",
                                       eqnErrs);
      if (!init) {
        for (const std::string &e : eqnErrs) errs.push_back(e);
        return false;
      }
      Expression *netFlow = BuildNetFlowSubscripted(inflowNames, outflowNames,
                                                    elementSubList);
      Expression *integExpr = BuildIntegExpression(netFlow, init);
      ExpressionVariable *lhsVar =
          new ExpressionVariable(pSymbolNameSpace, v, elementSubList);
      LeftHandSide *lhs =
          new LeftHandSide(pSymbolNameSpace, lhsVar, NULL, NULL, 0);
      Equation *eq = new Equation(pSymbolNameSpace, lhs, integExpr, '=');
      v->AddEq(eq);
    }
  } else {
    // Apply-to-all subscripted stock (or scalar — Phase 4 case).
    auto *eqnEl = stock->FirstChildElement("eqn");
    if (!eqnEl) { errs.push_back("<stock> has no <eqn>"); return false; }
    std::vector<std::string> eqnErrs;
    Expression *init = ParseEquation(eqnEl->GetText() ? eqnEl->GetText() : "",
                                     eqnErrs);
    if (!init) {
      for (const std::string &e : eqnErrs) errs.push_back(e);
      return false;
    }
    SymbolList *lhsSubs = dimsChild ? BuildAppliesToAllSubs(dimsChild) : nullptr;
    Expression *netFlow = BuildNetFlowSubscripted(inflowNames, outflowNames,
                                                  lhsSubs);
    Expression *integExpr = BuildIntegExpression(netFlow, init);
    ExpressionVariable *lhsVar =
        new ExpressionVariable(pSymbolNameSpace, v, lhsSubs);
    LeftHandSide *lhs =
        new LeftHandSide(pSymbolNameSpace, lhsVar, NULL, NULL, 0);
    Equation *eq = new Equation(pSymbolNameSpace, lhs, integExpr, '=');
    v->AddEq(eq);
  }

  if (auto *u = stock->FirstChildElement("units"))
    if (const char *ut = u->GetText()) v->SetUnitsString(ut);
  if (auto *d = stock->FirstChildElement("doc"))
    if (const char *dt = d->GetText()) v->SetComment(dt);
  return true;
}

// Build INTEG arg 0 with subscripted flow references. When lhsSubs is non-null
// the references become inflow[lhsSubs] - outflow[lhsSubs]; when null,
// inflow - outflow (Phase 4 shape).
Expression *XmileReader::BuildNetFlowSubscripted(
    const std::vector<std::string> &inflowNames,
    const std::vector<std::string> &outflowNames,
    SymbolList *lhsSubs) {
  Expression *acc = nullptr;
  for (const std::string &n : inflowNames) {
    Variable *fv = InsertVariable(n);
    // CRITICAL: deep-copy lhsSubs per reference — multiple references on the
    // same equation must not share the SymbolList pointer (deletion order in
    // SymbolNameSpace would otherwise double-free). VensimParse::SymList builds
    // a fresh list per call site.
    SymbolList *subsCopy = lhsSubs ? lhsSubs->Clone() : nullptr;
    Expression *ev = new ExpressionVariable(pSymbolNameSpace, fv, subsCopy);
    acc = acc ? static_cast<Expression *>(new ExpressionAdd(pSymbolNameSpace, acc, ev)) : ev;
  }
  if (!acc) acc = new ExpressionNumber(pSymbolNameSpace, 0.0);
  for (const std::string &n : outflowNames) {
    Variable *fv = InsertVariable(n);
    SymbolList *subsCopy = lhsSubs ? lhsSubs->Clone() : nullptr;
    Expression *ev = new ExpressionVariable(pSymbolNameSpace, fv, subsCopy);
    acc = new ExpressionSubtract(pSymbolNameSpace, acc, ev);
  }
  return acc;
}

// Wrap the net-flow + init into a fully-formed INTEG ExpressionFunctionMemory.
// Identical to Phase 4 Task 2; extracted as a helper here so both paths reuse it.
Expression *XmileReader::BuildIntegExpression(Expression *netFlow, Expression *init) {
  Function *integ = static_cast<Function *>(pSymbolNameSpace->Find("INTEG"));
  // ExpressionList ctor takes SymbolNameSpace* (no default ctor exists).
  ExpressionList *args = new ExpressionList(pSymbolNameSpace);
  args->Append(netFlow);
  args->Append(init);
  return new ExpressionFunctionMemory(pSymbolNameSpace, integ, args);
}
```

**Required subtask: add `SymbolList::Clone()`.** `SymbolList` does **not** currently have a deep-copy method (verified by codebase investigation). The Vensim path dodges this by constructing fresh `SymbolList` instances per call site; the XMILE reader cannot, because the same LHS subscript list must be referenced by every inflow/outflow `ExpressionVariable` in a subscripted stock's net-flow expression. Without a clone, the `SymbolNameSpace` destructor's `Symbol` cleanup pass will double-delete shared instances.

Add to `src/Symbol/SymbolList.h`:

```cpp
// Deep copy: new SymbolList in the same namespace whose entries are
// independent of this list. EntryType_SYMBOL and EntryType_BANG_SYMBOL entries
// share the underlying Variable* (those are owned by the namespace, not this
// list). EntryType_LIST entries are recursively cloned. Used by the XMILE
// reader to attach a distinct subscript-list instance to each flow reference
// in a synthesized INTEG arg-0 expression.
SymbolList *Clone() const;
```

Implementation in `SymbolList.cpp`:
- Allocate `new SymbolList(pSymbolNameSpace, /*first=*/nullptr, false)` (or whatever ctor variant takes the namespace).
- Iterate `vSymbols` and `Append` each entry by value, recursively `Clone()`-ing for `EntryType_LIST` entries.
- Copy any other state (e.g., a map flag if present).
- Confirm Clone is a const method (no side effects on the original).

This subtask runs **as part of Phase 5 Task 2** before `BuildNetFlowSubscripted` calls `Clone()`. The implementor adds the SymbolList method first, then writes the helper that consumes it.

**Verification:**
- `ninja -C out/Debug xmutil_test` succeeds.
- Existing tests still pass.

**Commit:** `feat(xmile): subscripted aux/flow/stock equations`
<!-- END_TASK_2 -->

<!-- END_SUBCOMPONENT_A -->

<!-- START_SUBCOMPONENT_B (tasks 3-3) -->

<!-- START_TASK_3 -->
### Task 3: ArrayRoundTripTest — dimensions + apply-to-all + per-element

**Verifies:** AC2.5, AC3.1, AC3.2 (array slice).

**Files:**
- Create: `test/xmile/ArrayRoundTripTest.cpp`
- Modify: `XMUtil.gyp` (`xmutil_test` `sources`: add the new file)

**Implementation notes:**

```cpp
#include <string>
#include "../../src/Model.h"
#include "../../src/Symbol/Variable.h"
#include "../TestHarness.h"
#include "RoundTrip.h"

namespace {

const char *kApplyToAllAux = R"(<?xml version="1.0" encoding="utf-8"?>
<xmile version="1.0" xmlns="http://docs.oasis-open.org/xmile/ns/XMILE/v1.0">
  <sim_specs method="euler"><start>0</start><stop>10</stop><dt>1</dt></sim_specs>
  <model>
    <dimensions><dim name="Dim"><elem name="a"/><elem name="b"/><elem name="c"/></dim></dimensions>
    <variables>
      <aux name="inflow">
        <eqn>5</eqn>
        <dimensions><dim name="Dim"/></dimensions>
      </aux>
    </variables>
  </model>
</xmile>
)";

const char *kPerElementAux = R"(<?xml version="1.0" encoding="utf-8"?>
<xmile version="1.0" xmlns="http://docs.oasis-open.org/xmile/ns/XMILE/v1.0">
  <sim_specs method="euler"><start>0</start><stop>10</stop><dt>1</dt></sim_specs>
  <model>
    <dimensions><dim name="Dim"><elem name="a"/><elem name="b"/></dim></dimensions>
    <variables>
      <aux name="inflow">
        <element subscript="a"><eqn>1</eqn></element>
        <element subscript="b"><eqn>2</eqn></element>
        <dimensions><dim name="Dim"/></dimensions>
      </aux>
    </variables>
  </model>
</xmile>
)";

const char *kApplyToAllStock = R"(<?xml version="1.0" encoding="utf-8"?>
<xmile version="1.0" xmlns="http://docs.oasis-open.org/xmile/ns/XMILE/v1.0">
  <sim_specs method="euler"><start>0</start><stop>10</stop><dt>1</dt></sim_specs>
  <model>
    <dimensions><dim name="Dim"><elem name="a"/><elem name="b"/><elem name="c"/></dim></dimensions>
    <variables>
      <stock name="s">
        <eqn>10</eqn>
        <dimensions><dim name="Dim"/></dimensions>
        <inflow>in</inflow>
        <outflow>out</outflow>
      </stock>
      <flow name="in"><eqn>5</eqn><dimensions><dim name="Dim"/></dimensions></flow>
      <flow name="out"><eqn>2</eqn><dimensions><dim name="Dim"/></dimensions></flow>
    </variables>
  </model>
</xmile>
)";

const char *kPerElementStock = R"(<?xml version="1.0" encoding="utf-8"?>
<xmile version="1.0" xmlns="http://docs.oasis-open.org/xmile/ns/XMILE/v1.0">
  <sim_specs method="euler"><start>0</start><stop>10</stop><dt>1</dt></sim_specs>
  <model>
    <dimensions><dim name="Dim"><elem name="a"/><elem name="b"/></dim></dimensions>
    <variables>
      <stock name="s">
        <element subscript="a"><eqn>10</eqn></element>
        <element subscript="b"><eqn>20</eqn></element>
        <inflow>in</inflow>
        <dimensions><dim name="Dim"/></dimensions>
      </stock>
      <flow name="in"><eqn>5</eqn><dimensions><dim name="Dim"/></dimensions></flow>
    </variables>
  </model>
</xmile>
)";

const char *kIndexedDim = R"(<?xml version="1.0" encoding="utf-8"?>
<xmile version="1.0" xmlns="http://docs.oasis-open.org/xmile/ns/XMILE/v1.0">
  <sim_specs method="euler"><start>0</start><stop>10</stop><dt>1</dt></sim_specs>
  <model>
    <dimensions><dim name="Dim" size="3"/></dimensions>
    <variables>
      <aux name="inflow"><eqn>5</eqn><dimensions><dim name="Dim"/></dimensions></aux>
    </variables>
  </model>
</xmile>
)";

}  // namespace

TEST(Array_apply_to_all_aux_round_trip) {
  std::vector<std::string> diffs = xmileroundtrip::RoundTripDiffs(kApplyToAllAux);
  for (const std::string &d : diffs) printf("  diff: %s\n", d.c_str());
  CHECK(diffs.empty());
}

TEST(Array_per_element_aux_round_trip) {
  std::vector<std::string> diffs = xmileroundtrip::RoundTripDiffs(kPerElementAux);
  for (const std::string &d : diffs) printf("  diff: %s\n", d.c_str());
  CHECK(diffs.empty());
}

TEST(Array_apply_to_all_stock_round_trip) {
  std::vector<std::string> diffs = xmileroundtrip::RoundTripDiffs(kApplyToAllStock);
  for (const std::string &d : diffs) printf("  diff: %s\n", d.c_str());
  CHECK(diffs.empty());
}

TEST(Array_per_element_stock_round_trip) {
  std::vector<std::string> diffs = xmileroundtrip::RoundTripDiffs(kPerElementStock);
  for (const std::string &d : diffs) printf("  diff: %s\n", d.c_str());
  CHECK(diffs.empty());
}

TEST(Array_dim_classified_as_ARRAY_after_pipeline) {
  std::vector<std::string> errs;
  Model *m = xmileroundtrip::ParseXMILE(kApplyToAllAux, errs);
  CHECK(m != nullptr); CHECK(errs.empty());
  if (!m) return;
  Variable *dim = static_cast<Variable *>(m->GetNameSpace()->Find("Dim"));
  CHECK(dim != nullptr);
  if (dim) CHECK(dim->VariableType() == XMILE_Type_ARRAY);
  Variable *a = static_cast<Variable *>(m->GetNameSpace()->Find("a"));
  CHECK(a != nullptr);
  if (a) CHECK(a->VariableType() == XMILE_Type_ARRAY_ELM);
  delete m;
}

TEST(Array_indexed_dim_expands_to_numeric_elements) {
  std::vector<std::string> errs;
  Model *m = xmileroundtrip::ParseXMILE(kIndexedDim, errs);
  CHECK(m != nullptr); CHECK(errs.empty());
  if (!m) return;
  // Synthesized elements "1", "2", "3" should exist in the namespace.
  CHECK(m->GetNameSpace()->Find("1") != nullptr);
  CHECK(m->GetNameSpace()->Find("2") != nullptr);
  CHECK(m->GetNameSpace()->Find("3") != nullptr);
  delete m;
}

TEST(Array_apply_to_all_to_mdl) {
  std::vector<std::string> diffs = xmileroundtrip::XmileToMdlDiffs(kApplyToAllAux);
  for (const std::string &d : diffs) printf("  diff: %s\n", d.c_str());
  CHECK(diffs.empty());
}
```

**Verification:**
- `ninja -C out/Debug xmutil_test` succeeds.
- All seven new tests pass.
- All Phase 1-4 tests still pass.

**Commit:** `test(xmile): array round-trip (apply-to-all + per-element)`
<!-- END_TASK_3 -->

<!-- END_SUBCOMPONENT_B -->

---

## Phase 5 done when

- Apply-to-all subscripted auxes / flows / stocks round-trip XMILE → XMILE with no diffs.
- Per-element subscripted auxes / flows / stocks round-trip XMILE → XMILE.
- `<dim size="N"/>` indexed dimensions expand correctly to synthetic numeric elements.
- A subscripted aux round-trips XMILE → MDL with an equivalent re-parsed `Model`.
- `MarkTypes` post-pass correctly assigns `XMILE_Type_ARRAY` to dim Variables and `XMILE_Type_ARRAY_ELM` to elements.
- All Phase 1-4 tests still pass.

## Out of scope for Phase 5

- Subscript-mapping syntax (`<-` / `->`) — XMILE writer discards maps; reader has nothing to read.
- EXCEPT clauses on arrays (mirrors current MDL writer scope, per design "Out of scope (v1)").
- `<gf>` on subscripted variables (Phase 6 — but the apply-to-all subscripted aux's `<gf>` would behave the same way).
- View / sketch for arrayed variables (Phase 7 — the view walker only sees the variable name, dimension structure is irrelevant to geometry).
- Corpus testing on arrayed models (no corpus model has arrays; the hand-written fixtures here are the bar).
