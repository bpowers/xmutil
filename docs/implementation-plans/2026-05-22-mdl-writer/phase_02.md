# Vensim mdl Writer Implementation Plan — Phase 2: Vensim expression walker

**Goal:** Convert any `Expression` tree to Vensim text via a recursive walker in `MDLGenerator`, emitting native Vensim function names and operators with no XMILE expansion, and porting simlin's operator-precedence parenthesization.

**Architecture:** A recursive `MDLGenerator::RenderExpression(Expression*)` dispatches on `Expression::GetType()`. Functions emit uniformly as `GetName()(args)` (no XMILE rewrites). Operators emit infix with precedence-derived parentheses (ported from simlin `mdl_paren_if_necessary`). The walker **unwraps** `ExpressionParen` nodes and re-derives parentheses from precedence — matching simlin, whose AST has no paren nodes. It does **not** reuse `Expression::OutputComputable` (which emits XMILE: underscored names, `ComputableName()`, and unconditional structural rewrites like `PULSE`->`IF...`).

**Tech Stack:** C++17, gyp. Reference: `third_party/simlin/src/simlin-engine/src/mdl/writer.rs` (the `MdlPrintVisitor::walk` at lines 637-729, `mdl_paren_if_necessary` at 257-284, `needs_mdl_quoting`/`escape_mdl_quoted_ident`/`format_mdl_ident` at 38-99, `BinaryOp::precedence` in `src/ast/expr0.rs:43-61`).

**Scope:** Phase 2 of 7.

**Codebase verified:** 2026-05-22. Key facts (from codebase-investigator over `src/Symbol/Expression.{h,cpp}`, `src/Symbol/ExpressionList.{h,cpp}`, `src/Symbol/SymbolList.{h,cpp}`, `src/Function/Function.{h,cpp}`, `src/Vensim/VYacc.y`):
- `Expression::GetType()` returns an `EXPTYPE` enum (`src/Symbol/Expression.h:22-34`): `EXPTYPE_None/Variable/Symlist/Number/Literal/NumberTable/Function/FunctionMemory/Lookup/Table/Operator`. **There is no `EXPTYPE_Logical`** — `ExpressionLogical` inherits `EXPTYPE_None`.
- Arithmetic ops are `ExpressionOperator2` subclasses (`Expression.h:474-576`): `ExpressionMultiply/Divide/Add/Subtract/Power` (`GetOperator()` returns `* / + - ^`, `GetBefore()` returns `""`), plus `ExpressionParen` (`GetBefore()=="("`, `GetOperator()==""`) and `ExpressionUnaryMinus` (`GetBefore()=="-"`, `GetOperator()==""`). All report `EXPTYPE_Operator`. Children via `GetArg(0)`/`GetArg(1)`. **There is no `GetAfter()`** and no `EXPTYPE` for Mod (xmutil has no MOD operator — MODULO is a function).
- `ExpressionLogical` (`Expression.h:578-633`): private `pE1`, `pE2`, `int mOper`; `mOper` holds an ASCII char (`'='`,`'<'`,`'>'`) or a yacc token (`VPTT_le`,`VPTT_ge`,`VPTT_ne`,`VPTT_and`,`VPTT_or`,`VPTT_not`). For `VPTT_not` the operand is in `pE2` (`pE1==NULL`). No getters today.
- `ExpressionNumber` (`Expression.h:178-209`): private `double value`, no getter. `:NA:` parses to `value == -1e38` (`VYacc.y:212`).
- `ExpressionLiteral` (`Expression.h:211-239`): private `std::string value`, no getter.
- `ExpressionFunction::GetType()==EXPTYPE_Function`, `ExpressionFunctionMemory::GetType()==EXPTYPE_FunctionMemory` (subclass; inherits `GetFunction()`/`GetArgs()`). `GetFunction()->GetName()` (inherited from `Symbol::GetName()`, `src/Symbol/Symbol.h:55`) is the **Vensim** name (e.g. `IF THEN ELSE`, `PULSE`, `DELAY FIXED`, `INTEG`, `WITH LOOKUP`); `Function::ComputableName()` (`Function.h:38`) is the XMILE name (do not use it).
- `ExpressionList` (`src/Symbol/ExpressionList.h:9-35`): `int Length()`, `Expression *GetExp(int)`.
- `SymbolList` (`src/Symbol/SymbolList.h:9-62`): `int Length()`, `const SymbolListEntry &operator[](int)`; entry `eType ∈ {EntryType_SYMBOL, EntryType_BANG_SYMBOL, EntryType_LIST}`, `entry.u.pSymbol` / `entry.u.pSymbolList`. `Symbol::GetName()` returns the original name (`src/Symbol/Symbol.cpp:26-28`).
- simlin `BinaryOp::precedence`: And/Or=1, Eq/Neq=2, Gt/Lt/Gte/Lte=3, Add/Sub=4, Mul/Div/Mod=5, Exp=6.
- Adding `EXPTYPE_Logical` and overriding `ExpressionLogical::GetType()` is **safe**: an audit of all `->GetType()` call sites found none compares against `EXPTYPE_None` on a logical (the two `!= EXPTYPE_Number` checks are parse-time on number-list elements). See `src/Function/Function.cpp:50,296`, `src/Symbol/{Expression,Equation,Variable}.cpp`, `src/Dynamo/*`, `src/Vensim/VensimParse.cpp:131,134` — all switch on Variable/Operator/Number/Symlist/Table/Function only.

