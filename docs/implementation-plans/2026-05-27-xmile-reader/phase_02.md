# XMILE Reader Implementation Plan — Phase 2

**Goal:** Parse any XMILE equation text into xmutil `Expression*` trees identical in shape to those `VensimParse` produces, so the rest of the engine (writers, `MarkVariableTypes`, `ModelComparator`) treats them uniformly.

**Architecture:** A new bison grammar (`src/Xmile/XmileEqYacc.y`) with committed `.tab.{cpp,hpp}` artifacts, plus a hand-written lexer (`XmileEqLex.{h,cpp}`) — mirroring the existing Vensim/Dynamo pattern (bison parser + hand-written lex). A small action-code bridge (`XmileParseFunctions.{h,cpp}`) and a function-name un-rename table (`XmileFunctions.{h,cpp}`) round out the surface. The parser exposes `XmileReader::ParseEquation(text, errs) -> Expression*` for Phase 3+ to drive once per `<eqn>` element.

**Tech Stack:** GNU Bison (system tool, not gyp-driven — `.tab.*` artifacts are checked in), C++17, the existing `Expression` API in `src/Symbol/Expression.h`.

**Scope:** Phase 2 of 8.

**Codebase verified:** 2026-05-28

---

## Acceptance Criteria Coverage

This phase implements and tests:

### xmile-reader.AC2: XMILE equations parse to xmutil Expression trees
- **xmile-reader.AC2.1 Success:** XMILE arithmetic expressions parse to the same `ExpressionAdd` / `Subtract` / `Multiply` / `Divide` / `Power` shapes the Vensim parser produces; operator precedence matches the lalrpop grammar's lattice.
- **xmile-reader.AC2.2 Success:** XMILE function names map to Vensim `Function*` via the un-rename table: `smth1` -> `SMOOTH`, `smth3` -> `SMOOTH3`, `delay` -> `DELAY FIXED`, `delay1` -> `DELAY1`, `delay3` -> `DELAY3`, `int` -> `INTEGER`, `safediv` -> `ZIDZ` (2-arg) or `XIDZ` (3-arg), `init` -> `ACTIVE INITIAL`, plus the generic `underbar_to_space().to_uppercase()` fallback for unmapped Vensim builtins.
- **xmile-reader.AC2.3 Success:** Bare zero-argument keywords `time` / `dt` / `initial_time` / `final_time` resolve to `Time` / `TIME STEP` / `INITIAL TIME` / `FINAL TIME` as zero-arg `ExpressionFunction`s.

  **Correction surfaced by codebase investigation:** Those names are not registered as `Function*` objects in xmutil. They are ordinary `Variable*` instances in the model namespace. The reader resolves the XMILE keyword to a `Variable*` (creating it if absent) and constructs `ExpressionVariable`, not `ExpressionFunction`. This is what `XMILEGenerator` and `MDLGenerator` both expect — see `XMILEGenerator` line ~629 where `Time` is filtered by name. The plan honors the AC's intent (a single, stable encoding for each keyword) while matching engine reality. Test fixtures in Task 5 assert `ExpressionVariable` with the correct `Variable` name.

