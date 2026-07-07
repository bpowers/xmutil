# Vensim mdl Writer Design

## Summary

xmutil is a C++ tool that converts system dynamics models between formats. Today it reads
Vensim `.mdl` and Dynamo source into an in-memory `Model` and writes it out as XMILE
(`convert_mdl_to_xmile`). This work adds the missing direction: a writer that serializes that
same in-memory `Model` back to Vensim `.mdl` text. The result enables Vensim -> Vensim (a
round-trip that normalizes a model) and Dynamo -> Vensim conversion. There is deliberately no
XMILE reader, so XMILE -> Vensim stays out of scope.

The approach mirrors the existing XMILE writer rather than inventing a new pipeline. A new
`MDLGenerator` class consumes the `Model` after `MarkVariableTypes` has classified each
variable as a stock, flow, aux, or lookup -- the same prepared state the `XMILEGenerator`
already consumes -- and assembles the `.mdl` text section by section (equations, control/sim
specs, sketch diagram, settings, and optional macros). Because Vensim equation syntax differs
from XMILE, the generator includes its own expression walker that emits native Vensim function
names and operators with no XMILE-specific rewriting. A central constraint shapes the design:
xmutil cannot generate diagram geometry, so the writer only re-serializes a sketch that the
parser already captured (`VensimView`); models without one, such as those from Dynamo, emit no
sketch. Correctness is proven without needing Vensim itself: a test harness parses a corpus of
models, emits `.mdl`, re-parses, and compares the two `Model`s for structural equivalence.
Format-specific algorithms with no xmutil precedent (stock reconstruction, operator
parenthesization, identifier quoting, line wrapping, settings codes) are ported from simlin's
Rust `mdl/writer.rs`, used as a vetted reference.

## Definition of Done

### Deliverable
A new C++ `MDLGenerator` (in `src/Mdl/`) that serializes xmutil's in-memory `Model` to
Vensim `.mdl` text, plus:
- a `Model::PrintMDL(...)` method (sibling of `Model::PrintXMILE`),
- a new conversion entry point in `XMUtil.{h,cpp}`,
- a CLI flag in `Main.cpp` to emit `.mdl`,
- the **full Vensim sketch (diagram) section**, produced by *re-serializing* the parsed
  `VensimView` (see the hard constraint below).

Scope is **writer-only**: the input is xmutil's `Model` as populated by the existing Vensim
or Dynamo parser. This delivers **Vensim -> Vensim** (round-trip / normalize) and
**Dynamo -> Vensim**. It does **not** include an XMILE reader; XMILE -> Vensim is out of scope.

### Hard design constraint: no view generation
xmutil has **no view generation**. The mdl writer MUST NOT synthesize diagram geometry; it
only serializes an existing `VensimView`. Consequently:
- **Vensim -> Vensim:** re-emit the parsed sketch faithfully.
- **Dynamo -> Vensim:** no parsed sketch exists, so emit **no sketch section (or an empty
  view)**.

### Primary correctness bar (CI-testable, no Vensim required)
For a corpus of Vensim `.mdl` inputs, `parse -> emit -> re-parse` yields an **equivalent
`Model`**:
- the same set of variables, each with an equivalent equation AST and variable type
  (stock / flow / aux / lookup);
- the same subscripts/dimensions, units, and comments;
- the same sim specs (INITIAL TIME / FINAL TIME / TIME STEP / SAVEPER, integration method)
  and groups;
- **equivalent sketch geometry** for Vensim input (variables, valves, clouds, connectors,
  positions, polarity).

Equivalence is structural/semantic, **not byte-identical**.

For **Dynamo -> Vensim**, the bar is "emits valid `.mdl` that re-parses to an equivalent
`Model`" (no sketch comparison, since there is no/empty view).

### Out of scope (v1)
- XMILE -> Vensim, and any XMILE reader.
- Byte-for-byte reproduction of Vensim's output.

## Acceptance Criteria

