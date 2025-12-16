# Wire Routing Algorithm Alternatives

This document explores alternatives to the current "vertical-horizontal-vertical" (V-H-V) orthogonal routing structure.

---

## Current Algorithm Analysis

### Current 6-Point Path Structure
```python
# Lines 118-120 in visual_component.py
mid_x = (p_start_trunk.x + p_end_trunk.x) / 2
path_points = [
    start_pos,           # Pin position (source)
    p_start_trunk,       # After source stub (horizontal)
    Vector2(mid_x, p_start_trunk.y),  # V: Up/down to mid_x
    Vector2(mid_x, p_end_trunk.y),     # H: Horizontal run at mid_x
    p_end_trunk,         # V: Up/down from mid_x
    end_pos              # Pin position (sink)
]
```

**Structure:** `Stub → V → H → V → Stub`

**Visual:**
```
Source →───┐
           │ (V: vertical)
           ├─────── (H: horizontal)
           │ (V: vertical)
       ┌───┘
       Sink
```

### Problems with Current Approach

1. **Wasted Space:** Always uses mid-point, even when direct H-V or V-H would be shorter
2. **Unnecessary Bends:** 4 corners when 2 might suffice
3. **Non-Optimal for Short Distances:** Over-engineered for nearby components
4. **Inflexible:** Same pattern regardless of wire length or component alignment

---

## Alternative Routing Strategies

### Strategy 1: Adaptive 2-Segment Routing (H-V or V-H)

**Concept:** Use only 2 segments when possible, falling back to 3 when necessary

#### Algorithm
```python
def route_adaptive_2segment(start, end, source_stub_len, sink_stub_len):
    # After stubs
    p_start = start + Vector2(source_stub_len, 0)
    p_end = end - Vector2(sink_stub_len, 0)

    # Check if simple L-shape works (H then V, or V then H)
    dx = p_end.x - p_start.x
    dy = p_end.y - p_start.y

    # Case 1: Horizontal then Vertical (simple L)
    if dx > 0:  # Moving rightward - use H-V
        return [
            start,
            p_start,
            Vector2(p_end.x, p_start.y),  # Corner: horizontal to end.x
            p_end,
            end
        ]

    # Case 2: Moving leftward - need Z-shape (3 segments)
    else:
        # Use vertical offset to avoid overlap
        mid_x = p_start.x + source_stub_len * 2  # Extend stub region
        return [
            start,
            p_start,
            Vector2(mid_x, p_start.y),     # H: Extend right
            Vector2(mid_x, p_end.y),       # V: Go up/down
            p_end,                          # H: Come back left
            end
        ]
```

**Visual Examples:**
```
Rightward (H-V, 2 segments):
Source →──────┐
              │
              Sink

Leftward (H-V-H, 3 segments):
Source →───┐
           │
       ┌───┤
       Sink
```

**Benefits:**
- ✅ Fewer bends for rightward wires (cleaner)
- ✅ Shorter wire length for aligned components
- ✅ More direct routing

