# XMILE Reader Implementation Plan — Phase 3

**Goal:** Walk the XMILE DOM and populate scalar-equation variables (auxes), sim specs, unit definitions, and groups into the same `Model` shape `VensimParse` produces.

**Architecture:** `XmileReader::ProcessFile` is filled in from its Phase 1 stub. It walks `<xmile>` → `<header>`, `<sim_specs>`, `<model_units>`, `<model>/<variables>`, and (where present) `<views>/<view>/<group>` elements. Per-element handlers create `Variable`s via `InsertVariable`, parse `<eqn>` text via `XmileReader::ParseEquation` (Phase 2), attach `Equation`s, and store units / comments. Unknown-namespace elements (`isee:*`, `simlin:*`) and Stella UI widgets in the default XMILE namespace are silently skipped. The `<module>` element is hard-rejected at this phase.

**Tech Stack:** tinyxml2 DOM walk, the `Expression` + `Equation` + `Variable` + `UnitExpression` + `ModelGroup` APIs.

**Scope:** Phase 3 of 8.

**Codebase verified:** 2026-05-28

---

## Acceptance Criteria Coverage

This phase implements and tests:

### xmile-reader.AC1: Model populates from XMILE via a stable API
- **xmile-reader.AC1.1 Success** (partial, scalar slice): a Model containing only auxes runs through `MarkVariableTypes` / `AdjustGroupNames` / `CheckGhostOwners` without error.

### xmile-reader.AC3: Round-trip preserves the model
- **xmile-reader.AC3.1 Success** (scalar slice): for an auxes-only model, XMILE → Model → XMILE → Model' yields a `Model` equivalent to the first per `ModelComparator`.
- **xmile-reader.AC3.2 Success:** Dimensions, unit definitions, groups, and sim specs (start, stop, dt, save_step, integration method) are equivalent across the XMILE round-trip. *(Dimensions are Phase 5; this phase covers units, sim specs, and groups.)*

### xmile-reader.AC5: Out-of-scope features hard-error or are silently skipped
- **xmile-reader.AC5.1 Failure:** A `<module>` element inside `<variables>` is rejected with a clear error citing "modules are not supported" and the offending element.
- **xmile-reader.AC5.4 Edge:** Unknown-namespace elements (`isee:*`, `simlin:*`) and Stella UI widgets in the XMILE namespace (`<button>`, `<knob>`, `<slider>`, `<graph>`, `<numeric_input>`, ...) are silently skipped; the rest of the model loads.

---

## Codebase verification findings

- ✓ `Variable` is constructed via `new Variable(SymbolNameSpace *, const std::string &name)` and self-registers in the namespace via the `Symbol` base ctor (`src/Symbol/Symbol.cpp:11-17`). The XMILE reader uses the same `InsertVariable` lookup-or-create pattern as `VensimParse::InsertVariable` (`src/Vensim/VensimParse.cpp:441-452`):
  ```cpp
  Variable *XmileReader::InsertVariable(const std::string &name) {
    Variable *v = static_cast<Variable *>(pSymbolNameSpace->Find(name));
    if (!v) v = new Variable(pSymbolNameSpace, name);
    return v;
  }
  ```