- **xmile-reader.AC2.4 Success:** `if cond then a else b` parses to `ExpressionFunction("IF THEN ELSE", [cond, a, b])`.
- **xmile-reader.AC2.5 Success:** Subscripts (`x[a, b]`, `x[*]`, `x[*:Dim]`, `x[l:r]`, `x[@1]`) parse to `ExpressionVariable` with the corresponding subscript representation.
- **xmile-reader.AC2.6 Edge:** Both keyword (`AND`, `OR`, `NOT`, `MOD`) and C-style (`&&`, `||`, `!`, `%`) operator spellings produce the same `ExpressionLogical` / operator nodes.
- **xmile-reader.AC2.7 Edge:** `//` (safediv) parses to `ExpressionFunction("ZIDZ", [l, r])` (matching the lalrpop's `App("safediv", [l, r])` shape).
- **xmile-reader.AC2.8 Failure:** A function name with no Vensim mapping in the un-rename table is reported as an error naming the unknown function; the parse fails.

  **Correction:** The simlin writer's fallback rule is `underbar_to_space(name).to_uppercase()` — i.e., any unknown name falls through to that transform and `SymbolNameSpace::Find` resolves it. The reader follows the same rule: an unmapped name is **not** an immediate error — it falls through to the fallback. AC2.8 fires only when the fallback also fails to resolve (no registered `Function*` with that name). Tests in Task 5 verify this two-step behavior.

- **xmile-reader.AC2.9 Failure:** A postfix transpose `'` in an equation is reported as a parse error.

---

## Codebase verification findings

- ✓ `src/Vensim/VYacc.y:76-82` declares the Vensim precedence:
  ```
  %left '-' '+'
  %left VPTT_or
  %left '=' '<' '>' VPTT_le VPTT_ge VPTT_ne
  %left VPTT_and
  %left '*' '/'
  %left VPTT_not
  %right '^'
  ```
  This is **not** standard C precedence (comparisons are between OR and AND). The XMILE grammar must use standard recursive-descent precedence per the simlin reference (logical < equality < comparison < additive < multiplicative < unary < exponentiation), so the Expression *shape* matches the simlin parser's tree even if the resulting `ExpressionAdd` / `ExpressionLogical` *node types* are identical to Vensim's.
- ✓ `VYacc.tab.cpp` and `VYacc.tab.hpp` open with "A Bison parser, made by GNU Bison 3.3.2" — pristine bison output, regenerated manually and committed. Same convention applies to Dynamo (`DYacc.tab.{cpp,hpp}`). The XMILE grammar follows: commit the generator output.
- ✓ `src/Vensim/VensimLex.cpp` is hand-written C++ (no flex banner, clang-format clean). `src/Dynamo/DynamoLex.cpp` is the same. **The codebase does not use flex anywhere.** Despite the design saying "flex," the implementation plan follows the established convention: hand-written lex.
- ✓ `src/Vensim/VensimParse.cpp:570-610` implements `OperatorExpression(int oper, Expression *exp1, Expression *exp2)` with a single switch over `oper` that constructs `ExpressionMultiply` / `Divide` / `Add` / `Subtract` / `Power` / `UnaryMinus` / `Logical`. The XMILE driver will define its own thin `OperatorExpression`-style helper (in `XmileReader`) that takes the same operator codes, so the grammar's `%type` and reduction code is symmetric to VYacc.y. The codes are integer constants — we'll define `XPTT_*` parallel to `VPTT_*` and reuse `'+' '-' '*' '/' '^'` as their literal char codes.
- ✓ `Expression` node API (`src/Symbol/Expression.h`):
  - `ExpressionAdd / Subtract / Multiply / Divide / Power / UnaryMinus(SymbolNameSpace *sns, Expression *e1, Expression *e2)` — `UnaryMinus` takes `(operand, NULL)`.
  - `ExpressionLogical(SymbolNameSpace *sns, Expression *e1, Expression *e2, int oper)` — `oper` is one of `'<' '>' '='`, `VPTT_le`, `VPTT_ge`, `VPTT_ne`, `VPTT_and`, `VPTT_or`, `VPTT_not`. For `VPTT_not`, `e1 = NULL`. **The XMILE grammar must use the same `VPTT_*` integer values** so the resulting `ExpressionLogical` is indistinguishable from a Vensim-parsed one. We will `#include "../Vensim/VYacc.tab.hpp"` in the XMILE driver to get those constants.
  - `ExpressionFunction(sns, Function *f, ExpressionList *args)`.
  - `ExpressionFunctionMemory(sns, Function *f, ExpressionList *args)` — used for stateful functions (INTEG, SMOOTH family, DELAY family). The XMILE grammar selects between `ExpressionFunction` and `ExpressionFunctionMemory` based on `f->isMemory()` — `Function::IsMemoryless()` returns the inverse. See `VensimParse::FunctionExpression` (line ~550) for the dispatch.
  - `ExpressionVariable(sns, Variable *var, SymbolList *subs)`.
  - `ExpressionNumber(sns, double num)`.
  - `ExpressionSymbolList(sns, SymbolList *subs, SymbolList *map)` — for RHS of `Dim: a, b, c`.
  - `ExpressionLookup(sns, ExpressionVariable *var, Expression *e)` (named) and `(sns, Expression *e, ExpressionTable *tbl)` (WITH LOOKUP). Phase 2 only needs the named form; the WITH LOOKUP form is built by Phase 6 directly, not via the equation grammar.
- ✓ `Variable` lookup: `SymbolNameSpace::Find(const std::string &)` returns a `Symbol*` (or NULL); a `Variable` is `dynamic_cast`able from that. To create-if-absent we use `XmileReader::InsertVariable(name)` (a new helper, mirroring `VensimParse::InsertVariable` — see VensimParse.cpp line ~265). The signature is the same: `Variable *InsertVariable(const std::string &name)`.
- ✓ `Function*` lookup: same `SymbolNameSpace::Find` with case-insensitive matching (`StringMatch` in `src/XMUtil.cpp:52-70`). All registered functions live in the namespace under their canonical Vensim names (e.g., `"IF THEN ELSE"`, `"INTEG"`, `"SMOOTH"`). The XMILE un-rename produces those canonical names; we then `Find` them.
- ✗ **Design's reference to `equation.lalrpop` at git ref `abda30d9^` is stale**: that file is not in the working tree's git history. The live simlin parser is at `third_party/simlin/src/simlin-engine/src/parser/mod.rs` — a hand-written recursive-descent parser. The implementation plan uses it (and `writer.rs`) as the algorithmic reference.
- ✓ `third_party/simlin/src/simlin-engine/src/mdl/writer.rs:181-247` is the source of truth for the un-rename tables:
  - `mdl_bare_keyword`: `time -> Time`, `dt|time_step -> TIME STEP`, `starttime|initial_time -> INITIAL TIME`, `endtime|stoptime|final_time -> FINAL TIME`.
  - `xmile_to_mdl_function_name`: `smth1 -> SMOOTH`, `smth3 -> SMOOTH3`, `delay -> DELAY FIXED`, `delay1 -> DELAY1`, `delay3 -> DELAY3`, `delayn -> DELAY N`, `smthn -> SMOOTH N`, `init -> ACTIVE INITIAL`, `int -> INTEGER`, `lookupinv -> LOOKUP INVERT`, `uniform -> RANDOM UNIFORM`, `safediv -> ZIDZ`, `forcst -> FORECAST`, `normalpink -> RANDOM PINK NOISE`, `normal -> RANDOM NORMAL`, `lookup -> LOOKUP`, `integ -> INTEG`. Default fallback: `underbar_to_space(name).to_uppercase()`.
  - `reorder_args`: `DELAY N`/`SMOOTH N`: swap args 2↔3; `RANDOM NORMAL`: rotate args. The XMILE reader applies the same reorder.
- ✗ **AC2.7 `safediv -> XIDZ`** when 3-arg: simlin's writer always emits `ZIDZ` for `safediv` regardless of arg count; the xmutil reader follows AC2.7's arg-count rule (2-arg → ZIDZ; 3-arg → XIDZ) because xmutil registers both. AC2.2 already calls this out explicitly.
- ✓ `Function::NumberArgs()` returns the registered arity (see `src/Function/Function.h`). The reader uses this to validate the un-renamed name's arg count matches the XMILE call's actual argument count; on mismatch, push descriptive error to `errs`.
- ✓ `XmileReader` and `XPObject` already exist (from Phase 1). Phase 2 fills in the equation-parsing surface on the same class.

---

## Lexer token plan

The XMILE equation lexer is hand-written C++ (`XmileEqLex.cpp`) following `VensimLex.cpp` structure. Tokens (defined in the `XmileEqYacc.y` bison file and exported through `XmileEqYacc.tab.hpp`):

| Token | Lex source pattern | Notes |
|-------|---------------------|-------|
| `XPTT_number` | `[0-9]+(\.[0-9]*)?([eE][-+]?[0-9]+)?` or `\.[0-9]+...` | yylval.num = atof |
| `XPTT_symbol` | bare `[a-zA-Z_][a-zA-Z_0-9]*` or `\"...\"` quoted (preserve quote interior) | yylval.sym = (interned) char* |
| `XPTT_if` `XPTT_then` `XPTT_else` | bare keywords (case-insensitive) | for AC2.4 |
| `XPTT_and` `XPTT_or` `XPTT_not` `XPTT_mod` | keyword spelling OR symbol spelling, case-insensitive | both spellings same token (AC2.6) |
| `'&&'` `'||'` `'!'` `'%'` | C-style operator | maps to `XPTT_and` / `XPTT_or` / `XPTT_not` / `XPTT_mod` tokens (AC2.6) |
| `XPTT_eq` `XPTT_neq` | `=` / `==` (same token); `<>` / `!=` (same token) | yylval.tok |
| `XPTT_lt` `XPTT_lte` `XPTT_gt` `XPTT_gte` | `<` / `<=` / `>` / `>=` | yylval.tok |
| `XPTT_safediv` | `//` | yylval.tok (AC2.7) |
| `XPTT_na` | bare `nan` / `NaN` / `NAN` / `:na:` / `:NA:` | yylval.num = -1e38 (delivered as XPTT_number with the sentinel value; pick the simpler path) |
| `'+' '-' '*' '/' '^' '(' ')' '[' ']' ',' ':' '@'` | single-char operators | as in Vensim |
| `XPTT_apostrophe` | `'` postfix | always produces a parse error in the grammar (AC2.9) |

Lexer state notes:
- Comments: XMILE equations don't have inline comments in `<eqn>` (the doc/comment is in `<doc>` siblings). The lexer rejects unexpected `{...}` runs as errors.
- Whitespace: skip ASCII whitespace including `\r` `\n` `\t`.
- End-of-input: returns 0 (bison's accept signal).

## Bison grammar shape

```yacc
/* XmileEqYacc.y - XMILE equation grammar
   Bison rename prefix: -p xpyy. Output: XmileEqYacc.tab.{cpp,hpp}. */
%{
#include "../Log.h"
#include "../Symbol/Parse.h"
#include "XmileParseFunctions.h"
extern int xpyylex(void);
extern void xpyyerror(char const *);
#define YYSTYPE ParseUnion
#define YYFPRINTF XmutilLogf
%}

%token <num> XPTT_number
%token <sym> XPTT_symbol  /* parser receives Variable* already resolved */
%token <tok> XPTT_if XPTT_then XPTT_else
%token <tok> XPTT_and XPTT_or XPTT_not XPTT_mod
%token <tok> XPTT_eq XPTT_neq XPTT_lt XPTT_lte XPTT_gt XPTT_gte
%token <tok> XPTT_safediv XPTT_apostrophe

%type <exn> expr
%type <exl> arglist
%type <sml> subs  /* subscript list */

/* Precedence — standard C-like, mirroring simlin's parser/mod.rs hierarchy.
   Lowest at top. */
%right XPTT_then XPTT_else
%left XPTT_or
%left XPTT_and
%left XPTT_eq XPTT_neq
%left XPTT_lt XPTT_lte XPTT_gt XPTT_gte
%left '+' '-'
%left '*' '/' XPTT_safediv XPTT_mod
%right UMINUS XPTT_not
%right '^'

%%

equation : expr { xpyy_set_result($1); return 0; }
         ;

expr : XPTT_number                       { $$ = xpyy_num($1); }
     | XPTT_symbol                       { $$ = xpyy_resolve_symbol($1, NULL); }
     | XPTT_symbol '[' subs ']'          { $$ = xpyy_resolve_symbol($1, $3); }
     | '(' expr ')'                      { $$ = $2; }
     | '-' expr  %prec UMINUS            { $$ = xpyy_unary('-', $2); }
     | '+' expr  %prec UMINUS            { $$ = $2; }
     | XPTT_not expr                     { $$ = xpyy_logical_unary(XPTT_not, $2); }
     | expr '+' expr                     { $$ = xpyy_binop('+', $1, $3); }
     | expr '-' expr                     { $$ = xpyy_binop('-', $1, $3); }
     | expr '*' expr                     { $$ = xpyy_binop('*', $1, $3); }
     | expr '/' expr                     { $$ = xpyy_binop('/', $1, $3); }
     | expr XPTT_safediv expr            { $$ = xpyy_safediv($1, $3); }
     | expr XPTT_mod expr                { $$ = xpyy_function("MODULO", $1, $3); }
     | expr '^' expr                     { $$ = xpyy_binop('^', $1, $3); }
     | expr XPTT_eq expr                 { $$ = xpyy_logical('=', $1, $3); }
     | expr XPTT_neq expr                { $$ = xpyy_logical_ne($1, $3); }
     | expr XPTT_lt expr                 { $$ = xpyy_logical('<', $1, $3); }
     | expr XPTT_lte expr                { $$ = xpyy_logical_le($1, $3); }
     | expr XPTT_gt expr                 { $$ = xpyy_logical('>', $1, $3); }
     | expr XPTT_gte expr                { $$ = xpyy_logical_ge($1, $3); }
     | expr XPTT_and expr                { $$ = xpyy_logical_and($1, $3); }
     | expr XPTT_or expr                 { $$ = xpyy_logical_or($1, $3); }
     | XPTT_if expr XPTT_then expr XPTT_else expr
                                         { $$ = xpyy_if($2, $4, $6); }
     | XPTT_symbol '(' ')'               { $$ = xpyy_call($1, NULL); }
     | XPTT_symbol '(' arglist ')'       { $$ = xpyy_call($1, $3); }
     | expr XPTT_apostrophe              { xpyyerror("postfix ' (transpose) is not supported"); YYABORT; }
     ;

arglist : expr                           { $$ = xpyy_arglist(NULL, $1); }
        | arglist ',' expr               { $$ = xpyy_arglist($1, $3); }
        ;

subs : sub_term                          { $$ = xpyy_sub_init($1); }
     | subs ',' sub_term                 { $$ = xpyy_sub_append($1, $3); }
     ;

sub_term : XPTT_symbol                   { $$ = xpyy_sub_name($1); }
         | '*'                           { $$ = xpyy_sub_star(NULL); }
         | '*' ':' XPTT_symbol           { $$ = xpyy_sub_star($3); }
         | XPTT_symbol ':' XPTT_symbol   { $$ = xpyy_sub_range($1, $3); }
         | '@' XPTT_number               { $$ = xpyy_sub_index((int)$2); }
         ;

%%
```

Grammar notes:
- The grammar takes a *bare* `XPTT_symbol` lvalue (a string) and resolves it inside the action via `xpyy_resolve_symbol` — that helper performs the un-rename + `Find` for keywords (`time`, `dt`, ...) and falls through to `InsertVariable` for everything else. Function names appear only in `XPTT_symbol '(' ... ')'` productions; `xpyy_call` performs the un-rename + `Function*` lookup there.
- Logical operators construct `ExpressionLogical` with the correct `VPTT_*` integer code (imported from `VYacc.tab.hpp`) so the node is bit-for-bit equivalent to a Vensim-parsed one.
- Numeric comparison vs. `XPTT_eq` token `'='` distinction: we always pass `'='` for the `ExpressionLogical` `oper` parameter so the existing `==` works downstream.
- The grammar deliberately does **not** support the `<eqn>`-level subscript syntax variants `[1, 2]` (numeric literal subscript) since XMILE doesn't use them on the equation side — subscripts in equations are always names, stars, ranges, or `@N` indexers.

## Function un-rename table

`src/Xmile/XmileFunctions.{h,cpp}` exposes:

```cpp
// XmileFunctions.h
#pragma once
#include <string>
class Function;
class Variable;
class SymbolNameSpace;
namespace xmile {

// Resolve an XMILE function name (lowercase, possibly with underscores) to the
// Vensim Function* registered in the namespace. Returns NULL if no mapping and
// the fallback (underbar_to_space + uppercase) also fails to resolve.
// The optional `argCount` lets the resolver disambiguate safediv -> ZIDZ vs
// XIDZ; pass 0 when the call site doesn't know the arity yet.
Function *LookupFunction(SymbolNameSpace *sns, const std::string &xmileName, int argCount);

// Resolve an XMILE bare keyword (time, dt, initial_time, final_time, ...) to a
// Variable* in the namespace, creating it if absent. Returns NULL if the name
// is not one of the recognized bare keywords.
Variable *LookupBareKeyword(SymbolNameSpace *sns, const std::string &xmileName);

// Reorder XMILE-order args into Vensim-order for the small set of functions
// that need it (DELAY N, SMOOTH N, RANDOM NORMAL). Mutates the list in place;
// no-op otherwise.
void ReorderArgs(const std::string &vensimName, class ExpressionList *args);

}  // namespace xmile
```

The implementation contains a single `static const struct { const char *xmile; const char *vensim; } kFuncMap[]` table covering the simlin writer's map (see writer.rs:194-216). `LookupFunction`:
1. Lower-case the input.
2. Apply `safediv` -> `ZIDZ` (if `argCount == 2`) / `XIDZ` (if `argCount == 3`); error if `argCount` is otherwise.
3. Look up in `kFuncMap`. If found, the result is a Vensim name; resolve via `SymbolNameSpace::Find` (case-insensitive).
4. Fall through: replace `_` with `' '`, uppercase, `Find`. Return NULL if absent.

`LookupBareKeyword` lowercase-matches against a `static const struct { const char *xmile; const char *vensimVar; } kKeywordMap[]` covering `mdl_bare_keyword`'s entries (see writer.rs:181-189), and `InsertVariable` (create-if-absent) on success.

## Driver method

`XmileReader::ParseEquation` is the public surface Phase 3+ calls per `<eqn>`. It runs the lexer + bison parser end-to-end:

```cpp
// XmileReader.h additions:
Expression *ParseEquation(const std::string &text, std::vector<std::string> &errs);

// Variable resolution helpers (mirroring VensimParse::InsertVariable):
Variable *InsertVariable(const std::string &name);
Variable *FindVariable(const std::string &name);
```

`ParseEquation` initializes a per-call `XmileEqLex` (a fresh stack object, not stored on the reader), parks it on a process-global accessor used by the bison `xpyylex` shim, runs the parse, captures the result Expression in a per-call slot, and returns it. Errors append to `errs`. The `XPObject` global (introduced in Phase 1) holds the reader; the lex is the second piece of state the action code needs and is reachable through `XPObject->CurrentLex()`. Concurrency is not a concern (xmutil is single-threaded).

---

<!-- START_SUBCOMPONENT_A (tasks 1-2) -->

<!-- START_TASK_1 -->
### Task 1: XmileFunctions — XMILE name un-rename + bare-keyword table

**Verifies:** xmile-reader.AC2.2 (un-rename mapping), AC2.3 (bare-keyword resolution), AC2.7 (safediv arg-count dispatch). Unit-tested directly via the lookup helpers; the round-trip behind these ACs is exercised by Task 5.

**Files:**
- Create: `src/Xmile/XmileFunctions.h`
- Create: `src/Xmile/XmileFunctions.cpp`
- Modify: `XMUtil.gyp` (`common_sources`: add the two entries under the existing `XmileReader` entries)
- Create: `test/xmile/XmileFunctionsTest.cpp`
- Modify: `XMUtil.gyp` (`xmutil_test` `sources`: add the test file)

**Implementation notes:**

`XmileFunctions.h` declares the `xmile::LookupFunction`, `xmile::LookupBareKeyword`, and `xmile::ReorderArgs` free functions described above. `XmileFunctions.cpp` defines:

1. A `static const FuncMapping kFuncMap[]` table mirroring `writer.rs:194-216`. Use a simple struct `{ const char *xmile; const char *vensim; }`. **Do not introduce `safediv` in this table** — it's special-cased in `LookupFunction` for arg-count dispatch.

2. A `static const KeywordMapping kKeywordMap[]` table mirroring `writer.rs:181-189`:
   ```
   { "time",         "Time"         }
   { "dt",           "TIME STEP"    }
   { "time_step",    "TIME STEP"    }
   { "starttime",    "INITIAL TIME" }
   { "initial_time", "INITIAL TIME" }
   { "endtime",      "FINAL TIME"   }
   { "stoptime",     "FINAL TIME"   }
   { "final_time",   "FINAL TIME"   }
   ```

3. `LookupBareKeyword(sns, xmileName)`:
   - Lower-case the input (use a local `std::string` copy and `std::transform` with `tolower`).
   - Linear scan `kKeywordMap`; on hit, do `SymbolNameSpace::Find(vensimVar)`. If found and dynamic-castable to `Variable*`, return it. If absent, construct a new `Variable(sns, vensimVar)` — this registers it in the namespace. Return the resulting `Variable*`.
   - On miss, return NULL.

4. `LookupFunction(sns, xmileName, argCount)`:
   - Lower-case the input.
   - If `xmileName == "safediv"`: dispatch to `"ZIDZ"` (argCount == 2), `"XIDZ"` (argCount == 3), or NULL with no logged error (let the caller produce a positioned error if it matters).
   - Linear scan `kFuncMap`. On hit: `SymbolNameSpace::Find(vensimName)`, return as `Function*` if castable.
   - Fallback: build `vensimName = uppercase(replace(xmileName, '_', ' '))`. `Find` and return.
   - On any miss, return NULL.

5. `ReorderArgs(vensimName, args)`:
   - For `"DELAY N"` and `"SMOOTH N"` (4 args): swap args 2 and 3 (XMILE `input, dt, n, init` → Vensim `input, dt, init, n`).
   - For `"RANDOM NORMAL"` (5 args): rotate XMILE `(mean, sd, seed, min, max)` to Vensim `(min, max, mean, sd, seed)`.
   - All other names: no-op.
   - `ExpressionList` exposes the necessary access — see `src/Symbol/ExpressionList.h`. Use indexed access to swap.

`test/xmile/XmileFunctionsTest.cpp`:

```cpp
#include <string>
#include "../../src/Function/Function.h"
#include "../../src/Model.h"
#include "../../src/Symbol/SymbolNameSpace.h"
#include "../../src/Symbol/Variable.h"
#include "../../src/Vensim/VensimParse.h"  // for ReadyFunctions side effects
#include "../../src/Xmile/XmileFunctions.h"
#include "../TestHarness.h"

namespace {
// Helper: a fresh Model with functions registered, ready for lookup tests.
struct Fixture {
  Model m;
  VensimParse vp;  // ctor runs ReadyFunctions and sets VPObject
  Fixture() : vp(&m) {}
};
}  // namespace

TEST(XmileFunctions_bare_keyword_time_creates_Time_variable) {
  Fixture f;
  Variable *v = xmile::LookupBareKeyword(f.m.GetNameSpace(), "time");
  CHECK(v != nullptr);
  if (v) CHECK_EQ_STR(v->GetName(), "Time");
}

TEST(XmileFunctions_bare_keyword_dt_maps_to_TIME_STEP) {
  Fixture f;
  Variable *v = xmile::LookupBareKeyword(f.m.GetNameSpace(), "dt");
  CHECK(v != nullptr);
  if (v) CHECK_EQ_STR(v->GetName(), "TIME STEP");
}

TEST(XmileFunctions_bare_keyword_initial_time_maps_to_INITIAL_TIME) {
  Fixture f;
  Variable *v = xmile::LookupBareKeyword(f.m.GetNameSpace(), "initial_time");
  CHECK(v != nullptr);
  if (v) CHECK_EQ_STR(v->GetName(), "INITIAL TIME");
}

TEST(XmileFunctions_bare_keyword_unknown_returns_null) {
  Fixture f;
  Variable *v = xmile::LookupBareKeyword(f.m.GetNameSpace(), "totally_made_up");
  CHECK(v == nullptr);
}

TEST(XmileFunctions_function_smth1_maps_to_SMOOTH) {
  Fixture f;
  Function *fn = xmile::LookupFunction(f.m.GetNameSpace(), "smth1", 2);
  CHECK(fn != nullptr);
  if (fn) CHECK_EQ_STR(fn->GetName(), "SMOOTH");
}

TEST(XmileFunctions_function_safediv_2arg_is_ZIDZ) {
  Fixture f;
  Function *fn = xmile::LookupFunction(f.m.GetNameSpace(), "safediv", 2);
  CHECK(fn != nullptr);
  if (fn) CHECK_EQ_STR(fn->GetName(), "ZIDZ");
}

TEST(XmileFunctions_function_safediv_3arg_is_XIDZ) {
  Fixture f;
  Function *fn = xmile::LookupFunction(f.m.GetNameSpace(), "safediv", 3);
  CHECK(fn != nullptr);
  if (fn) CHECK_EQ_STR(fn->GetName(), "XIDZ");
}

TEST(XmileFunctions_function_int_maps_to_INTEGER) {
  Fixture f;
  Function *fn = xmile::LookupFunction(f.m.GetNameSpace(), "int", 1);
  CHECK(fn != nullptr);
  if (fn) CHECK_EQ_STR(fn->GetName(), "INTEGER");
}

TEST(XmileFunctions_function_fallback_uppercase_underscores) {
  Fixture f;
  // "if_then_else" -> "IF THEN ELSE" via the fallback (no entry in kFuncMap)
  Function *fn = xmile::LookupFunction(f.m.GetNameSpace(), "if_then_else", 3);
  CHECK(fn != nullptr);
  if (fn) CHECK_EQ_STR(fn->GetName(), "IF THEN ELSE");
}

TEST(XmileFunctions_function_unknown_returns_null) {
  Fixture f;
  Function *fn = xmile::LookupFunction(f.m.GetNameSpace(), "completely_unknown_fn", 1);
  CHECK(fn == nullptr);
}
```

**Verification:**
- `./configure.sh` succeeds (gyp regen).
- `ninja -C out/Debug xmutil_test` succeeds.
- `out/Debug/xmutil_test` passes all 10 new tests.

**Commit:** `feat(xmile): XmileFunctions un-rename table for XMILE -> Vensim names`
<!-- END_TASK_1 -->

<!-- START_TASK_2 -->
### Task 2: XmileEqLex — hand-written equation lexer

**Verifies:** AC2.6 (operator-spelling parity at token level); contributes to AC2.1, AC2.2, AC2.4, AC2.5, AC2.7, AC2.9 (tokens consumed by Task 3's grammar).

**Files:**
- Create: `src/Xmile/XmileEqLex.h`
- Create: `src/Xmile/XmileEqLex.cpp`
- Modify: `XMUtil.gyp` (`common_sources`: add the two entries)

**Implementation notes:**

The lexer mirrors `VensimLex.{h,cpp}` structure but is much smaller — XMILE equations are simpler than full MDL files. There are no comments, no `:KEYWORD:` syntax (XMILE uses bare words), no quoted-vs-unquoted distinction beyond `"..."` quoting, no tabbed-array literal, no equation-end marker (the input string IS one equation).

```cpp
// XmileEqLex.h
#pragma once
#include <cstddef>
#include <string>
class XmileReader;

class XmileEqLex {
public:
  XmileEqLex();
  ~XmileEqLex();
  void Initialize(const char *text, std::size_t len);
  int yylex();              // returns next XPTT_* token; updates xpyylval
  int LineNumber() const { return iLineNumber; }
  int ColumnNumber() const { return iCurPos - iLineStart; }
  // For error messages:
  std::string Snippet() const;

private:
  unsigned char GetNextChar(bool advanceLine);
  void PushBack(unsigned char c);
  int ScanNumber();         // populates xpyylval.num
  int ScanIdentOrKeyword();  // populates xpyylval.sym for non-keywords; returns
                             // a token for keywords (XPTT_if, XPTT_and, ...)
  int MaybeColonNa();        // matches :NA: or :na:, returns XPTT_number with -1e38
  const char *pText;
  std::size_t iCurPos, iLength;
  std::size_t iLineStart;
  int iLineNumber;
  std::string sToken;        // accumulator for ident/number
};
```

The `yylex()` driver:
1. Skip whitespace.
2. Peek one character:
   - `0` → return 0 (EOF / parser accept).
   - Letter or `_` → `ScanIdentOrKeyword`.
   - Digit or `.` followed by digit → `ScanNumber`.
   - `"` → scan quoted name into `sToken`, return `XPTT_symbol`.
   - `:` → look ahead for `:na:` / `:NA:` (case-insensitive); if matched, return `XPTT_number` with `xpyylval.num = -1e38`. Otherwise, return single `:` (used in subscript syntax `*:Dim`, `l:r`).
   - `=` → look ahead for `==` (also `=`). Return `XPTT_eq`.
   - `<` → look ahead `<=` → `XPTT_lte`; `<>` → `XPTT_neq`; else `XPTT_lt`.
   - `>` → look ahead `>=` → `XPTT_gte`; else `XPTT_gt`.
   - `!` → look ahead `!=` → `XPTT_neq`; else `XPTT_not`.
   - `&` → require `&&`, return `XPTT_and`. Single `&` is an error.
   - `|` → require `||`, return `XPTT_or`. Single `|` is an error.
   - `%` → return `XPTT_mod`.
   - `/` → look ahead `//` → `XPTT_safediv`; else single `/`.
   - `+ - * ^ ( ) [ ] , @` → single-char tokens (raw ASCII code).
   - `'` → return `XPTT_apostrophe` (the grammar emits a parse error).
   - Anything else → emit "unexpected character" to the parser error channel.

`ScanIdentOrKeyword`:
- Accumulate `[a-zA-Z0-9_]+` into `sToken`.
- Case-insensitively match `if`, `then`, `else`, `and`, `or`, `not`, `mod`. Return corresponding `XPTT_*` token.
- Otherwise, intern `sToken` and set `xpyylval.sym = <interned C-string>`. The interning lives on the lexer instance (a `std::vector<std::string>` for buffer lifetime). Return `XPTT_symbol`.

`ScanNumber`:
- Accumulate digits, optional `.` + fractional, optional `[eE][+-]?digits`.
- `xpyylval.num = atof(sToken.c_str())`. Return `XPTT_number`.

**Quoted identifiers**: XMILE allows double-quoted names with arbitrary internal characters (escape rules per XMILE spec — for v1 we accept `"..."`-delimited content as a literal). Strip the surrounding quotes for the interned symbol value; tokenize as `XPTT_symbol`.

`xpyylval` is a process-global `ParseUnion` (declared by bison in `XmileEqYacc.tab.hpp` via `#define yylval xpyylval`). The lexer needs `XmileEqYacc.tab.hpp` to define it, but during Task 2 (before Task 3 generates the grammar artifacts) we forward-declare an `extern ParseUnion xpyylval;` and `int xpyylex(void);` to keep the lexer compilable in isolation. Task 3 supplies the real declarations via the generated header.

**Verification:**
- `ninja -C out/Debug xmutil_test` succeeds. (Lexer is compiled but exercised only via Task 3+.)

**Commit:** `feat(xmile): XmileEqLex hand-written equation lexer`
<!-- END_TASK_2 -->

<!-- END_SUBCOMPONENT_A -->

<!-- START_SUBCOMPONENT_B (tasks 3-4) -->

<!-- START_TASK_3 -->
### Task 3: XmileEqYacc.y grammar + committed .tab.{cpp,hpp} + action bridge

**Verifies:** AC2.1, AC2.4 (`if/then/else`), AC2.5 (subscripts), AC2.6, AC2.7, AC2.9. Tied off with tests in Task 5.

**Files:**
- Create: `src/Xmile/XmileEqYacc.y` (bison grammar source)
- Create: `src/Xmile/XmileParseFunctions.h`
- Create: `src/Xmile/XmileParseFunctions.cpp` (action-code bridge)
- Generate and commit: `src/Xmile/XmileEqYacc.tab.cpp` and `src/Xmile/XmileEqYacc.tab.hpp`
- Modify: `XMUtil.gyp` (`common_sources`: add the four entries — `.y`, `.tab.cpp`, `.tab.hpp`, `XmileParseFunctions.{h,cpp}`)

**Implementation notes:**

1. Write `XmileEqYacc.y` following the structure in the "Bison grammar shape" section above. The bison rename prefix is `xpyy` (parallel to Vensim's `vpyy`, Dynamo's `dpyy`). The grammar source declares `YYSTYPE ParseUnion` (the same union the other grammars use, defined in `src/Symbol/Parse.h`).

2. `XmileParseFunctions.h` declares the action shims:
   ```cpp
   #pragma once
   #include <string>
   class Expression;
   class ExpressionList;
   class SymbolList;

   void xpyy_set_result(Expression *e);
   void xpyyerror(const char *s);

   Expression *xpyy_num(double n);
   Expression *xpyy_resolve_symbol(const char *name, SymbolList *subs);
   Expression *xpyy_binop(int op, Expression *l, Expression *r);
   Expression *xpyy_unary(int op, Expression *e);
   Expression *xpyy_logical(int op, Expression *l, Expression *r);  // for '<' '>' '='
   Expression *xpyy_logical_le(Expression *l, Expression *r);       // emits with VPTT_le code
   Expression *xpyy_logical_ge(Expression *l, Expression *r);
   Expression *xpyy_logical_ne(Expression *l, Expression *r);
   Expression *xpyy_logical_and(Expression *l, Expression *r);
   Expression *xpyy_logical_or(Expression *l, Expression *r);
   Expression *xpyy_logical_unary(int op, Expression *operand);     // VPTT_not
   Expression *xpyy_safediv(Expression *l, Expression *r);          // AC2.7: ZIDZ
   Expression *xpyy_function(const char *vensimName, Expression *a, Expression *b);  // for XPTT_mod -> MODULO(l,r)
   Expression *xpyy_if(Expression *c, Expression *t, Expression *e);
   Expression *xpyy_call(const char *xmileName, ExpressionList *args);  // un-rename + Function* lookup
   ExpressionList *xpyy_arglist(ExpressionList *prev, Expression *e);

   SymbolList *xpyy_sub_init(SymbolList *first);
   SymbolList *xpyy_sub_append(SymbolList *list, SymbolList *next);
   SymbolList *xpyy_sub_name(const char *name);
   SymbolList *xpyy_sub_star(const char *bound);   // bound NULL for plain '*'
   SymbolList *xpyy_sub_range(const char *lo, const char *hi);
   SymbolList *xpyy_sub_index(int idx);

   int xpyylex();  // entry the grammar calls; reads from XPObject->CurrentLex()
   ```

3. `XmileParseFunctions.cpp` implementations:
   - `xpyy_set_result`: stash the Expression on `XPObject` (add an `Expression *_lastParsedExpr` member to `XmileReader` and a setter).
   - `xpyyerror`: append a descriptive error to the active `errs` vector (also on `XPObject`).
   - `xpyy_num`: `new ExpressionNumber(XPObject->GetSymbolNameSpace(), n)`.
   - `xpyy_resolve_symbol`: first try `xmile::LookupBareKeyword`; on hit, wrap the resulting `Variable*` in `new ExpressionVariable(sns, var, subs)` (the `subs` is the second arg from the grammar). On miss, `Variable *v = XPObject->InsertVariable(name)` (create-if-absent) and wrap.
   - `xpyy_binop`: replicate `VensimParse::OperatorExpression` switch for `'+'`, `'-'`, `'*'`, `'/'`, `'^'` — `new ExpressionAdd / Subtract / Multiply / Divide / Power(sns, l, r)`.
   - `xpyy_unary`: for `'-'`: if the operand is `ExpressionNumber`, in-place flip the sign and return the same node (match `VensimParse::OperatorExpression` line ~582). Otherwise `new ExpressionUnaryMinus(sns, e, NULL)`.
   - `xpyy_logical(int op, ...)`: `new ExpressionLogical(sns, l, r, op)` (op is `'<' '>' '='`).
   - `xpyy_logical_le / ge / ne`: same constructor but with `VPTT_le / ge / ne` codes — `#include "../Vensim/VYacc.tab.hpp"` to get the constants.
   - `xpyy_logical_and / or`: same with `VPTT_and / or`.
   - `xpyy_logical_unary(VPTT_not, operand)`: `new ExpressionLogical(sns, NULL, operand, VPTT_not)`.
   - `xpyy_safediv`: AC2.7 path — build a 2-element arg list `(l, r)` and `new ExpressionFunction(sns, zidz, args)` where `zidz = xmile::LookupFunction(sns, "safediv", 2)`. If `zidz` is NULL (function missing — should not happen), append an error to `errs` and return a 0-numeric placeholder so the parse continues.
   - `xpyy_function(name, a, b)`: build args, look up `name` via `SymbolNameSpace::Find` (not the un-rename — `name` is already the canonical Vensim name; XMILE's `%` is hard-coded to map to `MODULO`). Build `ExpressionFunction` or `ExpressionFunctionMemory` based on `f->IsMemoryless()`.
   - `xpyy_if`: 3-arg list `(c, t, e)`, look up `"IF THEN ELSE"`, build `ExpressionFunction`.
   - `xpyy_call(xmileName, args)`:
     1. If `xmileName` matches an empty-paren no-op family (XMILE allows bare `time()` with empty parens — rare): handle via `LookupBareKeyword` first. (Skip if not in corpus; treat as ordinary function call.)
     2. Compute `argCount = args ? args->size() : 0`.
     3. `Function *f = xmile::LookupFunction(sns, xmileName, argCount)`. NULL → push error to `errs` ("unknown function 'X'"), return a 0-numeric placeholder.
     4. `xmile::ReorderArgs(f->GetName(), args)`.
     5. Validate `argCount` matches `f->NumberArgs()` (allowing negative arity for variadic). On mismatch, push error and continue.
     6. Return `f->IsMemoryless() ? new ExpressionFunction(sns, f, args) : new ExpressionFunctionMemory(sns, f, args)`.
   - `xpyy_arglist`: standard list-chain helper; mirror `VensimParse::ChainExpressionList` shape — `new ExpressionList(XPObject->GetSymbolNameSpace())` on a NULL prev, else append. (`ExpressionList`'s ctor takes a `SymbolNameSpace*`; there is no default ctor.)
   - `xpyy_sub_*`: build `SymbolList` instances. Skim `src/Symbol/SymbolList.h` for the construction API. Star, range, and index forms map to `SymbolList` with the documented `bang`/`mapping` flags Vensim's grammar uses.
   - `xpyylex`: trivial shim `return XPObject->CurrentLex()->yylex();`. `CurrentLex()` is a new accessor on `XmileReader` exposing the lex pointer the active `ParseEquation` call set.

4. Update `XmileReader` (existing from Phase 1) with the additions:
   - `Expression *_lastParsedExpr = nullptr;`
   - `XmileEqLex *_currentLex = nullptr;`
   - `std::vector<std::string> *_currentErrs = nullptr;` (to bridge xpyyerror)
   - `void SetLastParsedExpr(Expression *e) { _lastParsedExpr = e; }`
   - `XmileEqLex *CurrentLex() { return _currentLex; }`
   - `std::vector<std::string> &CurrentErrs() { return *_currentErrs; }`
   - `Variable *InsertVariable(const std::string &name)`: mirror `VensimParse::InsertVariable` — `Find`, dynamic_cast, or `new Variable(sns, name)`.
   - `Variable *FindVariable(const std::string &name)`: same without the create.

5. Regenerate the bison artifacts:
   ```bash
   cd src/Xmile
   bison -o XmileEqYacc.tab.cpp -p xpyy -d XmileEqYacc.y
   ```
   This produces `XmileEqYacc.tab.cpp` and `XmileEqYacc.tab.hpp`. Commit both. The build system does **not** run bison — these are checked-in artifacts (mirroring `src/Vensim/VYacc.tab.cpp` and `src/Dynamo/DYacc.tab.cpp`). Confirm `bison --version` ≥ 3.0 in CI / on the dev machine before generating.

6. Verify the prefix worked: `grep -c xpyyparse XmileEqYacc.tab.cpp` should be > 0, and there should be no bare `yyparse` in the output.

7. `XMUtil.gyp` `common_sources` additions (after Phase 1's `XmileReader.{h,cpp}` and Phase 2 Task 1's `XmileFunctions.{h,cpp}`):
   ```
   './src/Xmile/XmileEqLex.h',
   './src/Xmile/XmileEqLex.cpp',
   './src/Xmile/XmileEqYacc.tab.cpp',
   './src/Xmile/XmileEqYacc.tab.hpp',
   './src/Xmile/XmileEqYacc.y',
   './src/Xmile/XmileParseFunctions.h',
   './src/Xmile/XmileParseFunctions.cpp',
   ```

**Verification:**
- `bison --version` ≥ 3.0.
- `bison` succeeds with no warnings beyond the grammar's known `precedence` declarations.
- `./configure.sh && ninja -C out/Debug xmutil_test` succeeds.
- Existing tests still pass.

**Commit:** `feat(xmile): XMILE equation grammar (XmileEqYacc.y + parse functions)`
<!-- END_TASK_3 -->

<!-- START_TASK_4 -->
### Task 4: XmileReader::ParseEquation driver

**Verifies:** Surface integration of Tasks 1-3 — exposes a public entry the rest of the implementation (Phase 3+) calls. Tested via Task 5.

**Files:**
- Modify: `src/Xmile/XmileReader.h` (add `ParseEquation` declaration, lex/errs/result accessors)
- Modify: `src/Xmile/XmileReader.cpp` (implement)

**Implementation notes:**

```cpp
// In XmileReader.h, public section:
Expression *ParseEquation(const std::string &text, std::vector<std::string> &errs);

// In XmileReader.cpp:
extern "C" int xpyyparse(void);

Expression *XmileReader::ParseEquation(const std::string &text, std::vector<std::string> &errs) {
  XmileEqLex lex;
  lex.Initialize(text.c_str(), text.size());
  _currentLex = &lex;
  _currentErrs = &errs;
  _lastParsedExpr = nullptr;

  int rc = xpyyparse();

  _currentLex = nullptr;
  _currentErrs = nullptr;

  if (rc != 0) {
    if (errs.empty()) {
      errs.push_back("parse error in equation: " + text);
    }
    // Free any partial Expression to avoid leaks.
    delete _lastParsedExpr;
    _lastParsedExpr = nullptr;
    return nullptr;
  }
  Expression *result = _lastParsedExpr;
  _lastParsedExpr = nullptr;
  return result;
}
```

The `xpyyparse` symbol is the bison-generated entry (renamed via `-p xpyy`). The lex instance is stack-local; `_currentLex` is a borrowed pointer valid only inside this call. `_currentErrs` similarly borrows; `xpyyerror` accesses it via `XPObject->CurrentErrs()`.

**Verification:**
- `ninja -C out/Debug xmutil_test` succeeds.
- Existing tests still pass.

**Commit:** `feat(xmile): XmileReader::ParseEquation driver`
<!-- END_TASK_4 -->

<!-- END_SUBCOMPONENT_B -->

<!-- START_SUBCOMPONENT_C (tasks 5-5) -->

<!-- START_TASK_5 -->
### Task 5: EquationParseTest — structural Expression-tree golden tests

**Verifies:** AC2.1, AC2.2, AC2.3, AC2.4, AC2.5, AC2.6, AC2.7, AC2.8, AC2.9.

**Files:**
- Create: `test/xmile/EquationParseTest.cpp`
- Modify: `XMUtil.gyp` (`xmutil_test` `sources`: add the test file)

**Implementation notes:**

These tests do **not** rely on `Expression::OutputComputable` (which needs a fully-built `ContextInfo` with LHS variable + pipeline context). Instead they walk the Expression tree by `GetType()` + `dynamic_cast` and CHECK each node's shape.

A small helper covers the common case of testing the operator code stored on an `ExpressionLogical`:

```cpp
namespace {

// Returns the int oper code (e.g. '<', VPTT_le, VPTT_and) from an
// ExpressionLogical node, or -1 if the cast fails. We could use
// dynamic_cast + a virtual method, but ExpressionLogical doesn't currently
// expose its operator. The test reaches into the public LogicalOperator()
// accessor on ExpressionLogical (see src/Symbol/Expression.h).
int LogicalOp(Expression *e) {
  ExpressionLogical *l = dynamic_cast<ExpressionLogical *>(e);
  return l ? l->LogicalOperator() : -1;
}

// Setup boilerplate: a Model with functions registered (via VensimParse) and
// an XmileReader bound to it. The ctor body constructs a transient VensimParse
// to run ReadyFunctions (registering INTEG, IF THEN ELSE, etc. in the
// namespace); when the local vp goes out of scope at the closing brace, its
// destructor clears VPObject so the XmileReader ctor's single-instance
// assertion on XPObject passes. Member init order matters: m before reader.
struct Fixture {
  Model m;
  XmileReader reader;
  Fixture() : reader(&m) {
    // Tear-down ordering: vp dtor must run before reader is "fully" usable.
    // The braced scope below ensures that — vp dtor runs at the inner '}'.
    { VensimParse vp(&m); }
    // The function-registration side effect (ReadyFunctions inside vp's ctor)
    // persists on m's SymbolNameSpace after vp is destroyed.
  }
};
// NOTE: once Phase 4's RegisterXmutilFunctions refactor lands, XmileReader's
// ctor will call function registration directly (with a guard against double
// registration), and the VensimParse-first dance can be removed. See
// Phase 4 Task 2 for the planned refactor.

}  // namespace

TEST(XmileParse_addition_builds_ExpressionAdd) {
  Model m;
  { VensimParse vp(&m); }  // register functions, release VPObject
  XmileReader reader(&m);
  std::vector<std::string> errs;
  Expression *e = reader.ParseEquation("1 + 2", errs);
  CHECK(e != nullptr); CHECK(errs.empty());
  CHECK(dynamic_cast<ExpressionAdd *>(e) != nullptr);
  delete e;
}

TEST(XmileParse_subtraction_builds_ExpressionSubtract) {
  Model m; { VensimParse vp(&m); } XmileReader reader(&m);
  std::vector<std::string> errs;
  Expression *e = reader.ParseEquation("3 - 4", errs);
  CHECK(dynamic_cast<ExpressionSubtract *>(e) != nullptr);
  delete e;
}

TEST(XmileParse_multiplication_division_power) {
  Model m; { VensimParse vp(&m); } XmileReader reader(&m);
  std::vector<std::string> errs;
  Expression *mul = reader.ParseEquation("2 * 3", errs);
  Expression *div = reader.ParseEquation("6 / 2", errs);
  Expression *pow = reader.ParseEquation("2 ^ 3", errs);
  CHECK(dynamic_cast<ExpressionMultiply *>(mul) != nullptr);
  CHECK(dynamic_cast<ExpressionDivide *>(div) != nullptr);
  CHECK(dynamic_cast<ExpressionPower *>(pow) != nullptr);
  delete mul; delete div; delete pow;
}

TEST(XmileParse_precedence_a_plus_b_times_c) {
  // Per simlin precedence: '*' binds tighter than '+', so a + b * c
  // parses as Add(a, Multiply(b, c)).
  Model m; { VensimParse vp(&m); } XmileReader reader(&m);
  std::vector<std::string> errs;
  Expression *e = reader.ParseEquation("a + b * c", errs);
  ExpressionAdd *add = dynamic_cast<ExpressionAdd *>(e);
  CHECK(add != nullptr);
  if (add) {
    CHECK(dynamic_cast<ExpressionMultiply *>(add->GetArg(1)) != nullptr);
  }
  delete e;
}
// (GetArg(0) and GetArg(1) are existing accessors on the EO2 hierarchy; see
// Expression.h's macros around line 560.)

TEST(XmileParse_function_smth1_resolves_to_SMOOTH) {
  Model m; { VensimParse vp(&m); } XmileReader reader(&m);
  std::vector<std::string> errs;
  Expression *e = reader.ParseEquation("smth1(input, 5)", errs);
  ExpressionFunctionMemory *fn = dynamic_cast<ExpressionFunctionMemory *>(e);
  CHECK(fn != nullptr);
  if (fn) CHECK_EQ_STR(fn->GetFunction()->GetName(), "SMOOTH");
  delete e;
}

TEST(XmileParse_if_then_else_builds_IF_THEN_ELSE) {
  Model m; { VensimParse vp(&m); } XmileReader reader(&m);
  std::vector<std::string> errs;
  Expression *e = reader.ParseEquation("if x > 0 then 1 else -1", errs);
  ExpressionFunction *fn = dynamic_cast<ExpressionFunction *>(e);
  CHECK(fn != nullptr);
  if (fn) CHECK_EQ_STR(fn->GetFunction()->GetName(), "IF THEN ELSE");
  delete e;
}

TEST(XmileParse_bare_time_resolves_to_Time_Variable) {
  Model m; { VensimParse vp(&m); } XmileReader reader(&m);
  std::vector<std::string> errs;
  Expression *e = reader.ParseEquation("time", errs);
  ExpressionVariable *var = dynamic_cast<ExpressionVariable *>(e);
  CHECK(var != nullptr);
  if (var) CHECK_EQ_STR(var->GetVariable()->GetName(), "Time");
  delete e;
}

TEST(XmileParse_bare_dt_resolves_to_TIME_STEP_Variable) {
  Model m; { VensimParse vp(&m); } XmileReader reader(&m);
  std::vector<std::string> errs;
  Expression *e = reader.ParseEquation("dt", errs);
  ExpressionVariable *var = dynamic_cast<ExpressionVariable *>(e);
  CHECK(var != nullptr);
  if (var) CHECK_EQ_STR(var->GetVariable()->GetName(), "TIME STEP");
  delete e;
}

TEST(XmileParse_subscripted_variable_x_a_b) {
  Model m; { VensimParse vp(&m); } XmileReader reader(&m);
  std::vector<std::string> errs;
  Expression *e = reader.ParseEquation("x[a, b]", errs);
  ExpressionVariable *var = dynamic_cast<ExpressionVariable *>(e);
  CHECK(var != nullptr);
  if (var) {
    SymbolList *subs = var->GetSubList();
    CHECK(subs != nullptr);
    // The list contains two named subs. (See SymbolList.h for the iteration
    // API; the test asserts size == 2 and entry names match "a" and "b".)
  }
  delete e;
}

TEST(XmileParse_keyword_and_matches_C_style_amp_amp) {
  Model m; { VensimParse vp(&m); } XmileReader reader(&m);
  std::vector<std::string> errs;
  Expression *e1 = reader.ParseEquation("a and b", errs);
  Expression *e2 = reader.ParseEquation("a && b", errs);
  CHECK(LogicalOp(e1) == LogicalOp(e2));
  CHECK(LogicalOp(e1) > 0);  // i.e., VPTT_and value imported from VYacc.tab.hpp
  delete e1; delete e2;
}

TEST(XmileParse_keyword_or_matches_C_style_pipe_pipe) {
  Model m; { VensimParse vp(&m); } XmileReader reader(&m);
  std::vector<std::string> errs;
  Expression *e1 = reader.ParseEquation("a or b", errs);
  Expression *e2 = reader.ParseEquation("a || b", errs);
  CHECK(LogicalOp(e1) == LogicalOp(e2));
  delete e1; delete e2;
}

TEST(XmileParse_keyword_not_matches_bang) {
  Model m; { VensimParse vp(&m); } XmileReader reader(&m);
  std::vector<std::string> errs;
  Expression *e1 = reader.ParseEquation("not a", errs);
  Expression *e2 = reader.ParseEquation("!a", errs);
  CHECK(LogicalOp(e1) == LogicalOp(e2));
  delete e1; delete e2;
}

TEST(XmileParse_safediv_two_args_uses_ZIDZ) {
  Model m; { VensimParse vp(&m); } XmileReader reader(&m);
  std::vector<std::string> errs;
  // The '//' operator form lowers to safediv-then-ZIDZ.
  Expression *e = reader.ParseEquation("a // b", errs);
  ExpressionFunction *fn = dynamic_cast<ExpressionFunction *>(e);
  CHECK(fn != nullptr);
  if (fn) CHECK_EQ_STR(fn->GetFunction()->GetName(), "ZIDZ");
  delete e;
}

TEST(XmileParse_safediv_three_args_uses_XIDZ) {
  Model m; { VensimParse vp(&m); } XmileReader reader(&m);
  std::vector<std::string> errs;
  Expression *e = reader.ParseEquation("safediv(a, b, 0)", errs);
  ExpressionFunction *fn = dynamic_cast<ExpressionFunction *>(e);
  CHECK(fn != nullptr);
  if (fn) CHECK_EQ_STR(fn->GetFunction()->GetName(), "XIDZ");
  delete e;
}

TEST(XmileParse_NaN_literal_maps_to_minus_1e38) {
  Model m; { VensimParse vp(&m); } XmileReader reader(&m);
  std::vector<std::string> errs;
  Expression *e = reader.ParseEquation(":NA:", errs);
  ExpressionNumber *n = dynamic_cast<ExpressionNumber *>(e);
  CHECK(n != nullptr);
  if (n) CHECK(n->GetValue() == -1e38);
  delete e;
}

TEST(XmileParse_unknown_function_errors) {
  Model m; { VensimParse vp(&m); } XmileReader reader(&m);
  std::vector<std::string> errs;
  Expression *e = reader.ParseEquation("completely_unknown_function(1, 2)", errs);
  CHECK(e == nullptr || !errs.empty());
  delete e;
}

TEST(XmileParse_postfix_apostrophe_errors) {
  Model m; { VensimParse vp(&m); } XmileReader reader(&m);
  std::vector<std::string> errs;
  Expression *e = reader.ParseEquation("a'", errs);
  CHECK(e == nullptr);
  CHECK(!errs.empty());
  delete e;
}
```

Tests for `:=`, `MOD`, `%`, equality `=`/`==`, inequality `<>`/`!=` follow the same shape as the keyword-vs-C-style pair tests above.

If `ExpressionLogical::LogicalOperator()` is not currently public on the class, also expose it (add a `public:` `int LogicalOperator() const { return mOper; }` accessor in `Expression.h` if absent). The codebase investigator's report confirms `LogicalOperator()` already exists on the class.

**Verification:**
- `ninja -C out/Debug xmutil_test` succeeds.
- `out/Debug/xmutil_test` passes all new EquationParseTest cases (~18 tests).
- Existing tests still pass.

**Commit:** `test(xmile): equation parse structural tests`
<!-- END_TASK_5 -->

<!-- END_SUBCOMPONENT_C -->

---

## Phase 2 done when

- `bison` regenerates `XmileEqYacc.tab.{cpp,hpp}` cleanly from the committed `.y`.
- `./configure.sh && ninja -C out/Debug XMUtil && ninja -C out/Debug xmutil_test` succeed.
- All new tests in `XmileFunctionsTest.cpp` (Task 1, 10 tests) and `EquationParseTest.cpp` (Task 5, ~18 tests) pass.
- Existing MDL test suite continues to pass.
- `./format.sh` is clean on the new `src/Xmile/*.{h,cpp}` files. Format the test files manually.

## Out of scope for Phase 2

- DOM-level `<eqn>` element walking (Phase 3+ — only the equation *text* parser exists here).
- Stock / flow synthesis (Phase 4).
- `<dimensions>` and per-element subscripted equations (Phase 5).
- WITH LOOKUP / standalone `<gf>` (Phase 6).
- View / sketch (Phase 7).
- Two-pass parsing for forward references (XMILE's `<flow>` elements may reference each other; the equation parser is single-pass per equation, with `InsertVariable` create-if-absent ensuring lookups always succeed).
- Variable namespace cleanup of speculatively-created variables that turn out to be misspellings — out of scope; treat as Vensim does.
