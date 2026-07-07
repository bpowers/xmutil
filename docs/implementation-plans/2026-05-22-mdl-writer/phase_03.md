# Vensim mdl Writer Implementation Plan — Phase 3: Equation section

**Goal:** Emit every variable kind with the `~ units ~ comment |` trailer: auxes/flows as `name = <expr> ~ ...`, stocks as `name = INTEG(<net_flow>, <init>)`, lookups in Vensim graphical-function syntax, dimension definitions `Dim: e1, e2, e3 ~~|`, subscripted variables as per-element/apply-to-all entries, with `\` line-continuation wrapping.

**Architecture:** `MDLGenerator` gains equation-section assembly that iterates `Model::GetVariables(nullptr)` (skipping `Unwanted()` control vars — Phase 4 — and `XMILE_Type_ARRAY_ELM`), dispatches on `VariableType()`, and emits one entry per stored `Equation` (each carries its own LHS subscripts, so apply-to-all and per-element forms both fall out naturally — no `SubscriptExpand` needed; see Decision below). The expression RHS is rendered by the Phase 2 walker. This phase also completes the walker's `RenderTableLike` (lookups) and **extends `ModelComparator` with structural, paren-insensitive expression-AST comparison** so equation round-trip tests can assert equivalence.

**Tech Stack:** C++17, gyp. Reference: simlin `writer.rs` — `write_stock_variable` (974-1031), `write_lookup_body` (784-811), `tokenize_for_wrapping` (1116-1197), `wrap_equation_with_continuations` (1205-1229), `write_single_entry` (1235+), `write_units_and_comment` (1354-1362).

**Scope:** Phase 3 of 7.

**Codebase verified:** 2026-05-22. Key facts:
- `Variable::VariableType()` returns `XMILE_Type` (`src/Symbol/Variable.h:24-32,297`): `UNKNOWN/AUX/DELAYAUX/STOCK/FLOW/ARRAY/ARRAY_ELM`. `DELAYAUX` emits as an aux.
- Stocks: `Inflows()`/`Outflows()` return `std::vector<Variable*>&` (`Variable.h:345-350`). Init value is the **2nd argument of the stock's `INTEG` expression** (no dedicated accessor); XMILEGenerator pulls it via the init-equation mechanism (`XMILEGenerator.cpp:343-355`).
- `Equation` (`src/Symbol/Equation.h:9-36`): `GetLeft()` returns a `LeftHandSide*` (`Equation.h:13`), NOT an `ExpressionVariable`; reach the LHS `ExpressionVariable` via `GetLeft()->GetExpressionVariable()` (`LeftHandSide.h:18`) and the LHS subscripts via `GetLeft()->GetSubs()` (`LeftHandSide.h:24-25`). `GetExpression()` (RHS), `GetTable()`. Units/comment live on `Variable`: `Units()`->`UnitExpression::GetEquationString()` else `GetUnitsString()` (`Variable.h:227,270`); `Comment()` (`Variable.h:236`). Variable's equations via `GetAllEquations()` (and `GetAllInitEquations()` for stock init).
- **`:EXCEPT:` is out of scope for v1.** `LeftHandSide` also carries a `SymbolListList *pExceptList` (`src/Symbol/LeftHandSide.h:31`) for Vensim's `name[a]:EXCEPT:[b] = ...` form, plus an interp/data-equation mode. v1 does NOT emit or compare the except clause; see the scope note in Task 2 and the corpus filter in Phase 7.
- `ExpressionTable` (`Expression.h:407-472`): `GetXVals()`/`GetYVals()` (`std::vector<double>*`), `Extrapolate()`. **No reliable x/y range getters** (the `AddRange` storage is buggy) — compute the `[(xmin,ymin)-(xmax,ymax)]` box from min/max of the val vectors (as `XMILEGenerator.cpp:482-500` does for the y-scale).
- `ExpressionLookup` (post-Phase-2): `GetTable()` (inline table, NULL for the `LOOKUP(var,x)` form), `GetInput()`, `GetLookupVariable()`.
- Dimensions: `XMILE_Type_ARRAY` variables whose `GetEquation(0)->GetExpression()` is `EXPTYPE_Symlist` (`ExpressionSymbolList`, `SymList()` -> `SymbolList`); see `XMILEGenerator.cpp:254-263`.
- `ExpressionNumberTable` (`Expression.h:240-275`): `GetVals()` -> `const std::vector<double>&`. After `MarkVariableTypes`, a `name[Dim]=1,2,3` is usually blown out into per-element `ExpressionNumber` equations (`Variable.cpp:152-180`); the residual `NumberTable` only remains if expansion failed.

## Decision: emit stored equations, not `SubscriptExpand` output

The design (AC2.4) suggests expanding subscripted variables per-element via `Equation::SubscriptExpand`. After investigation, the simpler and more faithful approach is to **emit each stored `Equation` with its own LHS subscripts** (from `Equation::GetLeft()`). This naturally produces: scalar (`name = ...`), apply-to-all (`name[Dim] = ...`, one equation), and per-element (`name[e1] = ...`, N equations) — exactly the forms Vensim accepts and re-parses. Because both sides of the round-trip are compared at the post-`MarkVariableTypes` level, and `MarkVariableTypes` already blew out number-table data into per-element equations, emitting the stored equations yields a clean structural match without the extra normalization (and verbosity) `SubscriptExpand` would introduce. `SubscriptExpand` remains available if a future need arises. (XMILEGenerator uses `SubscriptExpand` only because XMILE requires explicit `<element>` children; the `.mdl` form does not.)

---

## Acceptance Criteria Coverage

This phase implements and tests:

### mdl-writer.AC2: Equations emit as valid Vensim
- **mdl-writer.AC2.2 Success:** Stocks emit as `name = INTEG(<net_flow>, <init>)`, where `<net_flow>` is built from `Inflows()`/`Outflows()` and `<init>` from the stock's initial value.
- **mdl-writer.AC2.3 Success:** Lookups/graphical functions emit Vensim syntax `name([(xmin,ymin)-(xmax,ymax)],(x1,y1),...)` (standalone) and `WITH LOOKUP(input, (...))` (embedded).
- **mdl-writer.AC2.4 Success:** Dimension definitions emit `Dim: e1, e2, e3 ~~|`; subscripted variables emit per-element entries `name[elem] = ...`.
- **mdl-writer.AC2.6 Edge:** Equations longer than Vensim's line budget wrap with `\` continuations and re-parse to the same expression.

### mdl-writer.AC3: Round-trip preserves the model
- **mdl-writer.AC3.1 Success:** For each corpus model, `parse -> MarkVariableTypes -> M0`, `emit -> mdl'`, `parse -> MarkVariableTypes -> M1` yields the same variables with equivalent equation ASTs and variable types.
- **mdl-writer.AC3.2 Success:** Subscripts/dimensions, units, and comments are equivalent across the round-trip.