---

## Acceptance Criteria Coverage

This phase implements and tests:

### mdl-writer.AC2: Equations emit as valid Vensim
- **mdl-writer.AC2.1 Success:** Aux/flow variables emit as `name = <expr> ~ units ~ comment |`, with Vensim function names via `Function::GetName()` and no XMILE expansion (e.g. `PULSE`, `IF THEN ELSE`, `DELAY FIXED` round-trip unchanged). *(Phase 2 delivers the `<expr>` walker that makes this true; the full `name = ... ~ ... ~ ... |` trailer is Phase 3.)*
- **mdl-writer.AC2.5 Edge:** Identifiers requiring quoting are quoted; numbers format correctly, including the `:NA:` sentinel for `-1e38`.

---

<!-- START_SUBCOMPONENT_A (tasks 1-3) -->
<!-- START_TASK_1 -->
### Task 1: Add `EXPTYPE_Logical` and `ExpressionLogical` accessors

**Verifies:** mdl-writer.AC2.1 (enables logical operators in the walker)

**Files:**
- Modify: `src/Symbol/Expression.h` (enum at lines 22-34; `ExpressionLogical` at 578-633)

**Implementation:**
1. Append `EXPTYPE_Logical` to the `EXPTYPE` enum (after `EXPTYPE_Operator`).
2. In `ExpressionLogical`, add:
   ```cpp
   virtual EXPTYPE GetType(void) override { return EXPTYPE_Logical; }
   Expression *GetLeft() { return pE1; }    // NULL for unary :NOT:
   Expression *GetRight() { return pE2; }   // operand for :NOT: lives here
   int LogicalOperator() { return mOper; }  // ASCII char or VPTT_* token
   ```
   (Names: do not collide with the base `GetArg`/`GetOperator`; use `GetLeft`/`GetRight`/`LogicalOperator`.)

**Testing:** No standalone test; exercised by Task 4's golden tests and the AST-comparison upgrade in Phase 3. A regression guard: the existing `XMUtil`/`xmutil_test` build still compiles and the existing XMILE smoke conversion (C-LEARN) still produces identical output (the new enum value is unused by existing code).

**Verification:** `ninja -C out/Debug XMUtil xmutil_test` builds cleanly.

**Commit:** `feat(mdl): add EXPTYPE_Logical and ExpressionLogical accessors`
<!-- END_TASK_1 -->

<!-- START_TASK_2 -->
### Task 2: Add value getters to `ExpressionNumber` / `ExpressionLiteral`

**Verifies:** mdl-writer.AC2.5

**Files:**
- Modify: `src/Symbol/Expression.h` (`ExpressionNumber` 178-209; `ExpressionLiteral` 211-239)

**Implementation:**
- `ExpressionNumber`: add `double GetValue() const { return value; }`.
- `ExpressionLiteral`: add `const std::string &GetValue() const { return value; }`.

**Testing:** Exercised by Task 4 (`:NA:` / number-format golden tests).

**Verification:** builds cleanly.

**Commit:** `feat(mdl): add value getters to ExpressionNumber/ExpressionLiteral`
<!-- END_TASK_2 -->

