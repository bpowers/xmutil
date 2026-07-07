# Vensim mdl Writer Implementation Plan — Phase 5: Sketch section

**Goal:** Re-serialize an existing `VensimView` back to Vensim sketch records (types 10/11/12/1) with correct coordinates, attached/ghost flags, connector links, and polarity; emit nothing geometric when no view exists. xmutil never synthesizes geometry.

**Architecture:** `MDLGenerator::GenerateSketch` (the Phase 4 stub) is replaced with a real emitter: for each `VensimView` it emits the `\\\---///` / `V300` / `*Title` / `$default` framing, then one record per non-NULL element of `vElements` (the array index is the on-wire UID), then the shared `///---\\\` terminator. Records mirror the parser's field layout exactly (`src/Vensim/VensimView.cpp`), emitting Vensim defaults for the many fields the parser discards. Coordinates are emitted as-is (the parsed view is already in Vensim coordinates; `SetViewStart` rescaling is XMILE-only and never runs on the `.mdl` path).

**Tech Stack:** C++17, gyp. No simlin port needed (this is xmutil-internal format mirroring); reference is the parser at `src/Vensim/VensimView.cpp:6-202` and `src/Vensim/VensimParse.cpp:275-322`.

**Scope:** Phase 5 of 7.

**Codebase verified:** 2026-05-22. Key facts:
- `Model::Views()` -> `std::vector<View*>&` (`src/Model.h:50-55`); xmutil only ever creates `VensimView` (`src/Vensim/VensimParse.cpp:287-289`); Dynamo creates none (empty `Views()`).
- `VensimView` (`src/Vensim/VensimView.h:131-166`): `VensimViewElements &Elements()` (a `std::vector<VensimViewElement*>`, **sparse** — holes are NULL; the **array index is the UID** referenced by connectors); `const std::string &Title()` **already exists at `VensimView.h:133-135`** (returns `sTitle`, set via `SetTitle(buf+1)`).
- `VensimViewElement` (`VensimView.h:13-53`): `ElementType Type()` (`ElementTypeVARIABLE/VALVE/COMMENT/CONNECTOR`); `X()/Y()/Width()/Height()`.
- `VensimVariableElement` (`VensimView.h:55-81`): `GetVariable()`, `Attached()`, ghost via `Ghost(nullptr)` / `_ghost`.
- `VensimValveElement` (`VensimView.h:82-94`): `Attached()`.
- `VensimCommentElement` (`VensimView.h:95-101`): geometry only.
- `VensimConnectorElement` (`VensimView.h:102-129`): `From()`, `To()` (UIDs), `Polarity()` (a `char`: `'+'`, `'-'`, or other), single point `(X(),Y())`, `_npoints==1`. `Invalidate()` sets `From()==To()==0` — **skip those**.
- Parser framing (`VensimParse.cpp:275-322`): `\\\---///` opener, then a `V300 `/`V364 ` version line, then a `*`-prefixed title line (`SetTitle(buf+1)`), then a `$...` default-font line (parsed for ppi then discarded — emit a fixed default), then element records via `ReadView`, until a non-digit line; multiple views each re-open with `\\\---///`; the section ends with `///---\\\`, after which the `:L...` settings (Phase 4) are read.
- `ReadView` (`VensimView.cpp:163-202`): every record is `type,uid,...`; dispatch `10`=variable, `11`=valve, `12`=comment/cloud, `1`=connector; `30` ignored; **any other code aborts in debug** — emit only 10/11/12/1.
- Record field layouts (parser reads only the listed fields; rest discarded):
  - **10 variable** (`VensimView.cpp:6-49`): `10,uid,name,x,y,width,height,shape,bits,...`. `_attached = (shape & (1<<5))`; `_ghost = !(bits & 1)` (**inverted**: bit0 set => NOT a ghost). Resolves its `Variable*` by **name** via `FindVariable(name)`.
  - **11 valve** (`VensimView.cpp:84-98`): `11,uid,name,x,y,width,height,shape`. `_attached = (shape & (1<<5))`. Name discarded.
  - **12 comment/cloud** (`VensimView.cpp:64-82`): `12,uid,name,x,y,width,height,shape,bits`. If `bits & (1<<2)` the parser consumes the **next line** as scratch text — so emit `bits` with bit2 **clear** and no trailing text line. Name discarded.
  - **1 connector** (`VensimView.cpp:115-144`): after `1,uid`: `from,to,<ignored>,<ignored>,POL,<6 ignored fields incl. RGB and an empty>,N|(x,y)|`. `POL` is the polarity char's **ASCII code as a decimal int**: `43`=`+`, `45`=`-`, `0`/other = none. The trailing `N|(x,y)|` (`sscanf("%d|(%d,%d)")`) supplies `_npoints` (forced to 1) and the single point.