### mdl-writer.AC1: Model serializes to Vensim mdl via a stable API
- **mdl-writer.AC1.1 Success:** `Model::PrintMDL(errs)` returns Vensim `.mdl` text for a parsed, `MarkVariableTypes`-processed `Model`.
- **mdl-writer.AC1.2 Success:** The C entry (e.g. `convert_to_mdl(source, len, fileName, ...)`) returns a caller-owned `.mdl` string, mirroring `convert_mdl_to_xmile`.
- **mdl-writer.AC1.3 Success:** The CLI emits a `.mdl` file (e.g. `xmutil --to-mdl model.mdl`, or selected by output extension).
- **mdl-writer.AC1.4 Failure:** On generation error, messages are collected in `errs`; `PrintMDL` returns an empty string and the C entry returns NULL.

### mdl-writer.AC2: Equations emit as valid Vensim
- **mdl-writer.AC2.1 Success:** Aux/flow variables emit as `name = <expr> ~ units ~ comment |`, with Vensim function names via `Function::GetName()` and no XMILE expansion (e.g. `PULSE`, `IF THEN ELSE`, `DELAY FIXED` round-trip unchanged).
- **mdl-writer.AC2.2 Success:** Stocks emit as `name = INTEG(<net_flow>, <init>)`, where `<net_flow>` is built from `Inflows()`/`Outflows()` and `<init>` from the stock's initial value.
- **mdl-writer.AC2.3 Success:** Lookups/graphical functions emit Vensim syntax `name([(xmin,ymin)-(xmax,ymax)],(x1,y1),...)` (standalone) and `WITH LOOKUP(input, (...))` (embedded).
- **mdl-writer.AC2.4 Success:** Dimension definitions emit `Dim: e1, e2, e3 ~~|`; subscripted variables emit per-element entries `name[elem] = ...` via `Equation::SubscriptExpand`.
- **mdl-writer.AC2.5 Edge:** Identifiers requiring quoting are quoted; numbers format correctly, including the `:NA:` sentinel for `-1e38`.
- **mdl-writer.AC2.6 Edge:** Equations longer than Vensim's line budget wrap with `\` continuations and re-parse to the same expression.

### mdl-writer.AC3: Round-trip preserves the model (primary bar)
- **mdl-writer.AC3.1 Success:** For each corpus model, `parse -> MarkVariableTypes -> M0`, `emit -> mdl'`, `parse -> MarkVariableTypes -> M1` yields the same variables with equivalent equation ASTs and variable types.
- **mdl-writer.AC3.2 Success:** Subscripts/dimensions, units, and comments are equivalent across the round-trip.
- **mdl-writer.AC3.3 Success:** Sim specs (initial/final/dt/saveper, integration method) and groups are equivalent across the round-trip.
- **mdl-writer.AC3.4 Success:** `VensimView` geometry retained by xmutil (variables, valves, clouds, connectors, positions, polarity) is equivalent across the round-trip.
- **mdl-writer.AC3.5 Guard:** The `Model` comparator detects and reports a deliberately introduced non-equivalence (round-trip tests cannot pass vacuously).

### mdl-writer.AC4: Sketch policy honors "no view generation"
- **mdl-writer.AC4.1 Success:** Vensim input with a sketch produces a sketch section that re-parses to an equivalent `VensimView`.
- **mdl-writer.AC4.2 Success:** Input without a `VensimView` (Dynamo or external consumer) produces no/empty sketch section and still parses.
- **mdl-writer.AC4.3 Constraint:** No diagram geometry is fabricated: variables absent from the original sketch are not assigned synthetic coordinates.

### mdl-writer.AC5: Dynamo to Vensim
- **mdl-writer.AC5.1 Success:** A `.dyn` model converts to valid `.mdl` that re-parses to an equivalent `Model` (no sketch comparison).

### mdl-writer.AC6: Macros
- **mdl-writer.AC6.1 Success:** A model with a Vensim macro emits a `:MACRO: ... :END OF MACRO:` block that re-parses to an equivalent macro. (Deferrable to a fast-follow; if deferred, macro-using models are excluded from the round-trip corpus until it lands.)