<!-- START_TASK_3 -->
### Task 3: Add input/variable accessors to `ExpressionLookup`

**Verifies:** mdl-writer.AC2.1 (so Phase 3 can emit `WITH LOOKUP`/lookup-call forms)

**Files:**
- Modify: `src/Symbol/Expression.h` (`ExpressionLookup` 355-405)

**Implementation:** add public getters for the two private members that currently lack them:
```cpp
Expression *GetInput() { return pExpression; }                 // the input/argument expr
ExpressionVariable *GetLookupVariable() { return pExpressionVariable; }  // NULL for WITH LOOKUP
```
(`GetTable()` already exists at line 376 and returns the inline table for the `WITH LOOKUP` form, NULL for the `LOOKUP(var,x)` form.)

**Testing:** none here; consumed by Phase 3.

**Verification:** builds cleanly.

**Commit:** `feat(mdl): add ExpressionLookup input/variable accessors`
<!-- END_TASK_3 -->
<!-- END_SUBCOMPONENT_A -->

<!-- START_SUBCOMPONENT_B (tasks 4-6) -->
<!-- START_TASK_4 -->
### Task 4: Identifier quoting + number formatting helpers

**Verifies:** mdl-writer.AC2.5

**Files:**
- Create: `src/Mdl/MDLFormat.h`
- Create: `src/Mdl/MDLFormat.cpp`
- Modify: `XMUtil.gyp` (add the two files to `common_sources`)

**Implementation:** free functions in an `mdl` namespace, ported from simlin (`writer.rs:26-99`, `format_f64` 833-845).

```cpp
// MDLFormat.h
#pragma once
#include <string>
namespace mdl {
// True if the (already-dequoted) name must be wrapped in double quotes for Vensim.
bool NeedsMDLQuoting(const std::string &name);
// Escape interior quotes/newlines/backslashes for a quoted Vensim ident.
std::string EscapeMDLQuotedIdent(const std::string &name);
// Strip optional surrounding quotes, then quote+escape iff needed. The single
// entry point the walker uses for every emitted identifier.
std::string FormatMDLIdent(const std::string &rawName);
// Vensim number text: ":NA:" for -1e38, integer form for whole values, else
// shortest round-trippable decimal.
std::string FormatMDLNumber(double v);
}  // namespace mdl
```

`NeedsMDLQuoting` (ASCII port of `writer.rs:38-60` — interior spaces are allowed unquoted; quote on empty, leading/trailing space, a leading non-`[A-Za-z_]`, or any interior char outside `[A-Za-z0-9_ ]`):
```cpp
bool mdl::NeedsMDLQuoting(const std::string &name) {
  if (name.empty()) return true;
  if (name.front() == ' ' || name.back() == ' ') return true;
  char c0 = name[0];
  bool start_ok = (c0 == '_') || (c0 >= 'A' && c0 <= 'Z') || (c0 >= 'a' && c0 <= 'z');
  if (!start_ok) return true;
  for (char c : name) {
    if (c == ' ') continue;
    bool cont_ok = (c == '_') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
    if (!cont_ok) return true;
  }
  return false;
}
```
`EscapeMDLQuotedIdent` (port of `writer.rs:62-85`): map `'\n'`->`\\n`, `'\\'`->`\\\\` (collapsing an existing `\n` escape), `'"'`->`\\\"`, else copy.

`FormatMDLIdent`:
```cpp
std::string mdl::FormatMDLIdent(const std::string &raw) {
  // Normalize: strip a single pair of surrounding quotes if present, so the
  // result is consistent whether or not the parser stored quotes in sName.
  std::string core = raw;
  if (core.size() >= 2 && core.front() == '"' && core.back() == '"')
    core = core.substr(1, core.size() - 2);
  if (NeedsMDLQuoting(core)) return "\"" + EscapeMDLQuotedIdent(core) + "\"";
  return core;
}
```
**Verify during implementation:** parse a model containing a quoted identifier (e.g. simlin fixture `.../sdeverywhere/models/specialchars/specialchars.mdl`) and inspect `Variable::GetName()` to confirm whether `sName` retains surrounding quotes — the strip-then-requote logic above handles both cases, but confirm the round-trip is byte-stable for a quoted name.

