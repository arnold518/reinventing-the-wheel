import pygame
from visual_component import VisualComponent

class Button:
    """A customizable, clickable button with visual feedback."""

    def __init__(self, x, y, width, height, text='Button', on_click=None, text_size=28, text_color=(255, 255, 255)):
        self.rect = pygame.Rect(x, y, width, height)
        self.on_click = on_click
        self.text = text
        self.text_color = text_color
        self.colors = {'normal': '#3D5A80', 'hover': '#98C1D9', 'pressed': '#293241'}
        self.border_radius = 20
        self.elevation = 5
        self.dynamic_elevation = self.elevation
        self.original_y = y
        self.font = VisualComponent.get_font(text_size)
        self.text_surf = self.font.render(text, True, self.text_color)
        self.text_rect = self.text_surf.get_rect(center=self.rect.center)
        self.is_pressed = False

    def set_text(self, new_text):
        """--- NEW --- Updates the button's text and re-creates the text surface and rect."""
        self.text = new_text
        self.text_surf = self.font.render(self.text, True, self.text_color)
        self.text_rect = self.text_surf.get_rect(center=self.rect.center)

    def handle_event(self, event):
        mouse_pos = pygame.mouse.get_pos()
        if self.rect.collidepoint(mouse_pos):
            if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
                self.is_pressed = True
            elif event.type == pygame.MOUSEBUTTONUP and event.button == 1 and self.is_pressed:
                self.is_pressed = False
                if self.on_click: self.on_click()
        else: self.is_pressed = False

    def draw(self, screen):
        if self.is_pressed:
            self.dynamic_elevation = 0
            bg_color = self.colors['pressed']
        else:
            self.dynamic_elevation = self.elevation
            bg_color = self.colors['hover'] if self.rect.collidepoint(pygame.mouse.get_pos()) else self.colors['normal']
        
        self.rect.y = self.original_y - self.dynamic_elevation
        self.text_rect.center = self.rect.center
        
        bottom_rect = pygame.Rect(self.rect.left, self.rect.top, self.rect.width, self.rect.height)
        bottom_rect.height += self.dynamic_elevation
        pygame.draw.rect(screen, '#202020', bottom_rect, border_radius=self.border_radius)
        pygame.draw.rect(screen, bg_color, self.rect, border_radius=self.border_radius)
        screen.blit(self.text_surf, self.text_rect)

class Slider:
    """A slider for selecting a value within a range, with discrete steps."""

    def __init__(self, x, y, width, height, min_val, max_val, initial_val, on_change=None, step=1.0):
        self.rect = pygame.Rect(x, y, width, height)
        self.min_val = min_val
        self.max_val = max_val
        self.val = initial_val
        self.on_change = on_change
        self.step = step
        self.track_color = '#505050'
        self.handle_color = '#98C1D9'
        self.handle_width = 10
        self.handle_rect = pygame.Rect(0, self.rect.y - 5, self.handle_width, self.rect.height + 10)
        self._update_handle_pos()
        self.font = VisualComponent.get_font(20)
        self.text_color = (200, 200, 200)
        self.is_dragging = False

    def _update_handle_pos(self):
        ratio = (self.val - self.min_val) / (self.max_val - self.min_val) if self.max_val > self.min_val else 0
        self.handle_rect.centerx = self.rect.x + ratio * self.rect.width

    def _update_value_from_pos(self, x_pos):
        x_pos = max(self.rect.left, min(self.rect.right, x_pos))
        ratio = (x_pos - self.rect.left) / self.rect.width
        raw_val = self.min_val + ratio * (self.max_val - self.min_val)
        snapped_val = round(raw_val / self.step) * self.step
        if self.val != snapped_val:
            self.val = snapped_val
            if self.on_change: self.on_change(self.val)

    def handle_event(self, event):
        if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
            if self.handle_rect.collidepoint(event.pos) or self.rect.collidepoint(event.pos):
                self.is_dragging = True
                self._update_value_from_pos(event.pos[0])
                self._update_handle_pos()
        elif event.type == pygame.MOUSEBUTTONUP and event.button == 1:
            self.is_dragging = False
        elif event.type == pygame.MOUSEMOTION and self.is_dragging:
            self._update_value_from_pos(event.pos[0])
            self._update_handle_pos()

    def get_value(self):
        return self.val

    def draw(self, screen):
        pygame.draw.rect(screen, self.track_color, self.rect, border_radius=5)
        pygame.draw.rect(screen, self.handle_color, self.handle_rect, border_radius=3)