"""
Canny Edge Detector implementation from scratch.
Based on: Canny, J., "A Computational Approach to Edge Detection", IEEE PAMI 1986.
"""

import numpy as np
from scipy.ndimage import convolve, gaussian_filter
from typing import Tuple, Optional


def gaussian_kernel(size: int, sigma: float) -> np.ndarray:
    """
    Create a 2D Gaussian kernel.
    
    Args:
        size: Kernel size (should be odd)
        sigma: Standard deviation
        
    Returns:
        2D Gaussian kernel
    """
    kernel = np.zeros((size, size))
    center = size // 2
    
    for i in range(size):
        for j in range(size):
            x, y = i - center, j - center
            kernel[i, j] = np.exp(-(x*x + y*y) / (2*sigma*sigma))
    
    kernel /= (2 * np.pi * sigma * sigma)
    kernel /= kernel.sum()  # Normalize
    
    return kernel


def sobel_filters() -> Tuple[np.ndarray, np.ndarray]:
    """
    Create Sobel filters for gradient computation.
    
    Returns:
        Tuple of (Gx, Gy) Sobel kernels
    """
    Gx = np.array([[-1, 0, 1],
                   [-2, 0, 2],
                   [-1, 0, 1]], dtype=np.float32)
    
    Gy = np.array([[-1, -2, -1],
                   [0, 0, 0],
                   [1, 2, 1]], dtype=np.float32)
    
    return Gx, Gy


def non_maximum_suppression(gradient_magnitude: np.ndarray, 
                            gradient_direction: np.ndarray) -> np.ndarray:
    """
    Apply non-maximum suppression to thin edges.
    
    Args:
        gradient_magnitude: Gradient magnitude image
        gradient_direction: Gradient direction image (in radians)
        
    Returns:
        Thinned edge image
    """
    h, w = gradient_magnitude.shape
    suppressed = np.zeros((h, w), dtype=np.float32)
    
    # Quantize directions to 4 angles: 0, 45, 90, 135 degrees
    angle = gradient_direction * 180.0 / np.pi
    angle[angle < 0] += 180
    
    for i in range(1, h-1):
        for j in range(1, w-1):
            # Skip if magnitude is zero
            if gradient_magnitude[i, j] == 0:
                continue
            
            # Determine neighbors based on gradient direction
            if (0 <= angle[i, j] < 22.5) or (157.5 <= angle[i, j] <= 180):
                # Horizontal edge (0 degrees)
                neighbor1 = gradient_magnitude[i, j-1]
                neighbor2 = gradient_magnitude[i, j+1]
            elif 22.5 <= angle[i, j] < 67.5:
                # 45 degrees
                neighbor1 = gradient_magnitude[i-1, j+1]
                neighbor2 = gradient_magnitude[i+1, j-1]
            elif 67.5 <= angle[i, j] < 112.5:
                # Vertical edge (90 degrees)
                neighbor1 = gradient_magnitude[i-1, j]
                neighbor2 = gradient_magnitude[i+1, j]
            else:  # 112.5 to 157.5
                # 135 degrees
                neighbor1 = gradient_magnitude[i-1, j-1]
                neighbor2 = gradient_magnitude[i+1, j+1]
            
            # Keep if magnitude is greater than both neighbors
            if (gradient_magnitude[i, j] >= neighbor1 and 
                gradient_magnitude[i, j] >= neighbor2):
                suppressed[i, j] = gradient_magnitude[i, j]
    
    return suppressed


def double_threshold_hysteresis(image: np.ndarray, low_threshold: float, 
                               high_threshold: float, weak: int = 75, 
                               strong: int = 255) -> np.ndarray:
    """
    Apply double threshold and hysteresis for edge tracking.
    
    Args:
        image: Non-maximum suppressed image
        low_threshold: Low threshold for weak edges
        high_threshold: High threshold for strong edges
        weak: Value assigned to weak edges
        strong: Value assigned to strong edges
        
    Returns:
        Final edge map
    """
    h, w = image.shape
    result = np.zeros((h, w), dtype=np.uint8)
    
    # Identify strong and weak pixels
    strong_i, strong_j = np.where(image >= high_threshold)
    weak_i, weak_j = np.where((image >= low_threshold) & (image < high_threshold))
    
    result[strong_i, strong_j] = strong
    result[weak_i, weak_j] = weak
    
    # Hysteresis: weak edges connected to strong edges become strong
    for i in range(1, h-1):
        for j in range(1, w-1):
            if result[i, j] == weak:
                # Check 8-connected neighbors
                if (result[i-1:i+2, j-1:j+2] == strong).any():
                    result[i, j] = strong
                else:
                    result[i, j] = 0
    
    return result


def canny_edge_detector(image: np.ndarray, sigma: float = 1.0, 
                        low_threshold: float = 0.05, 
                        high_threshold: float = 0.15,
                        kernel_size: int = 5) -> np.ndarray:
    """
    Complete Canny edge detector pipeline.
    
    Args:
        image: Input grayscale image (0-255)
        sigma: Gaussian blur sigma
        low_threshold: Low threshold ratio (relative to max gradient)
        high_threshold: High threshold ratio (relative to max gradient)
        kernel_size: Gaussian kernel size
        
    Returns:
        Edge map (binary)
    """
    # Convert to float and normalize
    if image.dtype == np.uint8:
        img_float = image.astype(np.float32) / 255.0
    else:
        img_float = image.astype(np.float32)
        if img_float.max() > 1.0:
            img_float /= 255.0
    
    # 1. Gaussian smoothing
    kernel = gaussian_kernel(kernel_size, sigma)
    smoothed = convolve(img_float, kernel, mode='constant', cval=0.0)
    
    # 2. Gradient computation
    Gx, Gy = sobel_filters()
    grad_x = convolve(smoothed, Gx, mode='constant', cval=0.0)
    grad_y = convolve(smoothed, Gy, mode='constant', cval=0.0)
    
    # Magnitude and direction
    magnitude = np.sqrt(grad_x**2 + grad_y**2)
    magnitude = magnitude / magnitude.max()  # Normalize
    
    direction = np.arctan2(grad_y, grad_x)
    
    # 3. Non-maximum suppression
    suppressed = non_maximum_suppression(magnitude, direction)
    
    # 4. Double threshold and hysteresis
    # Convert thresholds from ratios to actual values
    max_val = suppressed.max()
    low = low_threshold * max_val
    high = high_threshold * max_val
    
    edges = double_threshold_hysteresis(suppressed, low, high)
    
    return edges