# ui_elements.py

import pygame

class Button:
    """A customizable, clickable button with visual feedback."""

    def __init__(self, x, y, width, height, text='Button', on_click=None, font=None, text_color=(255, 255, 255)):
        self.rect = pygame.Rect(x, y, width, height)
        self.on_click = on_click
        self.text = text
        self.text_color = text_color

        # Visual states
        self.colors = {
            'normal': '#3D5A80',
            'hover': '#98C1D9',
            'pressed': '#293241',
        }
        self.border_radius = 10
        self.elevation = 5
        self.dynamic_elevation = self.elevation
        self.original_y = y

        # Font handling
        if font is None:
            self.font = pygame.font.Font(None, 32)
        else:
            self.font = font
        
        self.text_surf = self.font.render(text, True, self.text_color)
        self.text_rect = self.text_surf.get_rect(center=self.rect.center)
        
        self.is_pressed = False

    def handle_event(self, event):
        """Processes mouse events to update the button's state and trigger actions."""
        mouse_pos = pygame.mouse.get_pos()
        if self.rect.collidepoint(mouse_pos):
            if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
                self.is_pressed = True
            elif event.type == pygame.MOUSEBUTTONUP and event.button == 1:
                if self.is_pressed:
                    self.is_pressed = False
                    if self.on_click:
                        self.on_click()
        else:
            self.is_pressed = False

    def draw(self, screen):
        """Draws the button on the screen, changing appearance based on state."""
        # Update elevation and position based on state
        if self.is_pressed:
            self.dynamic_elevation = 0
            bg_color = self.colors['pressed']
        else:
            self.dynamic_elevation = self.elevation
            if self.rect.collidepoint(pygame.mouse.get_pos()):
                bg_color = self.colors['hover']
            else:
                bg_color = self.colors['normal']
        
        self.rect.y = self.original_y - self.dynamic_elevation
        self.text_rect.center = self.rect.center
        
        # Draw the shadow (bottom rect) and the button itself
        bottom_rect = pygame.Rect(self.rect.left, self.rect.top, self.rect.width, self.rect.height)
        bottom_rect.height += self.dynamic_elevation
        pygame.draw.rect(screen, '#202020', bottom_rect, border_radius=self.border_radius)
        pygame.draw.rect(screen, bg_color, self.rect, border_radius=self.border_radius)
        
        # Draw the text
        screen.blit(self.text_surf, self.text_rect)

class Slider:
    """A slider for selecting a value within a range."""

    def __init__(self, x, y, width, height, min_val, max_val, initial_val, on_change=None):
        self.rect = pygame.Rect(x, y, width, height)
        self.min_val = min_val
        self.max_val = max_val
        self.val = initial_val
        self.on_change = on_change
        
        # Visuals
        self.track_color = '#505050'
        self.handle_color = '#98C1D9'
        self.handle_width = 10
        self.handle_rect = pygame.Rect(0, self.rect.y - 5, self.handle_width, self.rect.height + 10)
        self._update_handle_pos()

        self.font = pygame.font.Font(None, 24)
        self.text_color = (200, 200, 200)

        self.is_dragging = False

    def _update_handle_pos(self):
        """Internal method to position the handle based on the current value."""
        # Map the value from [min_val, max_val] to [x, x + width]
        ratio = (self.val - self.min_val) / (self.max_val - self.min_val)
        self.handle_rect.centerx = self.rect.x + ratio * self.rect.width

    def _update_value_from_pos(self, x_pos):
        """Internal method to update the value based on the handle's x-position."""
        # Clamp the position within the track's bounds
        x_pos = max(self.rect.left, min(self.rect.right, x_pos))
        ratio = (x_pos - self.rect.left) / self.rect.width
        new_val = self.min_val + ratio * (self.max_val - self.min_val)
        
        if self.val != new_val:
            self.val = new_val
            if self.on_change:
                self.on_change(self.val)

    def handle_event(self, event):
        """Processes mouse events to control the slider."""
        if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
            if self.handle_rect.collidepoint(event.pos):
                self.is_dragging = True
        elif event.type == pygame.MOUSEBUTTONUP and event.button == 1:
            self.is_dragging = False
        elif event.type == pygame.MOUSEMOTION and self.is_dragging:
            self._update_value_from_pos(event.pos[0])
            self._update_handle_pos()

    def get_value(self):
        """Returns the current value of the slider."""
        return self.val

    def draw(self, screen):
        """Draws the slider on the screen."""
        # Draw the track
        pygame.draw.rect(screen, self.track_color, self.rect, border_radius=5)
        
        # Draw the handle
        pygame.draw.rect(screen, self.handle_color, self.handle_rect, border_radius=3)
        
        # Draw the value text
        value_text = f"{self.val:.1f}"
        text_surf = self.font.render(value_text, True, self.text_color)
        text_rect = text_surf.get_rect(centerx=self.handle_rect.centerx, y=self.handle_rect.bottom + 5)
        screen.blit(text_surf, text_rect)