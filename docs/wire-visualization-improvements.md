# Wire Visualization Improvement Ideas

This document analyzes the current wire visualization algorithm and proposes improvements for cleaner rendering, especially for multi-bit buses.

---

## Current Implementation Analysis

### Current Wire Routing Algorithm (Lines 89-124 in visual_component.py)

**Algorithm:** Orthogonal routing with fixed horizontal stubs

```
Source Pin → Horizontal Stub → Vertical segment → Horizontal segment → Vertical segment → Horizontal Stub → Sink Pin
```

**Key parameters:**
- `wire_stub_ratio = 0.15` - Length of horizontal stubs (15% of component width)
- Routing: 6-point path with mid-point at `(p_start_trunk.x + p_end_trunk.x) / 2`

### Identified Problems

#### Problem 1: Reversed Stubs (Dirty Look)
When sink is **left** of source (leftward connections):
- Source stub points **right** (+x direction)
- Sink stub points **left** (-x direction)
- **Result:** Stubs point toward each other, creating visual clutter

```
Current (UGLY):
Source →|
        |    ← Stubs oppose each other
     |← Sink

Desired:
Source →|
   |----| ← Aligned flow
   |← Sink
```

#### Problem 2: Overlapping Wires (No Distinction)
Multiple wires between same components render on identical paths:
- No visual separation between different signals
- Cannot distinguish wire count or individual wires
- Especially problematic for buses (upcoming feature)

```
Current:
A → [───────] → B   ← 5 wires, all overlapping (invisible)

Desired:
A → [━━━━━━━] → B   ← Clear visual distinction
    [━━━━━━━]
    [━━━━━━━]
```

---

## Proposed Solutions

### Solution 1: Adaptive Stub Direction (RECOMMENDED)

**Concept:** Stubs always point toward the midpoint between components

**Algorithm:**
```python
direction = 1 if end_pos.x > start_pos.x else -1  # Rightward = +1, Leftward = -1

# Source stub
p_start_trunk = start_pos + Vector2(source_stub_len * direction, 0)

# Sink stub (opposite direction of approach)
p_end_trunk = end_pos - Vector2(sink_stub_len * direction, 0)
```

**Benefits:**
- ✅ Stubs always form smooth transitions
- ✅ No opposing stubs (visual consistency)
- ✅ Works for all wire directions
- ✅ Minimal code change

**Edge Cases:**
- Vertical alignment (same x-coordinate): Default to rightward stubs
- Parent-child containment: Already handled by existing logic (lines 111-116)

---

### Solution 2: Wire Bundle Offset (For Overlapping Wires)

**Concept:** Offset parallel wires perpendicular to their main direction

**Algorithm:**
```python
# For each wire between same source/sink pair:
wire_index = get_wire_index_in_bundle(source, sink)  # 0, 1, 2, ...
total_wires = get_wire_count_in_bundle(source, sink)

# Calculate perpendicular offset
offset_step = 8  # pixels between wires
max_offset = (total_wires - 1) * offset_step / 2
wire_offset = (wire_index * offset_step) - max_offset

# Apply offset perpendicular to wire direction
perpendicular = Vector2(-dy, dx).normalize()  # 90° rotation
offset_vector = perpendicular * wire_offset

# Apply to all path points (except endpoints)
for point in path_points[1:-1]:
    point += offset_vector
```

**Benefits:**
- ✅ Clearly shows multiple wires between components
- ✅ Bundle appears as a group (wires close together)
- ✅ Scales to arbitrary wire counts
- ✅ Endpoints remain at pins (no visual disconnect)

**Visual Example:**
```
Single wire:
A →──────→ B

3 wires (offset by 8px):
A →══════→ B
  →══════→
  →══════→
```

**Considerations:**
- Need to track wire bundles (group wires by source/sink pair)
- Works best with straight or simple paths
- May need larger offset for zoomed-out views

---

### Solution 3: Multi-Bit Bus Representation (FUTURE)

When transitioning to multi-bit wires, several visualization strategies:

#### 3A. Thick Lines with Value Display

**Visual:** Single thick line with bit values shown

```
     ┌───────┐
A ━━━▶│ Adder │
     └───────┘
     ▲
   [0xFF]  ← Hex value overlay
```

