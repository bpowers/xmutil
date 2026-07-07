# XMILE Reader Design

## Summary

xmutil is a C++ tool that converts between system dynamics model formats. Today it reads Vensim `.mdl` and Dynamo `.dyn` into a native in-memory `Model` and writes that `Model` out as either XMILE or `.mdl`. This work adds the missing input direction: an XMILE reader, which together with the existing writers delivers **XMILE -> XMILE** (round-trip / normalize) and **XMILE -> MDL**. There is deliberately no parallel datamodel type -- the reader populates the same `Model` that the Vensim and Dynamo parsers populate, so the existing `XMILEGenerator` and `MDLGenerator` serve both new paths unchanged.

The approach mirrors xmutil's existing `VensimParse` shape rather than inventing a new pipeline. A new `XmileReader` walks the XMILE DOM using the already-vendored tinyxml2, dispatches per element type (`<aux>`, `<stock>`, `<flow>`, `<gf>`, `<dim>`, `<view>`, ...), and drives a new bison/flex equation grammar (`XmileEqYacc.y` / `XmileEqLex.l`) that is a faithful port of simlin's archived `equation.lalrpop`. A small `XmileFunctions` table un-renames XMILE function names to their Vensim equivalents (`smth1` -> `SMOOTH`, `int` -> `INTEGER`, ...) so the produced `Expression` trees are identical in shape to those `VensimParse` produces. A separate `XmileView` component synthesizes `VensimView` geometry from the XMILE `<view>` so the MDL writer's sketch emitter works end-to-end. The post-parse pipeline (`MarkVariableTypes`, `AdjustGroupNames`, `CheckGhostOwners`) runs unchanged.

## Definition of Done

