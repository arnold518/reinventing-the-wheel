import pygame
import circuit_backend
import math

# --- Module-level Constants ---

STATE_COLORS = {
    circuit_backend.LogicValue.HIGH: (76, 175, 80),
    circuit_backend.LogicValue.LOW: (211, 47, 47),
    circuit_backend.LogicValue.UNKNOWN: (158, 158, 158),
    circuit_backend.LogicValue.HIGH_Z: (3, 155, 229)
}
# Restored stub length to a more visible value
WIRE_STUB_LENGTH = 10
BOUNDARY_OFFSET = WIRE_STUB_LENGTH + 7

# --- Visual Classes ---

class VisualPin:
    """A visual representation of a single input or output pin, drawn as a triangle."""
    
    def __init__(self, parent_component, cpp_pin_handle, pin_type: str):
        self.parent = parent_component
        self.cpp_handle = cpp_pin_handle
        self.pin_type = pin_type
        self.name = self.cpp_handle.get_name()
        
        self.rect = pygame.Rect(0, 0, 14, 14)
        self.color = STATE_COLORS[circuit_backend.LogicValue.UNKNOWN]
        self.text_color = (220, 220, 220)
        
        self.base_points = [pygame.Vector2(-7, -7), pygame.Vector2(7, 0), pygame.Vector2(-7, 7)]

    def update_state(self):
        current_value = self.cpp_handle.get_value()
        self.color = STATE_COLORS.get(current_value, STATE_COLORS[circuit_backend.LogicValue.UNKNOWN])

    def draw(self, screen, camera):
        screen_center = camera.apply(self.rect.center)
        zoomed_points = [screen_center + p * camera.zoom for p in self.base_points]
        
        pygame.draw.polygon(screen, self.color, zoomed_points)
        
        font_size = int(14 * camera.zoom)
        if font_size >= 10:
            font = VisualComponent.get_font(font_size)
            text_surf = font.render(self.name, True, self.text_color)
            
            if self.pin_type == 'input':
                text_rect = text_surf.get_rect(midleft=(screen_center.x + (10 * camera.zoom), screen_center.y))
            else: # output
                text_rect = text_surf.get_rect(midright=(screen_center.x - (10 * camera.zoom), screen_center.y))
            
            screen.blit(text_surf, text_rect)

class VisualWire:
    """A visual representation of a wire connecting one or more pins."""
    
    def __init__(self, cpp_wire_handle):
        self.cpp_handle = cpp_wire_handle
        self.name = cpp_wire_handle.get_name()
        self.source_vpin = None
        self.sink_vpins = []
        self.color = STATE_COLORS[circuit_backend.LogicValue.UNKNOWN]

    def connect(self, pin_map):
        source_cpp = self.cpp_handle.get_source_pin()
        if source_cpp:
            self.source_vpin = pin_map.get(source_cpp)
        for sink_cpp in self.cpp_handle.get_sink_pins():
            vpin = pin_map.get(sink_cpp)
            if vpin:
                self.sink_vpins.append(vpin)

    def update_state(self):
        current_value = self.cpp_handle.get_value()
        self.color = STATE_COLORS.get(current_value, STATE_COLORS[circuit_backend.LogicValue.UNKNOWN])

    def draw(self, screen, camera):
        self.update_state()
        if not self.source_vpin or not self.sink_vpins:
            return

        line_width = max(1, int(2 * camera.zoom))
        start_pos = pygame.Vector2(self.source_vpin.rect.center)
        
        for sink_pin in self.sink_vpins:
            end_pos = pygame.Vector2(sink_pin.rect.center)
            
            # --- FIX 3: New robust 5-segment orthogonal routing ---
            # Define stub points
            p_start_stub = pygame.Vector2(start_pos.x + WIRE_STUB_LENGTH, start_pos.y)
            p_end_stub = pygame.Vector2(end_pos.x - WIRE_STUB_LENGTH, end_pos.y)
            
            # Define the vertical line's position and the other two corners
            mid_y = (p_start_stub.y + p_end_stub.y) / 2
            p_corner1 = pygame.Vector2(p_start_stub.x, mid_y)
            p_corner2 = pygame.Vector2(p_end_stub.x, mid_y)

            # Draw the 5 segments explicitly to avoid overlap
            pygame.draw.line(screen, self.color, camera.apply(start_pos), camera.apply(p_start_stub), line_width)
            pygame.draw.line(screen, self.color, camera.apply(p_start_stub), camera.apply(p_corner1), line_width)
            pygame.draw.line(screen, self.color, camera.apply(p_corner1), camera.apply(p_corner2), line_width)
            pygame.draw.line(screen, self.color, camera.apply(p_corner2), camera.apply(p_end_stub), line_width)
            pygame.draw.line(screen, self.color, camera.apply(p_end_stub), camera.apply(end_pos), line_width)