## Glossary

- **System dynamics**: A modeling approach where behavior emerges from stocks (accumulations) and the flows that change them over time. Vensim, Dynamo, and XMILE are all system dynamics tools/formats.
- **Vensim `.mdl`**: The native text file format of the Vensim simulation tool. Contains an equation section, a sketch (diagram) section delimited by `\\\---///`, and a trailing settings section. This document's writer produces this format.
- **XMILE**: An XML-based open interchange standard for system dynamics models. xmutil's existing output format, produced by `XMILEGenerator`.
- **Dynamo**: An older system dynamics modeling language. xmutil can parse `.dyn` source but Dynamo files carry no diagram, so Dynamo -> Vensim emits no sketch.
- **simlin**: A separate system dynamics project (vendored under `third_party/simlin`). Its Rust `mdl/writer.rs` serializes simlin's `datamodel` to Vensim `.mdl` and serves as the de-risked reference algorithm for Vensim-format details; its algorithms are ported, not its code.
- **Stock / flow / aux / lookup**: The variable kinds in a model. A *stock* (level) accumulates over time; *flows* (rates) add to or drain it; an *aux* (auxiliary) is an intermediate computed value; a *lookup* (graphical function / table function) maps an input to an output via a piecewise table.
- **`INTEG`**: Vensim's integration function. A stock is written `name = INTEG(net_flow, initial_value)`, where the net flow is the sum of inflows minus outflows. xmutil splits `INTEG` into separate flows on input and reconstructs it on output.
- **Subscripts / dimensions**: Vensim's array mechanism. A *dimension* (e.g. `Dim: e1, e2, e3`) defines a set of elements; a *subscripted* variable has one equation per element or per dimension. The writer expands these per element via `Equation::SubscriptExpand`.
- **Sketch**: The diagram portion of a `.mdl` file -- the visual layout of variables, valves, clouds, and connector arrows with their coordinates. Re-serialized from the parsed `VensimView`; never synthesized.
- **Valve / cloud / connector**: Sketch elements. A *valve* is the flow-rate control drawn on a flow pipe; a *cloud* is a source/sink at a flow's open end (a stock outside the model boundary); a *connector* is a causal arrow between elements, carrying a polarity (`+`/`-`) sign.
- **Polarity**: The `+`/`-` sign on a causal connector indicating whether the source increases or decreases the target.
- **Sim specs**: Simulation settings: `INITIAL TIME`, `FINAL TIME`, `TIME STEP`, `SAVEPER` (save interval), and the integration method (Euler, RK2, RK4). In Vensim these live in a `.Control` section.
- **Macro**: A Vensim user-defined function delimited by `:MACRO: ... :END OF MACRO:`, with its own local namespace of equations. Represented in xmutil as a `MacroFunction`.
- **`Model`**: xmutil's central in-memory representation of a parsed model: a `SymbolNameSpace` of variables/equations/expressions plus views, sim specs, groups, and unit equivalences. The writer's input.
- **`MarkVariableTypes`**: The `Model` transform that classifies each variable as stock/flow/aux/array and splits inline `INTEG` equations into discrete flows. Both the XMILE and `.mdl` writers run after this step, so equivalence is defined on the post-`MarkVariableTypes` state.
- **`SymbolNameSpace`**: xmutil's symbol table mapping names to `Variable`, `Equation`, `Expression`, and `Function` objects. A `Model` has a main one; each macro has its own local one.
- **`Expression`**: xmutil's AST node for the right-hand side of an equation. Subclasses (`ExpressionFunction`, `ExpressionVariable`, `ExpressionNumber`, `ExpressionLogical`, operator nodes, etc.) are distinguished by `GetType()` and walked recursively by the generator.
- **`OutputComputable`**: The existing `Expression`/`Variable` method that emits XMILE-flavored text -- underscored names, XMILE function names, and structural function expansions. The `.mdl` writer does **not** reuse it and instead adds a parallel Vensim walker.
- **`CF_xmile_output`**: The compute-type flag that gates XMILE-specific rewrites in `OutputComputable` (e.g. expanding `PULSE` into `IF...THEN...ELSE`). The Vensim walker performs none of these expansions, emitting builtins verbatim.
- **`Function::GetName()` vs `ComputableName()`**: A `Function` carries both its Vensim name (e.g. `IF THEN ELSE`, via `GetName()`) and its XMILE computable name (e.g. `IF`). The `.mdl` writer emits `GetName()`; the XMILE writer emits `ComputableName()`.
- **`ExpressionTable`**: The AST representation of a lookup's table of `(x, y)` pairs, exposing `GetXVals()`/`GetYVals()`. Emitted as Vensim graphical-function syntax (standalone) or `WITH LOOKUP(...)` (embedded).
- **`VensimView`**: xmutil's parsed representation of one sketch (a `View` subclass), holding `VensimViewElement`s (variable, valve, comment/cloud, connector) with coordinates and connector from/to links. The sole source of sketch geometry on output.
- **`ModelGroup` / groups**: Named partitions of variables in a model (`Variable::GetGroup()` / `Model::Groups()`), emitted in Vensim as `***...*** .Group ***...***` banners.
- **`:NA:` sentinel / `-1e38`**: Vensim's "not available" marker. xmutil's parser maps the `:NA:` token to the number `-1e38`; the writer must format that value back to `:NA:` on output.
- **CRLF**: Carriage-return + line-feed line endings (`\r\n`), which Vensim `.mdl` files use and the writer must emit.
- **`{UTF-8}` header**: The encoding marker on the first line of a Vensim `.mdl` file. The Phase 1 skeleton emits this plus empty sections as a minimal valid shell.
- **gyp / CMake**: The two build systems used by xmutil (`XMUtil.gyp` and a CMake configuration); the new files must be wired into both.
- **`ModelComparator`**: A test-only utility (introduced by this work) that checks two `Model`s for structural/semantic equivalence, used by the round-trip harness and as the guard against vacuously passing tests.

