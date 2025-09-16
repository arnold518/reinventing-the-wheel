import pygame
import circuit_backend

STATE_COLORS = {
    circuit_backend.LogicValue.HIGH: (76, 175, 80),
    circuit_backend.LogicValue.LOW: (211, 47, 47),
    circuit_backend.LogicValue.UNKNOWN: (158, 158, 158),
    circuit_backend.LogicValue.HIGH_Z: (3, 155, 229)
}

class VisualPin:
    def __init__(self, parent_component, cpp_pin_handle, pin_type: str):
        self.parent = parent_component
        self.cpp_handle = cpp_pin_handle
        self.pin_type = pin_type
        self.name = self.cpp_handle.get_name()
        self.rect = pygame.Rect(0, 0, 1, 1)
        self.color = STATE_COLORS[circuit_backend.LogicValue.UNKNOWN]
        self.text_color = (220, 220, 220)
        self.base_points = []
        self._text_surf = None
        self._last_font_size = -1
        self.is_hovered = False

    def update_state(self):
        self.color = STATE_COLORS.get(self.cpp_handle.get_value(), STATE_COLORS[circuit_backend.LogicValue.UNKNOWN])

    def draw(self, screen, camera):
        screen_center = camera.apply(self.rect.center)
        zoomed_points = [screen_center + p * camera.zoom for p in self.base_points]
        pygame.draw.polygon(screen, self.color, zoomed_points)
        if self.is_hovered:
            pygame.draw.polygon(screen, (255, 255, 0), zoomed_points, width=2)
        
        font_size = int(self.parent.rect.width * (self.parent.font_width_ratio * 0.7) * camera.zoom)
        if font_size >= 8:
            if font_size != self._last_font_size:
                font = VisualComponent.get_font(font_size)
                self._text_surf = font.render(self.name, True, self.text_color)
                self._last_font_size = font_size
            if self._text_surf:
                if self.pin_type == 'input':
                    text_rect = self._text_surf.get_rect(midleft=(screen_center.x + (self.rect.width/2 * camera.zoom) + 5, screen_center.y))
                else:
                    text_rect = self._text_surf.get_rect(midright=(screen_center.x - (self.rect.width/2 * camera.zoom) - 5, screen_center.y))
                screen.blit(self._text_surf, text_rect)

class VisualWire:
    def __init__(self, cpp_wire_handle):
        self.cpp_handle = cpp_wire_handle
        self.source_vpin = None
        self.sink_vpins = []
        self.color = STATE_COLORS[circuit_backend.LogicValue.UNKNOWN]
        self.is_hovered = False
        self._world_paths = []

    def connect(self, pin_map):
        source_cpp = self.cpp_handle.get_source_pin()
        if source_cpp and (owner := source_cpp.get_owner()):
            key = f"{owner.get_id()}.{source_cpp.get_name()}"
            self.source_vpin = pin_map.get(key)
        
        for sink_cpp in self.cpp_handle.get_sink_pins():
            if (owner := sink_cpp.get_owner()):
                key = f"{owner.get_id()}.{sink_cpp.get_name()}"
                if (vpin := pin_map.get(key)):
                    self.sink_vpins.append(vpin)

    def update_state(self):
        self.color = STATE_COLORS.get(self.cpp_handle.get_value(), STATE_COLORS[circuit_backend.LogicValue.UNKNOWN])

    def collidepoint(self, world_pos, camera):
        threshold_sq = (5 / camera.zoom) ** 2
        for path in self._world_paths:
            for i in range(len(path) - 1):
                p1, p2 = path[i], path[i+1]
                line_vec = p2 - p1
                line_len_sq = line_vec.length_squared()
                if line_len_sq == 0: continue
                
                point_vec = world_pos - p1
                t = max(0, min(1, point_vec.dot(line_vec) / line_len_sq))
                projection = p1 + t * line_vec
                
                if (world_pos - projection).length_squared() < threshold_sq:
                    return True
        return False
        
    def draw(self, screen, camera):
        self.update_state()
        if not self.source_vpin or not self.sink_vpins:
            return

        line_width = max(1, int(2 * camera.zoom))
        if self.is_hovered:
            line_width *= 2

        start_pos = pygame.Vector2(self.source_vpin.rect.center)
        self._world_paths.clear()

        for sink_pin in self.sink_vpins:
            end_pos = pygame.Vector2(sink_pin.rect.center)
            source_owner = self.source_vpin.parent
            sink_owner = sink_pin.parent

            source_stub_len = source_owner.rect.width * source_owner.wire_stub_ratio
            sink_stub_len = sink_owner.rect.width * sink_owner.wire_stub_ratio

            p_start_trunk = start_pos + pygame.Vector2(source_stub_len, 0)

            if source_owner.rect.contains(sink_owner.rect):
                p_end_trunk = end_pos + pygame.Vector2(sink_stub_len, 0)
            elif sink_owner.rect.contains(source_owner.rect):
                p_end_trunk = end_pos - pygame.Vector2(sink_stub_len, 0)
            else:
                p_end_trunk = end_pos - pygame.Vector2(sink_stub_len, 0)

            mid_x = (p_start_trunk.x + p_end_trunk.x) / 2
            path_points = [start_pos, p_start_trunk, pygame.Vector2(mid_x, p_start_trunk.y),
                           pygame.Vector2(mid_x, p_end_trunk.y), p_end_trunk, end_pos]
            
            self._world_paths.append(path_points)
            screen_points = [camera.apply(p) for p in path_points]
            pygame.draw.lines(screen, self.color, False, screen_points, line_width)