### Primary deliverables
- A C++ XMILE reader in `src/Xmile/` that parses an XMILE document into xmutil's native `Model` (Symbols, Variables, Expression trees, `VensimView` geometry) -- covering auxiliaries, stocks/flows, lookups/graphical functions, sim specs, dimensions/arrays/subscripts, units, groups, and views/sketch.
- A new bison/flex XMILE-equation grammar (ported from simlin's `equation.lalrpop`), producing the same `Expression` node types xmutil's Vensim parser produces, with XMILE -> Vensim function-name un-renaming (`smth1` -> `SMOOTH`, `int` -> `INTEGER`, etc.).
- `src/Main.cpp` autodetects `.xmile` / `.stmx` for input; `--to-mdl` continues to select output direction. New extern-C entry points so the WASM build and embedders can drive XMILE input.
- A `test/xmile/` round-trip harness on the same `TEST` / `CHECK` framework as `test/mdl/`, using **fishbanks, logistic-growth, reliability** as the corpus.

### Success criteria
- All three corpus models load without error, round-trip **XMILE -> XMILE** through the existing `XMILEGenerator`, and convert **XMILE -> MDL** to a `.mdl` that re-parses under xmutil. Synthesized Vensim sketch geometry is produced from the XMILE view on the XMILE -> MDL path.
- An XMILE file containing a top-level `<module>` element is rejected with a clear error (and points to the offending element).
- `xmutil_test` (existing MDL tests) continues to pass; XMILE output for existing MDL inputs stays byte-identical (no regression on the current `mdl -> xmile` path).
- WASM build still configures and builds.
- `./format.sh` clean; no emoji or AI-attributed lines in commits.

### Out of scope (v1)
- `<module>` / submodels (hard error if present).
- `<macro>` siblings.
- simlin's `simlin:*` vendor-extension elements/attrs.
- isee:* UI widgets and Stella interface elements (`<button>`, `<knob>`, `<slider>`, `<graph>`, ...) -- silently ignored at read.
- EXCEPT clauses on arrays (mirrors current MDL writer scope).
- Multiple top-level `<model>` elements (xmutil is single-model).

## Acceptance Criteria

### xmile-reader.AC1: Model populates from XMILE via a stable API
- **xmile-reader.AC1.1 Success:** `Model::ParseXMILE(filename, contents, len, errs)` populates a `Model` that subsequently runs through `MarkVariableTypes` / `AdjustGroupNames` / `CheckGhostOwners` without error for a well-formed corpus XMILE input.
- **xmile-reader.AC1.2 Success:** The extern-C entry `convert_xmile_to_xmile(source, len, fileName, longNames, sectors)` returns a caller-owned XMILE string for a well-formed input, with the same ownership contract as `convert_mdl_to_xmile`.
- **xmile-reader.AC1.3 Success:** The extern-C entry `convert_xmile_to_mdl(source, len, fileName, longNames)` returns a caller-owned MDL string for a well-formed input.
- **xmile-reader.AC1.4 Success:** The CLI dispatches `.xmile` and `.stmx` inputs (case-insensitive) to the XMILE reader; without `--to-mdl` it emits XMILE; with `--to-mdl` it emits MDL.
- **xmile-reader.AC1.5 Failure:** On a parse / conversion error, messages are appended to `errs` and the extern-C entry returns NULL; `Model::ParseXMILE` returns false.
- **xmile-reader.AC1.6 Edge:** XMILE -> XMILE on a file named `model.xmile` writes `model.regen.xmile`; the input is not overwritten.

### xmile-reader.AC2: XMILE equations parse to xmutil Expression trees
- **xmile-reader.AC2.1 Success:** XMILE arithmetic expressions parse to the same `ExpressionAdd` / `Subtract` / `Multiply` / `Divide` / `Power` shapes the Vensim parser produces; operator precedence matches the lalrpop grammar's lattice.
- **xmile-reader.AC2.2 Success:** XMILE function names map to Vensim `Function*` via the un-rename table: `smth1` -> `SMOOTH`, `smth3` -> `SMOOTH3`, `delay` -> `DELAY FIXED`, `delay1` -> `DELAY1`, `delay3` -> `DELAY3`, `int` -> `INTEGER`, `safediv` -> `ZIDZ` (2-arg) or `XIDZ` (3-arg), `init` -> `ACTIVE INITIAL`, plus the generic `underbar_to_space().to_uppercase()` fallback for unmapped Vensim builtins.
- **xmile-reader.AC2.3 Success:** Bare zero-argument keywords `time` / `dt` / `initial_time` / `final_time` resolve to `Time` / `TIME STEP` / `INITIAL TIME` / `FINAL TIME` as zero-arg `ExpressionFunction`s.
- **xmile-reader.AC2.4 Success:** `if cond then a else b` parses to `ExpressionFunction("IF THEN ELSE", [cond, a, b])`.
- **xmile-reader.AC2.5 Success:** Subscripts (`x[a, b]`, `x[*]`, `x[*:Dim]`, `x[l:r]`, `x[@1]`) parse to `ExpressionVariable` with the corresponding subscript representation.
- **xmile-reader.AC2.6 Edge:** Both keyword (`AND`, `OR`, `NOT`, `MOD`) and C-style (`&&`, `||`, `!`, `%`) operator spellings produce the same `ExpressionLogical` / operator nodes.
- **xmile-reader.AC2.7 Edge:** `//` (safediv) parses to `ExpressionFunction("ZIDZ", [l, r])` (matching the lalrpop's `App("safediv", [l, r])` shape).
- **xmile-reader.AC2.8 Failure:** A function name with no Vensim mapping in the un-rename table is reported as an error naming the unknown function; the parse fails.
- **xmile-reader.AC2.9 Failure:** A postfix transpose `'` in an equation is reported as a parse error.

### xmile-reader.AC3: Round-trip preserves the model (primary bar)
- **xmile-reader.AC3.1 Success:** For each corpus model (`fishbanks`, `logistic-growth`, `reliability`), `XMILE -> Model -> XMILE -> Model'` yields a `Model` equivalent to the first per `ModelComparator`: same variables, equivalent equation ASTs, same variable types.
- **xmile-reader.AC3.2 Success:** Dimensions, unit definitions, groups, and sim specs (start, stop, dt, save_step, integration method) are equivalent across the XMILE round-trip.
- **xmile-reader.AC3.3 Success:** For each corpus model, `XMILE -> Model -> MDL -> Model'` yields a `Model` equivalent to the first.
- **xmile-reader.AC3.4 Guard:** The `ModelComparator` (reused from `test/mdl/`) detects a deliberately introduced non-equivalence on the XMILE round-trip; the round-trip test cannot pass vacuously.

### xmile-reader.AC4: Sketch synthesis from XMILE views
- **xmile-reader.AC4.1 Success:** XMILE `<view>` elements with stocks, flows, auxes, connectors, and aliases produce `VensimView` geometry that the MDL writer's sketch emitter consumes without error; the resulting `.mdl` re-parses with equivalent geometry.
- **xmile-reader.AC4.2 Success:** Connector polarity (`positive` -> `+`, `negative` -> `-`, absent -> none) round-trips XMILE -> MDL -> re-parse correctly.
- **xmile-reader.AC4.3 Success:** A flow whose source or sink is a cloud (implicit, via missing `<from>` / `<to>` stock attribute) sets the connected Variable's `_hasUpstream` / `_hasDownstream` flags so the MDL writer emits the correct flow structure.
- **xmile-reader.AC4.4 Edge:** A model with multiple `<view>` elements takes the first; subsequent views are noted via `errs` as a soft warning and skipped.
- **xmile-reader.AC4.5 Success:** XMILE `<group>` elements become `ModelGroup`s; their members are emitted under group banners on the MDL output path.

### xmile-reader.AC5: Out-of-scope features hard-error or are silently skipped
- **xmile-reader.AC5.1 Failure:** A `<module>` element inside `<variables>` is rejected with a clear error citing "modules are not supported" and the offending element.
- **xmile-reader.AC5.2 Failure:** A `<macro>` sibling element of `<model>` is rejected with a clear error.
- **xmile-reader.AC5.3 Failure:** Multiple `<model>` elements (other than `<macro>`-classified ones) is rejected with a clear error.
- **xmile-reader.AC5.4 Edge:** Unknown-namespace elements (`isee:*`, `simlin:*`) and Stella UI widgets in the XMILE namespace (`<button>`, `<knob>`, `<slider>`, `<graph>`, `<numeric_input>`, ...) are silently skipped; the rest of the model loads.

### xmile-reader.AC6: No regression
- **xmile-reader.AC6.1 Success:** The existing `xmutil_test` MDL test suite continues to pass.
- **xmile-reader.AC6.2 Success:** XMILE output produced by the existing `convert_mdl_to_xmile` path is byte-identical to the pre-change output for the existing MDL corpus.
- **xmile-reader.AC6.3 Success:** The WASM build (configured via `./configure.sh` with the emscripten profile) succeeds with the new sources included.

## Glossary

- **System dynamics**: A modeling approach where behavior emerges from stocks (accumulations) and the flows that change them over time. XMILE, Vensim, and Dynamo are all SD formats/tools.
- **XMILE**: An XML-based open interchange standard for system dynamics models. xmutil's existing output format; this work adds the matching reader. File extensions `.xmile` and `.stmx` (Stella's flavor).
- **Vensim `.mdl`**: The native text file format of the Vensim tool. xmutil's other output format; XMILE -> MDL is one of the two new conversion paths.
- **simlin**: A separate SD project vendored under `third_party/simlin`. Its archived `equation.lalrpop` is the reference grammar for the new XMILE equation parser, and its `writer.rs` is the reference for the XMILE-to-Vensim function-name and bare-keyword mappings. Algorithms are ported, not code.
- **lalrpop**: The LR(1) parser generator used by simlin's Rust codebase. The XMILE equation grammar this work introduces is a port of a `.lalrpop` source to bison.
- **bison / flex**: The parser and lexer generators xmutil already uses for its Vensim and Dynamo grammars; the new XMILE equation parser uses the same toolchain, with committed `.tab.{cpp,hpp}` artifacts following xmutil's convention.
- **Stock / flow / aux / lookup**: The variable kinds in an SD model. A *stock* (level) accumulates over time; *flows* (rates) add to or drain it; an *aux* (auxiliary) is an intermediate computed value; a *lookup* (graphical function) maps an input to an output via a piecewise table.
- **`<gf>` / graphical function**: XMILE's lookup-table element. May be inline on a variable (consumed as `WITH LOOKUP`) or standalone (the variable's RHS is the table itself). Built from `<xpts>` / `<ypts>` or from `<xscale>` plus a y-point list.
- **Sim specs**: Simulation settings -- start time, stop time, dt, save_step, and integration method (Euler / RK2 / RK4). In XMILE they live under `<sim_specs>`; in xmutil they become real Variables (`INITIAL TIME`, `FINAL TIME`, `TIME STEP`, `SAVEPER`) plus setters on `Model`.
- **`dt reciprocal="true"`**: An XMILE `<dt>` attribute meaning the body value `N` denotes `dt = 1/N`. Resolved to its numeric value at read time; the round-trip comparator compares the resolved doubles.
- **Dimensions / subscripts / arrays**: SD's array mechanism. A *dimension* defines a set of element names; a *subscripted* variable has either one apply-to-all equation or one equation per element. xmutil represents these as `XMILE_Type_ARRAY` variables with an `ExpressionSymbolList` RHS holding the element list.
- **Group**: A named partition of variables in a model. Vensim emits groups under `***...*** .Group ***...***` banners; XMILE has a `<group>` element. xmutil holds them as `ModelGroup`s on `Model`.
- **Module**: An XMILE submodel instance (`<module>` inside `<variables>`). xmutil has no submodel concept, so v1 hard-rejects models that use modules.
- **Macro**: A user-defined function (Vensim `:MACRO: ... :END OF MACRO:` or XMILE `<macro>` sibling of `<model>`). v1 hard-rejects XMILE `<macro>` siblings.
- **Cloud**: The visual source/sink at the open end of a flow (a stock outside the model boundary). xmutil has no dedicated Cloud class -- clouds are conveyed via `_hasUpstream` / `_hasDownstream` flags on the connected Variable.
- **Connector polarity**: The `+` / `-` sign on a causal connector indicating whether the source increases or decreases the target. XMILE encodes it via the `polarity` attribute on `<connector>`.
- **`Model`**: xmutil's central in-memory representation: a `SymbolNameSpace` plus views, sim specs, groups, and unit equivalences. The XMILE reader's output; the writers' input.
- **`SymbolNameSpace`**: xmutil's symbol table mapping names to `Variable`, `Equation`, `Expression`, and `Function` objects. Both readers populate the model's single namespace; the new reader does not introduce a parallel one.
- **`Variable`**: xmutil's in-memory representation of one named quantity, with name, equations, units, comments, group, and (for view participants) the `_hasUpstream` / `_hasDownstream` flags used to convey clouds.
- **`Expression`**: xmutil's AST node for the right-hand side of an equation. Subclasses include `ExpressionAdd`, `ExpressionMultiply`, `ExpressionFunction`, `ExpressionLogical`, `ExpressionVariable`, `ExpressionNumber`, `ExpressionLookup`, `ExpressionTable`, and `ExpressionSymbolList`. The XMILE grammar constructs the same node types `VensimParse` produces.
- **`Function`**: xmutil's representation of a callable (e.g. `IF THEN ELSE`, `INTEG`, `SMOOTH`). `Function*` objects live in the namespace, registered once via `ReadyFunctions`; the XMILE reader does not register a parallel table and instead looks them up via the un-rename table.
- **`MarkVariableTypes`**: The `Model` post-parse transform that classifies each variable as stock / flow / aux / array and synthesizes `INTEG(net_flow, init)` from a stock's initial value plus its named inflows/outflows. The XMILE reader produces input it consumes unchanged.
- **`VensimView`**: xmutil's parsed representation of one sketch: variable, valve, connector, and (implicit) cloud elements with coordinates and from/to links. Element UIDs are array indices in the element vector. The XMILE reader populates this same class via `XmileView` rather than introducing a parallel "XMILE view" type.
- **`VensimVariableElement` / `VensimValveElement` / `VensimConnectorElement`**: The element kinds inside a `VensimView`. A flow decomposes on input into a variable element (the flow label) plus a valve element (the flow-rate control on the pipe).
- **`VPObject` / `XPObject`**: The process-global single-instance contexts used to bridge the bison/flex generated code to the C++ parser driver. `VPObject` is the existing Vensim parser's context; `XPObject` is its sibling for the new XMILE equation parser.
- **`ModelComparator`**: The test-only structural-equivalence checker introduced by the mdl-writer work. Format-agnostic (it walks `Model`, not text), so it is reused directly for the XMILE round-trip tests.
- **`:NA:` sentinel / `-1e38`**: Vensim's "not available" marker, used as the numeric value xmutil stores. The XMILE lexer maps the `:NA:` literal to `-1e38` to match the Vensim parser.
- **tinyxml2**: The vendored XML parser xmutil already uses for XMILE output; the new reader uses it for the input DOM walk. No new external dependency.
- **gyp**: xmutil's build system (ninja-backed). New sources are wired into `common_sources` and the `xmutil_test` `sources` list in `XMUtil.gyp`, then `./configure.sh` regenerates the ninja build.