## Architecture

The mdl writer adds a Vensim `.mdl` serializer that mirrors the existing XMILE writer.
Today, `convert_mdl_to_xmile` (`src/XMUtil.cpp`) parses source into a `Model` (a
`SymbolNameSpace` of `Variable`/`Equation`/`Expression` plus `VensimView` diagrams), runs
`Model::MarkVariableTypes` to classify stocks/flows/auxes and split `INTEG` into flows, then
calls `Model::PrintXMILE` -> `XMILEGenerator`. The writer reuses this pipeline up to and
including `MarkVariableTypes`, then calls a new `Model::PrintMDL` -> `MDLGenerator`.

`MDLGenerator` (`src/Mdl/MDLGenerator.{h,cpp}`) consumes the post-`MarkVariableTypes` Model —
the same "XMILE-like" state `XMILEGenerator` consumes: variables typed
`XMILE_Type_STOCK`/`FLOW`/`AUX`/`ARRAY`; stocks carrying `Inflows()`/`Outflows()` plus an
initial value; lookups as `ExpressionTable`; dimensions as subscript-range variables; views
as `VensimView`. This input contract is deliberate: any C++ project that can populate an
xmutil `Model` from XMILE-like in-memory data can drive mdl generation, making the writer a
general "XMILE-like Model -> Vensim mdl" serializer (the C++ analog of simlin's
`datamodel -> mdl` writer, used here as a de-risked reference algorithm).

`MDLGenerator::Print` assembles, in Vensim order: optional `:MACRO:` blocks; the equation
section (dimension definitions, grouped/ungrouped variable entries, and the `.Control`
section); the `\\\---///` sketch section; and the `:L...` settings section. Output uses CRLF
line endings.

### Components and boundaries

- **`MDLGenerator`** (`src/Mdl/MDLGenerator.{h,cpp}`) — section assembly, mirroring
  `XMILEGenerator` method-for-method (`generateEquation`, `generateDimensions`,
  `generateControl`/sim-specs, `generateSketch`, settings).