class VisualComponent:
    _font_cache = {}
    
    @staticmethod
    def get_font(size):
        size = int(size)
        if size not in VisualComponent._font_cache:
            try:
                VisualComponent._font_cache[size] = pygame.font.SysFont('arial', size, bold=False)
            except pygame.error:
                VisualComponent._font_cache[size] = pygame.font.Font(None, size)
        return VisualComponent._font_cache[size]

    def __init__(self, rect, cpp_handle, settings, layout_key, rel_info, depth=0, parent=None, color=(61, 90, 128, 100), aspect_ratio=1.0):
        self.rect = pygame.Rect(rect)
        self.cpp_handle = cpp_handle
        self.parent = parent
        self.children = []
        self.is_dragging = False
        self.is_resizing = False
        self._is_hovered = False
        self.drag_offset = pygame.Vector2(0, 0)
        self.depth = depth
        self.resize_mode = None
        self.min_width = 40
        
        self.title_bar_ratio = settings.get('title_bar_ratio', 0.15)
        self.font_width_ratio = settings.get('font_width_ratio', 0.18)
        self.wire_stub_ratio = settings.get('wire_stub_ratio', 0.15)
        self.pin_size_ratio = settings.get('pin_size_ratio', 0.1)

        self.base_color = tuple(color)
        self.hover_color = self.base_color[:3] + (150,)
        self.border_color = (238, 244, 255)
        self.text_color = (240, 240, 240)
        title_rgb = [max(0, c - 20) for c in self.base_color[:3]]
        self.title_bar_color = tuple(title_rgb + [self.base_color[3]])
        
        self._is_dirty = True
        self._body_surface = None
        self._title_surface = None
        self._last_zoom = -1
        
        self.rel_info = rel_info
        self.layout_key = layout_key
        self.aspect_ratio = aspect_ratio

    @property
    def name(self): return self.cpp_handle.get_name()
    @property
    def is_hovered(self): return self._is_hovered
    @is_hovered.setter
    def is_hovered(self, value):
        if self._is_hovered != value:
            self._is_hovered = value
            self._is_dirty = True

    def _render_surfaces(self, zoom):
        zoomed_w = max(1, int(self.rect.width * zoom))
        zoomed_h = max(1, int(self.rect.height * zoom))
        zoomed_title_h = max(1, int(zoomed_w * self.title_bar_ratio))
        body_h = max(1, zoomed_h - zoomed_title_h)
        
        fill_color = self.hover_color if self.is_hovered else self.base_color
        self._body_surface = pygame.Surface((zoomed_w, body_h), pygame.SRCALPHA)
        self._body_surface.fill(fill_color)
        
        self._title_surface = pygame.Surface((zoomed_w, zoomed_title_h), pygame.SRCALPHA)
        self._title_surface.fill(self.title_bar_color)
        self._is_dirty = False
        
    def draw(self, screen, camera):
        if camera.zoom != self._last_zoom:
            self._is_dirty = True
            self._last_zoom = camera.zoom
        if self._is_dirty or self._body_surface is None:
            self._render_surfaces(camera.zoom)
        
        screen_pos = camera.apply(self.rect.topleft)
        zoomed_size = (self._body_surface.get_width(), self._body_surface.get_height() + self._title_surface.get_height())
        zoomed_title_h = self._title_surface.get_height()

        screen.blit(self._body_surface, (screen_pos.x, screen_pos.y + zoomed_title_h))
        screen.blit(self._title_surface, screen_pos)
        pygame.draw.rect(screen, self.border_color, (screen_pos, zoomed_size), 4 if self.is_hovered else 2, border_radius=5)
        
        font_size = self.rect.width * self.font_width_ratio * camera.zoom
        if font_size >= 10:
            font = self.get_font(font_size)
            text_surf = font.render(self.name, True, self.text_color)
            title_center = (screen_pos.x + zoomed_size[0]/2, screen_pos.y + zoomed_title_h/2)
            screen.blit(text_surf, text_surf.get_rect(center=title_center))
            
        for child in self.children:
            child.draw(screen, camera)
    
    def get_content_rect(self):
        title_height = self.rect.width * self.title_bar_ratio
        stub_width = self.rect.width * self.wire_stub_ratio
        return pygame.Rect(self.rect.left + stub_width, self.rect.top + title_height, 
                           self.rect.width - 2 * stub_width, self.rect.height - title_height)

    def add_child(self, child_vc):
        self.children.append(child_vc)

    def get_hovered_border(self, world_pos, camera):
        if not self.is_hovered: return None
        threshold = 5 / camera.zoom
        on_left = abs(world_pos.x - self.rect.left) < threshold
        on_right = abs(world_pos.x - self.rect.right) < threshold
        on_top = abs(world_pos.y - self.rect.top) < threshold
        on_bottom = abs(world_pos.y - self.rect.bottom) < threshold
        in_y_range = self.rect.top < world_pos.y < self.rect.bottom
        in_x_range = self.rect.left < world_pos.x < self.rect.right
        
        if on_left and in_y_range: return 'left'
        if on_right and in_y_range: return 'right'
        if on_top and in_x_range: return 'top'
        if on_bottom and in_x_range: return 'bottom'
        return None

    def handle_event(self, event, camera, layout_manager):
        world_mouse_pos = camera.screen_to_world(pygame.mouse.get_pos())
        if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1 and self.is_hovered:
            border = self.get_hovered_border(world_mouse_pos, camera)
            if border:
                self.is_resizing, self.is_dragging = True, False
                self.resize_mode = border
            else:
                self.is_resizing, self.is_dragging = False, True
                self.drag_offset = world_mouse_pos - pygame.Vector2(self.rect.topleft)
            return True
        
        if event.type == pygame.MOUSEMOTION:
            if self.is_resizing:
                self.resize(self.resize_mode, world_mouse_pos, layout_manager)
                return True
            if self.is_dragging:
                new_pos = world_mouse_pos - self.drag_offset
                delta = new_pos - pygame.Vector2(self.rect.topleft)
                self.move(delta, layout_manager)
                return True
        return False

    def resize(self, mode, world_mouse_pos, lm):
        if not self.parent:
            p_rect = None
        else:
            p_rect = self.parent.get_content_rect()

        clamped_mouse_pos = pygame.Vector2(world_mouse_pos)
        if p_rect:
            clamped_mouse_pos.x = max(p_rect.left, min(clamped_mouse_pos.x, p_rect.right))
            clamped_mouse_pos.y = max(p_rect.top, min(clamped_mouse_pos.y, p_rect.bottom))

        ideal_w, ideal_h = 0, 0
        
        if mode in ['right', 'left']:
            if mode == 'right': ideal_w = clamped_mouse_pos.x - self.rect.left
            else: ideal_w = self.rect.right - clamped_mouse_pos.x
            ideal_w = max(self.min_width, ideal_w)
            ideal_h = ideal_w * self.aspect_ratio
        else: # top, bottom
            if mode == 'bottom': ideal_h = clamped_mouse_pos.y - self.rect.top
            else: ideal_h = self.rect.bottom - clamped_mouse_pos.y
            ideal_h = max(self.min_width * self.aspect_ratio, ideal_h)
            ideal_w = ideal_h / self.aspect_ratio

        final_w, final_h = ideal_w, ideal_h
        
        if p_rect:
            max_w, max_h = 0, 0
            if mode == 'right':
                max_w = p_rect.right - self.rect.left
                max_h = p_rect.bottom - self.rect.top
            elif mode == 'left':
                max_w = self.rect.right - p_rect.left
                max_h = p_rect.bottom - self.rect.top
            elif mode == 'bottom':
                max_w = p_rect.right - self.rect.left
                max_h = p_rect.bottom - self.rect.top
            elif mode == 'top':
                max_w = p_rect.right - self.rect.left
                max_h = self.rect.bottom - p_rect.top
            
            width_overflow = ideal_w / max_w if max_w > 0 else 1
            height_overflow = ideal_h / max_h if max_h > 0 else 1
            
            scale_factor = max(1.0, width_overflow, height_overflow)
            
            final_w = ideal_w / scale_factor
            final_h = ideal_h / scale_factor

        new_rect = self.rect.copy()
        new_rect.width, new_rect.height = final_w, final_h

        if mode == 'left':
            new_rect.left = self.rect.right - final_w
        elif mode == 'top':
            new_rect.top = self.rect.bottom - final_h
        
        self.rect = new_rect
        self._update_geometry()

        if self.parent:
            new_rel_pos = [(self.rect.left - self.parent.rect.left) / self.parent.rect.width, (self.rect.top - self.parent.rect.top) / self.parent.rect.height]
            new_rel_width = self.rect.width / self.parent.rect.width
            self.rel_info['rel_pos'] = new_rel_pos
            self.rel_info['rel_width'] = new_rel_width
            lm.update_child_position(self.parent.layout_key, self.name, new_rel_pos)
            lm.update_child_width(self.parent.layout_key, self.name, new_rel_width)
        else:
            lm.update_root_position(self.rect.topleft)
            lm.update_root_width(self.rect.width)

    def move(self, delta, lm):
        potential_pos = pygame.Vector2(self.rect.topleft) + delta
        if self.parent:
            p_rect = self.parent.get_content_rect()
            potential_pos.x = max(p_rect.left, min(potential_pos.x, p_rect.right - self.rect.width))
            potential_pos.y = max(p_rect.top, min(potential_pos.y, p_rect.bottom - self.rect.height))
        
        self.rect.topleft = potential_pos
        self._update_geometry()

        if self.parent:
            new_rel_pos = [(self.rect.left - self.parent.rect.left) / self.parent.rect.width, (self.rect.top - self.parent.rect.top) / self.parent.rect.height]
            self.rel_info['rel_pos'] = new_rel_pos
            lm.update_child_position(self.parent.layout_key, self.name, new_rel_pos)
        else:
            lm.update_root_position(self.rect.topleft)

    def _update_geometry(self):
        self.recalculate_children_layout()
        if isinstance(self, VisualIOComponent): self._layout_pins()
        self._is_dirty = True

    def recalculate_children_layout(self):
        for child in self.children:
            rel_pos, rel_width = child.rel_info['rel_pos'], child.rel_info['rel_width']
            
            new_width = self.rect.width * rel_width
            new_height = new_width * child.aspect_ratio
            new_left = self.rect.x + self.rect.width * rel_pos[0]
            new_top = self.rect.y + self.rect.height * rel_pos[1]
            
            child.rect.update(new_left, new_top, new_width, new_height)
            child._update_geometry()

