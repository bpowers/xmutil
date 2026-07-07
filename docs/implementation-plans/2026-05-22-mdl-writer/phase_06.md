# Vensim mdl Writer Implementation Plan — Phase 6: Macros

**Goal:** Emit Vensim `:MACRO: name(args) ... :END OF MACRO:` blocks (before the main equation section) that re-parse to an equivalent macro.

**Architecture:** `MDLGenerator::GenerateMacros` iterates `Model::MacroFunctions()`. For each macro it emits the header `:MACRO: <name>(<args>)`, then the body equations generated over the macro's own `SymbolNameSpace` (reusing the Phase 3 `GenerateVariableEntry`), then `:END OF MACRO:`. Macros are emitted right after the `{UTF-8}` header and before the main equations (matching simlin `writer.rs:2775` and the XMILE generator's macro-first handling).

**Tech Stack:** C++17, gyp. Reference: simlin `writer.rs:2775` (`write_macro_blocks`); xmutil parser at `src/Vensim/VensimParse.cpp:644-659` and the existing XMILE macro emission at `src/Xmile/XMILEGenerator.cpp:55-78`.

**Scope:** Phase 6 of 7. (The design marks this phase deferrable; it is included here because the body reuses the Phase 2-3 walker/equation emitter and good fixtures exist. If it must slip, exclude macro-using models from the Phase 7 corpus.)

**Codebase verified:** 2026-05-22. Key facts:
- `Model::MacroFunctions()` -> `std::vector<MacroFunction*>&` (`src/Model.h:57-59`).
- `MacroFunction` (`src/Function/Function.h:106-135`): inherits `Symbol::GetName()` (the macro name); `ExpressionList *Args()` (the formal parameters as `Expression*`s); `SymbolNameSpace *NameSpace()` (the macro's local namespace).
- **`MacroFunction::mEquations` is dead code** (zero callers). Body equations live as ordinary `Variable`s in `NameSpace()`, iterated via `Model::GetVariables(mf->NameSpace())` — exactly how `XMILEGenerator::generateModelAsSectors(macro, errs, mf->NameSpace(), false)` emits them (`XMILEGenerator.cpp:55-78,283-294`).
- The C entry already runs `MarkVariableTypes(mf->NameSpace())` per macro (`src/XMUtil.cpp:299-301`; mirrored by `convert_to_mdl` in Phase 1), so the macro body is in the same post-`MarkVariableTypes` state as the main model.
- Parser syntax (`VensimParse.cpp:644-659`, `VYacc.y:90-104`): header `:MACRO: <symbol>(<exprlist>)`; body equations in standard `name = expr ~ units ~ comment |` form; terminated by `:END OF MACRO:`. The lexer keywords are the literal strings `:MACRO:` and `:END OF MACRO:` (`VensimLex.cpp:401-420`). A macro body may contain a stock (`= INTEG(...)`).

---

## Acceptance Criteria Coverage

This phase implements and tests:

### mdl-writer.AC6: Macros
- **mdl-writer.AC6.1 Success:** A model with a Vensim macro emits a `:MACRO: ... :END OF MACRO:` block that re-parses to an equivalent macro.

---

<!-- START_TASK_1 -->
### Task 1: `GenerateMacros` emission

**Verifies:** mdl-writer.AC6.1

**Files:**
- Modify: `src/Mdl/MDLGenerator.{h,cpp}` (add `GenerateMacros`; call it from `Print` before the main equations)

**Implementation:**

```cpp
void MDLGenerator::GenerateMacros(std::string &out) {
  for (MacroFunction *mf : _model->MacroFunctions()) {
    // Header: :MACRO: name(arg1, arg2, ...)
    out += ":MACRO: " + mdl::FormatMDLIdent(mf->GetName()) + "(";
    ExpressionList *args = mf->Args();
    int n = args ? args->Length() : 0;
    for (int i = 0; i < n; i++) {
      if (i) out += ", ";
      out += RenderExpression(args->GetExp(i));   // formal params (usually ExpressionVariable)
    }
    out += ")\n";
    // Body: every variable in the macro's local namespace, via the Phase 3 emitter.
    std::vector<Variable *> body = _model->GetVariables(mf->NameSpace());
    // Optional: sort by GetName() for determinism.
    for (Variable *v : body) {
      if (v->VariableType() == XMILE_Type_ARRAY_ELM) continue;
      GenerateVariableEntry(out, v);   // handles aux/flow/stock/array/lookup uniformly
    }
    out += ":END OF MACRO:\n\n";
  }
}
```
Call `GenerateMacros(out)` in `Print` immediately after the `{UTF-8}` header and before the grouped/main equations.

Notes:
- The macro body must NOT apply the `IsControlVar`/group logic from Phase 4 — macros have no `.Control` group or banners. Iterating `GetVariables(mf->NameSpace())` and calling `GenerateVariableEntry` directly (as above) avoids that logic.
- A macro-body stock emits as `INTEG(...)` via the Phase 3 stock path, exercised by the `macro_stock` fixture.

**Testing:** Covered by Task 2.

**Verification:** builds; macro round-trip in Task 2.

**Commit:** `feat(mdl): emit :MACRO: ... :END OF MACRO: blocks`
<!-- END_TASK_1 -->

<!-- START_TASK_2 -->
### Task 2: Macro comparison + round-trip tests

**Verifies:** mdl-writer.AC6.1

**Files:**
- Modify: `test/mdl/ModelComparator.cpp` (compare `MacroFunctions()`)
- Create: `test/mdl/MacroRoundTripTest.cpp` (added to `xmutil_test`)

**Implementation:**

Extend `ModelComparator::Compare` to compare macros:
- Match `MacroFunctions()` by `GetName()`. Report any macro present on one side only.
- For each matched macro, compare arg count and each rendered arg (or structurally via `ExpressionsEqual`), and compare the body: run the same variable comparison used for the main model, but over `GetVariables(mf->NameSpace())` on each side (variable presence, type, equation ASTs via `ExpressionsEqual`, units, comments, subscripts). Refactor the Phase 1/3 variable-comparison routine to take a `SymbolNameSpace*` (or a variable list) so it can be reused for both main and macro namespaces.

**Testing (describe):** `roundtrip::RoundTripDiffs` over macro fixtures (assert empty diffs):
- `third_party/simlin/test/test-models/tests/macro_expression/test_macro_expression.mdl` (a macro with no stock, single expression output).
- `third_party/simlin/test/test-models/tests/macro_stock/test_macro_stock.mdl` (a macro body containing `= INTEG(...)`).
- Add a focused assertion that the macro name and parameter list survive, and that the body equation's RHS is structurally equal.

**Verification:** `out/Debug/xmutil_test` passes the macro round-trip tests.

**Commit:** `test(mdl): macro round-trip tests and comparator`
<!-- END_TASK_2 -->

## Phase 6 Done When
- Round-trip tests over macro-using models pass (AC6.1). If this phase is deferred, macro-using models are excluded from the Phase 7 corpus until it lands.
