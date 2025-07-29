# visual_component.py

import pygame
# This will be your actual compiled C++ module
# For now, it will be mocked in main.py
import circuit_backend

class VisualComponent:
    """
    A visual representation that WRAPS a C++ Component object.
    It PRESERVES the original move/draw logic while getting its identity
    from the C++ handle.
    """
    _font_cache = {}
    
    @staticmethod
    def get_font(size):
        if size not in VisualComponent._font_cache:
            try:
                VisualComponent._font_cache[size] = pygame.font.SysFont('arial', size, bold=False)
            except pygame.error:
                VisualComponent._font_cache[size] = pygame.font.SysFont(None, size, bold=False)
        return VisualComponent._font_cache[size]

    def __init__(self, pos, size, cpp_handle: circuit_backend.Component, parent=None, color=(61, 90, 128, 100)):
        # The C++ handle is the source of truth for identity
        self.cpp_handle = cpp_handle
        
        # Python-side visual properties
        self.parent = parent  # VisualComponent parent
        self.children = []    # List of VisualComponent children

        self.rect = pygame.Rect(pos, size)
        
        self.relative_pos = pygame.Vector2(0, 0)
        if parent:
            self.relative_pos = pygame.Vector2(self.rect.topleft) - pygame.Vector2(parent.rect.topleft)
            
        self.is_dragging = False
        self.is_hovered = False
        self.drag_offset = pygame.Vector2(0, 0)

        # Visual appearance properties
        self.base_color = color
        self.hover_color = (152, 193, 217, 150)
        self.border_color = (238, 244, 255)
        self.base_font_size = 28
        self.text_color = (240, 240, 240)

    # --- Property wrappers to access C++ data ---
    @property
    def id(self):
        return self.cpp_handle.get_name()

    @property
    def name(self):
        return self.cpp_handle.get_name()

    def add_child(self, child_visual_component):
        """
        Adds a child to the Python VISUAL hierarchy, matching the original logic.
        """
        self.children.append(child_visual_component)
        child_visual_component.parent = self
        child_visual_component.relative_pos = pygame.Vector2(child_visual_component.rect.topleft) - pygame.Vector2(self.rect.topleft)

    def handle_event(self, event, camera):
        """Processes a single Pygame event. (Original Logic)"""
        if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
            if self.is_hovered:
                world_mouse_pos = camera.screen_to_world(pygame.mouse.get_pos())
                self.is_dragging = True
                self.drag_offset = world_mouse_pos - pygame.Vector2(self.rect.topleft)
                return True

        elif event.type == pygame.MOUSEBUTTONUP and event.button == 1:
            if self.is_dragging:
                self.is_dragging = False
                return True

        elif event.type == pygame.MOUSEMOTION:
            if self.is_dragging:
                world_mouse_pos = camera.screen_to_world(pygame.mouse.get_pos())
                new_pos = world_mouse_pos - self.drag_offset
                move_delta = new_pos - pygame.Vector2(self.rect.topleft)
                self.move(move_delta)
                return True
        return False

    def move(self, delta):
        """
        Moves the component and its children, clamped to the parent's bounds.
        """
        current_pos = pygame.Vector2(self.rect.topleft)
        potential_pos = current_pos + delta
        
        if self.parent:
            parent_rect = self.parent.rect
            potential_pos.x = max(parent_rect.left, min(potential_pos.x, parent_rect.right - self.rect.width))
            potential_pos.y = max(parent_rect.top, min(potential_pos.y, parent_rect.bottom - self.rect.height))

        actual_delta = potential_pos - current_pos
        self.rect.topleft += actual_delta
        
        # Recursively call move on children so they can perform their own clamping
        for child in self.children:
            child.move(actual_delta)

    def draw(self, screen, camera):
        """Draws the component, its name, and its children."""
        screen_pos = camera.apply(self.rect.topleft)
        zoomed_size = (max(1, int(self.rect.width * camera.zoom)), max(1, int(self.rect.height * camera.zoom)))
        
        fill_color = self.hover_color if self.is_hovered else self.base_color
        border_width = 4 if self.is_hovered else 2

        try:
            body_surface = pygame.Surface(zoomed_size, pygame.SRCALPHA)
            body_surface.fill(fill_color)
            screen.blit(body_surface, screen_pos)
        except pygame.error:
            pass
        
        pygame.draw.rect(screen, self.border_color, (screen_pos, zoomed_size), border_width, border_radius=5)
        
        font_size = int(self.base_font_size * camera.zoom)
        if font_size >= 10:
            try:
                font = self.get_font(font_size)
                text_surf = font.render(self.name, True, self.text_color)
                padding = int(8 * camera.zoom)
                text_rect = text_surf.get_rect(topleft=(screen_pos.x + padding, screen_pos.y + padding))
                screen.blit(text_surf, text_rect)
            except pygame.error as e:
                print(f"Font rendering error: {e}")

        for child in self.children:
            child.draw(screen, camera)