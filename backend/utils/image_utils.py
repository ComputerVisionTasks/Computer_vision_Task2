"""
Image utility functions for conversion between PIL and NumPy,
base64 encoding/decoding, and image preprocessing.
"""

import base64
import io
import numpy as np
from PIL import Image, ImageOps
from typing import Tuple, Optional, Union


def pil_to_numpy(image: Image.Image) -> np.ndarray:
    """
    Convert PIL Image to NumPy array.
    
    Args:
        image: PIL Image object
        
    Returns:
        NumPy array (H, W, C) for RGB or (H, W) for grayscale
    """
    return np.array(image)


def numpy_to_pil(array: np.ndarray) -> Image.Image:
    """
    Convert NumPy array to PIL Image.
    
    Args:
        array: NumPy array
        
    Returns:
        PIL Image object
    """
    if array.dtype != np.uint8:
        array = np.clip(array, 0, 255).astype(np.uint8)
    
    if len(array.shape) == 2:
        return Image.fromarray(array, mode='L')
    elif len(array.shape) == 3 and array.shape[2] == 3:
        return Image.fromarray(array, mode='RGB')
    elif len(array.shape) == 3 and array.shape[2] == 4:
        return Image.fromarray(array, mode='RGBA')
    else:
        raise ValueError(f"Unsupported array shape: {array.shape}")


def base64_to_image(base64_string: str) -> Image.Image:
    """
    Convert base64 string to PIL Image.
    
    Args:
        base64_string: Base64 encoded image string (with or without data URL prefix)
        
    Returns:
        PIL Image object
    """
    # Remove data URL prefix if present
    if ',' in base64_string:
        base64_string = base64_string.split(',')[1]
    
    # Decode base64
    image_bytes = base64.b64decode(base64_string)
    
    # Create PIL Image
    image = Image.open(io.BytesIO(image_bytes))
    
    # Convert to RGB if necessary
    if image.mode not in ('RGB', 'L'):
        image = image.convert('RGB')
    
    return image


def image_to_base64(image: Image.Image, format: str = 'PNG') -> str:
    """
    Convert PIL Image to base64 string with data URL prefix.
    
    Args:
        image: PIL Image object
        format: Image format (PNG, JPEG, etc.)
        
    Returns:
        Base64 encoded string with data URL prefix
    """
    buffered = io.BytesIO()
    
    # Convert to RGB if saving as JPEG
    if format.upper() == 'JPEG' and image.mode == 'RGBA':
        image = image.convert('RGB')
    
    image.save(buffered, format=format)
    img_str = base64.b64encode(buffered.getvalue()).decode()
    
    mime_type = f"image/{format.lower()}"
    return f"data:{mime_type};base64,{img_str}"


def resize_image(image: Image.Image, max_size: Tuple[int, int] = (1024, 1024)) -> Image.Image:
    """
    Resize image while maintaining aspect ratio if it exceeds max_size.
    
    Args:
        image: PIL Image object
        max_size: Maximum (width, height)
        
    Returns:
        Resized PIL Image
    """
    image.thumbnail(max_size, Image.Resampling.LANCZOS)
    return image


def to_grayscale(image: Image.Image) -> Image.Image:
    """
    Convert image to grayscale.
    
    Args:
        image: PIL Image object
        
    Returns:
        Grayscale PIL Image
    """
    if image.mode != 'L':
        return image.convert('L')
    return image


def overlay_points(image: np.ndarray, points: list, color: Tuple[int, int, int] = (0, 255, 0), 
                   radius: int = 3) -> np.ndarray:
    """
    Overlay points on image.
    
    Args:
        image: NumPy array (H, W, C)
        points: List of (x, y) coordinates
        color: RGB color tuple
        radius: Point radius
        
    Returns:
        Image with overlaid points
    """
    result = image.copy()
    h, w = result.shape[:2]
    
    for x, y in points:
        x_int, y_int = int(round(x)), int(round(y))
        if 0 <= x_int < w and 0 <= y_int < h:
            # Draw circle manually
            for dy in range(-radius, radius + 1):
                for dx in range(-radius, radius + 1):
                    if dx*dx + dy*dy <= radius*radius:
                        nx, ny = x_int + dx, y_int + dy
                        if 0 <= nx < w and 0 <= ny < h:
                            result[ny, nx] = color
    
    return result