`FormatMDLNumber` — use C++17 `std::to_chars` for shortest round-trippable output (libstdc++ on the Fedora/GCC toolchain supports the float overload):
```cpp
#include <charconv>
#include <cmath>
std::string mdl::FormatMDLNumber(double v) {
  if (v == -1e38) return ":NA:";                          // Vensim :NA: sentinel
  if (std::isfinite(v) && v == std::trunc(v) && std::fabs(v) < 1e15) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(v));
    return buf;
  }
  char buf[64];
  auto res = std::to_chars(buf, buf + sizeof(buf), v);    // shortest round-trippable
  return std::string(buf, res.ptr);
}
```
If `std::to_chars(double)` is unavailable on the toolchain, fall back to `snprintf(buf, n, "%.17g", v)` (guarantees round-trip; uglier).

**Testing (describe — task-implementor writes assertions in a new `test/mdl/MDLFormatTest.cpp`, added to the `xmutil_test` target):**
- mdl-writer.AC2.5: `FormatMDLNumber(-1e38) == ":NA:"`; `FormatMDLNumber(3.0) == "3"`; `FormatMDLNumber(0.5) == "0.5"`; a value like `0.1` round-trips (parse the emitted text back to the same double). `FormatMDLIdent("Susceptible Population") == "Susceptible Population"` (no quoting); `FormatMDLIdent("Lag #")` is quoted; an already-quoted input is not double-quoted.

**Verification:** `out/Debug/xmutil_test` passes the format tests.

**Commit:** `feat(mdl): add Vensim identifier quoting and number formatting`
<!-- END_TASK_4 -->

<!-- START_TASK_5 -->
### Task 5: The recursive Vensim expression walker

**Verifies:** mdl-writer.AC2.1, mdl-writer.AC2.5

**Files:**
- Modify: `src/Mdl/MDLGenerator.h` (add the walker methods)
- Modify: `src/Mdl/MDLGenerator.cpp` (implement)

**Implementation:**

Add a public entry `std::string RenderExpression(Expression *e)` and private recursive helpers. Include the needed headers (`Symbol/Expression.h`, `Symbol/ExpressionList.h`, `Symbol/SymbolList.h`, `Symbol/Variable.h`, `Function/Function.h`, `Vensim/VYacc.tab.hpp` or wherever `VPTT_*` tokens are declared — verify the token header; the lexer/parser includes it).

Dispatch on `GetType()`. **Always unwrap `ExpressionParen` first** so parentheses are derived purely from precedence (matching simlin).

```cpp
// Follow ExpressionParen -> child until a non-paren node.
static Expression *Unwrap(Expression *e) {
  while (e && e->GetType() == EXPTYPE_Operator && e->GetBefore() &&
         std::string(e->GetBefore()) == "(") {
    e = e->GetArg(0);
  }
  return e;
}
```

Precedence + classification helpers (simlin values):
```cpp
// Binary-operator precedence, or 100 (no-paren) for non-binary nodes.
static int MdlPrecedence(Expression *e) {
  e = Unwrap(e);
  if (!e) return 100;
  if (e->GetType() == EXPTYPE_Operator) {
    const char *op = e->GetOperator();
    if (op && *op) {                         // binary arithmetic (before == "")
      switch (op[0]) {
        case '+': case '-': return 4;
        case '*': case '/': return 5;
        case '^': return 6;
      }
    }
    return 100;                               // paren/unary-minus: not a binary parent
  }
  if (e->GetType() == EXPTYPE_Logical) {
    int o = static_cast<ExpressionLogical *>(e)->LogicalOperator();
    if (o == VPTT_and || o == VPTT_or) return 1;
    if (o == '=' || o == VPTT_ne) return 2;
    if (o == '<' || o == '>' || o == VPTT_le || o == VPTT_ge) return 3;
    return 100;                               // VPTT_not is unary
  }
  return 100;
}
static bool IsBinaryOp(Expression *e) { return MdlPrecedence(e) < 100; }
```