**Implementation:**
```python
# Wire thickness proportional to bit width
line_width = 2 + (bit_width // 4)  # 8-bit = 4px, 32-bit = 10px

# Value display (on hover or always)
if show_value:
    mid_point = path_points[len(path_points) // 2]
    value_text = f"0x{wire_value:0{bit_width//4}X}"  # Hex
    draw_text(value_text, mid_point, background=True)
```

**Benefits:**
- ✅ Clear distinction from single-bit wires
- ✅ Shows actual data values
- ✅ Compact representation

**Limitations:**
- ⚠ Value may change every cycle (visual noise)
- ⚠ Hex not ideal for non-byte-aligned widths

---

#### 3B. Hash Marks or Slash Notation

**Visual:** Diagonal slashes with bit width label (IEEE standard)

```
     ┌───────┐
A ══╱8╱═▶│ Adder │
     └───────┘
```

**Implementation:**
```python
# Draw thick bus line
draw_line(path, width=4, color=color)

# Add slash marks at 1/3 and 2/3 points
for i in [1, 2]:
    slash_pos = path_points[len(path_points) * i // 3]
    slash_angle = 45  # degrees
    draw_slash(slash_pos, angle=slash_angle, length=12)

# Draw bit width label
label_pos = path_points[len(path_points) // 2]
draw_text(f"{bit_width}", label_pos + offset)
```

**Benefits:**
- ✅ Industry-standard notation (familiar to engineers)
- ✅ Clear bit width indication
- ✅ No dynamic value display (stable visuals)

**Limitations:**
- ⚠ Doesn't show actual values
- ⚠ Requires labels (may clutter at small zoom)

---

#### 3C. Color-Coded Bit Patterns

**Visual:** Each bit position has unique color in a pattern

```
     ┌───────┐
A ▓▓▒▒░░██▶│ Adder │  ← Striped pattern (bits 7-0)
     └───────┘
```

**Implementation:**
```python
# Generate color pattern from bit value
bit_colors = []
for i in range(bit_width):
    bit = (value >> i) & 1
    color = color_high if bit else color_low
    bit_colors.append(color)

# Draw wire as segmented multi-color line
segment_length = total_length / bit_width
for i, color in enumerate(bit_colors):
    start = path_start + (i * segment_length)
    end = start + segment_length
    draw_line_segment(start, end, color, width=thick)
```

**Benefits:**
- ✅ Shows actual bit pattern visually
- ✅ Immediate visual feedback on value changes
- ✅ Unique per wire (easy to trace)

**Limitations:**
- ⚠ Hard to read for wide buses (32+ bits)
- ⚠ Color aliasing at small zoom levels
- ⚠ Not intuitive for hex interpretation

---

#### 3D. Expanded Bus View (On-Demand)

**Visual:** Single bus expands to show individual bit lanes on click/hover

```
Normal:
A ══════▶ B

Expanded (on hover):
A ─0─────▶ B
  ─1─────▶
  ─2─────▶
  ─3─────▶
  ...
  ─7─────▶
```

**Implementation:**
```python
if wire.is_bus and (wire.is_hovered or wire.is_expanded):
    # Fan out to individual bit lanes
    for bit_index in range(bit_width):
        offset = (bit_index - bit_width/2) * bit_spacing
        bit_path = offset_path(main_path, perpendicular * offset)

        bit_value = (wire_value >> bit_index) & 1
        bit_color = color_high if bit_value else color_low

        draw_line(bit_path, width=1, color=bit_color)
        draw_text(f"{bit_index}", bit_path[0] + small_offset)
else:
    # Collapsed: draw thick bus
    draw_line(main_path, width=4, color=bus_color)
```

**Benefits:**
- ✅ Best of both worlds (compact + detailed)
- ✅ Shows individual bit values clearly
- ✅ Interactive (user controls detail level)

**Limitations:**
- ⚠ Requires interaction model (click/hover)
- ⚠ May be cluttered for 32+ bit buses

---

## Recommended Implementation Priority

### Phase 1: Immediate Improvements (Single-Bit Wires)
1. **Adaptive Stub Direction** (Solution 1)
   - Fixes the "dirty look" problem
   - Low complexity, high visual impact
   - ~20 lines of code change

2. **Wire Bundle Offset** (Solution 2)
   - Fixes overlapping wire problem
   - Requires bundle tracking (new data structure)
   - ~50 lines of code + bundle detection