- Coordinates: the parsed view holds original Vensim coordinates; `VensimView::SetViewStart` (rescaling) is called only from `XMILEGenerator` (`XMILEGenerator.cpp:672,842`), never on the `.mdl` path. Emit `X()/Y()/Width()/Height()` directly.
- Name quoting in records: `GetString` requires a quoted name's closing `"` be immediately followed by `,` and escapes interior `"` as `\"`. Use `mdl::FormatMDLIdent` for the variable name (so the parser's `FindVariable` matches the equation-section name).

---

## Acceptance Criteria Coverage

This phase implements and tests:

### mdl-writer.AC3: Round-trip preserves the model
- **mdl-writer.AC3.4 Success:** `VensimView` geometry retained by xmutil (variables, valves, clouds, connectors, positions, polarity) is equivalent across the round-trip.

### mdl-writer.AC4: Sketch policy honors "no view generation"
- **mdl-writer.AC4.1 Success:** Vensim input with a sketch produces a sketch section that re-parses to an equivalent `VensimView`.
- **mdl-writer.AC4.2 Success:** Input without a `VensimView` (Dynamo or external consumer) produces no/empty sketch section and still parses.
- **mdl-writer.AC4.3 Constraint:** No diagram geometry is fabricated: variables absent from the original sketch are not assigned synthetic coordinates.

---

<!-- START_SUBCOMPONENT_A (tasks 1-2) -->
<!-- START_TASK_1 -->
### Task 1: element record emitters

**Verifies:** mdl-writer.AC3.4

**Files:**
- Modify: `src/Mdl/MDLGenerator.{h,cpp}` (add per-element record emitters)

**Implementation:**

`VensimView::Title()` already exists (`VensimView.h:133-135`) — use it directly; no header change is needed. If `sTitle` is empty, the emitter falls back to a default like `View 1`.

Add private record-emitter helpers in `MDLGenerator` (each appends one record line, `\n`-terminated; the field semantics are stated above). Use the observed Vensim default templates (verify against `third_party/simlin/.../pysimlin/tests/fixtures/teacup.mdl` and `.../simlin-serve/tests/fixtures/SIR.mdl`):