- ✓ `Variable::AddEq(Equation *eq, bool init = false)` lazily creates `VariableContentVar` on first call (`src/Symbol/Variable.cpp:313-323`); default `init=false` is correct for aux equations.
- ✓ `Variable::SetGroup`, `SetComment`, `SetUnitsString` are direct setters (lines 217-226). `AddUnits(UnitExpression *)` is the structured path. For Phase 3 we use **`SetUnitsString`** (raw string) — it's simpler, and the `XMILEGenerator` already falls back to it when `UnitExpression` is absent (`XMILEGenerator.cpp:561-565`).
- ✓ `Equation(SymbolNameSpace *, LeftHandSide *, Expression *, int token)` — pass `'='` for the token.
- ✓ `LeftHandSide(SymbolNameSpace *, ExpressionVariable *, SymbolList *, SymbolListList *, int interpmode)` — pass `NULL, NULL, 0` for scalar auxes.
- ✓ Sim spec representation: control vars (`INITIAL TIME`, `FINAL TIME`, `TIME STEP`, `SAVEPER`) are ordinary `Variable`s in the namespace with constant-numeric equations. `Model::GetConstanValue(name, default)` reads the first equation's `EXPTYPE_Number` and falls back to the passed default (`Model.cpp:605-617`). **Both** the variable and the parallel `Model::set_initial_time` / `set_finall_time` (note: misspelled with double-l in the codebase) / `set_dt` setters must be called so both reader paths work — exactly mirroring how `VensimParse` ends up populating them.
- ✓ Integration type writer-side strings (`XMILEGenerator.cpp:126-131`): `"RK4"`, `"RK2"`, `"Euler"`. The reader matches case-insensitively because the corpus uses lowercase (`<sim_specs method="euler">` in `logistic-growth/model.xmile`).
- ✓ `Model::UnitEquivs()` (`Model.h:46-48`) holds comma-separated raw strings (e.g., `"Dollar,$,Dollars,$s"` = name, eqn, then aliases). The XMILE writer parses these in `generateModelUnits` (`XMILEGenerator.cpp:189-243`); the reader reconstructs the same shape.
- ✓ `ModelGroup` constructor: `ModelGroup(const std::string &name, ModelGroup *owner)` (and a 3-arg variant with depth). Public `vVariables` vector (`src/Symbol/Symbol.h:17-32`). Both sides of group membership must be set: `var->SetGroup(g)` AND `g->vVariables.push_back(var)` — mirroring `VensimParse::AddFullEq` lines 182-185.
- ✓ tinyxml2's `XMLElement::Name()` returns the qualified element name verbatim (e.g., `"isee:loop_indicator"`). The reader prefix-matches on `:` to skip foreign-namespace siblings.
- ✗ The design assumes a `reciprocal="true"` attribute on `<dt>`. The xmutil **writer** never emits it, so a self-round-trip never sees it. But for third-party XMILE (e.g., Stella files), the reader must still handle it — multiply 1/N at read time. The implementation honors this design intent.
- ✗ `set_finall_time` (double-l) is the actual setter name in `Model.h:97`. Use it verbatim.
- ✗ The design suggests `Model::Groups().push_back` directly. That field accessor returns a reference (`Model.h:70-72`), so `_model->Groups().push_back(g)` is valid. Mirrors the Vensim path.
- ✓ Corpus `logistic-growth/model.xmile` confirms the shape: `<xmile>` envelope with default XMILE namespace plus `xmlns:isee` and `xmlns:simlin` declarations; `<header><name>...</name></header>`; `<sim_specs method="euler"><start><stop><dt>`; `<model><variables>` with `<stock>`, `<flow>`, `<aux>`; `<views><view ...>` with isee-attributed elements that the reader skips.

---

## DOM walk plan

```
<xmile>
├── <header>                — read <name> only (informational; not stored)
├── <sim_specs method="...">
│   ├── <start>             — numeric body → INITIAL TIME
│   ├── <stop>              — numeric body → FINAL TIME
│   ├── <dt reciprocal="..."> — numeric body → TIME STEP (resolve reciprocal)
│   └── <save_step>         — numeric body → SAVEPER (default = TIME STEP)
├── <model_units>
│   └── <unit>+             — name, eqn, alias* → Model::UnitEquivs() comma-string
├── <model>
│   └── <variables>
│       ├── <aux>           — Phase 3 (this phase)
│       ├── <stock>         — Phase 4 (skipped in Phase 3 tests, deferred)
│       ├── <flow>          — Phase 4
│       ├── <dimensions>    — Phase 5
│       ├── <module>        — HARD ERROR (AC5.1)
│       └── isee:*, simlin:* — silently skipped (AC5.4)
└── <macro>                 — HARD ERROR (AC5.2) — Phase 8
```

Top-level `<group>` elements (if any) and view-level `<group>` are handled here as far as ModelGroup creation + member assignment goes; Phase 7 layers view geometry on top of the same ModelGroup vector.

For Phase 3, the smoke tests use **auxes-only** XMILE fixtures (no `<stock>` / `<flow>`). The reader's `<stock>` / `<flow>` handlers in Phase 4 will be no-ops or stubs at the end of Phase 3 (defer their bodies); a Phase 3 fixture with no stocks/flows is round-trip-equivalent.

---

<!-- START_SUBCOMPONENT_A (tasks 1-2) -->

<!-- START_TASK_1 -->
### Task 1: DOM walker skeleton — XmileReader::ProcessFile body + namespace-skip helpers

**Verifies:** AC5.4 (silently skip foreign-namespace elements), partial AC5.1 (hard-reject `<module>`).

**Files:**
- Modify: `src/Xmile/XmileReader.h` (add helpers + private state for the walker)
- Modify: `src/Xmile/XmileReader.cpp` (replace Phase 1 stub `ProcessFile` body)

**Implementation notes:**

Replace the Phase 1 stub body (which only validated the `<xmile>` root) with a structured walker. Add private helpers:

```cpp
// XmileReader.h:
public:
  // Static helpers exposed for use by sibling classes (XmileView, in Phase 7).

  // Returns true if the qualified element name has a namespace prefix (contains
  // ':'). XMILE default-namespace elements have no prefix; isee:, simlin:, and
  // similar vendor-namespaced elements do.
  static bool IsForeignNamespace(const char *qualifiedName);

  // Returns true if the unprefixed element name is a known Stella UI widget
  // that should be silently dropped at read time (button, knob, slider, graph,
  // gauge, numeric_input, numeric_display, spatial_map, animation_object,
  // loop_indicator). UI annotations with no semantic content.
  static bool IsStellaUIWidget(const char *unprefixedName);

  // Collapse internal whitespace runs (including newlines from XMILE
  // line-wrapped labels) to a single space; trim leading/trailing whitespace.
  // Used by Phase 3 (aux/flow/group), Phase 4 (stock + flow names), and
  // Phase 7 (view element name resolution).
  static std::string NormalizeName(const char *raw);

private:
  bool ProcessHeader(tinyxml2::XMLElement *header, std::vector<std::string> &errs);
  bool ProcessSimSpecs(tinyxml2::XMLElement *simSpecs, std::vector<std::string> &errs);
  bool ProcessModelUnits(tinyxml2::XMLElement *units, std::vector<std::string> &errs);
  bool ProcessModel(tinyxml2::XMLElement *model, std::vector<std::string> &errs);
  bool ProcessAux(tinyxml2::XMLElement *aux, std::vector<std::string> &errs);
  // Phase 4 stubs (return true / no-op for now):
  bool ProcessStock(tinyxml2::XMLElement *stock, std::vector<std::string> &errs) { return true; }
  bool ProcessFlow(tinyxml2::XMLElement *flow, std::vector<std::string> &errs) { return true; }
```

`IsForeignNamespace` is `strchr(name, ':') != nullptr`. (XMILE 1.0 conformance: namespace declarations are accepted on the root and on descendants, but only the qualified name matters for filtering — the reader does not need full XML namespace resolution.)

`IsStellaUIWidget` is a small static array lookup over the documented UI widget names plus any encountered during corpus testing.

`ProcessFile` body shape:

```cpp
bool XmileReader::ProcessFile(const std::string &filename, const char *contents, size_t len,
                              std::vector<std::string> &errs) {
  tinyxml2::XMLDocument doc;
  if (doc.Parse(contents, len) != tinyxml2::XML_SUCCESS) {
    errs.push_back(filename + ": XML parse error: " + doc.ErrorStr());
    return false;
  }
  tinyxml2::XMLElement *root = doc.RootElement();
  if (!root || std::string(root->Name()) != "xmile") {
    errs.push_back(filename + ": root element is not <xmile>");
    return false;
  }
  // Hard-reject <macro> sibling of <model> at the envelope level (AC5.2).
  if (root->FirstChildElement("macro")) {
    errs.push_back(filename + ": <macro> elements are not supported");
    return false;
  }
  bool ok = true;
  for (tinyxml2::XMLElement *child = root->FirstChildElement(); child; child = child->NextSiblingElement()) {
    const char *name = child->Name();
    if (IsForeignNamespace(name)) continue;  // isee:prefs, simlin:* — silently skip
    std::string tag(name);
    if (tag == "header") ok &= ProcessHeader(child, errs);
    else if (tag == "sim_specs") ok &= ProcessSimSpecs(child, errs);
    else if (tag == "model_units") ok &= ProcessModelUnits(child, errs);
    else if (tag == "model") ok &= ProcessModel(child, errs);
    else if (tag == "style") continue;  // styling: dropped silently
    else if (IsStellaUIWidget(name)) continue;
    else {
      // Unknown default-namespace element. Log but don't fail — keeps the door
      // open for future XMILE 2.0 envelope additions.
      // (Could be tightened to a hard error if the corpus demands it.)
      continue;
    }
    if (!ok) break;
  }
  return ok;
}
```

`ProcessModel` walks `<variables>` and dispatches per element kind:

```cpp
bool XmileReader::ProcessModel(tinyxml2::XMLElement *model, std::vector<std::string> &errs) {
  // Detect multiple <model> elements at the envelope level. The current root
  // walker passes us one <model>; Phase 8 adds the multi-<model> rejection
  // (AC5.3) at the envelope walker. Here we just process the one we got.
  tinyxml2::XMLElement *variables = model->FirstChildElement("variables");
  if (!variables) return true;  // empty model is fine
  for (tinyxml2::XMLElement *child = variables->FirstChildElement(); child; child = child->NextSiblingElement()) {
    const char *name = child->Name();
    if (IsForeignNamespace(name)) continue;
    std::string tag(name);
    if (tag == "module") {
      const char *modname = child->Attribute("name");
      errs.push_back(std::string("<module") + (modname ? " name=\"" : "") +
                     (modname ? modname : "") + (modname ? "\"" : "") + ">: modules are not supported");
      return false;
    }
    if (tag == "aux") {
      if (!ProcessAux(child, errs)) return false;
    } else if (tag == "stock") {
      if (!ProcessStock(child, errs)) return false;
    } else if (tag == "flow") {
      if (!ProcessFlow(child, errs)) return false;
    } else if (tag == "dimensions") {
      // Phase 5: dimensions handler. Silently skip in Phase 3.
      continue;
    } else if (IsStellaUIWidget(name)) {
      continue;
    } else {
      // Unknown element inside <variables>: log as warning, keep going.
      continue;
    }
  }
  // <views> handling and <group> are nested under <model>. View-level <group>
  // is read by Phase 7. Here we only walk the variables.
  return true;
}
```