### Phase 2: Multi-Bit Preparation
3. **Hash Marks Notation** (Solution 3B)
   - Industry-standard, simple to implement
   - Works as foundation for other methods
   - ~30 lines of code

4. **Hex Value Display** (Solution 3A)
   - Add optional value overlays
   - Useful for debugging
   - ~40 lines of code

### Phase 3: Advanced Features
5. **Expanded Bus View** (Solution 3D)
   - Interactive detail-on-demand
   - Best for complex debugging
   - ~100 lines of code + UI state

---

## Wire Bundle Detection Algorithm

For Solution 2 (Wire Bundle Offset), need to group wires:

```python
class WireBundleManager:
    def __init__(self):
        self.bundles = {}  # (source_id, sink_id) → [wire1, wire2, ...]

    def add_wire(self, wire, source_pin, sink_pin):
        source_id = f"{source_pin.parent.name}.{source_pin.name}"
        sink_id = f"{sink_pin.parent.name}.{sink_pin.name}"
        key = (source_id, sink_id)

        if key not in self.bundles:
            self.bundles[key] = []
        self.bundles[key].append(wire)

    def get_wire_offset(self, wire, source_pin, sink_pin):
        source_id = f"{source_pin.parent.name}.{source_pin.name}"
        sink_id = f"{sink_pin.parent.name}.{sink_pin.name}"
        key = (source_id, sink_id)

        if key not in self.bundles:
            return 0

        bundle = self.bundles[key]
        wire_index = bundle.index(wire)
        total_wires = len(bundle)

        offset_step = 8  # pixels
        max_offset = (total_wires - 1) * offset_step / 2
        return (wire_index * offset_step) - max_offset
```

---

## Alternative Data Display Methods (Beyond Color)

For multi-bit wires, several ways to show values besides color:

### A. Text Overlays
- **Hex:** `0xDEADBEEF` (compact for 32-bit)
- **Decimal:** `3735928559` (natural for counters)
- **Binary:** `11011110...` (verbose, good for bit patterns)

### B. Waveform Preview
Small inline waveform showing recent value changes:
```
     ┌─────┐
A ━━━│ ▁▁▄▄│━▶ B   ← Last 8 cycles as mini-graph
     └─────┘
```

### C. Hover Tooltip
Rich tooltip on wire hover:
```
┌─────────────────┐
│ Wire: DATA_BUS  │
│ Width: 32 bits  │
│ Value: 0x00FF   │
│ Binary: 0000... │
│ Active: 87%     │
└─────────────────┘
```

### D. Width-Coded Thickness
Bus thickness = f(bit_width):
- 1-bit: 2px
- 8-bit: 3px
- 16-bit: 4px
- 32-bit: 6px
- 64-bit: 8px

### E. Animation Direction
Show data flow direction with animated dashes:
```
A ━━→━━→━━→ B   ← Animated chevrons
```

---

## Testing Strategy

### Test Cases for Stub Direction Fix
1. ✅ Left-to-right wires (current case)
2. ✅ Right-to-left wires (problematic case)
3. ✅ Vertical wires (same x-coordinate)
4. ✅ Parent-to-child wires (downward hierarchy)
5. ✅ Child-to-parent wires (upward hierarchy)
6. ✅ Sibling-to-sibling wires (peer-to-peer)

### Test Cases for Wire Bundles
1. ✅ Single wire between components (no offset)
2. ✅ 2 wires (symmetrical offset)
3. ✅ 3+ wires (staggered offsets)
4. ✅ Bidirectional bundles (A→B and B→A)
5. ✅ Fan-out (1 source, N sinks)
6. ✅ Fan-in (N sources, 1 sink)

### Test Cases for Multi-Bit Buses
1. ✅ Bus width: 1, 4, 8, 16, 32, 64 bits
2. ✅ Value transitions (0→1, 1→0, X→value)
3. ✅ All-zeros, all-ones, alternating patterns
4. ✅ Zoom levels (must be readable at all scales)
5. ✅ Mixed single-bit and multi-bit wires

---

## Performance Considerations

### Current Performance (Single-Bit)
- Wire count: O(N) wires → O(N) draw calls
- Path computation: O(6) points per wire (constant)
- Total: **O(N)** complexity