- **Vensim expression walker** (inside `MDLGenerator`) — a recursive emitter that converts an
  `Expression` tree to Vensim text, dispatching on `Expression::GetType()`. Functions emit
  uniformly as `Function::GetName()(args)` with **no XMILE expansion**; only leaves (variable
  names, numbers, lookups, logical operators) differ from XMILE. It does **not** reuse
  `Expression::OutputComputable`, which is XMILE-specific.
- **`Model::PrintMDL`** — entry on `Model`, sibling of `PrintXMILE`. Signature:
  `std::string PrintMDL(std::vector<std::string>& errs);` (no x/y scale args — the parsed
  `VensimView` is already in Vensim coordinates).
- **C entry** in `XMUtil.{h,cpp}` — `extern "C"` function returning a caller-owned string (or
  NULL on error), with a signature mirroring `convert_mdl_to_xmile` (source bytes + filename
  to distinguish `.dyn` from `.mdl`).
- **CLI** in `Main.cpp` — a flag (e.g. `--to-mdl`) routing through the new entry point.
- **Round-trip test harness + `Model` comparator** (test-only) — structural equivalence over
  two `Model`s.

### Data flow

source text -> parser (`VensimParse`/`DynamoParse`) -> `Model` -> `MarkVariableTypes` ->
`MDLGenerator::Print` -> `.mdl` text. The sketch is emitted only when the `Model` has a
`VensimView`; xmutil never synthesizes geometry, so Dynamo input and external consumers
without a `VensimView` produce no sketch section.

## Existing Patterns

The design follows `XMILEGenerator` (`src/Xmile/XMILEGenerator.{h,cpp}`) closely:
- A generator class constructed with `Model*`, exposing `Print(...)` that assembles sections
  and returns a string, with errors collected in a `std::vector<std::string>&`.
- Per-variable dispatch on `VariableType()` (the `generateEquation` pattern), including
  per-element subscript expansion via `Equation::SubscriptExpand` and lookup emission from
  `ExpressionTable::GetXVals/GetYVals`.
- `Model::PrintMDL` mirrors `Model::PrintXMILE` (construct generator, return its output).
- The C entry mirrors `convert_mdl_to_xmile` in `src/XMUtil.cpp` (build `Model`, parse,
  `MarkVariableTypes`, generate, return an owned copy of the string).

The Vensim expression walker is new but parallels the existing `Expression::OutputComputable`
traversal. It is a separate walker because `OutputComputable` emits XMILE: underscored names
(`SpaceToUnderBar`), XMILE function names (`Function::ComputableName`), and the structural
function expansions gated by `CF_xmile_output` (e.g. `PULSE` -> `IF ... THEN ... ELSE`).

simlin's `mdl/writer.rs` (vendored under `third_party/simlin/src/simlin-engine`) is the
reference for Vensim-format specifics with no xmutil precedent: stock `INTEG` reconstruction
(`write_stock_variable`), operator precedence/parenthesization (`mdl_paren_if_necessary`),
identifier quoting (`needs_mdl_quoting`/`escape_mdl_quoted_ident`), line wrapping, and the
settings-section type codes (`write_settings_section`). Algorithms are ported; code is not.

## Implementation Phases

<!-- START_PHASE_1 -->
### Phase 1: Scaffolding, entry points, and round-trip test infrastructure
**Goal:** A buildable `MDLGenerator` skeleton wired end-to-end, plus the test infrastructure
later phases depend on.

**Components:**
- `src/Mdl/MDLGenerator.{h,cpp}` — class constructed with `Model*`, `Print(errs)` returning a
  minimal valid `.mdl` shell (`{UTF-8}` header + empty/control sections).
- `Model::PrintMDL` in `src/Model.{h,cpp}` — constructs `MDLGenerator`, returns its output.
- C entry in `src/XMUtil.{h,cpp}` and CLI flag in `src/Main.cpp`.
- Build wiring in `XMUtil.gyp` and the CMake configuration.
- Test-only `ModelComparator` and a round-trip harness utility: parse -> `MarkVariableTypes`
  -> compare two `Model`s structurally (variables, types, equation ASTs, subscripts, units,
  comments, sim specs, groups, `VensimView` geometry).