---

<!-- START_SUBCOMPONENT_A (tasks 1-3) -->
<!-- START_TASK_1 -->
### Task 1: Equation-entry helper, units/comment trailer, line wrapping

**Verifies:** mdl-writer.AC2.6 (wrapping); supports AC2.1/2.2

**Files:**
- Modify: `src/Mdl/MDLFormat.h` / `.cpp` (add `WrapEquation`)
- Modify: `src/Mdl/MDLGenerator.h` / `.cpp` (add entry/trailer helpers)

**Implementation:**

Port `tokenize_for_wrapping` and `wrap_equation_with_continuations` (simlin `writer.rs:1116-1229`) into `MDLFormat`:
```cpp
std::vector<std::string> mdl::TokenizeForWrapping(const std::string &eqn);
std::string mdl::WrapEquation(const std::string &eqn, size_t maxLineLen);  // call with 80
```
Port faithfully: split at `,` (absorbing one trailing space), `(`/`)`/`[`/`]`, and binary `+ - * / ^` (with the unary-detection guard at `writer.rs:1143-1154`); treat `'...'` and `"..."` as atomic tokens. `WrapEquation` returns the input unchanged when `eqn.size() <= maxLineLen`; otherwise breaks before a token that would exceed the limit, trimming trailing spaces and inserting `"\\\n\t\t"` (backslash, newline, two tabs).