`ProcessHeader` is a one-liner that ignores everything except optionally `<name>` (no current Model field stores it — drop silently).

**Verification:**
- `./configure.sh && ninja -C out/Debug xmutil_test` succeeds.
- Existing tests still pass.
- A new smoke test (added in Task 5) confirms a fixture with `<isee:prefs>` inside `<xmile>` parses without error.

**Commit:** `feat(xmile): DOM walker skeleton with foreign-namespace skip`
<!-- END_TASK_1 -->

<!-- START_TASK_2 -->
### Task 2: Sim specs — INITIAL TIME / FINAL TIME / TIME STEP / SAVEPER + integration method + dt reciprocal

**Verifies:** AC3.2 (sim specs equivalent across round-trip).

**Files:**
- Modify: `src/Xmile/XmileReader.cpp` (`ProcessSimSpecs` body)
- Modify: `src/Xmile/XmileReader.h` (add `SetControlVariable` private helper)

**Implementation notes:**

```cpp
// Helper that creates a control Variable, attaches a constant-numeric Equation,
// and registers the value via the matching Model setter. Mirrors the shape
// VensimParse produces when parsing a Vensim .Control section.
void XmileReader::SetControlVariable(const std::string &name, double value) {
  Variable *v = InsertVariable(name);
  ExpressionNumber *rhs = new ExpressionNumber(pSymbolNameSpace, value);
  ExpressionVariable *lhsVar = new ExpressionVariable(pSymbolNameSpace, v, NULL);
  LeftHandSide *lhs = new LeftHandSide(pSymbolNameSpace, lhsVar, NULL, NULL, 0);
  Equation *eq = new Equation(pSymbolNameSpace, lhs, rhs, '=');
  v->AddEq(eq);
}

bool XmileReader::ProcessSimSpecs(tinyxml2::XMLElement *simSpecs, std::vector<std::string> &errs) {
  // method attribute -> Integration_Type. Case-insensitive compare against
  // "euler", "rk2", "rk4". Default = Euler if attribute is missing.
  const char *method = simSpecs->Attribute("method");
  if (method) {
    std::string m(method);
    std::transform(m.begin(), m.end(), m.begin(), ::tolower);
    if (m == "rk4") _model->SetIntegrationType(Integration_Type_RK4);
    else if (m == "rk2") _model->SetIntegrationType(Integration_Type_RK2);
    else _model->SetIntegrationType(Integration_Type_EULER);
  } else {
    _model->SetIntegrationType(Integration_Type_EULER);
  }

  // Numeric child elements. Each holds a single double in its text body.
  double startVal = 0.0;
  double stopVal = 100.0;
  double dtVal = 1.0;
  double saveStepVal = -1.0;  // sentinel: copy dt if not present

  if (auto *e = simSpecs->FirstChildElement("start")) startVal = e->DoubleText(0.0);
  if (auto *e = simSpecs->FirstChildElement("stop")) stopVal = e->DoubleText(100.0);
  if (auto *e = simSpecs->FirstChildElement("dt")) {
    dtVal = e->DoubleText(1.0);
    // XMILE's reciprocal attribute means dt = 1/N for body N. Resolve at read
    // time; the writer always emits the resolved double so round-trip is
    // information-preserving. For third-party XMILE files (e.g., Stella) the
    // attribute may be present and the resolution is what makes the reader
    // interoperable.
    const char *recip = e->Attribute("reciprocal");
    if (recip && (std::string(recip) == "true" || std::string(recip) == "1")) {
      if (dtVal != 0.0) dtVal = 1.0 / dtVal;
    }
  }
  if (auto *e = simSpecs->FirstChildElement("save_step")) saveStepVal = e->DoubleText(dtVal);
  else saveStepVal = dtVal;

  // Variables in the namespace + Model setters. Both paths must agree so the
  // writer's GetConstanValue fallback works.
  SetControlVariable("INITIAL TIME", startVal);
  SetControlVariable("FINAL TIME", stopVal);
  SetControlVariable("TIME STEP", dtVal);
  SetControlVariable("SAVEPER", saveStepVal);
  _model->set_initial_time(startVal);
  _model->set_finall_time(stopVal);  // note: misspelled in Model.h (double-l)
  _model->set_dt(dtVal);

  return true;
}
```

Note on `time_units`: XMILE's `<sim_specs time_units="...">` attribute maps to the units string on `TIME STEP` (per the writer's emission at `XMILEGenerator.cpp:159-172`, which reads units from `GetUnits("TIME STEP")` with fallback). For Phase 3 we set `time_units` on the `TIME STEP` Variable via `SetUnitsString(timeUnits)` after creating it. Add to `ProcessSimSpecs`:

```cpp
if (const char *tu = simSpecs->Attribute("time_units")) {
  Variable *ts = FindVariable("TIME STEP");
  if (ts) ts->SetUnitsString(tu);
}
```