**Dependencies:** None (first phase).

**Done when:** The project builds; `xmutil --to-mdl` produces a parseable shell `.mdl`; the
`ModelComparator` reports equivalence for a model parsed twice and non-equivalence for two
different models (covers **mdl-writer.AC1.1**, **mdl-writer.AC1.3**, **mdl-writer.AC3.5**).
<!-- END_PHASE_1 -->

<!-- START_PHASE_2 -->
### Phase 2: Vensim expression walker
**Goal:** Convert any `Expression` tree to Vensim text.

**Components:**
- The recursive walker in `MDLGenerator` — dispatch on `Expression::GetType()`:
  `ExpressionFunction` -> `GetName()(args)`; operator nodes infix via `GetBefore()`/
  `GetOperator()` with simlin's precedence/parenthesization rules; `ExpressionLogical` ->
  Vensim operators (`=`, `<>`, `:AND:`, `:OR:`, `:NOT:`); `ExpressionVariable` -> Vensim name
  (`GetName()`, quoted when needed) + subscripts; numbers/literals -> Vensim formatting with
  the `:NA:` sentinel.
- Minimal value getters added to `ExpressionNumber`/`ExpressionLiteral` in
  `src/Symbol/Expression.h`.

**Dependencies:** Phase 1.

**Done when:** Golden tests assert exact Vensim text for representative expressions (operators
with precedence, builtins, `IF THEN ELSE`, subscripted references, quoting, numbers/`:NA:`),
and those expressions re-parse to equivalent `Expression`s (covers **mdl-writer.AC2.1**,
**mdl-writer.AC2.5**).
<!-- END_PHASE_2 -->

<!-- START_PHASE_3 -->
### Phase 3: Equation section
**Goal:** Emit all variable kinds with the units/comment trailer.

**Components:**
- `MDLGenerator` equation emission, dispatching on `VariableType()`: aux/flow as
  `name = <expr> ~ units ~ comment |`; stock as `name = INTEG(<net_flow>, <init>)` with
  `<net_flow>` from `Inflows()`/`Outflows()`; lookups in Vensim graphical-function syntax;
  dimension definitions `Dim: e1, e2, e3 ~~|`; per-element subscripted entries via
  `Equation::SubscriptExpand`.
- Line wrapping with `\` continuations (simlin algorithm).

**Dependencies:** Phase 2.

**Done when:** Round-trip tests over equation-only fixtures (stocks/flows/auxes, lookups,
arrays, units, comments, long wrapped equations) pass via the comparator (covers
**mdl-writer.AC2.2**, **mdl-writer.AC2.3**, **mdl-writer.AC2.4**, **mdl-writer.AC2.6**,
**mdl-writer.AC3.1**, **mdl-writer.AC3.2**).
<!-- END_PHASE_3 -->

<!-- START_PHASE_4 -->
### Phase 4: Control and settings sections
**Goal:** Emit sim specs, groups, and the settings tail.

**Components:**
- `.Control` section from the model's `INITIAL TIME`/`FINAL TIME`/`TIME STEP`/`SAVEPER`
  variables and `IntegrationType()`.
- Group banners (`***...*** .Group ***...***`) from `Variable::GetGroup()`/`Model::Groups()`.
- Settings section: `:L...` marker, `22:` unit equivalences from `Model::UnitEquivs()`, `15:`
  integration method, and Vensim default type-code lines (simlin `write_settings_section`).

**Dependencies:** Phase 3.

**Done when:** Round-trip tests confirm equivalent sim specs, integration method, groups, and
unit equivalences (covers **mdl-writer.AC3.3**).
<!-- END_PHASE_4 -->

<!-- START_PHASE_5 -->
### Phase 5: Sketch section
**Goal:** Re-serialize an existing `VensimView`; emit nothing when absent.

**Components:**
- `MDLGenerator` sketch emission: the `\\\---///` / `V300` / `*View` / `///---\\\` framing;
  element records type 10 (variable), 11 (valve), 12 (comment/cloud), 1 (connector) at their
  `vElements` array-index UIDs, preserving connector `from`/`to`; xmutil-tracked fields
  (`x`/`y`/`width`/`height`, `_attached`->shape bit, `_ghost`->bits bit, polarity) plus Vensim
  default styling for discarded fields. One `*View` block per `VensimView`.