```cpp
// 10,uid,name,x,y,w,h,shape,bits,0,0,0,0,0,0
void EmitVariableRecord(std::string &out, int uid, VensimVariableElement *e) {
  int shape = 3; if (e->Attached()) shape |= (1 << 5);          // bit5 = attached
  int bits = e->Ghost(nullptr) ? 2 : 3;                          // bit0 set => NOT ghost
  out += "10," + std::to_string(uid) + "," + mdl::FormatMDLIdent(e->GetVariable()->GetName()) + ",";
  out += std::to_string(e->X()) + "," + std::to_string(e->Y()) + "," +
         std::to_string(e->Width()) + "," + std::to_string(e->Height()) + ",";
  out += std::to_string(shape) + "," + std::to_string(bits) + ",0,0,0,0,0,0\n";
}
// 11,uid,0,x,y,w,h,shape
void EmitValveRecord(std::string &out, int uid, VensimValveElement *e) {
  int shape = e->Attached() ? 34 : 2;                            // bit5 = attached
  out += "11," + std::to_string(uid) + ",0," + std::to_string(e->X()) + "," + std::to_string(e->Y()) +
         "," + std::to_string(e->Width()) + "," + std::to_string(e->Height()) + "," + std::to_string(shape) + "\n";
}
// 12,uid,0,x,y,w,h,shape,bits  (bits bit2 CLEAR -> no trailing scratch line)
void EmitCommentRecord(std::string &out, int uid, VensimCommentElement *e) {
  out += "12," + std::to_string(uid) + ",0," + std::to_string(e->X()) + "," + std::to_string(e->Y()) +
         "," + std::to_string(e->Width()) + "," + std::to_string(e->Height()) + ",8,0\n";
}
// 1,uid,from,to,1,0,POL,0,0,64,0,-1--1--1,,1|(x,y)|
void EmitConnectorRecord(std::string &out, int uid, VensimConnectorElement *e) {
  if (e->From() == 0 && e->To() == 0) return;                    // invalidated -> skip
  int pol = 0; char p = e->Polarity(); if (p == '+') pol = 43; else if (p == '-') pol = 45;
  out += "1," + std::to_string(uid) + "," + std::to_string(e->From()) + "," + std::to_string(e->To()) +
         ",1,0," + std::to_string(pol) + ",0,0,64,0,-1--1--1,,1|(" + std::to_string(e->X()) + "," +
         std::to_string(e->Y()) + ")|\n";
}
```
The non-`bit5`/non-`bit0` portions of `shape`/`bits` and the connector's `1,0,...,0,64,0,-1--1--1,` fields are Vensim cosmetic defaults the parser ignores; **the parser-meaningful bits are exactly: attached (`shape` bit5), ghost (`bits` bit0, inverted), polarity (`43`/`45`/`0`), and the `from`/`to`/coordinates.** Verify the chosen defaults re-parse with no `assert` (debug build) on the corpus fixtures.

**Testing:** Covered by Task 3 round-trip tests.

**Commit:** `feat(mdl): sketch record emitters`
<!-- END_TASK_1 -->

<!-- START_TASK_2 -->
### Task 2: `GenerateSketch` — framing + per-view records

**Verifies:** mdl-writer.AC3.4, mdl-writer.AC4.1, mdl-writer.AC4.2, mdl-writer.AC4.3

**Files:**
- Modify: `src/Mdl/MDLGenerator.cpp` (replace the Phase 4 `GenerateSketch` stub)

**Implementation:**

