# main.py

import pygame
import sys

# =============================================================================
# --- Mock Circuit Backend ---
# This section simulates your pybind11 module.
# REMOVE THIS when you have your actual C++ module compiled.
class MockComponent:
    def __init__(self, name):
        self._name = name
        self._children = []
        self._parent = None
    def get_name(self): return self._name
    def get_children(self): return self._children
    def get_parent(self): return self._parent
    def addChild(self, child):
        self._children.append(child)
        child._parent = self

class MockCircuitBackend:
    def __init__(self):
        self.Component = MockComponent
    def create_alu_example(self):
        root = self.Component("alu")
        child1 = self.Component("adder")
        grandchild1 = self.Component("and1")
        root.addChild(child1)
        child1.addChild(grandchild1)
        return root

sys.modules['circuit_backend'] = MockCircuitBackend()
# =============================================================================

import circuit_backend
from camera import Camera
from ui_elements import Button, Slider
from visual_component import VisualComponent

pygame.init()

# --- Layout Definition (Same as before) ---
COMPONENT_LAYOUTS = {
    'alu':    {'pos': (100, 100), 'size': (600, 500)},
    'adder':  {'pos': (150, 180), 'size': (300, 250), 'color': (148, 67, 255, 100)},
    'and1':   {'pos': (180, 240), 'size': (100, 80),  'color': (67, 175, 255, 100)},
}

def build_visual_hierarchy(cpp_component, parent_vc=None, created_map=None):
    """Recursively builds a tree of VisualComponents by reading the C++ hierarchy."""
    if created_map is None:
        created_map = {}
    name = cpp_component.get_name()
    layout = COMPONENT_LAYOUTS.get(name)
    if not layout:
        print(f"Warning: No layout defined for component '{name}'. Skipping.")
        return None
    visual_comp = VisualComponent(
        pos=layout['pos'],
        size=layout['size'],
        cpp_handle=cpp_component,
        parent=parent_vc,
        color=layout.get('color', (61, 90, 128, 100))
    )
    created_map[name] = visual_comp
    for cpp_child in cpp_component.get_children():
        child_vc = build_visual_hierarchy(cpp_child, parent_vc=visual_comp, created_map=created_map)
        if child_vc:
            visual_comp.add_child(child_vc)
    return visual_comp

# --- Screen and UI Setup (Original Logic) ---
SCREEN_WIDTH, SCREEN_HEIGHT = 1920, 1080
screen = pygame.display.set_mode((SCREEN_WIDTH, SCREEN_HEIGHT))
pygame.display.set_caption("Circuit Simulator - Component Update")
clock = pygame.time.Clock()
camera = Camera((SCREEN_WIDTH, SCREEN_HEIGHT))
def on_reset_button_click():
    camera.reset()
    zoom_slider.val = camera.zoom
    zoom_slider._update_handle_pos()
def on_zoom_slider_change(new_zoom):
    if abs(camera.zoom - new_zoom) > 0.01:
        camera.zoom = new_zoom
reset_button = Button(x=20, y=20, width=150, height=50, text='Reset View', on_click=on_reset_button_click)
zoom_slider = Slider(x=20, y=100, width=200, height=10, min_val=camera.min_zoom, max_val=camera.max_zoom, initial_val=camera.zoom, on_change=on_zoom_slider_change)
ui_elements = [reset_button, zoom_slider]

# --- HIERARCHY CREATION (New Architecture) ---
root_cpp_component = circuit_backend.create_alu_example()
all_components_map = {}
build_visual_hierarchy(root_cpp_component, created_map=all_components_map)
all_components = list(all_components_map.values())

# --- Main loop ---
running = True
while running:
    dt = clock.tick(60)
    mouse_pos = pygame.mouse.get_pos()
    mouse_over_ui = any(
        (hasattr(elem, 'rect') and elem.rect.collidepoint(mouse_pos)) or
        (hasattr(elem, 'handle_rect') and elem.handle_rect.collidepoint(mouse_pos))
        for elem in ui_elements
    )
    world_mouse_pos = camera.screen_to_world(mouse_pos)
    hovered_component = None
    if not mouse_over_ui:
        for component in reversed(all_components):
            if component.rect.collidepoint(world_mouse_pos):
                hovered_component = component
                break
    for component in all_components:
        component.is_hovered = (component == hovered_component)

    event_was_consumed = False
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False
        for element in ui_elements:
            element.handle_event(event)
        if not mouse_over_ui and hovered_component:
            if hovered_component.handle_event(event, camera):
                event_was_consumed = True
        is_mouse_event = event.type in (pygame.MOUSEBUTTONDOWN, pygame.MOUSEBUTTONUP, pygame.MOUSEMOTION, pygame.MOUSEWHEEL)
        if not (is_mouse_event and (mouse_over_ui or event_was_consumed)):
            camera.handle_event(event)

    if not zoom_slider.is_dragging and abs(zoom_slider.get_value() - camera.zoom) > 0.01:
        zoom_slider.val = camera.zoom
        zoom_slider._update_handle_pos()

    # --- Drawing ---
    screen.fill((30, 30, 30))
    camera.draw_grid(screen)
    
    for component in all_components:
        if component.parent is None:
            component.draw(screen, camera)

    for element in ui_elements:
        element.draw(screen)

    pygame.display.flip()

pygame.quit()