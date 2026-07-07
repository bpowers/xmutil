# XMILE Reader Implementation Plan — Phase 6

**Goal:** XMILE `<gf>` elements (inline on a variable, or standalone) populate `ExpressionLookup` / `ExpressionTable` consistent with how the existing writers consume them.

**Architecture:** A new `ProcessGf` helper parses an `<gf>` element into an `ExpressionTable*`. The aux / flow handler (Phase 3 / 5) checks for a `<gf>` child after parsing the `<eqn>`:

- **Inline `<gf>` + non-empty `<eqn>`** → WITH LOOKUP form: replace the parsed `<eqn>` expression `E` with `new ExpressionLookup(sns, E, table)`. The equation token stays `'='`.
- **Inline `<gf>` + no `<eqn>` (or empty `<eqn>`)** → Standalone graphical-function variable: the Equation's RHS is the `ExpressionTable*` directly, with equation token `'('` (matching `VensimParse::AddTable` for the standalone case).
- **`<gf>` on a `<stock>`** → silently skipped (the writer asserts this shape never appears, and no valid model produces it).

**Tech Stack:** Phases 1-5, plus the `ExpressionTable` and `ExpressionLookup` APIs.

**Scope:** Phase 6 of 8.

**Codebase verified:** 2026-05-28

---

## Acceptance Criteria Coverage

This phase implements and tests:

### xmile-reader.AC3: Round-trip preserves the model
- **xmile-reader.AC3.1 Success** (lookup slice): for a model with `<gf>` graphical functions (both inline-with-eqn and standalone), XMILE → Model → XMILE → Model' yields an equivalent `Model` per `ModelComparator`.

---

## Codebase verification findings

- ✓ `ExpressionTable(SymbolNameSpace *sns)` (`src/Symbol/Expression.h:422-487`). Methods: `AddPair(double x, double y)` push to lockstep vectors; `Extrapolate()` / `SetExtrapolate(bool)` for the `type="extrapolate"` XMILE attribute. No sorting / dedup enforced.
- ✓ `ExpressionLookup` has two constructor shapes (`src/Symbol/Expression.h:364-420`):
  - Named: `ExpressionLookup(sns, ExpressionVariable *var, Expression *input)` — for `table_name(input)`. Phase 2's `xpyy_call` already produces this when the XMILE equation references a named lookup variable; Phase 6 does not introduce it.
  - WITH LOOKUP / inline: `ExpressionLookup(sns, Expression *input, ExpressionTable *table)`. Phase 6 uses this for inline `<gf>` with a non-empty `<eqn>`.
- ✓ Standalone graphical-function variable: Vensim's `VensimParse::AddTable` (line 168) stores the `ExpressionTable*` **directly** as the Equation's expression with token `'('` (not `'='`). The XMILE reader matches: `new Equation(sns, lhs, /*Expression*=*/table, '(')`.
- ✓ XMILE writer `<gf>` emission (`src/Xmile/XMILEGenerator.cpp:444-501`):
  - Always emits explicit `<xpts>` and `<ypts>`, never `<xscale>`.
  - Emits `<yscale min max>` derived from y data range.
  - Asserts the variable type is `XMILE_Type_AUX` or `XMILE_Type_FLOW` (line 447) — `<gf>` on a stock is unreachable.
  - `type="extrapolate"` attribute on `<gf>` is emitted iff `Extrapolate()` is true.
- ✓ Corpus reality: `fishbanks/model.xmile` has 2 `<gf>` elements, both WITH LOOKUP shape with `<xscale>` + `<ypts>` (no `<xpts>`). The reader **must** derive xs from `xscale.min` / `max` evenly spaced when `<xpts>` is absent.
- ✓ Length mismatch handling: `WriteLookupBody` in MDLFormat.cpp:280 silently truncates to `min(xpts.size(), ypts.size())`. The reader follows the same defensive behavior — log a warning, then truncate.
- ✓ Even-spacing formula: for N y-values and `xscale min=a max=b`, x[i] = `a + i * (b - a) / (N - 1)` for `i = 0..N-1`. (Matches `SetXAxis`'s `xmin += increment` step.)
- ✓ XMILE `<gf>` attributes: `name` (optional, for top-level / named gf), `type="extrapolate"` (optional). XMILE 1.0 spec allows `type="continuous"` (default) or `"extrapolate"` or `"discrete"`. Phase 6 supports `"extrapolate"`; other values default to continuous (no flag set).
- ✓ XMILE round-trip behavior: an input file with `<xscale>` (no `<xpts>`) goes Reader → Model (xs derived) → Writer (emits `<xpts>` explicitly). Re-parsing that output preserves the same xs/ys, so the comparator sees an equivalent model.
- ✓ The Phase 2 equation grammar already supports `name(input)`-style XMILE function calls. If the XMILE has a top-level `<gf name="X">` (a standalone graphical function in the model's `<variables>` rather than inline on an aux), the reader treats it as a standalone graphical-function Variable named `X`. RHS expressions referencing `X(input)` parse via the grammar to `ExpressionFunction` / `ExpressionLookup` paths Phase 2 already covers.

---

<!-- START_SUBCOMPONENT_A (tasks 1-2) -->

<!-- START_TASK_1 -->
### Task 1: ProcessGf — parse a `<gf>` element into ExpressionTable

**Verifies:** Building block for AC3.1 lookup slice.

**Files:**
- Modify: `src/Xmile/XmileReader.h` (declare `ProcessGf`)
- Modify: `src/Xmile/XmileReader.cpp` (implement)

**Implementation notes:**

```cpp
// XmileReader.h, private section:
ExpressionTable *ProcessGf(tinyxml2::XMLElement *gf, std::vector<std::string> &errs);

// Helper: parse a comma-separated list of doubles from a text node.
static std::vector<double> ParseDoubleList(const char *text);
```

`ParseDoubleList` accepts a string like `"0,5,10"` or `"0, 5, 10"` (XMILE allows whitespace) and produces a vector. Use `strtod` for robustness; skip empty fragments; log nothing on per-token errors (just skip — tracks the writer's tolerance).

```cpp
ExpressionTable *XmileReader::ProcessGf(tinyxml2::XMLElement *gf,
                                        std::vector<std::string> &errs) {
  ExpressionTable *table = new ExpressionTable(pSymbolNameSpace);

  // type attribute: "continuous" (default), "extrapolate", "discrete".
  if (const char *gtype = gf->Attribute("type")) {
    if (std::string(gtype) == "extrapolate") table->SetExtrapolate(true);
    // "discrete" has no xmutil equivalent in v1; treat as continuous + warn.
    else if (std::string(gtype) == "discrete") {
      errs.push_back("<gf type=\"discrete\"> treated as continuous (xmutil has no discrete representation)");
    }
  }

  // y points are required.
  tinyxml2::XMLElement *yptsEl = gf->FirstChildElement("ypts");
  if (!yptsEl || !yptsEl->GetText()) {
    errs.push_back("<gf> has no <ypts>");
    delete table;
    return nullptr;
  }
  std::vector<double> ys = ParseDoubleList(yptsEl->GetText());
  if (ys.empty()) {
    errs.push_back("<gf><ypts> is empty");
    delete table;
    return nullptr;
  }

  // x points: explicit <xpts> takes priority; fall back to <xscale> + derived.
  std::vector<double> xs;
  tinyxml2::XMLElement *xptsEl = gf->FirstChildElement("xpts");
  if (xptsEl && xptsEl->GetText()) {
    xs = ParseDoubleList(xptsEl->GetText());
  } else if (tinyxml2::XMLElement *xscaleEl = gf->FirstChildElement("xscale")) {
    double xmin = xscaleEl->DoubleAttribute("min", 0.0);
    double xmax = xscaleEl->DoubleAttribute("max", 1.0);
    int n = static_cast<int>(ys.size());
    if (n == 1) {
      xs.push_back(xmin);
    } else {
      double step = (xmax - xmin) / (n - 1);
      for (int i = 0; i < n; ++i) {
        xs.push_back(xmin + i * step);
      }
    }
  } else {
    errs.push_back("<gf> has neither <xpts> nor <xscale>");
    delete table;
    return nullptr;
  }

  // Length mismatch: log + truncate (matches MDLFormat.cpp:280 behavior).
  size_t npairs = std::min(xs.size(), ys.size());
  if (xs.size() != ys.size()) {
    errs.push_back("<gf> has mismatched <xpts>/<ypts> lengths; truncating to "
                   + std::to_string(npairs));
  }
  for (size_t i = 0; i < npairs; ++i) {
    table->AddPair(xs[i], ys[i]);
  }
  return table;
}
```

**Verification:**
- `ninja -C out/Debug xmutil_test` succeeds.
- Existing tests still pass.

**Commit:** `feat(xmile): <gf> -> ExpressionTable parser`
<!-- END_TASK_1 -->

<!-- START_TASK_2 -->
### Task 2: Integrate `<gf>` into aux/flow handlers (WITH LOOKUP + standalone forms)

**Verifies:** AC3.1 (lookup slice).

**Files:**
- Modify: `src/Xmile/XmileReader.cpp` (extend `ProcessAux` / `ProcessFlow` to consult `<gf>`)

**Implementation notes:**

`ProcessAux` (and `ProcessFlow`) check for a `<gf>` child after attempting to parse `<eqn>`. The branching logic:

```cpp
// Inside ProcessAppliesToAllEquation (Phase 5) — generalized:
bool XmileReader::ProcessAppliesToAllEquation(
    tinyxml2::XMLElement *varEl, Variable *v,
    tinyxml2::XMLElement *dimsChild, std::vector<std::string> &errs) {

  tinyxml2::XMLElement *eqnEl = varEl->FirstChildElement("eqn");
  tinyxml2::XMLElement *gfEl = varEl->FirstChildElement("gf");

  // Determine LHS subscripts once (apply-to-all path).
  SymbolList *lhsSubs = dimsChild ? BuildAppliesToAllSubs(dimsChild) : nullptr;
  ExpressionVariable *lhsVar = new ExpressionVariable(pSymbolNameSpace, v, lhsSubs);
  LeftHandSide *lhs = new LeftHandSide(pSymbolNameSpace, lhsVar, NULL, NULL, 0);

  // Three cases:
  // 1. Non-empty <eqn>, <gf> absent     -> regular aux equation (Phase 3/5)
  // 2. Non-empty <eqn>, <gf> present    -> WITH LOOKUP shape
  // 3. <eqn> absent (or empty), <gf>    -> standalone graphical function
  // 4. <eqn> absent, <gf> absent        -> error (Phase 3 already logs this)

  if (eqnEl && eqnEl->GetText() && eqnEl->GetText()[0] != '\0' && !gfEl) {
    // Case 1 — same as Phase 3/5 scalar/apply-to-all aux.
    std::vector<std::string> eqnErrs;
    Expression *rhs = ParseEquation(eqnEl->GetText(), eqnErrs);
    if (!rhs) { for (const std::string &e : eqnErrs) errs.push_back(e); return false; }
    Equation *eq = new Equation(pSymbolNameSpace, lhs, rhs, '=');
    v->AddEq(eq);
  } else if (eqnEl && eqnEl->GetText() && eqnEl->GetText()[0] != '\0' && gfEl) {
    // Case 2 — WITH LOOKUP.
    std::vector<std::string> eqnErrs;
    Expression *input = ParseEquation(eqnEl->GetText(), eqnErrs);
    if (!input) { for (const std::string &e : eqnErrs) errs.push_back(e); return false; }
    ExpressionTable *table = ProcessGf(gfEl, errs);
    if (!table) { return false; }
    ExpressionLookup *rhs = new ExpressionLookup(pSymbolNameSpace, input, table);
    Equation *eq = new Equation(pSymbolNameSpace, lhs, rhs, '=');
    v->AddEq(eq);
  } else if (gfEl) {
    // Case 3 — standalone graphical function.
    ExpressionTable *table = ProcessGf(gfEl, errs);
    if (!table) return false;
    // Equation token '(' matches VensimParse::AddTable (VensimParse.cpp:168) for
    // the standalone case. The Expression* is the ExpressionTable directly.
    Equation *eq = new Equation(pSymbolNameSpace, lhs, table, '(');
    v->AddEq(eq);
  } else {
    // Case 4 — no equation and no gf. Already handled as a warning upstream;
    // free the LHS and return without an equation. (Variable still registered.)
    delete lhs;
    return true;
  }

  if (tinyxml2::XMLElement *u = varEl->FirstChildElement("units")) {
    if (const char *ut = u->GetText()) v->SetUnitsString(ut);
  }
  if (tinyxml2::XMLElement *d = varEl->FirstChildElement("doc")) {
    if (const char *dt = d->GetText()) v->SetComment(dt);
  }
  return true;
}
```

For the **per-element** path (`ProcessPerElementEquations`), each `<element>` could in principle carry its own `<gf>`, but XMILE in the wild and the writer's emission both treat `<gf>` as variable-level only. If a per-element subscripted aux carries a `<gf>`: log a warning and skip the gf (the per-element equations take precedence). The common case (one `<gf>` shared by all elements of a subscripted aux) is sufficient and rarely needed in practice — defer to a future task if the corpus demands.

For **`ProcessFlow`**: same as `ProcessAux`. Refactor to share the helper with aux explicitly: both now call `ProcessAppliesToAllEquation` (which handles eqn + gf) and `ProcessPerElementEquations`.

For **`ProcessStock`**: silently ignore any `<gf>` child. The writer asserts `<gf>` is aux/flow only (`XMILEGenerator.cpp:447`). Add a one-line guard:

```cpp
if (stock->FirstChildElement("gf")) {
  // No-op: <gf> on a stock has no valid meaning; the writer asserts it.
  // Log a warning to errs for debugging, but don't fail.
  errs.push_back("<stock> with <gf> child is ignored (graphical functions on stocks are not supported)");
}
```

**Verification:**
- `ninja -C out/Debug xmutil_test` succeeds.
- Phase 1-5 tests still pass.

**Commit:** `feat(xmile): inline <gf> WITH LOOKUP + standalone graphical-function forms`
<!-- END_TASK_2 -->

<!-- END_SUBCOMPONENT_A -->

<!-- START_SUBCOMPONENT_B (tasks 3-3) -->

<!-- START_TASK_3 -->
### Task 3: LookupRoundTripTest — fixtures for WITH LOOKUP, standalone, xscale, type="extrapolate"

**Verifies:** AC3.1 (lookup slice).

**Files:**
- Create: `test/xmile/LookupRoundTripTest.cpp`
- Modify: `XMUtil.gyp` (`xmutil_test` `sources`: add the new file)

**Implementation notes:**

```cpp
#include <string>
#include "../../src/Model.h"
#include "../../src/Symbol/Expression.h"
#include "../../src/Symbol/Variable.h"
#include "../TestHarness.h"
#include "RoundTrip.h"

namespace {

const char *kWithLookupExplicitXpts = R"(<?xml version="1.0" encoding="utf-8"?>
<xmile version="1.0" xmlns="http://docs.oasis-open.org/xmile/ns/XMILE/v1.0">
  <sim_specs method="euler"><start>0</start><stop>10</stop><dt>1</dt></sim_specs>
  <model><variables>
    <aux name="input"><eqn>5</eqn></aux>
    <aux name="output">
      <eqn>input</eqn>
      <gf>
        <xpts>0,5,10</xpts>
        <ypts>0,3,10</ypts>
      </gf>
    </aux>
  </variables></model>
</xmile>
)";

const char *kWithLookupXscaleOnly = R"(<?xml version="1.0" encoding="utf-8"?>
<xmile version="1.0" xmlns="http://docs.oasis-open.org/xmile/ns/XMILE/v1.0">
  <sim_specs method="euler"><start>0</start><stop>10</stop><dt>1</dt></sim_specs>
  <model><variables>
    <aux name="input"><eqn>5</eqn></aux>
    <aux name="output">
      <eqn>input</eqn>
      <gf>
        <xscale min="0" max="10"/>
        <ypts>0,3,10</ypts>
      </gf>
    </aux>
  </variables></model>
</xmile>
)";

const char *kStandaloneGf = R"(<?xml version="1.0" encoding="utf-8"?>
<xmile version="1.0" xmlns="http://docs.oasis-open.org/xmile/ns/XMILE/v1.0">
  <sim_specs method="euler"><start>0</start><stop>10</stop><dt>1</dt></sim_specs>
  <model><variables>
    <aux name="f">
      <gf>
        <xpts>0,5,10</xpts>
        <ypts>0,3,10</ypts>
      </gf>
    </aux>
  </variables></model>
</xmile>
)";

const char *kExtrapolateGf = R"(<?xml version="1.0" encoding="utf-8"?>
<xmile version="1.0" xmlns="http://docs.oasis-open.org/xmile/ns/XMILE/v1.0">
  <sim_specs method="euler"><start>0</start><stop>10</stop><dt>1</dt></sim_specs>
  <model><variables>
    <aux name="input"><eqn>5</eqn></aux>
    <aux name="output">
      <eqn>input</eqn>
      <gf type="extrapolate">
        <xpts>0,5,10</xpts>
        <ypts>0,3,10</ypts>
      </gf>
    </aux>
  </variables></model>
</xmile>
)";

}  // namespace

TEST(Lookup_with_lookup_explicit_xpts_round_trip) {
  std::vector<std::string> diffs = xmileroundtrip::RoundTripDiffs(kWithLookupExplicitXpts);
  for (const std::string &d : diffs) printf("  diff: %s\n", d.c_str());
  CHECK(diffs.empty());
}

TEST(Lookup_with_lookup_xscale_only_round_trip) {
  // xscale -> xs derived; output emits explicit <xpts>. Re-parse sees the
  // same xs (the derived ones) as the original — comparator equivalent.
  std::vector<std::string> diffs = xmileroundtrip::RoundTripDiffs(kWithLookupXscaleOnly);
  for (const std::string &d : diffs) printf("  diff: %s\n", d.c_str());
  CHECK(diffs.empty());
}

TEST(Lookup_standalone_gf_round_trip) {
  std::vector<std::string> diffs = xmileroundtrip::RoundTripDiffs(kStandaloneGf);
  for (const std::string &d : diffs) printf("  diff: %s\n", d.c_str());
  CHECK(diffs.empty());
}

TEST(Lookup_extrapolate_flag_round_trip) {
  // type="extrapolate" -> ExpressionTable::SetExtrapolate(true). The writer
  // emits the type attribute back on output, and the re-parse re-applies it.
  std::vector<std::string> diffs = xmileroundtrip::RoundTripDiffs(kExtrapolateGf);
  for (const std::string &d : diffs) printf("  diff: %s\n", d.c_str());
  CHECK(diffs.empty());
}

TEST(Lookup_with_lookup_shape_check) {
  // Direct shape check: an inline gf with non-empty eqn produces an
  // ExpressionLookup with both an input expression and a table.
  std::vector<std::string> errs;
  Model *m = xmileroundtrip::ParseXMILE(kWithLookupExplicitXpts, errs);
  CHECK(m != nullptr); CHECK(errs.empty());
  if (!m) return;
  Variable *out = static_cast<Variable *>(m->GetNameSpace()->Find("output"));
  CHECK(out != nullptr);
  if (out && !out->GetAllEquations().empty()) {
    Equation *eq = out->GetEquation(0);
    Expression *rhs = eq->GetExpression();
    CHECK(rhs != nullptr);
    if (rhs) {
      ExpressionLookup *lk = dynamic_cast<ExpressionLookup *>(rhs);
      CHECK(lk != nullptr);
      if (lk) CHECK(lk->GetTable() != nullptr);
    }
  }
  delete m;
}

TEST(Lookup_standalone_gf_shape_check) {
  // Standalone GF: the equation's expression IS the ExpressionTable.
  std::vector<std::string> errs;
  Model *m = xmileroundtrip::ParseXMILE(kStandaloneGf, errs);
  CHECK(m != nullptr); CHECK(errs.empty());
  if (!m) return;
  Variable *f = static_cast<Variable *>(m->GetNameSpace()->Find("f"));
  CHECK(f != nullptr);
  if (f && !f->GetAllEquations().empty()) {
    Equation *eq = f->GetEquation(0);
    Expression *rhs = eq->GetExpression();
    CHECK(rhs != nullptr);
    if (rhs) CHECK(rhs->GetType() == EXPTYPE_Table);
  }
  delete m;
}

TEST(Lookup_xscale_derives_correct_xs) {
  // Mid-level check: xscale min=0 max=10 with 3 ys -> xs = [0, 5, 10].
  std::vector<std::string> errs;
  Model *m = xmileroundtrip::ParseXMILE(kWithLookupXscaleOnly, errs);
  CHECK(m != nullptr); CHECK(errs.empty());
  if (!m) return;
  Variable *out = static_cast<Variable *>(m->GetNameSpace()->Find("output"));
  if (out && !out->GetAllEquations().empty()) {
    ExpressionLookup *lk = dynamic_cast<ExpressionLookup *>(out->GetEquation(0)->GetExpression());
    if (lk) {
      ExpressionTable *t = lk->GetTable();
      if (t) {
        std::vector<double> *xs = t->GetXVals();
        if (xs && xs->size() == 3) {
          CHECK((*xs)[0] == 0.0);
          CHECK((*xs)[1] == 5.0);
          CHECK((*xs)[2] == 10.0);
        }
      }
    }
  }
  delete m;
}
```

**Verification:**
- `ninja -C out/Debug xmutil_test` succeeds.
- All seven new tests pass.
- All Phase 1-5 tests still pass.

**Commit:** `test(xmile): lookup round-trip + xscale derivation + shape checks`
<!-- END_TASK_3 -->

<!-- END_SUBCOMPONENT_B -->

---

## Phase 6 done when

- Inline `<gf>` with explicit `<xpts>` round-trips.
- Inline `<gf>` with `<xscale>` only (no `<xpts>`) round-trips (xs derived correctly).
- Standalone graphical functions (no `<eqn>`, just `<gf>`) round-trip with the equation-token `'('` shape.
- `type="extrapolate"` attribute round-trips.
- All Phase 1-5 tests still pass.

## Out of scope for Phase 6

- `<gf>` on subscripted variables — supported for apply-to-all (the same `<gf>` covers all elements via the LHS subscripts); per-element-with-gf is rare and deferred.
- `<gf>` on stocks — silently skipped.
- Discrete graphical functions (`type="discrete"`) — treated as continuous with a warning. xmutil has no discrete representation.
- Named top-level `<gf name="X">` as a sibling of `<variables>` — this is rare and not in the corpus. If encountered, it would need separate handling (create a Variable named X with the table as RHS). Deferred unless needed.
- WITH LOOKUP on a subscripted-per-element aux — the per-element path doesn't read `<gf>` in Phase 6; only the apply-to-all path does.
- fishbanks corpus round-trip — Phase 7 needed for views. The Phase 6 fixtures cover the `<gf>` shapes that the fishbanks corpus uses (WITH LOOKUP with `<xscale>`); the corpus test itself lives in Phase 7/8.