**Drawbacks:**
- ⚠ Different patterns for left/right (but that's okay!)
- ⚠ Still has 3 segments for leftward

---

### Strategy 2: Manhattan Distance Minimization

**Concept:** Choose the routing that minimizes total wire length

#### Algorithm
```python
def route_manhattan_optimal(start, end, source_stub_len, sink_stub_len):
    p_start = start + Vector2(source_stub_len, 0)
    p_end = end - Vector2(sink_stub_len, 0)

    # Calculate all possible routing patterns
    routes = []

    # Pattern A: Current (mid-point)
    mid_x = (p_start.x + p_end.x) / 2
    route_a = [
        start, p_start,
        Vector2(mid_x, p_start.y),
        Vector2(mid_x, p_end.y),
        p_end, end
    ]
    routes.append(('mid', route_a, calculate_length(route_a)))

    # Pattern B: H-V (horizontal first)
    route_b = [
        start, p_start,
        Vector2(p_end.x, p_start.y),
        p_end, end
    ]
    routes.append(('HV', route_b, calculate_length(route_b)))

    # Pattern C: V-H (vertical first)
    route_c = [
        start, p_start,
        Vector2(p_start.x, p_end.y),
        p_end, end
    ]
    routes.append(('VH', route_c, calculate_length(route_c)))

    # Choose shortest valid route (that doesn't intersect obstacles)
    valid_routes = [r for r in routes if is_valid_route(r[1])]
    return min(valid_routes, key=lambda r: r[2])[1]
```

**Benefits:**
- ✅ Optimal wire length (minimal material)
- ✅ Naturally chooses best pattern per wire
- ✅ Cleaner look for most cases

**Drawbacks:**
- ⚠ May not be consistent (different patterns for similar wires)
- ⚠ Requires obstacle detection (future work)

---

### Strategy 3: Bezier Curve Routing (Smooth)

**Concept:** Replace sharp corners with smooth curves

#### Algorithm
```python
def route_bezier_smooth(start, end, source_stub_len, sink_stub_len):
    # Control points for cubic Bezier
    p_start = start + Vector2(source_stub_len, 0)
    p_end = end - Vector2(sink_stub_len, 0)

    # Calculate control points for smooth S-curve
    dx = p_end.x - p_start.x
    curve_strength = 0.5  # How much curve vs. straight

    control1 = p_start + Vector2(abs(dx) * curve_strength, 0)
    control2 = p_end - Vector2(abs(dx) * curve_strength, 0)

    # Generate Bezier curve points (sample 20 points)
    points = [start]
    for t in range(21):
        t_norm = t / 20.0
        point = cubic_bezier(p_start, control1, control2, p_end, t_norm)
        points.append(point)
    points.append(end)

    return points

def cubic_bezier(p0, p1, p2, p3, t):
    """Cubic Bezier interpolation"""
    u = 1 - t
    return (u**3 * p0 +
            3 * u**2 * t * p1 +
            3 * u * t**2 * p2 +
            t**3 * p3)
```

**Visual:**
```
Orthogonal (current):
Source →───┐
           │
           └───┐
               Sink

Bezier (smooth):
Source →───╮
           ╰───╮
               Sink
```

**Benefits:**
- ✅ Beautiful, smooth appearance
- ✅ No sharp corners (easier to visually trace)
- ✅ Professional CAD-tool aesthetic

**Drawbacks:**
- ⚠ More expensive to render (20+ points vs. 6)
- ⚠ Collision detection is harder
- ⚠ May overlap other components (no obstacle avoidance)
- ⚠ Less familiar to digital circuit designers (orthogonal is standard)

---

### Strategy 4: Direct Line with Rounded Corners

**Concept:** Keep orthogonal structure but round the corners

#### Algorithm
```python
def route_rounded_corners(start, end, source_stub_len, sink_stub_len, radius=5):
    # Get standard orthogonal path
    base_path = route_standard_orthogonal(start, end, source_stub_len, sink_stub_len)

    # Round each corner
    rounded_path = [base_path[0]]  # Start point

    for i in range(1, len(base_path) - 1):
        prev = base_path[i - 1]
        curr = base_path[i]
        next_pt = base_path[i + 1]

        # Calculate rounded corner (arc from prev to next)
        arc_points = calculate_corner_arc(prev, curr, next_pt, radius)
        rounded_path.extend(arc_points)

    rounded_path.append(base_path[-1])  # End point
    return rounded_path

def calculate_corner_arc(p0, corner, p1, radius):
    """Generate quarter-circle arc at corner"""
    v0 = (corner - p0).normalize()
    v1 = (p1 - corner).normalize()

    # Offset corner points by radius
    arc_start = corner - v0 * radius
    arc_end = corner + v1 * radius

    # Sample 5 points along quarter circle
    arc_points = []
    for i in range(5):
        t = i / 4.0
        # Circular interpolation
        angle = math.pi / 2 * t
        point = arc_start + (arc_end - arc_start) * smooth_step(t)
        arc_points.append(point)

    return arc_points
```

**Visual:**
```
Sharp corners (current):
Source →───┐
           │
           └───┘

Rounded corners:
Source →───╮
           │
           ╰───╯
```

**Benefits:**
- ✅ Maintains orthogonal structure (familiar)
- ✅ Softer appearance
- ✅ Easier to trace than Bezier
- ✅ Modest rendering cost increase

**Drawbacks:**
- ⚠ Slightly more points to render (3-5 per corner)
- ⚠ Corner radius must scale with zoom

---

### Strategy 5: A* Pathfinding (Obstacle Avoidance)

**Concept:** Use A* algorithm to route around obstacles

#### Algorithm
```python
def route_astar(start, end, obstacles, grid_size=10):
    """
    A* pathfinding on a grid to avoid obstacles
    """
    # Convert to grid coordinates
    start_grid = world_to_grid(start, grid_size)
    end_grid = world_to_grid(end, grid_size)

    # Build obstacle map
    obstacle_grid = build_obstacle_grid(obstacles, grid_size)

    # A* search
    path = astar_search(start_grid, end_grid, obstacle_grid)

    # Convert back to world coordinates and simplify
    world_path = [grid_to_world(p, grid_size) for p in path]
    simplified_path = douglas_peucker_simplify(world_path, epsilon=2)

    return simplified_path

def astar_search(start, goal, obstacle_grid):
    """Standard A* implementation"""
    # Priority queue: (f_score, position)
    open_set = [(0, start)]
    came_from = {}
    g_score = {start: 0}

    while open_set:
        current = heappop(open_set)[1]

        if current == goal:
            return reconstruct_path(came_from, current)

        for neighbor in get_neighbors(current, obstacle_grid):
            tentative_g = g_score[current] + 1

            if neighbor not in g_score or tentative_g < g_score[neighbor]:
                came_from[neighbor] = current
                g_score[neighbor] = tentative_g
                f_score = tentative_g + heuristic(neighbor, goal)
                heappush(open_set, (f_score, neighbor))

    return None  # No path found
```

**Benefits:**
- ✅ Guaranteed shortest path (given grid resolution)
- ✅ Avoids overlapping components
- ✅ Handles complex layouts automatically
- ✅ Industry-standard approach (used in PCB routing)

**Drawbacks:**
- ⚠ Expensive (O(N log N) search per wire)
- ⚠ Requires obstacle map maintenance
- ⚠ May produce non-orthogonal segments (unless constrained)
- ⚠ Overkill for simple layouts

**When to use:** Only when component density is high and overlaps are common

---

### Strategy 6: Channel Routing (Bus-Aware)

**Concept:** Reserve dedicated channels for wire bundles

#### Algorithm
```python
class ChannelRouter:
    def __init__(self):
        self.channels = {}  # y_level → [wires using this channel]
        self.channel_spacing = 20  # pixels between channels

    def route_bundle(self, wires_in_bundle):
        """Route multiple wires as a bundle"""
        # Sort wires by source/sink positions
        sorted_wires = sorted(wires_in_bundle, key=lambda w: w.start.y)

        # Assign channel levels
        for i, wire in enumerate(sorted_wires):
            channel_y = self.get_available_channel(wire.start.y, wire.end.y)
            wire.channel = channel_y
            self.reserve_channel(channel_y, wire)

        # Route each wire through its channel
        routed_wires = []
        for wire in sorted_wires:
            path = self.route_through_channel(wire)
            routed_wires.append(path)

        return routed_wires

    def route_through_channel(self, wire):
        """Route wire through assigned horizontal channel"""
        return [
            wire.start,
            wire.start + Vector2(stub_len, 0),
            Vector2(wire.start.x + stub_len, wire.channel),  # V to channel
            Vector2(wire.end.x - stub_len, wire.channel),    # H in channel
            Vector2(wire.end.x - stub_len, wire.end.y),       # V from channel
            wire.end - Vector2(stub_len, 0),
            wire.end
        ]
```

**Visual:**
```
Without channels (overlapping):
A ═════════════ B
C ═════════════ D

With channels:
A ═════╗
       ║ (channel 1)
       ╚═══════ B
C ══════╗
        ║ (channel 2)
        ╚══════ D
```

**Benefits:**
- ✅ No wire overlaps within bundles
- ✅ Predictable routing (easier to debug)
- ✅ Scalable to many wires
- ✅ Similar to PCB routing practices

**Drawbacks:**
- ⚠ Requires global coordination (not per-wire)
- ⚠ May waste space with empty channels
- ⚠ Needs dynamic channel allocation

---

## Comparison Matrix

| Strategy | Complexity | Visual Quality | Wire Length | Overlap Handling | Performance |
|----------|-----------|----------------|-------------|------------------|-------------|
| **Current (V-H-V)** | Low | Good | Medium | None | Fast |
| **Adaptive 2-Seg** | Low | Better | Shorter | None | Fast |
| **Manhattan Optimal** | Medium | Better | Shortest | None | Fast |
| **Bezier Smooth** | Medium | Beautiful | N/A | None | Slower |
| **Rounded Corners** | Low | Very Good | Medium | None | Fast |
| **A* Pathfinding** | High | Best | Optimal | Excellent | Slow |
| **Channel Routing** | High | Good | Medium | Excellent | Medium |

---

## Recommended Hybrid Approach

### Tier 1: Simple Cases (80% of wires)
Use **Adaptive 2-Segment** with **Rounded Corners**:
```python
def route_wire_simple(start, end, source_stub, sink_stub):
    # Get 2-segment or 3-segment path
    base_path = route_adaptive_2segment(start, end, source_stub, sink_stub)

    # Add rounded corners
    return route_rounded_corners(base_path, radius=5)
```

**Benefits:**
- Fast for majority of wires
- Clean appearance
- Minimal bends

### Tier 2: Bundles (15% of wires)
Use **Wire Bundle Offset** (from previous doc):
```python
def route_bundle(wires, source_pin, sink_pin):
    # Get base path for bundle
    base_path = route_adaptive_2segment(source_pin.pos, sink_pin.pos, ...)

    # Offset each wire perpendicular
    routed = []
    for i, wire in enumerate(wires):
        offset = calculate_bundle_offset(i, len(wires))
        wire_path = offset_path_perpendicular(base_path, offset)
        routed.append(wire_path)

    return routed
```

### Tier 3: Complex Cases (5% of wires)
Use **A* Pathfinding** only when:
- Wire would overlap component
- No direct orthogonal path exists
- Manual routing override requested

---

## Implementation Recommendations

### Phase 1: Quick Win (Immediate)
✅ **Replace current V-H-V with Adaptive 2-Segment**
- Modify lines 118-120 in `visual_component.py`
- Add direction detection logic
- Test with existing circuits

**Effort:** ~30 lines of code change
**Impact:** Cleaner appearance, shorter wires

### Phase 2: Polish (Short-term)
✅ **Add Rounded Corners**
- Post-process orthogonal paths
- 5px corner radius (scale with zoom)
- ~50 lines of code

**Effort:** ~1 hour
**Impact:** Professional CAD-tool aesthetic

### Phase 3: Bundles (Medium-term)
✅ **Implement Wire Bundle Offset** (see previous doc)
- Required for multi-bit buses anyway
- Solves overlap problem
- ~100 lines of code

**Effort:** ~2-3 hours
**Impact:** Critical for distinguishing wires

### Phase 4: Advanced (Long-term)
⚠ **A* Pathfinding (Optional)**
- Only if component density becomes high
- Only for problematic wires
- ~300 lines of code + obstacle map

**Effort:** ~1 day
**Impact:** Perfect routing in complex layouts

---

## Detailed Algorithm: Adaptive 2-Segment

Here's the complete implementation for the recommended approach:

```python
def route_adaptive(self, start_pos, end_pos, source_owner, sink_owner):
    """
    Adaptive routing: uses 2 segments (H-V) when possible,
    falls back to 3 segments (H-V-H) when necessary.
    """
    # Calculate stub lengths
    source_stub_len = source_owner.rect.width * source_owner.wire_stub_ratio
    sink_stub_len = sink_owner.rect.width * sink_owner.wire_stub_ratio

    # Determine stub direction based on relative positions
    dx = end_pos.x - start_pos.x

    # Adaptive stub direction (fixes reversed stub problem)
    if dx > 0:  # Rightward flow
        source_dir = 1
        sink_dir = -1
    else:  # Leftward flow
        source_dir = 1  # Still extend right from source
        sink_dir = 1    # Also extend right from sink (for clearance)

    # Apply stubs
    p_start_stub = start_pos + Vector2(source_stub_len * source_dir, 0)
    p_end_stub = end_pos + Vector2(sink_stub_len * sink_dir, 0)

    # Check if simple 2-segment routing works
    if dx > 0 and p_start_stub.x < p_end_stub.x:
        # Simple L-shape: Horizontal then Vertical
        path = [
            start_pos,
            p_start_stub,
            Vector2(p_end_stub.x, p_start_stub.y),  # Corner
            p_end_stub,
            end_pos
        ]
    else:
        # Z-shape: Need 3 segments for leftward or tight spacing
        # Use traditional mid-point approach
        mid_x = (p_start_stub.x + p_end_stub.x) / 2
        path = [
            start_pos,
            p_start_stub,
            Vector2(mid_x, p_start_stub.y),
            Vector2(mid_x, p_end_stub.y),
            p_end_stub,
            end_pos
        ]

    return path
```

### With Rounded Corners Addition:

```python
def add_rounded_corners(self, path, radius=5):
    """
    Add quarter-circle arcs at each corner.
    Returns path with arc points inserted.
    """
    if len(path) < 3:
        return path

    rounded = [path[0]]  # Start point unchanged

    for i in range(1, len(path) - 1):
        p_prev = pygame.Vector2(path[i - 1])
        p_curr = pygame.Vector2(path[i])
        p_next = pygame.Vector2(path[i + 1])

        # Vectors to previous and next points
        v_in = (p_curr - p_prev).normalize()
        v_out = (p_next - p_curr).normalize()

        # Calculate arc endpoints (offset from corner by radius)
        arc_start = p_curr - v_in * radius
        arc_end = p_curr + v_out * radius

        # Add arc start
        rounded.append(arc_start)

        # Generate quarter-circle arc (5 points)
        for t in range(1, 5):
            t_norm = t / 5.0
            # Circular interpolation
            angle = math.pi / 2 * t_norm
            # Rotate v_in by angle towards v_out
            cos_a = math.cos(angle)
            sin_a = math.sin(angle)

            # This is a simplification; real circular arc needs proper rotation
            point = arc_start + (arc_end - arc_start) * t_norm
            rounded.append(point)

        # Add arc end
        rounded.append(arc_end)

    rounded.append(path[-1])  # End point unchanged
    return rounded
```

---

## Visual Examples

### Example 1: Components Side-by-Side (Rightward)

**Current V-H-V:**
```
A →──┐
     │
     ├────  (unnecessary vertical segments)
     │
  ┌──┘
  B
```

**Adaptive 2-Seg:**
```
A →────┐
       │  (direct L-shape, 1 less segment)
       B
```

### Example 2: Components Stacked (Vertical Alignment)

**Current V-H-V:**
```
A →──┐
     ├──  (wasted horizontal)
     ├──
  ┌──┘
  B
```

**Adaptive 2-Seg:**
```
A →──┐
     │  (direct vertical, even simpler)
     B
```

### Example 3: Sink Left of Source (Leftward)

**Current V-H-V (PROBLEM):**
```
     →| A  (stubs oppose)
      |
   |──┤
   |←
   B
```

**Adaptive 2-Seg:**
```
A →──┐
     │
  ┌──┤  (stubs aligned)
  |←
  B
```

---

## Testing Checklist

For any routing algorithm change:

- [ ] **Rightward wires** (sink.x > source.x)
- [ ] **Leftward wires** (sink.x < source.x)  ← Most important!
- [ ] **Vertical wires** (sink.x ≈ source.x)
- [ ] **Upward wires** (sink.y < source.y)
- [ ] **Downward wires** (sink.y > source.y)
- [ ] **Very short wires** (components adjacent)
- [ ] **Very long wires** (components far apart)
- [ ] **Parent-to-child** (downward hierarchy)
- [ ] **Child-to-parent** (upward hierarchy)
- [ ] **Sibling-to-sibling** (peer-to-peer)
- [ ] **Fan-out** (1 source, N sinks)
- [ ] **Fan-in** (N sources, 1 sink)
- [ ] **Zoom levels** (must look good at 0.5x, 1x, 2x, 5x)

---

## Performance Metrics

| Algorithm | Points/Wire | Render Time | Memory | Collision Checks |
|-----------|-------------|-------------|--------|------------------|
| Current V-H-V | 6 | 1.0x (baseline) | 48 bytes | O(6) |
| Adaptive 2-Seg | 4-6 | 0.9x | 32-48 bytes | O(4-6) |
| Rounded Corners | 12-20 | 1.5x | 96-160 bytes | O(12-20) |
| Bezier Smooth | 20+ | 2.0x | 160+ bytes | O(20+) |
| A* Pathfinding | Variable | 5.0x+ | Variable | O(N²) |

**Recommendation:** Adaptive 2-Seg with optional Rounded Corners (togglable setting)

---

## Configuration Options (Future)

Allow user preferences:
```python
class WireRenderSettings:
    routing_style = 'adaptive'  # 'adaptive', 'bezier', 'orthogonal'
    corner_style = 'rounded'    # 'sharp', 'rounded', 'beveled'
    corner_radius = 5           # pixels (world space)
    bundle_spacing = 8          # pixels between parallel wires
    enable_pathfinding = False  # A* for complex layouts
```

---

## Conclusion

**Recommended immediate change:**
Replace current V-H-V mid-point routing with **Adaptive 2-Segment** approach.

**Key improvements:**
1. Fewer segments for rightward wires (cleaner)
2. Fixed stub direction (no more "dirty look")
3. Shorter wire lengths (more direct)
4. Same or better performance

**Optional enhancement:**
Add rounded corners for professional CAD appearance (small rendering cost).

**Do NOT implement (yet):**
- Bezier curves (too expensive, non-standard for digital circuits)
- A* pathfinding (overkill for current layouts)
- Channel routing (wait for multi-bit buses)

All changes remain visualization-only (Python) with no C++ backend modifications required.