**Dependencies:** Phase 3 (variable names) and Phase 4 (control variables appear in sketches).

**Done when:** Round-trip tests confirm equivalent `VensimView` geometry for sketched Vensim
models, and models without a `VensimView` emit no/empty sketch yet still parse (covers
**mdl-writer.AC4.1**, **mdl-writer.AC4.2**, **mdl-writer.AC4.3**, **mdl-writer.AC3.4**).
<!-- END_PHASE_5 -->

<!-- START_PHASE_6 -->
### Phase 6: Macros
**Goal:** Emit Vensim `:MACRO:` blocks.

**Components:**
- `MDLGenerator` macro emission over `Model::MacroFunctions()`: `:MACRO: name(args)` ...
  body equations (generated over the macro's `SymbolNameSpace`) ... `:END OF MACRO:`, before
  the main equation section.

**Dependencies:** Phases 2-3 (reuses the walker and equation emission per namespace).

**Done when:** Round-trip tests over macro-using models pass (covers **mdl-writer.AC6.1**).
This phase may be deferred to a fast-follow; if deferred, macro-using models are excluded from
the corpus until it lands.
<!-- END_PHASE_6 -->

<!-- START_PHASE_7 -->
### Phase 7: Corpus round-trip, CLI end-to-end, and Dynamo
**Goal:** Validate breadth and the Dynamo path.

**Components:**
- A corpus round-trip test over `test_models/` and selected `third_party/simlin` fixtures.
- CLI end-to-end test (file in -> `.mdl` out, then re-parse).
- Dynamo -> Vensim validation: `.dyn` input emits valid `.mdl` (no sketch) that re-parses to
  an equivalent `Model`.

**Dependencies:** Phases 3-5 (and Phase 6 if kept).

**Done when:** The corpus round-trips through the comparator, the CLI test passes, and a
`.dyn` fixture converts and re-parses equivalently (covers **mdl-writer.AC1.2**,
**mdl-writer.AC1.4**, **mdl-writer.AC5.1**, and the breadth of **mdl-writer.AC3**).
<!-- END_PHASE_7 -->

## Additional Considerations

**Coordinates.** The parsed `VensimView` holds original Vensim coordinates; XMILE rescaling
happens later in `XMILEGenerator`/`VensimView::SetViewStart`, which the mdl path skips. The
sketch therefore re-emits as-is and round-trips, and `PrintMDL` needs no scale arguments.

**Round-trip baseline.** Equivalence is defined at the post-`MarkVariableTypes` level on both
sides; the comparator runs after that transform. Inline-`INTEG` models are emitted as
`INTEG(net_flow, init)` built from `Inflows()`/`Outflows()`, which re-parses to the same stock
structure — a stable normalization, not a fidelity loss.

**Sketch fidelity.** xmutil's parser already discards most sketch fields (fonts, colors, line
styles) and keeps a single connector point (`VensimConnectorElement`, `_npoints = 1`). v1
round-trips what xmutil retains and emits Vensim defaults for the rest. Higher-fidelity sketch
output (preserving raw fields, like simlin's `ViewElementCompat`) would require extending
`VensimViewElement` and the parser — a deliberate follow-up, not v1.

**Macro deferral.** Phase 6 is the one phase that may slip to a fast-follow if its cost is
high; the corpus then excludes macro-using models until it lands.

**External consumers.** Building an xmutil `Model` from external XMILE-like data is the
consumer's responsibility using existing xmutil APIs; this design adds no model-builder
convenience layer.