def overlay_lines(image: np.ndarray, lines: list, color: Tuple[int, int, int] = (0, 0, 255), 
                  thickness: int = 2) -> np.ndarray:
    """
    Overlay lines on image.
    
    Args:
        image: NumPy array (H, W, C)
        lines: List of (rho, theta) for Hough lines
        color: RGB color tuple
        thickness: Line thickness
        
    Returns:
        Image with overlaid lines
    """
    result = image.copy()
    h, w = result.shape[:2]
    
    for rho, theta in lines:
        a = np.cos(theta)
        b = np.sin(theta)
        x0 = a * rho
        y0 = b * rho
        
        # Find two points on the line
        x1 = int(x0 + 1000 * (-b))
        y1 = int(y0 + 1000 * (a))
        x2 = int(x0 - 1000 * (-b))
        y2 = int(y0 - 1000 * (a))
        
        # Draw line with thickness
        for t in range(thickness):
            offset = t - thickness // 2
            for i in range(100):
                alpha = i / 99
                x = int(x1 + alpha * (x2 - x1)) + offset
                y = int(y1 + alpha * (y2 - y1))
                if 0 <= x < w and 0 <= y < h:
                    result[y, x] = color
    
    return result


def overlay_circles(image: np.ndarray, circles: list, color: Tuple[int, int, int] = (0, 255, 0), 
                    thickness: int = 2) -> np.ndarray:
    """
    Overlay circles on image.
    
    Args:
        image: NumPy array (H, W, C)
        circles: List of (x, y, r)
        color: RGB color tuple
        thickness: Circle thickness
        
    Returns:
        Image with overlaid circles
    """
    result = image.copy()
    h, w = result.shape[:2]
    
    for x, y, r in circles:
        x_int, y_int, r_int = int(round(x)), int(round(y)), int(round(r))
        
        # Draw circle using parametric equation
        for t in range(360):
            rad = np.radians(t)
            cx = x_int + int(r_int * np.cos(rad))
            cy = y_int + int(r_int * np.sin(rad))
            
            if 0 <= cx < w and 0 <= cy < h:
                for dt in range(-thickness//2, thickness//2 + 1):
                    nx = cx + dt
                    ny = cy + dt
                    if 0 <= nx < w:
                        result[cy, nx] = color
                    if 0 <= ny < h:
                        result[ny, cx] = color
    
    return result


def overlay_ellipses(image: np.ndarray, ellipses: list, color: Tuple[int, int, int] = (255, 0, 0), 
                     thickness: int = 2) -> np.ndarray:
    """
    Overlay ellipses on image.
    
    Args:
        image: NumPy array (H, W, C)
        ellipses: List of ellipse parameters
        color: RGB color tuple
        thickness: Ellipse thickness
        
    Returns:
        Image with overlaid ellipses
    """
    result = image.copy()
    h, w = result.shape[:2]
    
    for params in ellipses:
        # Expecting (x, y, a, b, angle) where a and b are semi-axes
        if len(params) >= 5:
            x, y, a, b, angle = params[:5]
        else:
            continue
        
        # Generate ellipse points
        t = np.linspace(0, 2*np.pi, 100)
        cos_t = np.cos(t)
        sin_t = np.sin(t)
        
        # Rotated ellipse
        cos_angle = np.cos(angle)
        sin_angle = np.sin(angle)
        
        for i in range(100):
            xt = x + a * cos_t[i] * cos_angle - b * sin_t[i] * sin_angle
            yt = y + a * cos_t[i] * sin_angle + b * sin_t[i] * cos_angle
            
            x_int, y_int = int(round(xt)), int(round(yt))
            
            if 0 <= x_int < w and 0 <= y_int < h:
                result[y_int, x_int] = color
                
                # Add thickness
                for dt in range(-thickness//2, thickness//2 + 1):
                    if dt != 0:
                        nx = x_int + dt
                        ny = y_int + dt
                        if 0 <= nx < w:
                            result[y_int, nx] = color
                        if 0 <= ny < h:
                            result[ny, x_int] = color
    
    return result