`mdl_paren_if_necessary` port (`writer.rs:257-284`):
```cpp
// parent and child are already unwrapped. Wrap childStr in parens when the
// child's precedence is lower than the parent's, or (for the right child of
// non-associative -, /) when equal.
std::string MDLGenerator::ParenIfNecessary(Expression *parent, Expression *child,
                                           bool isRightChild, const std::string &childStr) {
  bool needs = false;
  if (IsBinaryOp(parent) && IsBinaryOp(child)) {
    int pp = MdlPrecedence(parent), cp = MdlPrecedence(child);
    if (pp > cp) {
      needs = true;
    } else if (isRightChild && pp == cp) {
      const char *po = parent->GetOperator();          // '-' or '/' only (no Mod in xmutil)
      needs = po && (po[0] == '-' || po[0] == '/');
    }
  } else if (IsUnaryOp(parent) && IsBinaryOp(child)) {  // Op1 over Op2 -> parenthesize
    needs = true;
  }
  return needs ? "(" + childStr + ")" : childStr;
}
```
(`IsUnaryOp`: `ExpressionUnaryMinus` — `EXPTYPE_Operator`, `GetBefore()=="-"`, `GetOperator()==""` — or `ExpressionLogical` with `mOper==VPTT_not`.)

The main walker:
```cpp
std::string MDLGenerator::RenderExpression(Expression *e) {
  e = Unwrap(e);
  if (!e) return "";
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
    case EXPTYPE_Lookup:   // ExpressionLookup
    case EXPTYPE_Table:    // standalone ExpressionTable
    case EXPTYPE_NumberTable:
    case EXPTYPE_Symlist:
      return RenderTableLike(e);   // implemented in Phase 3; for Phase 2 return a TODO marker
    default:
      return "";
  }
}
```

`RenderOperator(e)` (e is `EXPTYPE_Operator`, already unwrapped, so not a paren):
- If `GetOperator()` non-empty → binary: `l = ParenIfNecessary(e, GetArg(0), false, RenderExpression(GetArg(0)))`, `r = ParenIfNecessary(e, GetArg(1), true, RenderExpression(GetArg(1)))`, return `l + " " + GetOperator() + " " + r` (spaces around operator, matching `writer.rs:720`).
- Else if `GetBefore()=="-"` (unary minus): `c = ParenIfNecessary(e, GetArg(0), false, RenderExpression(GetArg(0)))`, return `"-" + c` (no space, `writer.rs:685`). The operand for unary minus is in `GetArg(0)` (`pE1`).

`RenderLogical(lg)`:
- Map `LogicalOperator()` to a Vensim symbol: `'='`->`=`, `VPTT_ne`->`<>`, `'<'`->`<`, `'>'`->`>`, `VPTT_le`->`<=`, `VPTT_ge`->`>=`, `VPTT_and`->`:AND:`, `VPTT_or`->`:OR:`, `VPTT_not`->`:NOT:`.
- For `VPTT_not` (unary, operand in `GetRight()`): return `":NOT: " + ParenIfNecessary(lg, GetRight(), false, RenderExpression(GetRight()))`.
- Else binary: `l = ParenIfNecessary(lg, GetLeft(), false, RenderExpression(GetLeft()))`, `r = ParenIfNecessary(lg, GetRight(), true, RenderExpression(GetRight()))`, return `l + " " + sym + " " + r`.

`RenderFunction(fn)` (`fn->GetFunction()->GetName()` is the Vensim name; args via `GetArgs()`):
```cpp
Function *f = fn->GetFunction();
ExpressionList *args = fn->GetArgs();
std::string name = mdl::FormatMDLIdent(f->GetName());   // builtins won't need quoting; macros use their name
int n = args ? args->Length() : 0;
if (n == 0) return name;                                 // bare keyword: TIME, DT (no parens) -- writer.rs:646-651
std::string out = name + "(";
for (int i = 0; i < n; i++) {
  if (i) out += ", ";
  out += RenderExpression(args->GetExp(i));
}
out += ")";
return out;
```
**Note (no XMILE expansion):** because we read `GetName()` and recurse args ourselves — never calling `Function::OutputComputable` — the unconditional XMILE rewrites in `src/Function/Function.cpp` (PULSE->IF, IF->`( IF ...)`, etc.) are entirely bypassed. `IF THEN ELSE(c, t, f)`, `PULSE(a, b)`, `DELAY FIXED(...)`, `INTEG(...)` all emit verbatim. **Verify** during implementation that `Time`/`DT` appear as zero-arg functions (so the bare-keyword rule fires) or as variables (which already emit as names) — parse a model using `Time` and check the AST node type; adjust if `Time` is a 0-arg function whose `GetName()` is not literally `Time`.