class VisualIOComponent(VisualComponent):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.input_pins, self.output_pins = [], []
        self._create_pins()
        self._layout_pins()

    def _create_pins(self):
        for name, pin_handle in self.cpp_handle.get_input_pins().items():
            self.input_pins.append(VisualPin(self, pin_handle, 'input'))
        for name, pin_handle in self.cpp_handle.get_output_pins().items():
            self.output_pins.append(VisualPin(self, pin_handle, 'output'))

    def _layout_pins(self):
        content_rect = self.get_content_rect()
        pin_height = content_rect.width * self.pin_size_ratio
        pin_width = pin_height

        if self.input_pins:
            spacing = content_rect.height / (len(self.input_pins) + 1)
            for i, pin in enumerate(self.input_pins):
                pin.rect.size = (pin_width, pin_height)
                w2, h2 = pin.rect.width/2, pin.rect.height/2
                pin.base_points = [pygame.Vector2(-w2, -h2), pygame.Vector2(w2, 0), pygame.Vector2(-w2, h2)]
                pin.rect.center = (self.rect.left, content_rect.top + spacing * (i + 1))
        
        if self.output_pins:
            spacing = content_rect.height / (len(self.output_pins) + 1)
            for i, pin in enumerate(self.output_pins):
                pin.rect.size = (pin_width, pin_height)
                w2, h2 = pin.rect.width/2, pin.rect.height/2
                pin.base_points = [pygame.Vector2(-w2, -h2), pygame.Vector2(w2, 0), pygame.Vector2(-w2, h2)]
                pin.rect.center = (self.rect.right, content_rect.top + spacing * (i + 1))

    def get_all_pins(self):
        return self.input_pins + self.output_pins
        
    def draw(self, screen, camera):
        super().draw(screen, camera)
        for pin in self.get_all_pins():
            pin.update_state()
            pin.draw(screen, camera)