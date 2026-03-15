"""
Hough Line Transform implementation from scratch.
Based on: Hough, P.V.C., "Method and means for recognizing complex patterns", 1962.
"""

import numpy as np
import math
from typing import List, Tuple, Optional


def hough_lines(edge_image: np.ndarray, theta_res: float = 1.0, 
                rho_res: float = 1.0, threshold: int = 50) -> List[Tuple[float, float]]:
    """
    Detect straight lines using Hough transform.
    
    Args:
        edge_image: Binary edge image (0 or 255)
        theta_res: Angular resolution in degrees
        rho_res: Distance resolution in pixels
        threshold: Minimum votes to consider a line
        
    Returns:
        List of (rho, theta) for detected lines
    """
    h, w = edge_image.shape
    
    # Find edge points
    edge_points = np.argwhere(edge_image > 0)
    if len(edge_points) == 0:
        return []
    
    # Theta range: 0 to 180 degrees (in radians)
    theta_vals = np.deg2rad(np.arange(0, 180, theta_res))
    
    # Rho range: -diag to diag
    diag_len = int(np.ceil(np.sqrt(w*w + h*h)))
    rho_vals = np.arange(-diag_len, diag_len + 1, rho_res)
    
    # Initialize accumulator
    accumulator = np.zeros((len(rho_vals), len(theta_vals)), dtype=np.int64)
    
    # Vote for each edge point
    for y, x in edge_points:
        for theta_idx, theta in enumerate(theta_vals):
            # Compute rho = x*cos(theta) + y*sin(theta)
            rho = x * np.cos(theta) + y * np.sin(theta)
            
            # Find closest rho index
            rho_idx = np.argmin(np.abs(rho_vals - rho))
            accumulator[rho_idx, theta_idx] += 1
    
    # Find peaks in accumulator
    lines = []
    
    # Simple peak detection: find all points above threshold
    # More sophisticated peak detection could use non-maximum suppression
    peak_indices = np.argwhere(accumulator >= threshold)
    
    for rho_idx, theta_idx in peak_indices:
        rho = rho_vals[rho_idx]
        theta = theta_vals[theta_idx]
        lines.append((rho, theta))
    
    # Sort by accumulator value (confidence)
    lines.sort(key=lambda l: accumulator[
        np.argmin(np.abs(rho_vals - l[0])), 
        np.argmin(np.abs(theta_vals - l[1]))
    ], reverse=True)
    
    return lines


def probabilistic_hough_lines(edge_image: np.ndarray, threshold: int = 50, 
                             line_length: int = 10, line_gap: int = 5,
                             theta_res: float = 1.0, rho_res: float = 1.0) -> List[Tuple[float, float]]:
    """
    Probabilistic Hough Line Transform (simplified version).
    
    Args:
        edge_image: Binary edge image
        threshold: Minimum votes threshold
        line_length: Minimum line length
        line_gap: Maximum gap between line segments
        theta_res: Angular resolution
        rho_res: Distance resolution
        
    Returns:
        List of line segments (x1, y1, x2, y2)
    """
    # This is a simplified version - full implementation would be more complex
    lines = hough_lines(edge_image, theta_res, rho_res, threshold)
    
    # Convert to line segments (simplified)
    segments = []
    h, w = edge_image.shape
    
    for rho, theta in lines:
        # Get two points on the line
        a = np.cos(theta)
        b = np.sin(theta)
        x0 = a * rho
        y0 = b * rho
        
        # Find intersection with image boundaries
        points = []
        
        # Check intersections with image borders
        # Left border (x=0)
        if abs(a) > 1e-10:
            y_left = y0 - (x0 * b) / a if a != 0 else y0
            if 0 <= y_left <= h:
                points.append((0, y_left))
        
        # Right border (x=w)
        if abs(a) > 1e-10:
            y_right = y0 + ((w - x0) * b) / a if a != 0 else y0
            if 0 <= y_right <= h:
                points.append((w, y_right))
        
        # Top border (y=0)
        if abs(b) > 1e-10:
            x_top = x0 - (y0 * a) / b if b != 0 else x0
            if 0 <= x_top <= w:
                points.append((x_top, 0))
        
        # Bottom border (y=h)
        if abs(b) > 1e-10:
            x_bottom = x0 + ((h - y0) * a) / b if b != 0 else x0
            if 0 <= x_bottom <= w:
                points.append((x_bottom, h))
        
        # Add segment if we have at least 2 points
        if len(points) >= 2:
            # Take the two points that are farthest apart
            if len(points) > 2:
                dists = []
                for i in range(len(points)):
                    for j in range(i+1, len(points)):
                        p1, p2 = points[i], points[j]
                        dist = math.hypot(p2[0]-p1[0], p2[1]-p1[1])
                        dists.append((dist, i, j))
                if dists:
                    _, i, j = max(dists, key=lambda x: x[0])
                    p1, p2 = points[i], points[j]
                    segments.append((p1[0], p1[1], p2[0], p2[1]))
            else:
                p1, p2 = points[0], points[1]
                segments.append((p1[0], p1[1], p2[0], p2[1]))
    
    return segments