`MDLGenerator` private helpers:
```cpp
// Returns "~\t<units>\n\t~\t<comment>\n\t|" trailer text for a variable.
std::string UnitsCommentTrailer(Variable *v);
// Emits "<lhs> = <wrapped rhs>\n\t~\t<units>\n\t~\t<comment>\n\t|\n".
void EmitEquationEntry(std::string &out, const std::string &lhs, const std::string &rhs, Variable *v);
```
`UnitsCommentTrailer` mirrors `write_units_and_comment` (`writer.rs:1354-1362`): units from `v->Units() ? v->Units()->GetEquationString() : v->GetUnitsString()`; comment from `v->Comment()`. `EmitEquationEntry` wraps `lhs + " = " + rhs` with `WrapEquation(..., 80)` then appends the trailer.

**Testing (describe):** Unit-test `WrapEquation` on a >80-char string: assert it contains `"\\\n\t\t"` and that stripping the continuations reconstructs the original token stream. (Round-trip of a real long equation is tested in Task 7.)

**Verification:** `out/Debug/xmutil_test` passes.

**Commit:** `feat(mdl): equation-entry helper, units/comment trailer, line wrapping`
<!-- END_TASK_1 -->

<!-- START_TASK_2 -->
### Task 2: Aux/flow equation emission

**Verifies:** mdl-writer.AC2.1, mdl-writer.AC3.1, mdl-writer.AC3.2

**Files:**
- Modify: `src/Mdl/MDLGenerator.{h,cpp}` (add `GenerateEquations` + per-variable dispatch; call it from `Print`)

**Implementation:**

Add `void GenerateEquations(std::string &out, SymbolNameSpace *ns)` and `void GenerateVariableEntry(std::string &out, Variable *v)`. `Print` calls `GenerateEquations(out, nullptr)` after the `{UTF-8}` header (Phase 4 inserts the `.Control` group and Phase 6 the macros before it).

`GenerateEquations` mirrors `XMILEGenerator::generateModelAsSectors` (`XMILEGenerator.cpp:289-294`):
```cpp
std::vector<Variable *> vars = _model->GetVariables(ns);
// Optional: sort by GetName() for deterministic output (comparator is set-based,
// so not required for correctness, but aids reproducibility).
for (Variable *v : vars) {
  if (v->Unwanted()) continue;                       // control vars (Phase 4) and suppressed
  if (v->VariableType() == XMILE_Type_ARRAY_ELM) continue;  // emitted via their array parent
  GenerateVariableEntry(out, v);
}
```

