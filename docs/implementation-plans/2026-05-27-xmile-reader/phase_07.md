# XMILE Reader Implementation Plan — Phase 7

**Goal:** XMILE `<view>` elements populate `VensimView` geometry (variables, valves+flows, aliases, connectors, clouds, groups) so the MDL writer's sketch emitter produces a faithful Vensim diagram on XMILE → MDL.

**Architecture:** A new `XmileView` component (`src/Xmile/XmileView.{h,cpp}`) walks a single `<view>` element. For each child:
- `<stock>` / `<aux>` → `VensimVariableElement` at `(x, y)` for the resolved Variable.
- `<flow>` → a `VensimValveElement` at the flow's `(x, y)` immediately followed (UID = valve_uid + 1) by a `VensimVariableElement` for the flow's label. Plus pipe `VensimConnectorElement`s to the source/sink ends defined by the `<pts>` children.
- `<alias>` → a ghost `VensimVariableElement` (`_ghost` set).
- `<connector>` → a `VensimConnectorElement(from_uid, to_uid, x, y, polarity)`.
- **Clouds** (a flow `<pt>` that doesn't match any stock position) → `VensimCommentElement` (type 12) at the cloud position — the MDL writer recognizes this shape as a cloud endpoint.
- `<group>` → handled by the Phase 3 `ProcessGroup` helper (already wired).

UID allocation is sequential array indices in emit order. Connector `from`/`to` UIDs resolve via a name-keyed lookup map built in a first pass.

`XmileReader::ProcessViews` (Phase 3 stub) is replaced with a real walker that constructs and populates one `XmileView` per `<view>` element. v1 processes the first `<view>` fully; subsequent views are noted as a soft warning.

**Tech Stack:** Phases 1-6, plus the `VensimView` / `VensimViewElement` API in `src/Vensim/VensimView.h`. Two small extensions to the existing element classes (a programmatic `VensimValveElement` constructor, a polarity-bearing `VensimConnectorElement` constructor) keep the XMILE path symmetric with the Vensim parser path.

**Scope:** Phase 7 of 8.

**Codebase verified:** 2026-05-28

---

## Acceptance Criteria Coverage

This phase implements and tests:

### xmile-reader.AC3: Round-trip preserves the model
- **xmile-reader.AC3.4 Guard:** The `ModelComparator` (reused from `test/mdl/`) detects a deliberately introduced non-equivalence on the XMILE round-trip; the round-trip test cannot pass vacuously. *(Implemented as a "negative control" test that hand-corrupts the Model post-parse and asserts the comparator catches it.)*

### xmile-reader.AC4: Sketch synthesis from XMILE views
- **xmile-reader.AC4.1 Success:** XMILE `<view>` elements with stocks, flows, auxes, connectors, and aliases produce `VensimView` geometry that the MDL writer's sketch emitter consumes without error; the resulting `.mdl` re-parses with equivalent geometry.
- **xmile-reader.AC4.2 Success:** Connector polarity (`positive` → `+`, `negative` → `-`, absent → none) round-trips XMILE → MDL → re-parse correctly.

  **Correction surfaced by codebase investigation:** XMILE's `polarity` attribute value is the literal character `+` or `-`, not the strings `"positive"` / `"negative"`. The XMILE writer emits `_polarity` directly as a single-char attribute (`XMILEGenerator.cpp:1072-1077`); the reader's parser reads the attribute and stores the same char on the connector's `_polarity` field.

- **xmile-reader.AC4.3 Success:** A flow whose source or sink is a cloud (implicit, via missing `<from>` / `<to>` stock attribute) sets the connected Variable's `_hasUpstream` / `_hasDownstream` flags so the MDL writer emits the correct flow structure.

  **Correction surfaced by codebase investigation:** `_hasUpstream` / `_hasDownstream` are set by `is_all_plus_minus` during equation-level stock-flow decomposition (`src/Symbol/Expression.cpp:86-132`); they have no role in sketch geometry. The MDL writer represents clouds as `VensimCommentElement` (type 12) nodes inside the view's element vector, and the XMILE writer recognizes `ElementTypeCOMMENT` as a valid flow endpoint (`XMILEGenerator.cpp:979`). **The plan implements clouds as `VensimCommentElement` nodes, not via the upstream/downstream flags.** The intent of AC4.3 — "clouds round-trip correctly via the sketch path" — is preserved; only the mechanism is corrected.

- **xmile-reader.AC4.4 Edge:** A model with multiple `<view>` elements takes the first; subsequent views are noted via `errs` as a soft warning and skipped.
- **xmile-reader.AC4.5 Success:** XMILE `<group>` elements become `ModelGroup`s; their members are emitted under group banners on the MDL output path. *(Phase 3 implemented the `ProcessGroup` helper; Phase 7 wires the view walk to call it.)*

---

## Codebase verification findings

- ✓ `VensimView` (`src/Vensim/VensimView.h:137-172`) extends `View`. Public methods: `SetTitle`, `Title`, `GetNextUID()` (first-NULL slot scan, auto-resizes), `Elements()` (mutable vector access), `AddVarDefinition(Variable*, x, y)` (creates and slots a `VensimVariableElement`), `FindVariable(Variable*, x, y)` (lookup-or-create), `AddFlowDefinition(Variable*, upstream, downstream)`.
- ✓ `VensimVariableElement` programmatic constructor: `VensimVariableElement(VensimView *view, Variable *var, int x, int y)` (`src/Vensim/VensimView.cpp:50-62`). Sets `_x`, `_y`, `_variable`, and derives `_ghost = (var->GetView() != NULL)` — a second reference to the same Variable becomes a ghost. Calls `_variable->SetView(view)` on first attachment.
- ✗ `VensimValveElement` has **no** programmatic constructor — only a parse-from-sketch ctor. Phase 7 needs `VensimValveElement(VensimView *, int x, int y)`. Implementation: zero-init `_attached`, set `_x`, `_y`. **Add this constructor in Task 1.**
- ✗ `VensimValveElement` carries **no** `Variable *` field. The association between a valve and its flow Variable is purely positional: the valve's UID is exactly one less than the flow's variable UID in the `Elements()` vector. The XMILE writer relies on `elements[local_uid - 1]` to find a flow variable's preceding valve. **Phase 7 must allocate the valve and the flow's variable in adjacent slots (valve at UID N, variable at UID N+1).**
- ✓ `VensimConnectorElement` programmatic constructor: `VensimConnectorElement(int from, int to, int x, int y)` (`VensimView.cpp:146-152`). Sets `_from`, `_to`, `_npoints=1`, `_x`, `_y`. **Does not set `_polarity`.** Phase 7 needs either a 5-arg overload `VensimConnectorElement(int from, int to, int x, int y, char polarity)` or a public `SetPolarity(char)` setter. **Add the 5-arg overload in Task 1.**
- ✓ `VensimCommentElement` (type 12) is what the MDL writer / XMILE writer use to represent a cloud at a specific position. The XMILE writer's flow-pipe emission (`XMILEGenerator.cpp:979`) treats `ElementTypeCOMMENT` at a pipe endpoint as a cloud (a flow source or sink "outside the model boundary"). **Phase 7 uses `VensimCommentElement` for cloud endpoints, not `_hasUpstream` / `_hasDownstream`.**
- ✓ MDL sketch wire format (`src/Mdl/MDLGenerator.cpp:312-377`): each non-empty `VensimView` emits a `\\\---///` header, the view title, default styles, then per-element records (`10` variable, `11` valve, `12` comment, `1` connector) in UID order. The UID IS the array index. NULL slots in the vector are skipped in emission but their indices are still consumed. Closes with `///---\\\`.
- ✓ Polarity ASCII codes in the MDL wire format: `43` for `+`, `45` for `-`, `0` for none (`MDLGenerator.cpp:300-306`). XMILE attribute is the literal `+` / `-` char.
- ✓ Fishbanks corpus view structure:
  ```xml
  <view view_type="stock_flow">
    <stock name="Fish stock" x="703.84" y="199.0" label_side="top"/>
    <flow name="New fish per year" x="602.73" y="197.56" label_side="top">
      <pts>
        <pt x="513.6" y="197.56"/>
        <pt x="681.34" y="197.56"/>
      </pts>
    </flow>
    <aux name="Net regeneration" x="594.32" y="276.24" label_side="left"/>
    <connector angle="57.31">
      <from>Maximum\nfishery size</from>
      <to>Fish density</to>
    </connector>
  </view>
  ```
  Positions are floating-point screen coordinates. `<flow>` mandates a `<pts>` child with exactly 2 `<pt>`s (source and sink, in that order). `<connector>` has `<from>` and `<to>` children with the source/target variable names as text content. `<connector>` may have a `polarity="+|-"` attribute (absent in fishbanks; no polarity).
- ✓ Reliability and SIR corpora use `<alias>` with `<of>name</of>` child and `uid=` attribute:
  ```xml
  <alias x="291" y="453" uid="27"><of>Contact_Rate_c</of></alias>
  ```
  Aliases carry their own UID in XMILE (so connectors can reference them); the reader assigns its own sequential UID and builds a uid-attribute → reader-UID map to resolve `<from><alias uid="N"/></from>` references.
- ✓ Cloud detection: a flow `<pt>` whose position doesn't match any `<stock>` or `<aux>` position in the view is a cloud. Compare positions with a small epsilon (e.g., `±2.0` pixels) to absorb floating-point rounding.
- ✓ View-level `<group>` (`name`, `x`, `y`, `width`, `height` attributes with `<item uid="N"/>` children) is the layout-grouping form. Phase 3's `ProcessGroup` already handles the `<var>name</var>`-child form used by the writer's `generateSectorViews` path. The `<item uid="N"/>` form is a different shape — Phase 7 adds support: look up each UID in the per-view UID map and find the corresponding Variable.
- ✓ The XMILEGenerator emits **one** `<view>` per `VensimView` and concatenates multiple views into a single output `<view>` when the view list is non-empty (`XMILEGenerator.cpp:830-867`). So a multi-view XMILE input that the v1 reader collapses to one view will round-trip correctly enough that the comparator passes (positions may shift but the structure is equivalent).

---

## XmileView design

### Class shape

```cpp
// src/Xmile/XmileView.h
#pragma once
#include <string>
#include <unordered_map>
#include <vector>

namespace tinyxml2 { class XMLElement; }
class Model;
class SymbolNameSpace;
class Variable;
class VensimView;
class XmileReader;

class XmileView {
public:
  XmileView(XmileReader *reader, Model *model, VensimView *view);
  bool ProcessView(tinyxml2::XMLElement *viewEl, std::vector<std::string> &errs);

private:
  // Pass 1: walk children, allocate elements, build name -> UID maps.
  bool AllocateElements(tinyxml2::XMLElement *viewEl, std::vector<std::string> &errs);
  // Pass 2: walk <connector> children, resolve from/to names to UIDs.
  bool ResolveConnectors(tinyxml2::XMLElement *viewEl, std::vector<std::string> &errs);
  // Pass 3: walk <group> children with view-level UID maps available.
  bool ProcessViewGroups(tinyxml2::XMLElement *viewEl, std::vector<std::string> &errs);

  // Helpers
  int AllocateVariableElement(Variable *var, int x, int y);
  int AllocateValveAndVariable(Variable *flowVar, int x, int y);  // returns the VARIABLE UID
  int AllocateCloud(int x, int y);                                // VensimCommentElement
  int AllocateGhost(Variable *var, int x, int y);                 // VensimVariableElement w/ _ghost=true
  int AllocateConnector(int fromUid, int toUid, int midX, int midY, char polarity);
  // Find a previously-allocated UID by variable name (case-insensitive); -1 if missing.
  int FindUIDByName(const std::string &normName) const;
  // Find a cloud endpoint matching a position; allocates a new comment element
  // if no nearby endpoint exists.
  int ResolveFlowEndpoint(double x, double y);

  XmileReader *_reader;
  Model *_model;
  SymbolNameSpace *_sns;
  VensimView *_view;
  // Case-insensitive name -> UID (sequential index into VensimView::Elements()).
  std::unordered_map<std::string, int> _nameToUid;
  // For <alias uid="N"> -> our internal UID: XMILE's per-file uid -> our sequential uid.
  std::unordered_map<int, int> _xmileUidToOurUid;
  // Variable position map (for cloud detection): variable name -> (x, y).
  std::unordered_map<std::string, std::pair<int, int>> _varPositions;
};
```

### Pass-1 element allocation

```cpp
bool XmileView::AllocateElements(tinyxml2::XMLElement *viewEl,
                                 std::vector<std::string> &errs) {
  for (tinyxml2::XMLElement *child = viewEl->FirstChildElement(); child;
       child = child->NextSiblingElement()) {
    if (XmileReader::IsForeignNamespace(child->Name())) continue;
    std::string tag(child->Name());
    if (tag == "stock" || tag == "aux") {
      const char *name = child->Attribute("name");
      if (!name) continue;
      std::string norm = XmileReader::NormalizeName(name);
      Variable *v = _reader->InsertVariable(norm);
      int x = static_cast<int>(child->DoubleAttribute("x", 0.0));
      int y = static_cast<int>(child->DoubleAttribute("y", 0.0));
      int uid = AllocateVariableElement(v, x, y);
      _nameToUid[norm] = uid;
      _varPositions[norm] = {x, y};
      if (const char *xmileUid = child->Attribute("uid")) {
        _xmileUidToOurUid[atoi(xmileUid)] = uid;
      }
    } else if (tag == "flow") {
      const char *name = child->Attribute("name");
      if (!name) continue;
      std::string norm = XmileReader::NormalizeName(name);
      Variable *v = _reader->InsertVariable(norm);
      int x = static_cast<int>(child->DoubleAttribute("x", 0.0));
      int y = static_cast<int>(child->DoubleAttribute("y", 0.0));
      // Allocate valve + variable in adjacent UID slots.
      int varUid = AllocateValveAndVariable(v, x, y);
      _nameToUid[norm] = varUid;
      _varPositions[norm] = {x, y};
      if (const char *xmileUid = child->Attribute("uid")) {
        _xmileUidToOurUid[atoi(xmileUid)] = varUid;
      }
      // Pipe endpoints: <pts><pt x= y=/><pt x= y=/></pts>.
      tinyxml2::XMLElement *pts = child->FirstChildElement("pts");
      if (pts) {
        std::vector<std::pair<int, int>> endpoints;
        for (tinyxml2::XMLElement *pt = pts->FirstChildElement("pt"); pt;
             pt = pt->NextSiblingElement("pt")) {
          endpoints.emplace_back(static_cast<int>(pt->DoubleAttribute("x", 0.0)),
                                 static_cast<int>(pt->DoubleAttribute("y", 0.0)));
        }
        if (endpoints.size() == 2) {
          int srcUid = ResolveFlowEndpoint(endpoints[0].first, endpoints[0].second);
          int dstUid = ResolveFlowEndpoint(endpoints[1].first, endpoints[1].second);
          int valveUid = varUid - 1;
          // Pipe from source -> valve -> sink. Polarity is none for pipes.
          AllocateConnector(srcUid, valveUid, (endpoints[0].first + x) / 2,
                            (endpoints[0].second + y) / 2, 0);
          AllocateConnector(valveUid, dstUid, (x + endpoints[1].first) / 2,
                            (y + endpoints[1].second) / 2, 0);
        } else {
          errs.push_back(std::string("<flow name=\"") + name +
                         "\">: <pts> does not have exactly 2 <pt> children");
        }
      }
    } else if (tag == "alias") {
      const char *xmileUid = child->Attribute("uid");
      // Resolve the aliased Variable from <of>name</of>.
      tinyxml2::XMLElement *ofEl = child->FirstChildElement("of");
      if (!ofEl || !ofEl->GetText()) continue;
      std::string ofName = XmileReader::NormalizeName(ofEl->GetText());
      // Aliases use canonical names with underscores; convert back to spaces.
      // (XMILE writes Foo_Bar; xmutil stores "Foo Bar".)
      std::replace(ofName.begin(), ofName.end(), '_', ' ');
      Variable *v = _reader->InsertVariable(ofName);
      int x = static_cast<int>(child->DoubleAttribute("x", 0.0));
      int y = static_cast<int>(child->DoubleAttribute("y", 0.0));
      int uid = AllocateGhost(v, x, y);
      if (xmileUid) _xmileUidToOurUid[atoi(xmileUid)] = uid;
    } else if (tag == "connector") {
      // Defer to pass 2 — names need to be resolved against the full name map.
      continue;
    } else if (tag == "group") {
      // Defer to pass 3.
      continue;
    } else {
      // isee:* and unknown elements skipped silently.
    }
  }
  return true;
}
```

### Cloud handling

```cpp
int XmileView::ResolveFlowEndpoint(double x, double y) {
  // First, look for a stock/aux at the position (within tolerance).
  for (const auto &kv : _varPositions) {
    int vx = kv.second.first;
    int vy = kv.second.second;
    if (std::abs(vx - x) <= 4 && std::abs(vy - y) <= 4) {
      auto it = _nameToUid.find(kv.first);
      if (it != _nameToUid.end()) return it->second;
    }
  }
  // No match -> synthesize a VensimCommentElement (cloud) at the position.
  return AllocateCloud(static_cast<int>(x), static_cast<int>(y));
}
```

The position-tolerance constant (4 pixels) is a heuristic. The fishbanks corpus has `<flow>` endpoints that match `<stock>` positions exactly (same x, different y, same x); a small tolerance handles minor float→int rounding from re-parsed coordinates without false-matching distant elements.

### Connector pass

```cpp
bool XmileView::ResolveConnectors(tinyxml2::XMLElement *viewEl,
                                  std::vector<std::string> &errs) {
  for (tinyxml2::XMLElement *child = viewEl->FirstChildElement("connector"); child;
       child = child->NextSiblingElement("connector")) {
    if (XmileReader::IsForeignNamespace(child->Name())) continue;
    // Polarity: literal '+' or '-' char. Absent -> 0 (none).
    char polarity = 0;
    if (const char *p = child->Attribute("polarity")) {
      if (p[0] == '+' || p[0] == '-') polarity = p[0];
    }
    tinyxml2::XMLElement *fromEl = child->FirstChildElement("from");
    tinyxml2::XMLElement *toEl = child->FirstChildElement("to");
    if (!fromEl || !toEl) {
      errs.push_back("<connector> missing <from> or <to>");
      continue;
    }
    int fromUid = ResolveEndpoint(fromEl);
    int toUid = ResolveEndpoint(toEl);
    if (fromUid < 0 || toUid < 0) {
      errs.push_back("<connector> references unknown variable");
      continue;
    }
    int midX = static_cast<int>(child->DoubleAttribute("x", 0.0));
    int midY = static_cast<int>(child->DoubleAttribute("y", 0.0));
    AllocateConnector(fromUid, toUid, midX, midY, polarity);
  }
  return true;
}

// Helper: resolve <from>name</from> OR <from><alias uid="N"/></from>.
int XmileView::ResolveEndpoint(tinyxml2::XMLElement *endpoint) {
  // Check for nested <alias uid="N"/>.
  if (tinyxml2::XMLElement *alias = endpoint->FirstChildElement("alias")) {
    if (const char *uidStr = alias->Attribute("uid")) {
      auto it = _xmileUidToOurUid.find(atoi(uidStr));
      if (it != _xmileUidToOurUid.end()) return it->second;
    }
  }
  // Otherwise, text content is the variable name.
  const char *text = endpoint->GetText();
  if (!text) return -1;
  std::string norm = XmileReader::NormalizeName(text);
  auto it = _nameToUid.find(norm);
  return it == _nameToUid.end() ? -1 : it->second;
}
```

### Group pass (Phase 7's piece)

For `<group>` with `<item uid="N"/>` children (the view-level layout-group form used by Stella), look up each item's variable via `_xmileUidToOurUid` and add it to the `ModelGroup`:

```cpp
bool XmileView::ProcessViewGroups(tinyxml2::XMLElement *viewEl,
                                  std::vector<std::string> &errs) {
  for (tinyxml2::XMLElement *child = viewEl->FirstChildElement("group"); child;
       child = child->NextSiblingElement("group")) {
    const char *name = child->Attribute("name");
    if (!name) continue;
    std::string norm = XmileReader::NormalizeName(name);
    ModelGroup *group = nullptr;
    for (ModelGroup *g : _model->Groups()) {
      if (g->sName == norm) { group = g; break; }
    }
    if (!group) {
      group = new ModelGroup(norm, /*owner=*/nullptr);
      _model->Groups().push_back(group);
    }
    // <item uid="N"/> children: resolve each UID -> Variable -> group membership.
    for (tinyxml2::XMLElement *item = child->FirstChildElement("item"); item;
         item = item->NextSiblingElement("item")) {
      const char *uidStr = item->Attribute("uid");
      if (!uidStr) continue;
      auto it = _xmileUidToOurUid.find(atoi(uidStr));
      if (it == _xmileUidToOurUid.end()) continue;
      VensimViewElement *e = _view->Elements()[it->second];
      VensimVariableElement *ve = dynamic_cast<VensimVariableElement *>(e);
      if (!ve) continue;
      Variable *v = ve->GetVariable();
      if (!v) continue;
      v->SetGroup(group);
      group->vVariables.push_back(v);
    }
    // <var>name</var> child form (writer-emitted) is already handled by
    // ProcessGroup; call it for completeness:
    if (child->FirstChildElement("var")) {
      _reader->ProcessGroup(child, errs);
    }
  }
  return true;
}
```

---

<!-- START_SUBCOMPONENT_A (tasks 1-2) -->

<!-- START_TASK_1 -->
### Task 1: Extend VensimView element constructors

**Verifies:** Infrastructure for AC4.1-AC4.5. No direct test; verified by Task 5's round-trip tests.

**Files:**
- Modify: `src/Vensim/VensimView.h` (declare new ctors)
- Modify: `src/Vensim/VensimView.cpp` (implement)

**Implementation notes:**

Add two new constructors:

```cpp
// VensimView.h additions (near existing constructors):

class VensimValveElement {
public:
  // Existing parse-from-sketch ctor stays.
  VensimValveElement(char *curpos, char *buf, VensimParse *parser);
  // NEW: programmatic ctor — used by the XMILE reader path.
  VensimValveElement(int x, int y);
  // ...
};

class VensimConnectorElement {
public:
  // Existing ctors stay.
  VensimConnectorElement(char *curpos, char *buf, VensimParse *parser);
  VensimConnectorElement(int from, int to, int x, int y);
  // NEW: 5-arg overload with explicit polarity.
  VensimConnectorElement(int from, int to, int x, int y, char polarity);
  // ...
};
```

Implementations:

```cpp
// VensimView.cpp additions:

VensimValveElement::VensimValveElement(int x, int y) {
  _x = x;
  _y = y;
  _width = 0;
  _height = 0;
  _attached = false;
}

VensimConnectorElement::VensimConnectorElement(int from, int to, int x, int y, char polarity) {
  _from = from;
  _to = to;
  _x = x;
  _y = y;
  _npoints = 1;
  _polarity = polarity;
}
```

Verify the existing 4-arg ctor doesn't initialize `_polarity` (the design discovered it doesn't). Either fix that ctor to default `_polarity = 0` or document it as a "use the 5-arg form" preference. The fix is one-character: add `_polarity = 0;` to the existing body. **Make the fix as part of this task** — leaving uninitialized memory is a latent bug.

**Verification:**
- `ninja -C out/Debug XMUtil && ninja -C out/Debug xmutil_test` succeed.
- All existing tests (especially the Vensim sketch round-trip tests in `test/mdl/SketchRoundTripTest.cpp`) still pass.

**Commit:** `feat(vensim): programmatic VensimValveElement + polarity-bearing connector ctors`
<!-- END_TASK_1 -->

<!-- START_TASK_2 -->
### Task 2: XmileView class — element allocation + cloud synthesis

**Verifies:** AC4.1 (variables + clouds), AC4.3 (cloud round-trip via VensimCommentElement).

**Files:**
- Create: `src/Xmile/XmileView.h`
- Create: `src/Xmile/XmileView.cpp`
- Modify: `XMUtil.gyp` (`common_sources`: add the two new files)
- Modify: `src/Xmile/XmileReader.h` (expose `NormalizeName`, `IsForeignNamespace`, `ProcessGroup` to `XmileView` — already done in earlier phases; verify)

**Implementation notes:**

Implement the `XmileView` class per the design above. Key methods:

- Constructor takes a fresh `VensimView*` allocated by the caller (`ProcessViews` in Task 4). `XmileView` populates it; it doesn't own it.
- `AllocateVariableElement(Variable*, x, y)`: `int uid = _view->GetNextUID(); _view->Elements()[uid] = new VensimVariableElement(_view, var, x, y); return uid;`
- `AllocateValveAndVariable(Variable*, x, y)`: allocate two adjacent slots in `Elements()`. The valve goes at UID N, the variable at UID N+1. To guarantee adjacency, find the first NULL slot, *also* check the slot immediately after is NULL (else advance until both are free), insert both. Add a helper `AllocateAdjacentPair` on `VensimView` (or inline the logic in `XmileView`).
- `AllocateCloud(x, y)`: allocate one slot with `new VensimCommentElement(...)`. Confirm `VensimCommentElement` has a programmatic constructor; if not, add one analogous to `VensimValveElement`'s in Task 1 (and update Task 1 to bundle it).
- `AllocateGhost(Variable*, x, y)`: `VensimVariableElement` with `_ghost=true`. The existing ctor's auto-derivation `_ghost = (var->GetView() != NULL)` is wrong for aliases that come BEFORE any non-ghost reference. Override after construction by exposing a `SetGhost(bool)` setter on `VensimVariableElement` and calling it explicitly.

Wire `XmileView` into the gyp `common_sources` list (after `XmileReader.{h,cpp}`).

**Verification:**
- `ninja -C out/Debug XMUtil` succeeds.
- Existing tests still pass.

**Commit:** `feat(xmile): XmileView class — element allocation + cloud synthesis`
<!-- END_TASK_2 -->

<!-- END_SUBCOMPONENT_A -->

<!-- START_SUBCOMPONENT_B (tasks 3-4) -->

<!-- START_TASK_3 -->
### Task 3: Connector resolution + group integration

**Verifies:** AC4.2 (connector polarity), AC4.5 (group members).

**Files:**
- Modify: `src/Xmile/XmileView.cpp` (`ResolveConnectors` and `ProcessViewGroups` implementations)

**Implementation notes:**

Per the design above. Three notes for the implementor:

1. **Polarity validation**: only accept `'+'` or `'-'` as the first char of the `polarity` attribute. Anything else (e.g., `"positive"` from a non-conforming XMILE writer) is treated as no polarity with a warning logged. The XMILE 1.0 spec uses `"+"` / `"-"`; some Stella exports use `"positive"` / `"negative"` — handle both with case-insensitive prefix matching:

   ```cpp
   if (p[0] == '+' || (strncmp(p, "pos", 3) == 0)) polarity = '+';
   else if (p[0] == '-' || (strncmp(p, "neg", 3) == 0)) polarity = '-';
   ```

2. **Mid-point coordinates**: connectors have an `angle` attribute in XMILE (the geometric direction); the MDL sketch format uses `x, y` mid-point. The reader passes through whatever is on the `<connector>`'s `x` / `y` attributes (often absent in XMILE corpora). If absent, the midpoint defaults to halfway between the from and to elements' positions; otherwise the writer can recompute later. Acceptable to use `(0, 0)` initially and let the writer handle layout — the MDL writer at `MDLGenerator.cpp:300-309` writes the connector's `_x` / `_y` directly.

3. **Aliases**: when `<from><alias uid="N"/></from>` is seen, the alias element is referenced by its XMILE UID. The reader looks up the alias's UID in `_xmileUidToOurUid` (populated when the `<alias>` element was processed in pass 1). If the alias hasn't been processed yet (forward reference within the view), defer the connector to a third pass — XMILE typically declares aliases before connectors that reference them, but be defensive.

**Verification:**
- `ninja -C out/Debug xmutil_test` succeeds.
- Existing tests still pass.

**Commit:** `feat(xmile): XmileView connector + group resolution`
<!-- END_TASK_3 -->

<!-- START_TASK_4 -->
### Task 4: ProcessViews driver + multi-view warning

**Verifies:** AC4.4 (first view processed, others warned).

**Files:**
- Modify: `src/Xmile/XmileReader.cpp` (`ProcessViews` body — replace Phase 3 stub)
- Modify: `src/Xmile/XmileReader.h` (`#include "XmileView.h"`)

**Implementation notes:**

```cpp
bool XmileReader::ProcessViews(tinyxml2::XMLElement *viewsEl,
                               std::vector<std::string> &errs) {
  int viewCount = 0;
  for (tinyxml2::XMLElement *viewEl = viewsEl->FirstChildElement("view"); viewEl;
       viewEl = viewEl->NextSiblingElement("view")) {
    if (viewCount == 0) {
      VensimView *view = new VensimView();
      // XMILE views don't have explicit titles; use <view name="..."> if present
      // or fall back to "main".
      if (const char *vn = viewEl->Attribute("name")) {
        view->SetTitle(vn);
      } else {
        view->SetTitle("main");
      }
      _model->AddView(view);
      XmileView xv(this, _model, view);
      if (!xv.ProcessView(viewEl, errs)) return false;
    } else {
      errs.push_back(std::string("multi-view XMILE: skipping view #") +
                     std::to_string(viewCount + 1) + " (v1 supports only the first view)");
    }
    ++viewCount;
  }
  return true;
}
```

**Verification:**
- `ninja -C out/Debug xmutil_test` succeeds.
- Existing tests still pass.

**Commit:** `feat(xmile): ProcessViews driver + multi-view warning`
<!-- END_TASK_4 -->

<!-- END_SUBCOMPONENT_B -->

<!-- START_SUBCOMPONENT_C (tasks 5-5) -->

<!-- START_TASK_5 -->
### Task 5: ViewRoundTripTest — fishbanks + reliability corpora + AC3.4 guard

**Verifies:** AC4.1, AC4.2, AC4.3, AC4.4, AC4.5, AC3.4.

**Files:**
- Create: `test/xmile/ViewRoundTripTest.cpp`
- Modify: `XMUtil.gyp` (`xmutil_test` `sources`: add the new file)

**Implementation notes:**

```cpp
#include <fstream>
#include <sstream>
#include <string>
#include "../../src/Model.h"
#include "../../src/Symbol/Variable.h"
#include "../../src/Vensim/VensimView.h"
#include "../TestHarness.h"
#include "RoundTrip.h"

namespace {

std::string ReadFile(const std::string &path);  // shared with StockFlowRoundTripTest

const char *kClouds = R"(<?xml version="1.0" encoding="utf-8"?>
<xmile version="1.0" xmlns="http://docs.oasis-open.org/xmile/ns/XMILE/v1.0">
  <sim_specs method="euler"><start>0</start><stop>10</stop><dt>1</dt></sim_specs>
  <model>
    <variables>
      <stock name="s"><eqn>0</eqn><inflow>in</inflow><outflow>out</outflow></stock>
      <flow name="in"><eqn>1</eqn></flow>
      <flow name="out"><eqn>1</eqn></flow>
    </variables>
    <views><view view_type="stock_flow">
      <stock name="s" x="200" y="200"/>
      <flow name="in" x="120" y="200">
        <pts><pt x="50" y="200"/><pt x="190" y="200"/></pts>
      </flow>
      <flow name="out" x="280" y="200">
        <pts><pt x="210" y="200"/><pt x="350" y="200"/></pts>
      </flow>
    </view></views>
  </model>
</xmile>
)";

const char *kPolarityConnector = R"(<?xml version="1.0" encoding="utf-8"?>
<xmile version="1.0" xmlns="http://docs.oasis-open.org/xmile/ns/XMILE/v1.0">
  <sim_specs method="euler"><start>0</start><stop>10</stop><dt>1</dt></sim_specs>
  <model>
    <variables>
      <aux name="a"><eqn>1</eqn></aux>
      <aux name="b"><eqn>a</eqn></aux>
    </variables>
    <views><view view_type="stock_flow">
      <aux name="a" x="100" y="100"/>
      <aux name="b" x="200" y="100"/>
      <connector polarity="+"><from>a</from><to>b</to></connector>
    </view></views>
  </model>
</xmile>
)";

const char *kMultiView = R"(<?xml version="1.0" encoding="utf-8"?>
<xmile version="1.0" xmlns="http://docs.oasis-open.org/xmile/ns/XMILE/v1.0">
  <sim_specs method="euler"><start>0</start><stop>10</stop><dt>1</dt></sim_specs>
  <model>
    <variables><aux name="a"><eqn>1</eqn></aux></variables>
    <views>
      <view view_type="stock_flow"><aux name="a" x="100" y="100"/></view>
      <view view_type="stock_flow"><aux name="a" x="200" y="200"/></view>
    </views>
  </model>
</xmile>
)";

}  // namespace

TEST(View_corpus_fishbanks_to_mdl) {
  std::string xmile = ReadFile(std::string(XMUTIL_SRC_ROOT) +
                               "/third_party/simlin/default_projects/fishbanks/model.xmile");
  CHECK(!xmile.empty()); if (xmile.empty()) return;
  std::vector<std::string> diffs = xmileroundtrip::XmileToMdlDiffs(xmile);
  for (const std::string &d : diffs) printf("  diff: %s\n", d.c_str());
  CHECK(diffs.empty());
}

TEST(View_corpus_reliability_to_mdl) {
  std::string xmile = ReadFile(std::string(XMUTIL_SRC_ROOT) +
                               "/third_party/simlin/default_projects/reliability/model.xmile");
  CHECK(!xmile.empty()); if (xmile.empty()) return;
  std::vector<std::string> diffs = xmileroundtrip::XmileToMdlDiffs(xmile);
  for (const std::string &d : diffs) printf("  diff: %s\n", d.c_str());
  CHECK(diffs.empty());
}

TEST(View_clouds_produce_VensimCommentElement) {
  std::vector<std::string> errs;
  Model *m = xmileroundtrip::ParseXMILE(kClouds, errs);
  CHECK(m != nullptr); CHECK(errs.empty());
  if (!m) return;
  // The view should contain 2 cloud comment elements (one for each cloud
  // endpoint that didn't match a stock).
  VensimView *view = dynamic_cast<VensimView *>(m->Views()[0]);
  CHECK(view != nullptr);
  int cloudCount = 0;
  for (VensimViewElement *e : view->Elements()) {
    if (e && e->Type() == VensimViewElement::ElementTypeCOMMENT) ++cloudCount;
  }
  CHECK(cloudCount == 2);
  delete m;
}

TEST(View_connector_polarity_plus_round_trips) {
  // XMILE -> MDL -> re-parse, then verify the resulting view has a connector
  // with polarity '+'.
  std::vector<std::string> diffs = xmileroundtrip::XmileToMdlDiffs(kPolarityConnector);
  for (const std::string &d : diffs) printf("  diff: %s\n", d.c_str());
  CHECK(diffs.empty());
}

TEST(View_multi_view_takes_first_warns_second) {
  std::vector<std::string> errs;
  Model *m = xmileroundtrip::ParseXMILE(kMultiView, errs);
  CHECK(m != nullptr);
  // One soft warning about the second view.
  bool sawWarning = false;
  for (const std::string &e : errs) {
    if (e.find("multi-view") != std::string::npos) sawWarning = true;
  }
  CHECK(sawWarning);
  // Model has exactly one view.
  CHECK(m->Views().size() == 1);
  delete m;
}

// AC3.4 guard: deliberately corrupt the round-tripped Model and verify the
// comparator catches it — proves the round-trip test isn't vacuous.
TEST(View_comparator_guard_detects_corruption) {
  std::vector<std::string> errs;
  Model *m0 = xmileroundtrip::ParseXMILE(kPolarityConnector, errs);
  CHECK(m0 != nullptr); if (!m0) return;
  std::vector<std::string> regenErrs;
  std::string regen = m0->PrintXMILE(false, regenErrs, 1.0, 1.0);
  CHECK(regenErrs.empty());
  Model *m1 = xmileroundtrip::ParseXMILE(regen, errs);
  CHECK(m1 != nullptr); if (!m1) { delete m0; return; }
  // Hand-corrupt m1: rename variable "a" to "z". The comparator should report
  // a difference.
  Variable *a = static_cast<Variable *>(m1->GetNameSpace()->Find("a"));
  CHECK(a != nullptr);
  if (a) m1->RenameVariable(a, "z");
  std::vector<std::string> diffs = ModelComparator::Compare(m0, m1);
  CHECK(!diffs.empty());  // comparator MUST detect the rename
  delete m0; delete m1;
}
```

**Verification:**
- `ninja -C out/Debug xmutil_test` succeeds.
- All six new tests pass.
- All Phase 1-6 tests still pass.

**Commit:** `test(xmile): view round-trip (fishbanks + reliability + cloud + polarity)`
<!-- END_TASK_5 -->

<!-- END_SUBCOMPONENT_C -->

---

## Phase 7 done when

- `fishbanks/model.xmile` converts XMILE → MDL with an equivalent `Model` (round-trip comparator empty).
- `reliability/model.xmile` converts XMILE → MDL with equivalent `Model`.
- Clouds are emitted as `VensimCommentElement` nodes in the view; pipes connect to them.
- `<connector polarity="+">` and `polarity="-">` round-trip XMILE → MDL → re-parse with the same polarity char.
- Multi-view inputs take the first view and produce a soft warning for the rest.
- The AC3.4 guard test detects a hand-introduced corruption — proves the round-trip tests aren't vacuous.
- All Phase 1-6 tests still pass.

## Notes — latent fixture risk

Phase 3's `AuxRoundTrip_group_round_trip` test (the `kWithGroup` fixture) round-trips through the writer's `generateSectorViews` empty-views branch (`XMILEGenerator.cpp:805-828`) — that branch fires when the model has groups but no views. Phase 7 introduces `ProcessViews`, which **does** populate a view from the same `<view>` element that `kWithGroup` contains. After Phase 7 lands, the writer takes the non-empty-views path (line 838+), which emits both groups *and* view geometry. The Phase 3 fixture must therefore still round-trip cleanly via that alternate writer path.

Verify during Phase 7 implementation:
1. Re-run `AuxRoundTrip_group_round_trip` (the Phase 3 test) after Phase 7 Task 4 lands. If it fails, the round-trip comparator likely flags a new view-element diff that didn't exist before Phase 7.
2. The fix is usually one of: (a) the fixture is now ambiguous about view-element positioning and needs explicit `<stock x= y=>` on its variables; (b) the writer's non-empty-views path doesn't emit groups identically and is itself a bug (but that's out of scope to fix here — log it as a follow-up).

If Phase 3's test fails after Phase 7 and the cause is fixture ambiguity, update the fixture to include explicit `<view>` positions for its auxes. Document the change in the Phase 7 commit.

## Out of scope for Phase 7

- Multi-view fidelity (only the first view is used).
- `view_type="interface"` or other non-`stock_flow` view types (treat the same as stock_flow; UI overlays inside them are silently dropped).
- Stella UI widgets within views (`<button>`, `<knob>`, `<slider>`, `<graph>`, etc.) — silently dropped at element-allocation time.
- Sub-view-level `<style>` blocks (color, font) — silently dropped.
- XMILE 2.0 features (`<external_inputs>`, etc.).
- View geometry preservation under XMILE → XMILE round-trip — the writer may re-layout slightly; the comparator is variable+equation-level, not pixel-level.