`RenderVariableRef(v)`:
```cpp
std::string out = mdl::FormatMDLIdent(v->GetVariable()->GetName());
SymbolList *subs = v->GetSubs();
if (subs && subs->Length() > 0) out += RenderSubscripts(subs);
return out;
```
`RenderSubscripts(subs)` iterates `(*subs)[i]` (NOT `SymbolList::OutputComputable`, which is XMILE):
```cpp
std::string out = "[";
for (int i = 0; i < subs->Length(); i++) {
  if (i) out += ", ";
  const SymbolList::SymbolListEntry &e = (*subs)[i];
  if (e.eType == SymbolList::EntryType_LIST) {
    out += RenderSubscripts(e.u.pSymbolList);   // nested (rare)
  } else {
    out += mdl::FormatMDLIdent(e.u.pSymbol->GetName());
    if (e.eType == SymbolList::EntryType_BANG_SYMBOL) out += "!";   // Vensim bang
  }
}
out += "]";
return out;
```

For Phase 2, `RenderTableLike` returns a placeholder (e.g. `"{table}"`); Phase 3 replaces it. Golden tests in this phase avoid lookup/table expressions.

**Testing (describe — assertions in `test/mdl/MDLGeneratorTest.cpp` or a new `WalkerTest.cpp`):**
Use this helper to render a parsed RHS:
```cpp
static std::string RenderRHS(const std::string &mdlText, const std::string &varName) {
  Model *m = roundtrip::ParseVensim(mdlText);   // parses + MarkVariableTypes
  Variable *v = /* find by name via m->GetVariables(nullptr) and GetName()==varName */;
  MDLGenerator g(m);
  return g.RenderExpression(v->GetEquation(0)->GetExpression());
}
```
Golden assertions (parse a one-line equation, render, assert exact text) — **mdl-writer.AC2.1, AC2.5**:
- Precedence/parens: `a + b * c` -> `a + b * c`; `(a + b) * c` -> `(a + b) * c`; `a - (b - c)` -> `a - (b - c)`; `a / (b / c)` -> `a / (b / c)`; `2 ^ 3 ^ 2` (right-assoc) preserves grouping.
- Builtins (verbatim Vensim names, no expansion): `IF THEN ELSE(x > 0, 1, 0)`; `PULSE(5, 2)`; `DELAY FIXED(x, 3, 0)`; `MAX(a, b)`; `WITH LOOKUP` is a Phase-3 case.
- Logical/comparison: `x > 0 :AND: y < 1`; `:NOT: done`; `x <> y`; `a <= b`.
- Subscripted reference: `flow[Region]` round-trips with the subscript.
- Numbers/`:NA:`: an equation with `:NA:` renders `:NA:`; integer/decimal constants format per Task 4.
Each golden expression must also **re-parse to an equivalent expression** (parse the rendered text wrapped as `tmp = <text> ~~|`, compare structurally — using the Phase-3 comparator once available, or for Phase 2 a re-render-and-string-compare round-trip: `render(parse(render(e))) == render(e)`).

**Verification:** `out/Debug/xmutil_test` passes all walker golden tests.

**Commit:** `feat(mdl): recursive Vensim expression walker with precedence parens`
<!-- END_TASK_5 -->

<!-- START_TASK_6 -->
### Task 6: Walker round-trip self-consistency tests

**Verifies:** mdl-writer.AC2.1, mdl-writer.AC2.5

**Files:**
- Modify: the walker test file from Task 5

**Implementation / Testing (describe):**
Add a render->parse->render idempotence test over a set of representative equations (the Task 5 list plus a few nested ones). For each: render the parsed RHS to text `s1`, wrap and parse `tmp = s1 ~~|`, render again to `s2`, assert `s1 == s2`. This catches precedence/quoting bugs that a single render would miss (idempotence is a strong property for a normalizer). This is a property-style guard; it does not yet use `ModelComparator` (equation comparison lands in Phase 3).

**Verification:** `out/Debug/xmutil_test` passes.

**Commit:** `test(mdl): walker render/parse idempotence tests`
<!-- END_TASK_6 -->
<!-- END_SUBCOMPONENT_B -->

## Phase 2 Done When
- Golden tests assert exact Vensim text for representative expressions (operators with precedence, builtins, `IF THEN ELSE`, subscripted references, quoting, numbers/`:NA:`), and those expressions re-parse to equivalent expressions (AC2.1, AC2.5).