## Architecture

xmutil today reads Vensim `.mdl` and Dynamo `.dyn` into a native `Model` (a `SymbolNameSpace` of `Variable` / `Equation` / `Expression` plus `VensimView` diagrams), runs `Model::MarkVariableTypes` to classify stocks / flows / auxes, and writes XMILE via `XMILEGenerator` or MDL via `MDLGenerator`. This work adds the missing input direction: an XMILE reader that populates the same `Model`, enabling **XMILE -> XMILE** (round-trip / normalize) and **XMILE -> MDL** through the existing writers, with no parallel datamodel type.

`XmileReader` (`src/Xmile/XmileReader.{h,cpp}`) is constructed with `Model*`, exposes `ProcessFile(filename, contents, len, errs)`, and mirrors `VensimParse` exactly: it grabs the model's `SymbolNameSpace`, ensures functions are registered (the same `ReadyFunctions` registration the Vensim parser uses; both readers share the same Vensim `Function*` table in the namespace), walks the XMILE DOM with the already-vendored tinyxml2, dispatches per element type, and drives the new XMILE equation parser per `<eqn>`. The caller then runs the same post-parse pipeline xmutil already uses for Vensim input: `MarkVariableTypes(nullptr)`, `AdjustGroupNames`, per-macro `MarkVariableTypes`, `CheckGhostOwners` (`src/XMUtil.cpp:278-304`).