**Verification:**
- `ninja -C out/Debug xmutil_test` succeeds.
- A new unit test (added in Task 5) confirms parse of a `<sim_specs>` element yields `INITIAL TIME = 0`, `FINAL TIME = 100`, `TIME STEP = 1`, `SAVEPER = 1`, and `IntegrationType() == Integration_Type_EULER`.

**Commit:** `feat(xmile): sim specs reader (control vars + integration method)`
<!-- END_TASK_2 -->

<!-- END_SUBCOMPONENT_A -->

<!-- START_SUBCOMPONENT_B (tasks 3-4) -->

<!-- START_TASK_3 -->
### Task 3: Aux variables — `<aux>` → Variable + Equation + units + comment

**Verifies:** AC1.1 (scalar slice), AC3.1 (scalar slice).

**Files:**
- Modify: `src/Xmile/XmileReader.cpp` (`ProcessAux` body)

**Implementation notes:**

```cpp
bool XmileReader::ProcessAux(tinyxml2::XMLElement *aux, std::vector<std::string> &errs) {
  const char *name = aux->Attribute("name");
  if (!name) {
    errs.push_back("<aux> with no name attribute");
    return false;
  }
  // XMILE element names can use newlines for line-wrapped labels (e.g., the
  // logistic-growth corpus has names like "fractional \ngrowth rate" with a
  // literal newline). Normalize to a single-space separator so the resulting
  // identifier is a valid Vensim name. This matches what MDLGenerator emits.
  std::string normName = NormalizeName(name);
  Variable *v = InsertVariable(normName);

  // <eqn> body — required for an aux. Parse via XmileReader::ParseEquation
  // (Phase 2). For a <dimensions>-bearing aux, Phase 5 takes a different path;
  // here we handle the scalar case only.
  tinyxml2::XMLElement *eqnEl = aux->FirstChildElement("eqn");
  if (!eqnEl) {
    // Standalone graphical-function form: handled by Phase 6. In Phase 3 we
    // require an <eqn> for auxes — log a warning and skip.
    errs.push_back(std::string("<aux name=\"") + name + "\"> has no <eqn> (graphical-function form deferred to Phase 6)");
    return true;  // not fatal in Phase 3
  }
  const char *eqnText = eqnEl->GetText();
  if (!eqnText) eqnText = "";
  std::vector<std::string> eqnErrs;
  Expression *rhs = ParseEquation(eqnText, eqnErrs);
  if (!rhs) {
    for (const std::string &e : eqnErrs) {
      errs.push_back(std::string("<aux name=\"") + name + "\">: " + e);
    }
    return false;
  }

  // Build LeftHandSide + Equation + attach to Variable. Mirrors the shape
  // VensimParse::AddEq + AddFullEq produces for a scalar aux.
  ExpressionVariable *lhsVar = new ExpressionVariable(pSymbolNameSpace, v, NULL);
  LeftHandSide *lhs = new LeftHandSide(pSymbolNameSpace, lhsVar, NULL, NULL, 0);
  Equation *eq = new Equation(pSymbolNameSpace, lhs, rhs, '=');
  v->AddEq(eq);

  // <units> child (optional). Phase 3 stores the raw string; the writer's
  // fallback at XMILEGenerator.cpp:561-565 reads SetUnitsString when no
  // structured UnitExpression is attached.
  if (tinyxml2::XMLElement *u = aux->FirstChildElement("units")) {
    if (const char *ut = u->GetText()) v->SetUnitsString(ut);
  }

  // <doc> child (optional) -> SetComment.
  if (tinyxml2::XMLElement *d = aux->FirstChildElement("doc")) {
    if (const char *dt = d->GetText()) v->SetComment(dt);
  }

  // Group assignment: if a Variable's parent <group> is active in the walker
  // context (Phase 7's view walk seeds this), assign here. Phase 3 implements
  // a top-level no-op; Phase 7 layers group context on top.

  return true;
}
```

Implement the `XmileReader::NormalizeName` static method declared in Task 1. XMILE allows newlines in name attributes for visual line wrapping (used by the simlin/Stella corpus). Vensim names are single-line, so the helper collapses internal whitespace runs to a single space and trims leading/trailing whitespace. Defined as a public static member on `XmileReader` so Phase 7's `XmileView` can call it from a separate translation unit.

**Verification:**
- `ninja -C out/Debug xmutil_test` succeeds.
- New smoke + round-trip tests (Task 5) confirm a hand-written XMILE with a few auxes round-trips through the comparator with no diffs.

**Commit:** `feat(xmile): <aux> reader (Variable + Equation + units + doc)`
<!-- END_TASK_3 -->

<!-- START_TASK_4 -->
### Task 4: Model units + groups

**Verifies:** AC3.2 (units + groups equivalent across round-trip), AC5.4 (foreign-namespace skip).

**Files:**
- Modify: `src/Xmile/XmileReader.cpp` (`ProcessModelUnits` + a new `ProcessGroup` helper)

**Implementation notes:**

`<model_units>` walks each `<unit>` child, builds a comma-separated string matching the format `Model::UnitEquivs()` already holds, and pushes it:

```cpp
bool XmileReader::ProcessModelUnits(tinyxml2::XMLElement *units,
                                    std::vector<std::string> &errs) {
  for (tinyxml2::XMLElement *u = units->FirstChildElement("unit"); u;
       u = u->NextSiblingElement("unit")) {
    const char *uname = u->Attribute("name");
    if (!uname) continue;
    std::string composed = uname;  // first field: canonical name
    if (tinyxml2::XMLElement *eqnEl = u->FirstChildElement("eqn")) {
      if (const char *txt = eqnEl->GetText()) {
        composed += ",";
        composed += txt;
      }
    }
    // Aliases (<alias>X</alias> children, zero or more).
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
```

`<group>` handling. The XMILE writer's group emission lives in two places:
- `generateSectorViews` (`XMILEGenerator.cpp:805-828`) emits `<view><group name="..."><var>X</var><var>Y</var>...</group></view>` for the no-views case with multiple groups.
- `generateModelAsGroups` (`XMILEGenerator.cpp:686+`) emits multiple `<model name="...">` sections — but **that requires `<module>` elements** which we hard-reject. So a round-trip XMILE → XMILE for a multi-group model that the writer chooses to emit via `generateModelAsGroups` is not supported in v1.

For Phase 3, we handle the `<view>/<group>` form (read by Phase 7's view walker, which calls a `ProcessGroup` helper for each `<group>` element). The helper signature lives on `XmileReader` so Phase 7 can call it:

```cpp
// XmileReader.h additions:
ModelGroup *ProcessGroup(tinyxml2::XMLElement *groupEl, std::vector<std::string> &errs);

// XmileReader.cpp:
ModelGroup *XmileReader::ProcessGroup(tinyxml2::XMLElement *groupEl,
                                      std::vector<std::string> &errs) {
  const char *name = groupEl->Attribute("name");
  if (!name) {
    errs.push_back("<group> with no name attribute");
    return nullptr;
  }
  std::string normName = NormalizeName(name);
  // Reuse existing group if one with the same name already exists; otherwise
  // create. Owner is looked up by name (XMILE owner attribute).
  ModelGroup *owner = nullptr;
  if (const char *ownerName = groupEl->Attribute("owner")) {
    std::string n = NormalizeName(ownerName);
    for (ModelGroup *g : _model->Groups()) {
      if (g->sName == n) { owner = g; break; }
    }
  }
  ModelGroup *group = nullptr;
  for (ModelGroup *g : _model->Groups()) {
    if (g->sName == normName) { group = g; break; }
  }
  if (!group) {
    group = new ModelGroup(normName, owner);
    _model->Groups().push_back(group);
  }
  // Assign member variables.
  for (tinyxml2::XMLElement *vEl = groupEl->FirstChildElement("var"); vEl;
       vEl = vEl->NextSiblingElement("var")) {
    if (const char *vname = vEl->GetText()) {
      std::string n = NormalizeName(vname);
      Variable *v = FindVariable(n);
      if (!v) {
        // Group member that hasn't been declared — create as a placeholder so
        // round-trip preserves the name; MarkVariableTypes will classify it.
        v = InsertVariable(n);
      }
      v->SetGroup(group);
      group->vVariables.push_back(v);
    }
  }
  return group;
}
```

Phase 3 does not invoke `ProcessGroup` at the top level (no `<views>` walk yet — that's Phase 7). The helper is implemented now so Phase 7 just calls it. Phase 3's tests cover a single-group fixture by hand-constructing a `<view><group><var></var></group></view>` and asserting the model's `Groups()` vector contents — see Task 5.

Wait — Phase 3 implements the helper but doesn't walk views yet. To allow Phase 3 tests to exercise the group machinery, we add a *minimal* `<views>` walker here that only recognizes `<group>` and ignores everything else. Phase 7 replaces the body with full view geometry handling:

```cpp
// Phase 3 stub for <views>; Phase 7 expands.
bool XmileReader::ProcessViews(tinyxml2::XMLElement *views,
                               std::vector<std::string> &errs) {
  for (tinyxml2::XMLElement *view = views->FirstChildElement("view"); view;
       view = view->NextSiblingElement("view")) {
    for (tinyxml2::XMLElement *child = view->FirstChildElement(); child;
         child = child->NextSiblingElement()) {
      if (IsForeignNamespace(child->Name())) continue;
      if (std::string(child->Name()) == "group") {
        if (!ProcessGroup(child, errs)) return false;
      }
      // Other view children deferred to Phase 7.
    }
  }
  return true;
}
```

And hook it into `ProcessModel` after the `<variables>` walk:

```cpp
if (tinyxml2::XMLElement *views = model->FirstChildElement("views")) {
  if (!ProcessViews(views, errs)) return false;
}
```

**Verification:**
- `ninja -C out/Debug xmutil_test` succeeds.
- New tests (Task 5) confirm `<model_units>` parsing yields `UnitEquivs` entries matching the writer's output for the same input, and a single `<group>` populates `Model::Groups()` with the expected variable assignments.

**Commit:** `feat(xmile): model units + group reader`
<!-- END_TASK_4 -->

<!-- END_SUBCOMPONENT_B -->

<!-- START_SUBCOMPONENT_C (tasks 5-5) -->

<!-- START_TASK_5 -->
### Task 5: AuxRoundTripTest — auxes-only round-trip + foreign-namespace skip + module rejection

**Verifies:** AC1.1 (scalar slice), AC3.1 (scalar slice), AC3.2 (sim specs + units + groups), AC5.1 (`<module>` rejection), AC5.4 (foreign-namespace skip).

**Files:**
- Create: `test/xmile/AuxRoundTripTest.cpp`
- Modify: `XMUtil.gyp` (`xmutil_test` `sources`: add the new test file)

**Implementation notes:**

The test fixtures are hand-written, multi-line C++ raw string literals so the inline XMILE is readable. Each test covers one scenario:

```cpp
#include <string>
#include "../../src/Model.h"
#include "../TestHarness.h"
#include "RoundTrip.h"

namespace {

const char *kAuxesOnly = R"(<?xml version="1.0" encoding="utf-8"?>
<xmile version="1.0" xmlns="http://docs.oasis-open.org/xmile/ns/XMILE/v1.0">
  <header><name>aux-only</name></header>
  <sim_specs method="euler">
    <start>0</start>
    <stop>100</stop>
    <dt>1</dt>
  </sim_specs>
  <model>
    <variables>
      <aux name="a"><eqn>2</eqn></aux>
      <aux name="b"><eqn>3</eqn></aux>
      <aux name="c"><eqn>a * b</eqn><units>widgets</units><doc>a comment</doc></aux>
    </variables>
  </model>
</xmile>
)";

const char *kWithModule = R"(<?xml version="1.0" encoding="utf-8"?>
<xmile version="1.0" xmlns="http://docs.oasis-open.org/xmile/ns/XMILE/v1.0">
  <header><name>with-module</name></header>
  <sim_specs method="euler"><start>0</start><stop>10</stop><dt>1</dt></sim_specs>
  <model>
    <variables>
      <aux name="x"><eqn>1</eqn></aux>
      <module name="m1"/>
    </variables>
  </model>
</xmile>
)";

const char *kForeignNamespace = R"(<?xml version="1.0" encoding="utf-8"?>
<xmile version="1.0" xmlns="http://docs.oasis-open.org/xmile/ns/XMILE/v1.0"
       xmlns:isee="http://iseesystems.com/XMILE">
  <header><name>with-isee</name></header>
  <isee:prefs show_module_prefix="false"/>
  <sim_specs method="euler"><start>0</start><stop>10</stop><dt>1</dt></sim_specs>
  <model>
    <variables>
      <aux name="x"><eqn>1</eqn></aux>
      <isee:loop_indicator/>
    </variables>
  </model>
</xmile>
)";

const char *kWithUnits = R"(<?xml version="1.0" encoding="utf-8"?>
<xmile version="1.0" xmlns="http://docs.oasis-open.org/xmile/ns/XMILE/v1.0">
  <header><name>with-units</name></header>
  <sim_specs method="euler"><start>0</start><stop>10</stop><dt>1</dt></sim_specs>
  <model_units>
    <unit name="Dollar"><eqn>$</eqn><alias>Dollars</alias><alias>$s</alias></unit>
    <unit name="Widget"><eqn>w</eqn></unit>
  </model_units>
  <model><variables><aux name="x"><eqn>1</eqn></aux></variables></model>
</xmile>
)";

const char *kSimSpecsReciprocalDt = R"(<?xml version="1.0" encoding="utf-8"?>
<xmile version="1.0" xmlns="http://docs.oasis-open.org/xmile/ns/XMILE/v1.0">
  <header><name>recip-dt</name></header>
  <sim_specs method="rk4">
    <start>0</start>
    <stop>10</stop>
    <dt reciprocal="true">4</dt>
  </sim_specs>
  <model><variables><aux name="x"><eqn>1</eqn></aux></variables></model>
</xmile>
)";

const char *kWithGroup = R"(<?xml version="1.0" encoding="utf-8"?>
<xmile version="1.0" xmlns="http://docs.oasis-open.org/xmile/ns/XMILE/v1.0">
  <header><name>with-group</name></header>
  <sim_specs method="euler"><start>0</start><stop>10</stop><dt>1</dt></sim_specs>
  <model>
    <variables>
      <aux name="x"><eqn>1</eqn></aux>
      <aux name="y"><eqn>x + 1</eqn></aux>
    </variables>
    <views>
      <view view_type="stock_flow">
        <group name="MyGroup"><var>x</var><var>y</var></group>
      </view>
    </views>
  </model>
</xmile>
)";

}  // namespace

TEST(AuxRoundTrip_auxes_only_round_trip) {
  std::vector<std::string> diffs = xmileroundtrip::RoundTripDiffs(kAuxesOnly);
  for (const std::string &d : diffs) printf("  diff: %s\n", d.c_str());
  CHECK(diffs.empty());
}

TEST(AuxRoundTrip_module_is_rejected) {
  std::vector<std::string> errs;
  Model *m = xmileroundtrip::ParseXMILE(kWithModule, errs);
  CHECK(m == nullptr);
  CHECK(!errs.empty());
  // Error message contains the offending element's tag.
  bool sawModuleError = false;
  for (const std::string &e : errs) {
    if (e.find("modules are not supported") != std::string::npos) sawModuleError = true;
  }
  CHECK(sawModuleError);
}

TEST(AuxRoundTrip_foreign_namespace_silently_skipped) {
  std::vector<std::string> errs;
  Model *m = xmileroundtrip::ParseXMILE(kForeignNamespace, errs);
  CHECK(m != nullptr);
  CHECK(errs.empty());
  // The aux survives.
  CHECK(m->GetNameSpace()->Find("x") != nullptr);
  delete m;
}

TEST(AuxRoundTrip_units_round_trip) {
  std::vector<std::string> diffs = xmileroundtrip::RoundTripDiffs(kWithUnits);
  for (const std::string &d : diffs) printf("  diff: %s\n", d.c_str());
  CHECK(diffs.empty());
}

TEST(AuxRoundTrip_dt_reciprocal_resolves) {
  std::vector<std::string> errs;
  Model *m = xmileroundtrip::ParseXMILE(kSimSpecsReciprocalDt, errs);
  CHECK(m != nullptr);
  CHECK(errs.empty());
  // dt = 1/4 = 0.25 after reciprocal resolution.
  CHECK(m->dt() == 0.25);
  CHECK(m->IntegrationType() == Integration_Type_RK4);
  delete m;
}

TEST(AuxRoundTrip_group_round_trip) {
  std::vector<std::string> diffs = xmileroundtrip::RoundTripDiffs(kWithGroup);
  for (const std::string &d : diffs) printf("  diff: %s\n", d.c_str());
  CHECK(diffs.empty());
}

TEST(AuxRoundTrip_xmile_to_mdl) {
  // Convert to MDL and verify the resulting Model is equivalent.
  std::vector<std::string> diffs = xmileroundtrip::XmileToMdlDiffs(kAuxesOnly);
  for (const std::string &d : diffs) printf("  diff: %s\n", d.c_str());
  CHECK(diffs.empty());
}
```

Note: `RoundTripDiffs` and `XmileToMdlDiffs` are the helpers introduced by Phase 1's `test/xmile/RoundTrip.{h,cpp}`. The Phase 1 versions are placeholders (the round-trip is vacuously true on empty Models); Phase 3 is the first phase where they actually find diffs and report them. If diffs surface bugs in this phase, fix at the corresponding `Process*` method.

The `kWithGroup` round-trip depends on the writer's group emission path. Looking at `generateSectorViews` (`XMILEGenerator.cpp:805-828`), the writer emits view-level `<group>` only when `views.empty() && !groups.empty()`. With Phase 3 not yet populating views (Phase 7 does), the writer takes this branch — so round-trip works.

**Verification:**
- `ninja -C out/Debug xmutil_test` succeeds.
- All seven new tests pass.
- Existing tests still pass.

**Commit:** `test(xmile): aux round-trip + sim specs + units + groups + rejection`
<!-- END_TASK_5 -->

<!-- END_SUBCOMPONENT_C -->

---

## Phase 3 done when

- A scalar-only XMILE model (e.g., `kAuxesOnly`) round-trips XMILE → XMILE through the comparator with no diffs.
- A scalar-only XMILE model converts XMILE → MDL with an equivalent re-parsed Model.
- `<module>` produces a clean error message naming the element.
- `<isee:*>` and `<simlin:*>` elements are silently dropped, model parses fine.
- `<dt reciprocal="true">N` resolves to `1/N`.
- All seven new tests in `AuxRoundTripTest.cpp` pass.
- Existing tests remain green.

## Out of scope for Phase 3

- Stocks and flows (Phase 4) — `ProcessStock`/`ProcessFlow` are stubs.
- Dimensions / arrays / subscripted variables (Phase 5) — `<dimensions>` is silently skipped.
- Graphical functions (Phase 6) — `<gf>` is silently skipped; standalone `<aux>` with no `<eqn>` is treated as a soft warning.
- View geometry (Phase 7) — only `<group>` is harvested from the `<views>` subtree; everything else is ignored.
- Corpus-level round-trip on `fishbanks` / `reliability` — Phase 8.
- Multi-`<model>` rejection (AC5.3) — Phase 8 (envelope-level check).
- `<macro>` rejection at the envelope level — Phase 3 does the simplest check (`if (root->FirstChildElement("macro")) return false`); the comprehensive AC5.2 test is in Phase 8.