class VisualComponent:
    _font_cache = {}
    
    @staticmethod
    def get_font(size):
        if size not in VisualComponent._font_cache:
            try:
                # FIX 2: Reverted font to be non-bold.
                VisualComponent._font_cache[size] = pygame.font.SysFont('arial', size, bold=False)
            except pygame.error:
                VisualComponent._font_cache[size] = pygame.font.Font(None, size)
        return VisualComponent._font_cache[size]

    def __init__(self, pos, size, cpp_handle: circuit_backend.Component, parent=None, color=(61, 90, 128, 100)):
        self.cpp_handle = cpp_handle
        self.parent = parent
        self.children = []
        self.rect = pygame.Rect(pos, size)
        self.relative_pos = pygame.Vector2(self.rect.topleft) - pygame.Vector2(parent.rect.topleft) if parent else pygame.Vector2(0, 0)
        
        self.is_dragging = False
        self.is_hovered = False
        self.drag_offset = pygame.Vector2(0, 0)
        
        self.title_bar_height = 30
        self.title_bar_color = tuple(max(0, c - 20) for c in color)
        self.base_color = color
        self.hover_color = (color[0], color[1], color[2], 150)
        self.border_color = (238, 244, 255)
        self.text_color = (240, 240, 240)

    @property
    def name(self):
        return self.cpp_handle.get_name()
        
    def get_content_rect(self):
        return pygame.Rect(
            self.rect.left,
            self.rect.top + self.title_bar_height,
            self.rect.width,
            self.rect.height - self.title_bar_height
        )

    def add_child(self, child_vc):
        self.children.append(child_vc)
        child_vc.parent = self
        child_vc.relative_pos = pygame.Vector2(child_vc.rect.topleft) - pygame.Vector2(self.rect.topleft)

    def handle_event(self, event, camera):
        if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1 and self.is_hovered:
            world_mouse_pos = camera.screen_to_world(pygame.mouse.get_pos())
            self.is_dragging = True
            self.drag_offset = world_mouse_pos - pygame.Vector2(self.rect.topleft)
            return True
        elif event.type == pygame.MOUSEBUTTONUP and event.button == 1 and self.is_dragging:
            self.is_dragging = False
            return True
        elif event.type == pygame.MOUSEMOTION and self.is_dragging:
            world_mouse_pos = camera.screen_to_world(pygame.mouse.get_pos())
            new_pos = world_mouse_pos - self.drag_offset
            self.move(new_pos - pygame.Vector2(self.rect.topleft))
            return True
        return False

    def move(self, delta):
        current_pos = pygame.Vector2(self.rect.topleft)
        potential_pos = current_pos + delta
        if self.parent:
            parent_content_rect = self.parent.get_content_rect()
            potential_pos.x = max(parent_content_rect.left + BOUNDARY_OFFSET, min(potential_pos.x, parent_content_rect.right - self.rect.width - BOUNDARY_OFFSET))
            potential_pos.y = max(parent_content_rect.top, min(potential_pos.y, parent_content_rect.bottom - self.rect.height))
        
        actual_delta = potential_pos - current_pos
        self.rect.topleft += actual_delta
        for child in self.children:
            child.move(actual_delta)

    def draw(self, screen, camera):
        screen_pos = camera.apply(self.rect.topleft)
        zoomed_size = (max(1, int(self.rect.width * camera.zoom)), max(1, int(self.rect.height * camera.zoom)))
        fill_color = self.hover_color if self.is_hovered else self.base_color
        border_width = 4 if self.is_hovered else 2

        zoomed_title_height = int(self.title_bar_height * camera.zoom)
        
        body_rect = pygame.Rect(
            screen_pos.x, screen_pos.y + zoomed_title_height,
            zoomed_size[0], zoomed_size[1] - zoomed_title_height
        )
        body_surface = pygame.Surface(body_rect.size, pygame.SRCALPHA)
        body_surface.fill(fill_color)
        screen.blit(body_surface, body_rect.topleft)
        
        title_rect = pygame.Rect(screen_pos, (zoomed_size[0], zoomed_title_height))
        title_surface = pygame.Surface(title_rect.size, pygame.SRCALPHA)
        title_surface.fill(self.title_bar_color)
        screen.blit(title_surface, title_rect.topleft)
        
        pygame.draw.rect(screen, self.border_color, (screen_pos, zoomed_size), border_width, border_radius=5)
        
        font_size = int(20 * camera.zoom)
        if font_size >= 10:
            font = self.get_font(font_size)
            text_surf = font.render(self.name, True, self.text_color)
            text_rect = text_surf.get_rect(center=title_rect.center)
            screen.blit(text_surf, text_rect)
            
        for child in self.children:
            child.draw(screen, camera)

class VisualIOComponent(VisualComponent):
    def __init__(self, pos, size, cpp_handle, parent=None, color=(61, 90, 128, 100)):
        super().__init__(pos, size, cpp_handle, parent, color)
        self.input_pins = []
        self.output_pins = []
        self._create_pins()
        self._layout_pins()

    def _create_pins(self):
        for name, pin_handle in self.cpp_handle.get_input_pins().items():
            self.input_pins.append(VisualPin(self, pin_handle, 'input'))
        for name, pin_handle in self.cpp_handle.get_output_pins().items():
            self.output_pins.append(VisualPin(self, pin_handle, 'output'))
            
    def _layout_pins(self):
        content_rect = self.get_content_rect()
        
        if self.input_pins:
            spacing = content_rect.height / (len(self.input_pins) + 1)
            for i, pin in enumerate(self.input_pins):
                pin.rect.centery = content_rect.top + spacing * (i + 1)
                pin.rect.centerx = self.rect.left
        if self.output_pins:
            spacing = content_rect.height / (len(self.output_pins) + 1)
            for i, pin in enumerate(self.output_pins):
                pin.rect.centery = content_rect.top + spacing * (i + 1)
                pin.rect.centerx = self.rect.right

    def get_all_pins(self):
        return self.input_pins + self.output_pins

    def move(self, delta):
        super().move(delta)
        self._layout_pins()

    def draw(self, screen, camera):
        super().draw(screen, camera)
        for pin in self.input_pins + self.output_pins:
            pin.update_state()
            pin.draw(screen, camera)