### Wire Bundle Offset Impact
- Bundle detection: O(N) one-time preprocessing
- Offset calculation: O(1) per wire
- Total: **O(N)** complexity (no change)

### Multi-Bit Bus Impact
- **Hash marks:** O(N) + small constant overhead (2-3 marks per bus)
- **Thick lines:** O(N) with larger line width (same draw calls)
- **Color segments:** O(N × bit_width) draw calls (⚠ may be expensive for 32-bit buses)
- **Expanded view:** O(N × bit_width) when expanded (⚠ avoid for all buses simultaneously)

**Recommendation:** Use hash marks + thick lines for always-visible buses, expanded view on-demand only.

---

## Visual Consistency Guidelines

When implementing improvements, maintain:

1. **Color semantics:**
   - GREEN (#4CAF50): Logic HIGH
   - RED (#D32F2F): Logic LOW
   - GRAY (#9E9E9E): UNKNOWN
   - BLUE (#039BE5): HIGH_Z
   - These colors are **sacred** - keep them consistent

2. **Line widths:**
   - Single-bit: 2px (current)
   - Multi-bit buses: 4-8px (proportional to width)
   - Hovered: 2× normal width

3. **Stub lengths:**
   - Keep proportional to component size (`wire_stub_ratio = 0.15`)
   - Adaptive direction, but same length

4. **Bundle spacing:**
   - 6-8px perpendicular offset (enough to distinguish, not too spread)
   - Scale with zoom (constant world-space distance)

---

## Implementation Checklist

### For Adaptive Stub Direction (Solution 1)
- [ ] Calculate direction based on sink position relative to source
- [ ] Modify `p_start_trunk` calculation (line 109)
- [ ] Modify `p_end_trunk` calculation (lines 112-116)
- [ ] Test all 6 wire direction cases
- [ ] Verify parent-child relationships still work

### For Wire Bundle Offset (Solution 2)
- [ ] Create `WireBundleManager` class
- [ ] Populate bundles during wire creation/connection
- [ ] Calculate perpendicular offset vector
- [ ] Apply offset to path_points[1:-1] (not endpoints)
- [ ] Test with varying bundle sizes (2, 3, 5, 10 wires)
- [ ] Verify offset scales with zoom

### For Multi-Bit Buses (Solution 3B - Hash Marks)
- [ ] Add `bit_width` property to Wire class
- [ ] Draw thick line for buses (width = f(bit_width))
- [ ] Draw diagonal slash marks at 1/3, 2/3 points
- [ ] Draw bit width label near slash marks
- [ ] Test with 1, 4, 8, 16, 32, 64 bit widths
- [ ] Ensure labels readable at all zoom levels

---

## Future Enhancements

### Smart Routing (Beyond Orthogonal)
- **A* pathfinding** around obstacles
- **Bezier curves** for smooth bends
- **Manhattan routing** with variable segments

### Signal Visualization
- **Pulse animation** on state transitions
- **Glitch indicators** for X/Z states
- **Timing diagrams** inline with wires

### Bus Operations
- **Bit extraction** visualization (show which bits are used)
- **Concatenation** indicators (multiple buses merging)
- **Width conversion** markers (32→8 bit narrowing)

### Performance Optimization
- **Spatial indexing** for wire collision detection (currently O(N) per pixel)
- **LOD (Level of Detail)** - simplified rendering when zoomed out
- **Culling** - skip drawing wires outside viewport

---

## References & Inspiration

- **IEEE Schematic Standards**: Hash mark notation for buses
- **Vivado/Quartus**: Expandable bus lanes, waveform previews
- **Digital**: Orthogonal routing with smart stubs
- **Falstad Circuit Simulator**: Color-coded current flow animation
- **KiCad**: Wire bundling and differential pair routing

---

## Conclusion

**Immediate Priority:** Implement Solution 1 (Adaptive Stub Direction) to fix the visual "dirty look" problem with minimal effort.

**Next Step:** Add Solution 2 (Wire Bundle Offset) to distinguish overlapping wires before multi-bit buses are needed.

**Long-Term:** Use Solution 3B (Hash Marks) as the foundation for multi-bit buses, with optional 3A (hex values) and 3D (expanded view) for debugging.

All solutions maintain the current orthogonal routing algorithm and require no changes to backend C++ code - only visualization layer modifications.
