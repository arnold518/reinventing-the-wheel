# Quick Visualizer Test Guide

Run the visualizer to see the new wire routing in action!

## Running the Visualizer

### Prerequisites
```bash
# Ensure Python module is built (automatic after make)
cd /home/arnold/arnold/github/reinventing-the-wheel/build
make circuit_backend
```

### Launch Visualizer
```bash
cd /home/arnold/arnold/github/reinventing-the-wheel/visualizer
python main.py
```

## What to Look For

### 1. Adaptive Stub Direction ✅
**Test:** Look at wires connecting components

**Before (OLD):**
```
Component A →|     |← Component B
             |     |
   (stubs oppose each other - UGLY)
```

**After (NEW):**
```
Component A →|
             |────|
                 |← Component B
   (stubs aligned - CLEAN)
```

### 2. Rounded Corners ✅
**Test:** Zoom in on any wire corner

**Before:** Sharp 90° angles `└`
**After:** Smooth curves `╰`

### 3. Shorter Paths ✅
**Test:** Rightward wires (source on left, sink on right)

**Before:** Always 3-segment (V-H-V)
**After:** Simple L-shape (H-V)

### 4. Boundary Compliance ✅
**Test:** Look at wires near component edges

**Check:** Stubs should NOT extend beyond component rectangles
**Margin:** 5px clearance from edges

## Interactive Testing

### Controls
- **Spacebar:** Play/pause simulation
- **Left/Right arrows:** Step through time
- **Mouse drag:** Pan camera
- **Mouse wheel:** Zoom in/out
- **Drag components:** Move them around

### Test Scenarios

#### Scenario 1: Rightward Flow
1. Find FullAdder test visualization
2. Look at wires from left components to right components
3. **Expected:** Clean L-shapes, fewer bends

#### Scenario 2: Leftward Flow
1. (If any wires go leftward in the circuit)
2. **Expected:** Z-shape with aligned stubs (both pointing right)

#### Scenario 3: Hierarchical (Parent-Child)
1. Look at wires from FullAdder to internal HalfAdders
2. **Expected:** Both stubs extend right (into parent space)

#### Scenario 4: Component Movement
1. Drag a component around
2. Watch wires update in real-time
3. **Expected:** Smooth transitions, no glitches

## Debugging

### If Visualizer Crashes
```python
# Check Python traceback for errors
# Common issues:
# 1. pygame not installed: pip install pygame
# 2. circuit_backend not found: rebuild C++ module
# 3. Syntax error: check visual_component.py
```

### If Wires Look Wrong
1. Check console for errors
2. Verify `visual_component.py` has new code (lines 89-288)
3. Try zooming in/out to see if it's a rendering issue
4. Report specific wire that looks wrong

## Performance Check

### FPS Counter
- Should maintain 60 FPS with < 100 wires
- Acceptable: 30+ FPS with rounded corners
- If < 30 FPS: May need optimization

### Visual Smoothness
- Wires should render smoothly at all zoom levels
- No flickering or stuttering
- Rounded corners should look circular, not jagged

## Comparison Screenshots (TODO)

Take screenshots showing:
1. **Before:** Old V-H-V routing with sharp corners
2. **After:** New adaptive routing with rounded corners
3. Save to `docs/images/wire-routing-comparison.png`

## Known Visual Artifacts

### Expected Behavior
- Rounded corners may skip on very short segments (< 10px)
- This is correct - prevents overlapping arcs

### NOT Expected (Report if Seen)
- Stubs extending beyond component boundaries
- Wires overlapping component rectangles
- Sharp corners on long segments
- Reversed stubs on leftward wires

## Next Steps

After visual verification:
1. ✅ Verify wire hover still works
2. ✅ Verify wire selection works
3. ✅ Test with different circuit types (HalfAdder, FullAdder, etc.)
4. ✅ Measure FPS with many wires
5. 🔲 Implement wire bundle offset (next feature)
6. 🔲 Add multi-bit bus visualization (future)

---

**Happy Testing!** The new routing should look significantly cleaner with smooth, professional-looking wire paths.