`GenerateVariableEntry` dispatches on `VariableType()`:
- `XMILE_Type_AUX`, `XMILE_Type_DELAYAUX`, `XMILE_Type_FLOW`: for each stored equation `eq` of the variable (`v->GetAllEquations()`): build the LHS text = `FormatMDLIdent(v->GetName())` + (subscripts from `eq->GetLeft()->GetSubs()`, rendered like the walker's `RenderSubscripts`); build RHS = `RenderExpression(eq->GetExpression())`; emit via `EmitEquationEntry`. If `eq->GetTable()` is non-NULL, this is a lookup — defer to Task 4.
  - **`:EXCEPT:` scope (v1):** do not read or emit `eq->GetLeft()`'s `pExceptList` (`LeftHandSide.h:31`). A model using `:EXCEPT:` would lose that clause silently, and the comparator would not catch it — so such models are **filtered out of the corpus** (Phase 7 Task 1) and this is a recorded v1 limitation, not an undetected gap. If a target model needs `:EXCEPT:`, it is a fast-follow: emit the `[a]:EXCEPT:[b]` LHS form and compare `pExceptList` in `ModelComparator`.
  - For a single equation, one entry `name = rhs ~ units ~ comment |`. For multiple equations (per-element), emit each; put the real units/comment trailer on the **last** entry and `\n\t~~|\n` between (mirror `write_arrayed_*`); or, simpler and still round-tripping, emit a full trailer on each — **verify with a round-trip test** which the parser prefers (start with trailer-on-each; if the parser rejects repeats, switch to trailer-on-last).
- `XMILE_Type_STOCK`: Task 3.
- `XMILE_Type_ARRAY`: Task 5 (dimension definition).
- `XMILE_Type_UNKNOWN`: skip (log nothing; matches XMILEGenerator behavior).

Reuse the walker's subscript renderer for the LHS — extract `RenderSubscripts(SymbolList*)` from Phase 2 into a shared private method so both LHS and RHS use it.

**Testing:** Covered by Task 7 round-trip tests (auxes/flows with units/comments).

**Verification:** builds; round-trip tests in Task 7.

**Commit:** `feat(mdl): emit aux/flow equations with units and comments`
<!-- END_TASK_2 -->

<!-- START_TASK_3 -->
### Task 3: Stock emission (`INTEG`)

**Verifies:** mdl-writer.AC2.2, mdl-writer.AC3.1

**Files:**
- Modify: `src/Mdl/MDLGenerator.cpp` (the `XMILE_Type_STOCK` branch)

**Implementation:**

Port `write_stock_variable` (`writer.rs:974-1031`), re-expressed for xmutil types. Net flow from `Inflows()`/`Outflows()` (`Variable*` -> `GetName()`):
```cpp
std::string net;
const auto &ins = v->Inflows();
const auto &outs = v->Outflows();
for (size_t i = 0; i < ins.size(); i++) {
  if (i) net += "+";
  net += mdl::FormatMDLIdent(ins[i]->GetName());
}
for (Variable *o : outs) {
  net += "-";
  net += mdl::FormatMDLIdent(o->GetName());
}
if (net.empty()) net = "0";
```
Init value: extract the **2nd argument of the stock's `INTEG` expression** and render it with the walker. Get the stock equation's RHS expression; verify it is `EXPTYPE_FunctionMemory` with `GetFunction()->GetName() == "INTEG"`; init = `RenderExpression(static_cast<ExpressionFunction*>(rhs)->GetArgs()->GetExp(1))`.
- **Verify during implementation** which mechanism xmutil actually exposes: (a) the INTEG node's 2nd arg (preferred), or (b) a separate init equation via `GetAllInitEquations()` whose expression is the init value. Use a quick probe: parse `s = INTEG(f, 100) ~~|` plus a flow `f`, run `MarkVariableTypes`, and confirm the path yields `100`. If the stock's stored RHS is no longer the INTEG node post-analysis, fall back to `GetAllInitEquations()`.

Emit `EmitEquationEntry(out, lhs, "INTEG(" + net + ", " + init + ")", v)`. For arrayed stocks, iterate per stored equation as in Task 2, building net flow and init per element (mirror `write_arrayed_stock_entries`, `writer.rs:1065-1092`).

**Round-trip note (design "Round-trip baseline"):** When `MarkVariableTypes` synthesized a `"<stock> net flow"` variable (the non-`+/-` case), `Inflows()` contains that synthetic flow; emitting `INTEG(<stock> net flow, init)` plus the synthetic flow's own equation re-parses to the identical post-`MarkVariableTypes` structure. This is a stable normalization, not a fidelity loss; the comparator (Task 6) treats it as equivalent because both M0 and M1 carry the same synthetic flow.

**Testing:** Task 7 (stock + flow round-trip).

**Commit:** `feat(mdl): emit stocks as INTEG(net_flow, init)`
<!-- END_TASK_3 -->
<!-- END_SUBCOMPONENT_A -->

<!-- START_SUBCOMPONENT_B (tasks 4-5) -->
<!-- START_TASK_4 -->
### Task 4: Lookups / graphical functions

**Verifies:** mdl-writer.AC2.3, mdl-writer.AC3.1

**Files:**
- Modify: `src/Mdl/MDLFormat.{h,cpp}` (add `WriteLookupBody`)
- Modify: `src/Mdl/MDLGenerator.cpp` (standalone GF in `GenerateVariableEntry`; complete `RenderTableLike` for `EXPTYPE_Lookup`)

**Implementation:**

`mdl::WriteLookupBody(const ExpressionTable *t)` ports `write_lookup_body` (`writer.rs:784-811`), computing the range box from data since xmutil lacks reliable range getters:
```cpp
// out: "[(xmin,ymin)-(xmax,ymax)],(x0,y0),(x1,y1),..."
std::string mdl::WriteLookupBody(ExpressionTable *t) {
  std::vector<double> &xs = *t->GetXVals();
  std::vector<double> &ys = *t->GetYVals();
  double xmin = /* min(xs) */, xmax = /* max(xs) */, ymin = /* min(ys) */, ymax = /* max(ys) */;
  std::string s = "[(" + FormatMDLNumber(xmin) + "," + FormatMDLNumber(ymin) + ")-(" +
                  FormatMDLNumber(xmax) + "," + FormatMDLNumber(ymax) + ")]";
  for (size_t i = 0; i < xs.size() && i < ys.size(); i++)
    s += ",(" + FormatMDLNumber(xs[i]) + "," + FormatMDLNumber(ys[i]) + ")";
  return s;
}
```
(Guard empty xs/ys.)

Three forms:
1. **Standalone graphical function** — a variable whose equation expression `GetType()==EXPTYPE_Table` (or `eq->GetTable()` non-NULL with no input). Emit `name(<body>)` (no `=`), then the units/comment trailer — i.e. `EmitEquationEntry`-style but with the lookup body as the "rhs" and no `=`:
   ```
   FormatMDLIdent(v->GetName()) + "(" + WriteLookupBody(table) + ")"  // then \n\t~ units ~ comment \n\t|
   ```
   In `GenerateVariableEntry`, detect this before the normal aux/flow path: if `eq->GetExpression()->GetType()==EXPTYPE_Table`, emit the standalone form.
2. **Embedded `WITH LOOKUP`** — `RenderTableLike` for an `ExpressionLookup` whose `GetTable()` is non-NULL: `"WITH LOOKUP(" + RenderExpression(lk->GetInput()) + ", (" + WriteLookupBody(lk->GetTable()) + "))"` (matches `write_single_entry`'s WITH LOOKUP branch).
3. **Lookup call `table(input)`** — `RenderTableLike` for an `ExpressionLookup` whose `GetLookupVariable()` is non-NULL (and `GetTable()` NULL): `FormatMDLIdent(lk->GetLookupVariable()->GetVariable()->GetName()) + "(" + RenderExpression(lk->GetInput()) + ")"` (matches `writer.rs:652-661`).

Wire `RenderTableLike` (the Phase 2 placeholder) to dispatch: `EXPTYPE_Lookup` -> forms 2/3; a bare `EXPTYPE_Table` appearing inside an expression -> `WriteLookupBody` wrapped per context (rare; the standalone case is handled at the equation level).

**Extrapolation:** `ExpressionTable::Extrapolate()` has no clean basic-Vensim syntax; for v1 do not emit an extrapolation marker and have the comparator ignore the flag (note as a known minor fidelity gap). Confirm round-trip still passes for non-extrapolating lookups.

**Testing:** Task 7 (standalone GF, WITH LOOKUP, lookup-call fixtures).

**Commit:** `feat(mdl): emit lookups (standalone GF, WITH LOOKUP, lookup call)`
<!-- END_TASK_4 -->

<!-- START_TASK_5 -->
### Task 5: Dimension definitions + arrayed constant data

**Verifies:** mdl-writer.AC2.4, mdl-writer.AC3.2

**Files:**
- Modify: `src/Mdl/MDLGenerator.cpp` (`XMILE_Type_ARRAY` branch; `EXPTYPE_NumberTable`/`EXPTYPE_Symlist` in `RenderTableLike`)

**Implementation:**

- **Dimension definitions** (`XMILE_Type_ARRAY`): the variable's `GetEquation(0)->GetExpression()` is `EXPTYPE_Symlist`. Emit `FormatMDLIdent(name) + ": " + e1 + ", " + e2 + ", " + ... + "\n\t~~|\n"` (empty units/comment, terminator `~~|`). Iterate `ExpressionSymbolList::SymList()` entries (NOT the XMILE expansion), emitting `FormatMDLIdent(entry.u.pSymbol->GetName())` per element; recurse `EntryType_LIST`. Subscript mapping: the mapping symlist is `ExpressionSymbolList::Map()` (`Expression.h:169`); a `SymbolList` also exposes `IsMapList()`/`MapRange()` (`SymbolList.h:46,49`) — these are distinct accessors on distinct classes, so pin the correct one during implementation. If a map is present, append the Vensim `-> MappedDim` mapping clause (verify the exact mapping syntax against a fixture; if no mapping fixtures exist in the corpus, emit just the element list and note mapping as a deferred minor case).
- **Arrayed constant data** (`EXPTYPE_NumberTable`, if it survives `MarkVariableTypes`): in `RenderTableLike`, emit `GetVals()` joined by `,` (e.g. `1,2,3`). Note the multi-row `;` form is already lost at parse time (`AddValue` ignores the row index) — a pre-existing limitation.
- **`EXPTYPE_Symlist` inside an expression** (rare): emit the bracketed element list.

**Testing:** Task 7 (a model with a dimension + a subscripted variable).

**Commit:** `feat(mdl): emit dimension definitions and arrayed constant data`
<!-- END_TASK_5 -->
<!-- END_SUBCOMPONENT_B -->

<!-- START_SUBCOMPONENT_C (tasks 6-7) -->
<!-- START_TASK_6 -->
### Task 6: Extend `ModelComparator` with structural expression-AST comparison

**Verifies:** mdl-writer.AC3.1, mdl-writer.AC3.2

**Files:**
- Modify: `test/mdl/ModelComparator.cpp` (fill in the Phase-1 `// TODO(phase3)` equation comparison)

**Implementation:**

Add `static bool ExpressionsEqual(Expression *a, Expression *b)` — paren-insensitive (unwrap `ExpressionParen` first, identical to the walker's `Unwrap`) structural comparison using the Phase 2 getters:
- Unwrap both; compare `GetType()`. If different -> not equal.
- `EXPTYPE_Number`: `GetValue()` within a small tolerance (e.g. `fabs(a-b) <= 1e-12 * max(1, |a|)`), with the `:NA:`/`-1e38` sentinel matched exactly.
- `EXPTYPE_Literal`: `GetValue()` strings equal.
- `EXPTYPE_Variable`: `GetVariable()->GetName()` equal AND subscript lists equal (compare `GetSubs()` entry-by-entry: count, each `GetName()`, and `eType` bang-ness).
- `EXPTYPE_Operator`: `GetOperator()` strings equal AND `GetArg(0)`/`GetArg(1)` recursively equal (unary minus: compare `GetBefore()` and `GetArg(0)` only).
- `EXPTYPE_Function`/`EXPTYPE_FunctionMemory`: `GetFunction()->GetName()` equal AND arg count equal AND each arg recursively equal.
- `EXPTYPE_Logical`: `LogicalOperator()` equal AND `GetLeft()`/`GetRight()` recursively equal (NULL-safe for `:NOT:`).
- `EXPTYPE_Lookup`: same form (var vs table), `GetInput()` recursively equal, and (table form) `GetTable()` xvals/yvals equal within tolerance.
- `EXPTYPE_Table`: xvals/yvals equal within tolerance.
- `EXPTYPE_NumberTable`: `GetVals()` equal within tolerance.
- `EXPTYPE_Symlist`: element-name lists equal.

In `compareVariables`, for each common variable compare its equation set: match equations by LHS subscript signature (the `GetLeft()` subscript names; scalar = empty signature), then `ExpressionsEqual(eq_a->GetExpression(), eq_b->GetExpression())`. Report any unmatched equation or RHS mismatch with a descriptive diff. Also compare stock structure (`Inflows()`/`Outflows()` name sets) for `XMILE_Type_STOCK` variables.

**Why structural, not re-render:** comparing by re-rendering through the walker would let an emit bug that drops information pass vacuously (the same walker drives emit). A getter-based structural compare is independent of the writer.

**Testing:** Strengthen the AC3.5 guard (Phase 1) — confirm `ExpressionsEqual` distinguishes `a + b` from `a - b`, and `f(x)` from `g(x)`. Confirm paren-insensitivity: `(a + b) * c` and `a + b * c` produce **non-equal** RHS (different structure), while `(a)` and `a` are equal.

**Verification:** `out/Debug/xmutil_test` passes.

**Commit:** `test(mdl): structural paren-insensitive expression comparison`
<!-- END_TASK_6 -->

<!-- START_TASK_7 -->
### Task 7: Equation round-trip tests

**Verifies:** mdl-writer.AC2.2, AC2.3, AC2.4, AC2.6, AC3.1, AC3.2

**Files:**
- Create: `test/mdl/EquationRoundTripTest.cpp` (added to `xmutil_test` sources)

**Implementation / Testing (describe):**
Use `roundtrip::RoundTripDiffs(mdlText)` (Phase 1) — parse -> `MarkVariableTypes` -> `PrintMDL` -> re-parse -> `ModelComparator::Compare` — and assert the returned diff list is empty for each fixture. Fixtures (small inline `.mdl` strings, each with the `{UTF-8}` header and a minimal `.Control` group so they parse; or omit control and rely on defaults):
- **Aux/flow + units + comment** (AC2.1, AC3.2): `c = a * b ~ widgets ~ a comment |`.
- **Stock + flows** (AC2.2): a stock `s = INTEG(inflow - outflow, 100)` with `inflow`/`outflow` auxes.
- **Standalone graphical function** (AC2.3): `f([(0,0)-(10,10)],(0,0),(5,3),(10,10)) ~~|`.
- **Embedded WITH LOOKUP** (AC2.3): `y = WITH LOOKUP(x, ([(0,0)-(10,10)],(0,0),(10,10))) ~~|`.
- **Dimension + subscripted variable** (AC2.4): `Dim: a, b, c ~~|` and `v[Dim] = 1, 2, 3 ~~|` (and a computed `w[Dim] = v[Dim] * 2`).
- **Long wrapped equation** (AC2.6): an aux whose RHS exceeds 80 chars (e.g. a long sum); assert the round-trip diff is empty (the `\` continuations re-parse to the same expression).
- **Builtins** (AC2.1): `z = IF THEN ELSE(x > 0, PULSE(1, 2), 0) ~~|`.

Also add at least one **broader** fixture: pick a small simlin corpus model with equations only (e.g. `third_party/simlin/.../sdeverywhere/models/delay/delay.mdl`) and assert empty diffs (this previews Phase 7's corpus test).

**Verification:** `out/Debug/xmutil_test` passes all equation round-trip tests.

**Commit:** `test(mdl): equation-section round-trip tests`
<!-- END_TASK_7 -->
<!-- END_SUBCOMPONENT_C -->

## Phase 3 Done When
- Round-trip tests over equation-only fixtures (stocks/flows/auxes, lookups, arrays, units, comments, long wrapped equations) pass via the comparator (AC2.2, AC2.3, AC2.4, AC2.6, AC3.1, AC3.2).