```cpp
void MDLGenerator::GenerateSketch(std::string &out) {
  const char *kVersion = "V300  Do not put anything below this section - it will be ignored";
  const char *kFontLine = "$192-192-192,0,Helvetica|10|B|0-0-0|0-0-0|-1--1--1|-1--1--1|96,96,100,0";
  std::vector<VensimView *> views;
  for (View *v : _model->Views()) {
    VensimView *vv = dynamic_cast<VensimView *>(v);   // xmutil only creates VensimView
    if (vv) views.push_back(vv);
  }
  if (views.empty()) {
    // No geometry to serialize: emit a single empty frame so the ///---\\\
    // terminator and the settings section re-parse. Fabricates NO coordinates
    // (zero element records) -> satisfies AC4.2/AC4.3.
    out += "\\\\\\---///\n";
    out += std::string(kVersion) + "\n";
    out += "*View 1\n";
    out += std::string(kFontLine) + "\n";
    out += "///---\\\\\\\n";
    return;
  }
  for (VensimView *vv : views) {
    out += "\\\\\\---///\n";
    out += std::string(kVersion) + "\n";
    std::string title = vv->Title().empty() ? "View 1" : vv->Title();
    out += "*" + title + "\n";
    out += std::string(kFontLine) + "\n";
    VensimView::VensimViewElements &elems = vv->Elements();
    for (size_t uid = 0; uid < elems.size(); uid++) {
      VensimViewElement *e = elems[uid];
      if (!e) continue;                                  // preserve sparse index numbering
      switch (e->Type()) {
        case VensimViewElement::ElementTypeVARIABLE:
          EmitVariableRecord(out, (int)uid, static_cast<VensimVariableElement *>(e)); break;
        case VensimViewElement::ElementTypeVALVE:
          EmitValveRecord(out, (int)uid, static_cast<VensimValveElement *>(e)); break;
        case VensimViewElement::ElementTypeCOMMENT:
          EmitCommentRecord(out, (int)uid, static_cast<VensimCommentElement *>(e)); break;
        case VensimViewElement::ElementTypeCONNECTOR:
          EmitConnectorRecord(out, (int)uid, static_cast<VensimConnectorElement *>(e)); break;
      }
    }
  }
  out += "///---\\\\\\\n";   // single terminator after all views
}
```
Key invariants:
- The UID emitted is the `vElements` index, so connector `From()`/`To()` (which are indices) stay valid after re-parse. NULL slots are skipped but their index is preserved (the next non-NULL keeps its true index).
- Coordinates are emitted verbatim (no rescaling).
- No element is invented for variables not already in the view (AC4.3).

**Testing:** Task 3.

**Commit:** `feat(mdl): re-serialize VensimView sketch records`
<!-- END_TASK_2 -->
<!-- END_SUBCOMPONENT_A -->

<!-- START_TASK_3 -->
### Task 3: Sketch round-trip tests + comparator view strengthening

**Verifies:** mdl-writer.AC3.4, mdl-writer.AC4.1, mdl-writer.AC4.2, mdl-writer.AC4.3

**Files:**
- Create: `test/mdl/SketchRoundTripTest.cpp` (added to `xmutil_test`)
- Modify: `test/mdl/ModelComparator.cpp` (finalize view comparison)

**Implementation:**

Finalize `ModelComparator` view comparison (the framework was added in Phase 1, "ignore empty views" in Phase 4):
- Compare `Model::Views()` filtered to `VensimView` with >0 elements. For each view (matched by order), compare `Elements().size()` and, **by UID index**, each non-NULL element: `Type()`, `X()/Y()/Width()/Height()`; for variable elements the resolved `GetVariable()->GetName()` plus `Attached()` and `Ghost(nullptr)`; for connectors `From()/To()` and `Polarity()`. Report a descriptive diff on any mismatch. Comparing by index is valid because the writer preserves UID numbering.

**Testing (describe):**
- **AC4.1 / AC3.4 (sketched Vensim model):** Use `third_party/simlin/.../pysimlin/tests/fixtures/teacup.mdl` (small, has a sketch) and `.../simlin-serve/tests/fixtures/SIR.mdl` (stocks/flows/valves/clouds/connectors with `+`/`-` polarity). `roundtrip::RoundTripDiffs` returns empty — element types, positions, connector from/to, and polarity all match. Add a focused assertion that at least one connector's polarity (`+` and `-`) survives.
- **AC4.2 (no view):** A small inline `.mdl` with equations but no sketch -> round-trip diffs empty (the empty frame re-parses; the comparator ignores the empty view). A Dynamo model is exercised in Phase 7.
- **AC4.3 (no fabrication):** Assert that for a model whose sketch references only some variables, the round-tripped view has the **same element count** (no new variable elements invented).

**Verification:** `out/Debug/xmutil_test` passes all sketch round-trip tests.

**Commit:** `test(mdl): sketch round-trip tests and view comparator`
<!-- END_TASK_3 -->

## Phase 5 Done When
- Round-trip tests confirm equivalent `VensimView` geometry for sketched Vensim models, and models without a `VensimView` emit no/empty sketch yet still parse (AC4.1, AC4.2, AC4.3, AC3.4).