The XMILE equation parser is a new bison/flex grammar (`src/Xmile/XmileEqYacc.y`, `XmileEqLex.l`, with committed `.tab.{cpp,hpp}` mirroring xmutil's existing convention -- `src/Vensim/VYacc.tab.*` and `src/Dynamo/DYacc.tab.*` are checked-in artifacts, regenerated manually). The grammar is a faithful port of simlin's archived `equation.lalrpop` (last present at `abda30d9^:src/simlin-engine/src/equation.lalrpop` in the vendored `third_party/simlin` git history). The lalrpop's productions translate directly to bison productions; tokens map one-for-one. A new process-global `XPObject` plays the role `VPObject` plays for the Vensim parser (single-instance, lex -> parser handoff).

`XmileFunctions` (`src/Xmile/XmileFunctions.{h,cpp}`) holds the XMILE-name -> Vensim `Function*` table. Public surface: `Function* LookupXmileFunction(SymbolNameSpace*, const char* xmileName)`. The table inverts simlin's `xmile_to_mdl_function_name` (`third_party/simlin/.../writer.rs:194`) and `mdl_bare_keyword` (`writer.rs:181`). The bison grammar calls this every time it builds an `ExpressionFunction`; an unmapped name is a hard error.

`XmileView` (`src/Xmile/XmileView.{h,cpp}`) synthesizes `VensimView` geometry from a single XMILE `<view>` element. Walks `<stock>` / `<flow>` / `<aux>` / `<connector>` / `<alias>` / `<group>` children, allocates dense UIDs in emit order (matching xmutil's array-index = UID convention), decomposes `<flow>` into a `VensimVariableElement` (the flow label) plus a `VensimValveElement`, builds pipe connectors to upstream / downstream stocks, and conveys clouds via the Variable's `_hasUpstream` / `_hasDownstream` flags rather than dedicated cloud elements (xmutil has no cloud type).

### Components and boundaries

- **`XmileReader`** (`src/Xmile/XmileReader.{h,cpp}`) -- DOM walker. Constructor `XmileReader(Model*)`. Entry `bool ProcessFile(const char* filename, const char* contents, size_t len, std::vector<std::string>& errs)`. Owns the `XPObject` global lifetime.
- **`XmileEqYacc.y` / `XmileEqLex.l`** + committed `.tab.{cpp,hpp}` -- XMILE equation grammar. Productions construct xmutil `Expression*` trees identical in shape to those `VensimParse` produces, so downstream code (`MarkVariableTypes`, writers, comparator) treats them uniformly.
- **`XmileFunctions`** (`src/Xmile/XmileFunctions.{h,cpp}`) -- XMILE-name -> `Function*` lookup. Single source of truth for the un-rename table.
- **`XmileView`** (`src/Xmile/XmileView.{h,cpp}`) -- XMILE view -> `VensimView` synthesizer. Called by `XmileReader` once per `<view>`.
- **`Model::ParseXMILE`** -- entry on `Model`, sibling of the implicit Vensim driver. Signature `bool ParseXMILE(const char* filename, const char* contents, size_t len, std::vector<std::string>& errs);`. Constructs `XmileReader`, runs `ProcessFile`, returns success.
- **Extern-C entries** in `src/XMUtil.{h,cpp}`: `char* convert_xmile_to_xmile(const char* source, uint32_t len, const char* fileName, int longNames, bool sectors)` and `char* convert_xmile_to_mdl(const char* source, uint32_t len, const char* fileName, int longNames)`.
- **CLI dispatch** in `src/Main.cpp`: extension check on the input path (`.xmile` / `.stmx` case-insensitive) selects the XMILE driver; `--to-mdl` continues to select output format.

### Data flow

XMILE bytes -> `tinyxml2::XMLDocument::Parse` -> DOM walk in `XmileReader::ProcessFile` -> per variable: `<eqn>` text -> XMILE equation parser (bison / flex) -> `Expression*` tree -> `Equation` attached to `Variable` -> `<view>` walk -> `VensimView` populated via `XmileView` -> `Model` returned. The post-parse pipeline (`MarkVariableTypes` etc.) runs in the CLI / extern-C driver as it does for Vensim input. From there the existing `XMILEGenerator` or `MDLGenerator` produces output unchanged.

## Existing Patterns

The design follows `VensimParse` and the surrounding pipeline closely:

- **Reader class shape.** Constructor takes `Model*`, captures `SymbolNameSpace*`, exposes a single `ProcessFile(filename, contents, len, errs)` entry. Identical to `VensimParse(Model*)` / `VensimParse::ProcessFile` (`src/Vensim/VensimParse.cpp:20,222`).
- **Post-parse pipeline.** `MarkVariableTypes` -> `AdjustGroupNames` -> per-macro `MarkVariableTypes` -> `CheckGhostOwners` is the shared post-parse contract (`src/XMUtil.cpp:278-304`). The XMILE driver invokes it unchanged.
- **Function registration.** `Function*` objects live in the model's `SymbolNameSpace`, registered once via `ReadyFunctions()`. The XMILE reader does not register a parallel function table; it looks up the existing Vensim `Function*` via the un-rename table.
- **Single-instance parser context.** The Vensim parser uses a process-global `VPObject` (`src/Vensim/VensimParse.cpp:18`) asserted unique during construction. The XMILE equation parser uses a sibling `XPObject` with the same single-instance contract.
- **Bison/flex artifacts.** `.tab.{cpp,hpp}` files are checked in alongside the grammar sources (`src/Vensim/VYacc.tab.{cpp,hpp}`, `src/Dynamo/DYacc.tab.{cpp,hpp}`). The XMILE reader's `XmileEqYacc.tab.{cpp,hpp}` follows the same pattern: regenerated manually via `bison`, committed in the same directory as the `.y` source, added to `common_sources` in `XMUtil.gyp`.
- **Expression construction.** Parsers `new` `Expression*` subclasses directly (no factory helpers exist or are introduced). The XMILE grammar's actions construct the same node types -- `ExpressionAdd`, `ExpressionMultiply`, `ExpressionFunction`, `ExpressionLogical`, `ExpressionVariable`, `ExpressionNumber`, `ExpressionLookup`, `ExpressionTable`, `ExpressionSymbolList`, etc. -- that the Vensim grammar produces.
- **Variable construction.** `new Variable(sns, name)` via `SymbolNameSpace::Find`-or-create; `VariableContentVar` lazy on first `AddEq`. Identical to `VensimParse::InsertVariable`.
- **View class.** `VensimView` extends `View` and is registered via `Model::AddView`. Element UIDs are array indices in the view's element vector. The XMILE reader populates this same structure rather than introducing a parallel "XMILE view" class.
- **Sim spec representation.** Sim specs live in the namespace as four real Variables -- `INITIAL TIME`, `FINAL TIME`, `TIME STEP`, `SAVEPER` -- with const numeric equations, AND `Model::set_initial_time` / `set_finall_time` / `set_dt` / `SetIntegrationType` setters are called. This mirrors how `VensimParse` represents them and means both writers' `.Control` / sim-spec sections work unchanged.
- **Test harness.** `test/xmile/` mirrors `test/mdl/`. Uses `test/TestHarness.{h,cpp}` (`TEST` / `CHECK` / `CHECK_EQ_STR` macros). `xmutil_test` is the single test binary defined in `XMUtil.gyp`; new test sources are added to its `sources` list and `./configure.sh` is re-run.
- **`ModelComparator`** (`test/mdl/ModelComparator.{h,cpp}`) is format-agnostic -- it walks `Model` structures, not MDL text -- and is reused directly by the XMILE test harness for round-trip equivalence checks.

**Reference algorithms** ported (not code) from simlin (`third_party/simlin/src/simlin-engine`):

- XMILE-name -> Vensim-name mapping inverts `xmile_to_mdl_function_name` (`mdl/writer.rs:194`) and `mdl_bare_keyword` (`mdl/writer.rs:181`).
- XMILE equation grammar is a direct port of `equation.lalrpop` (archived in the vendored simlin git history at `abda30d9^:src/simlin-engine/src/equation.lalrpop`).
- View synthesis mirrors the structural shape of `xmile/views.rs` (which produces simlin's `view_element` types; we produce `VensimView` instead).

## Implementation Phases

Eight phases. Each phase ends with a working build and a `xmutil_test` run that includes the new XMILE tests for prior phases. Phases are sequential -- earlier phases unblock later ones -- but the test corpus grows incrementally as each variable kind comes online.

<!-- START_PHASE_1 -->
### Phase 1: Scaffolding, entry points, and test infrastructure
**Goal:** A buildable `XmileReader` skeleton wired end-to-end, with the `test/xmile/` harness in place.

**Components:**
- `src/Xmile/XmileReader.{h,cpp}` -- constructor `XmileReader(Model*)`, entry `ProcessFile(filename, contents, len, errs)`. Stub: parses XML root, recognizes `<xmile>` envelope, returns success without populating Model.
- `Model::ParseXMILE` in `src/Model.{h,cpp}` -- sibling of the Vensim driver. Constructs `XmileReader`, returns success.
- Extern-C entries `convert_xmile_to_xmile` and `convert_xmile_to_mdl` in `src/XMUtil.{h,cpp}`, running the same post-parse pipeline (`MarkVariableTypes`, `AdjustGroupNames`, per-macro `MarkVariableTypes`, `CheckGhostOwners`) that `convert_mdl_to_xmile` runs.
- CLI dispatch in `src/Main.cpp`: extension check on input path (`.xmile` / `.stmx` case-insensitive); `<base>.regen.xmile` rename for XMILE -> XMILE to avoid overwriting input (mirrors the `.regen.mdl` pattern).
- Build wiring: `XMUtil.gyp` `common_sources` and `xmutil_test` `sources` additions; `./configure.sh` re-runs cleanly.
- `test/xmile/` directory with `RoundTrip.{h,cpp}` (XMILE-flavored wrapper around the existing `test/mdl/ModelComparator`), `BasicSmokeTest.cpp` (round-trips an empty `<xmile>` envelope and a one-variable hand-written XMILE).

**Dependencies:** None (first phase).

**Done when:** The project builds; `xmutil model.xmile` and `xmutil --to-mdl model.xmile` produce output files (likely structurally empty for now); the smoke test passes (covers **xmile-reader.AC1.1**, **xmile-reader.AC1.4**, **xmile-reader.AC1.6**).
<!-- END_PHASE_1 -->

<!-- START_PHASE_2 -->
### Phase 2: XMILE equation grammar
**Goal:** Parse any XMILE equation text into xmutil `Expression*` trees.

**Components:**
- `src/Xmile/XmileEqYacc.y` ported from simlin's `equation.lalrpop`. Grammar actions construct `ExpressionAdd` / `Subtract` / `Multiply` / `Divide` / `Power` / `UnaryMinus` / `Logical` / `Function` / `Variable` / `Number` / `SymbolList` directly. `if / then / else` lowers to `ExpressionFunction("IF THEN ELSE", [cond, t, f])`. `//` (safediv) lowers to `ExpressionFunction("ZIDZ", [l, r])`. `'` (postfix transpose) raises a parse error.
- `src/Xmile/XmileEqLex.l` (flex) tokenizes the equation. Accepts both XMILE keyword (`AND`, `OR`, `NOT`, `MOD`) and C-style (`&&`, `||`, `!`, `%`) operators as the same tokens. Identifiers bare or quoted; numbers including scientific notation; the `:NA:` literal maps to `-1e38`.
- Committed `XmileEqYacc.tab.{cpp,hpp}` and the flex-generated `XmileEqLex.cpp`, added to `common_sources`.
- `src/Xmile/XmileFunctions.{h,cpp}` -- XMILE-name -> Vensim `Function*` table. Public `Function* LookupXmileFunction(SymbolNameSpace*, const char* xmileName)`. Static table inverts simlin's `xmile_to_mdl_function_name` and `mdl_bare_keyword` lists. Unmapped name -> error returned via the parser's error channel.
- Process-global `XPObject` mirroring `VPObject`'s single-instance contract.
- `test/xmile/EquationParseTest.cpp`: golden Expression-tree comparisons for representative XMILE equation strings (operators with precedence, comparisons, logicals, `if / then / else`, function calls with renamed names, subscripts, bare zero-arg keywords, `:NA:`).

**Dependencies:** Phase 1.

**Done when:** All equation fixtures parse to the expected Expression trees; an unknown function name and a postfix `'` each produce a clear error (covers **xmile-reader.AC2.1** through **xmile-reader.AC2.9**).
<!-- END_PHASE_2 -->

<!-- START_PHASE_3 -->
### Phase 3: Auxes, sim specs, units, groups
**Goal:** Walk the XMILE DOM and populate scalar-equation variables, sim specs, unit definitions, and groups.

**Components:**
- `XmileReader::ProcessFile` walks `<xmile>` -> `<header>` (model name), `<sim_specs>` (start, stop, dt with optional `reciprocal="true"`, save_step, method), `<model_units>` -> `<unit>` entries, and `<model>` -> `<variables>`.
- For each `<aux>`: create `Variable` via the namespace, parse `<eqn>` via Phase 2, attach `Equation`. Read `<units>`, `<doc>` for the trailer.
- Sim specs: mint `INITIAL TIME`, `FINAL TIME`, `TIME STEP`, `SAVEPER` as real Variables with const-numeric equations AND call `Model::set_initial_time` / `set_finall_time` / `set_dt` / `SetIntegrationType` (XMILE `method="euler|rk2|rk4"`). `dt reciprocal="true"` resolves to `1 / N` at read time.
- Units: build `UnitExpression`s and attach to Variables. Unit equivalences accumulate as `Model::UnitEquivs()` strings.
- Groups: a `<group>` element creates a `ModelGroup` via `Model::Groups().push_back`; member variables are assigned via `Variable::SetGroup`.
- Unknown-namespace elements (`isee:*`, `simlin:*`) and Stella UI widgets silently skipped via namespace check.
- `test/xmile/AuxRoundTripTest.cpp`: round-trips a hand-written XMILE with auxes only (no flows, no arrays, no views), covering both XMILE -> XMILE and XMILE -> MDL.

**Dependencies:** Phase 2.

**Done when:** A scalar-only XMILE model round-trips XMILE -> XMILE and XMILE -> MDL through the comparator; a fixture with `isee:loop_indicator` etc. round-trips with those elements silently dropped (covers a subset of **xmile-reader.AC3.1**, **xmile-reader.AC3.2**, **xmile-reader.AC5.4**).
<!-- END_PHASE_3 -->

<!-- START_PHASE_4 -->
### Phase 4: Stocks and flows
**Goal:** Handle XMILE's stock / flow structure so the existing `MarkVariableTypes` reconstructs `INTEG` correctly.

**Components:**
- For each `<stock>`: create Variable, parse `<eqn>` (initial value), record `<inflow>` / `<outflow>` text references (which name `<flow>` variables by name). xmutil's `MarkVariableTypes` pass synthesizes `INTEG(net_flow, init)` from the stock's initial-value equation and the named inflows/outflows after parsing completes -- the same path the Vensim reader takes via separate flow equations. The XMILE reader matches that contract by recording the inflow/outflow names on the Variable for `MarkStockFlows` to consume.
- For each `<flow>`: create Variable, parse `<eqn>` (flow rate equation), attach `Equation`.
- `test/xmile/StockFlowRoundTripTest.cpp`: round-trips `logistic-growth/model.xmile` (clean simlin-exported stock+flow corpus).

**Dependencies:** Phase 3.

**Done when:** `logistic-growth` round-trips XMILE -> XMILE and XMILE -> MDL with equivalent stock structures (covers more of **xmile-reader.AC3.1**, **xmile-reader.AC3.3**).
<!-- END_PHASE_4 -->

<!-- START_PHASE_5 -->
### Phase 5: Dimensions, arrays, subscripts
**Goal:** XMILE dimension definitions and subscripted variables populate xmutil's `XMILE_Type_ARRAY` variables and apply-to-all / per-element equation structures.

**Components:**
- `<dimensions>` / `<dim name="..." size="N">` or `<elem name="..."/>` children build dimension Variables of `XMILE_Type_ARRAY` with an `ExpressionSymbolList` RHS holding the element list (matching what `XMILEGenerator` writes). An `<dim size="N">` indexed form expands to N synthetic element names (`1`...`N`).
- `<aux>`, `<flow>`, `<stock>` with a `<dimensions>` child get the appropriate subscripted shape: `<element subscript="...">` children become per-element equations; a top-level `<eqn>` with `<dimensions>` becomes apply-to-all. The reader attaches one `Equation` per element, mirroring `Equation::SubscriptExpand`'s output on the writer side.
- Subscript expressions in the equation parser (Phase 2) already produce `ExpressionVariable` with the right subs.
- `test/xmile/ArrayRoundTripTest.cpp`: hand-written XMILE with dimensions covering apply-to-all, per-element, and subdimension-map cases.

**Dependencies:** Phase 4.

**Done when:** Array fixtures round-trip; the comparator confirms equivalent dimensions and subscripted equations (covers the array slice of **xmile-reader.AC3.1**, **xmile-reader.AC3.2**).
<!-- END_PHASE_5 -->

<!-- START_PHASE_6 -->
### Phase 6: Lookups / graphical functions
**Goal:** XMILE `<gf>` elements (inline on a variable, or as a standalone graphical-function variable) populate `ExpressionTable` / `ExpressionLookup` consistent with how the existing writers consume them.

**Components:**
- `<gf>` with `<xpts>`, `<ypts>`, `<xscale>`, `<yscale>` child elements: read x/y point arrays (or derive xs from `xscale.min` / `max` evenly spaced when `<xpts>` is absent), build `ExpressionTable` via `AddPair(x, y)` in lockstep.
- Inline `<gf>` on an aux/flow with a non-empty `<eqn>` becomes `ExpressionLookup(input, table)` (WITH LOOKUP shape); inline `<gf>` on an aux with an empty equation becomes a standalone graphical-function variable (the RHS is the `ExpressionTable` directly, matching the empty-equation case the MDL writer treats specially).
- `test/xmile/LookupRoundTripTest.cpp`: hand-written XMILE with both WITH-LOOKUP and standalone graphical functions.

**Dependencies:** Phase 5.

**Done when:** Lookup fixtures round-trip; standalone graphical functions and WITH-LOOKUP forms both produce equivalent Models (covers the lookup slice of **xmile-reader.AC3.1**).
<!-- END_PHASE_6 -->

<!-- START_PHASE_7 -->
### Phase 7: View / sketch synthesis
**Goal:** XMILE `<views>` populate `VensimView` so the MDL writer's sketch section emits a faithful Vensim diagram on XMILE -> MDL.

**Components:**
- `src/Xmile/XmileView.{h,cpp}` -- walks a single `<view>` element. For each `<stock>` / `<flow>` / `<aux>` view element: create `VensimVariableElement` at `(x, y)`, resolve the variable by name from the namespace. For each `<flow>`: also create a `VensimValveElement` at the flow position and pipe `VensimConnectorElement`s to upstream/downstream stocks. For each `<connector>` (causal arrow): create `VensimConnectorElement(from_uid, to_uid, x, y)` with polarity from the `polarity` attribute. For each `<alias>`: create a ghost `VensimVariableElement` (`_ghost` flag set).
- UID allocation: sequential array indices in emit order. Connector `from` / `to` UIDs resolve by element-name -> UID lookup map built in a first pass.
- Cloud handling: a flow with `<from>` cloud or `<to>` cloud sets the source/sink Variable's `_hasUpstream` / `_hasDownstream` flags so the MDL writer emits the correct flow structure.
- Multi-view `<views>`: take the first `<view>` in v1; subsequent views logged via `errs` as a soft warning. (The corpus is single-view; multi-view fidelity is a fast-follow.)
- `isee:*` sketch elements (button/knob/slider/graph/etc.) silently skipped.
- `test/xmile/ViewRoundTripTest.cpp`: round-trips `fishbanks` and `reliability` XMILE -> MDL, asserting the resulting `.mdl` re-parses with equivalent `VensimView` geometry.

**Dependencies:** Phases 3-4 (variables and flows must exist before they can be referenced from a view).

**Done when:** `fishbanks` and `reliability` produce valid `.mdl` with synthesized Vensim sketches that re-parse equivalently (covers **xmile-reader.AC4.1** through **xmile-reader.AC4.5**, **xmile-reader.AC3.4**).
<!-- END_PHASE_7 -->

<!-- START_PHASE_8 -->
### Phase 8: Corpus round-trip, error rejection, WASM verification
**Goal:** End-to-end validation across the corpus and the rejection contract.

**Components:**
- `test/xmile/CorpusRoundTripTest.cpp`: round-trip `fishbanks`, `logistic-growth`, `reliability` XMILE -> XMILE and XMILE -> MDL, comparing each via `ModelComparator`.
- `test/xmile/ErrorRejectTest.cpp`: hand-written XMILE fixtures with `<module>`, `<macro>`, multiple `<model>`, and a postfix `'` operator -- each expected to hard-error and report the offending element.
- CLI end-to-end smoke test (extend `test/cli_roundtrip.sh` or add `test/cli_xmile_roundtrip.sh`): file in -> output file -> re-parse.
- WASM build verification: re-run `./configure.sh` with the existing emscripten profile and confirm the WASM artifact still builds with the new sources. (No new WASM-specific code; the new files use only tinyxml2 and standard C++.)
- Verify `xmutil_test` (mdl tests) still passes (regression bar).
- Verify XMILE output for an existing MDL input is byte-identical to the pre-change output (no regression on `mdl -> xmile`).

**Dependencies:** Phases 1-7.

**Done when:** All three corpus models pass corpus round-trip; all four rejection cases hard-error; WASM build succeeds; pre-existing MDL tests pass; pre-existing `mdl -> xmile` output stays byte-identical (covers **xmile-reader.AC1.2**, **xmile-reader.AC1.3**, **xmile-reader.AC1.5**, **xmile-reader.AC5.1** through **xmile-reader.AC5.3**, **xmile-reader.AC6.1**, **xmile-reader.AC6.2**, **xmile-reader.AC6.3**).
<!-- END_PHASE_8 -->

## Additional Considerations

**Function-name un-renaming table.** The table inverts simlin's `xmile_to_mdl_function_name` (`writer.rs:194`), so a future addition on the simlin side has a known location to mirror. The bare-keyword set (`time`, `dt`, `initial_time`, `final_time`) is handled separately because those produce zero-arg `ExpressionFunction`s with no parentheses in Vensim (`Time`, `TIME STEP`, `INITIAL TIME`, `FINAL TIME`). Both halves live in `XmileFunctions.cpp` so the file is the single source of truth.

**Sim spec `dt` reciprocal.** XMILE's `<dt reciprocal="true">N</dt>` means `dt = 1 / N`. xmutil's `Model::set_dt` takes a double -- the reader resolves the reciprocal to its numeric value at read time. On output the XMILE writer always writes the resolved double; the MDL writer always writes the numeric value too. This is a documented one-way lossy mapping; the test comparator compares resolved doubles.

**Single-view limitation.** The XMILE spec allows multiple `<view>` elements per model. xmutil's `VensimView` model is similarly multi-view (the MDL writer can emit multiple sketch segments), but v1 of the reader takes the first `<view>` only and logs a warning for the rest. The corpus is single-view; multi-view fidelity is a deliberate fast-follow.

**Cloud handling.** XMILE expresses flow source/sink clouds either implicitly (a flow with no `<from>` or `<to>` stock attribute) or with explicit `<cloud>` view elements (rare; not in our corpus). xmutil has no dedicated Cloud class -- clouds are conveyed via `_hasUpstream` / `_hasDownstream` flags on the connected Variable, and the MDL writer's sketch emits clouds based on those flags. The XMILE reader sets the flags accordingly.

**XMILE -> MDL output filename.** The CLI replaces the input extension with `.mdl` for the output. An XMILE input that lives in a directory next to a `.mdl` file of the same base name would overwrite it. The existing self-overwrite convention (used by MDL -> MDL: `.regen.mdl`) does not apply here because the extensions differ. v1 accepts this risk; users can use `--stdio` or rename if it matters.

**Stella UI widgets** (`<button>`, `<knob>`, `<slider>`, `<graph>`, `<gauge>`, `<numeric_input>`, `<numeric_display>`, `<spatial_map>`, `isee:animation_object`, etc.) are silently dropped. None of them affect simulation semantics; they're UI annotations. If a downstream tool relies on them, that tool will see them missing from the regenerated XMILE -- a documented limitation, not a defect.

**WASM build.** xmutil's gyp emscripten profile picks up new sources in `common_sources` automatically; no per-target manual wiring is needed. The new files use only tinyxml2 (already in the WASM build) and standard C++; no new dependencies.

**Corpus narrowing rationale.** Of simlin's four default-project models, `population` was dropped because it uses real `<module>` submodel instances inside `<variables>`, which v1 hard-rejects. `reliability` contains `<module>` only inside `<style>` (a stylesheet for hypothetical module elements) and is kept. The v1 corpus is `fishbanks`, `logistic-growth`, `reliability`.
