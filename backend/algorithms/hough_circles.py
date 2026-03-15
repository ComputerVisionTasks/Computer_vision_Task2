"""
Hough Circle Transform implementation from scratch.
Based on: Duda, R.O. and Hart, P.E., "Use of the Hough transformation to detect lines and curves in pictures", 1972.
"""

import numpy as np
from typing import List, Tuple, Optional


def hough_circles(edge_image: np.ndarray, radius_min: int, radius_max: int,
                  threshold: float = 0.5, center_threshold: float = 0.3) -> List[Tuple[int, int, int]]:
    """
    Detect circles using Hough transform.
    
    Args:
        edge_image: Binary edge image (0 or 255)
        radius_min: Minimum circle radius
        radius_max: Maximum circle radius
        threshold: Threshold for circle detection (fraction of max votes)
        center_threshold: Threshold for center detection (fraction of max votes)
        
    Returns:
        List of (x, y, r) for detected circles
    """
    h, w = edge_image.shape
    
    # Find edge points
    edge_points = np.argwhere(edge_image > 0)
    if len(edge_points) == 0:
        return []
    
    # Initialize 3D accumulator: (x, y, r)
    # To save memory, we'll process each radius separately
    circles = []
    
    # Calculate gradient directions for better circle detection
    # This is a simplified version - full implementation would use gradient information
    
    # For each radius
    for r in range(radius_min, radius_max + 1):
        # Initialize 2D accumulator for centers
        accumulator = np.zeros((h, w), dtype=np.int64)
        
        # For each edge point, vote for possible centers
        for y, x in edge_points:
            # A circle with radius r passing through (x, y) has center at (x ± r, y ± r)
            # In a full implementation, we'd use gradient direction to limit votes
            
            # Simple voting: all points at distance r from edge point
            for dy in range(-r, r + 1):
                dx = int(np.sqrt(r*r - dy*dy)) if abs(dy) <= r else 0
                
                if dx > 0:
                    # Two possible centers
                    cx1, cy1 = x + dx, y + dy
                    cx2, cy2 = x - dx, y + dy
                    cx3, cy3 = x + dx, y - dy
                    cx4, cy4 = x - dx, y - dy
                    
                    for cx, cy in [(cx1, cy1), (cx2, cy2), (cx3, cy3), (cx4, cy4)]:
                        if 0 <= cx < w and 0 <= cy < h:
                            accumulator[cy, cx] += 1
        
        # Find peaks in accumulator
        max_votes = accumulator.max()
        if max_votes > 0:
            threshold_votes = center_threshold * max_votes
            centers = np.argwhere(accumulator >= threshold_votes)
            
            for cy, cx in centers:
                circles.append((cx, cy, r))
    
    # Sort circles by accumulator value (confidence) and remove duplicates
    # This is simplified - real implementation would use non-maximum suppression
    circles.sort(key=lambda c: accumulator[c[1], c[0]] if 0 <= c[1] < h and 0 <= c[0] < w else 0, 
                 reverse=True)
    
    # Simple duplicate removal: keep only circles with distinct centers and radii
    unique_circles = []
    tolerance = 5  # pixels
    
    for circle in circles:
        duplicate = False
        for uc in unique_circles:
            if (abs(circle[0] - uc[0]) < tolerance and 
                abs(circle[1] - uc[1]) < tolerance and 
                abs(circle[2] - uc[2]) < tolerance):
                duplicate = True
                break
        if not duplicate:
            unique_circles.append(circle)
    
    return unique_circles


def gradient_based_hough_circles(edge_image: np.ndarray, gradient_x: np.ndarray, 
                                 gradient_y: np.ndarray, radius_min: int, radius_max: int,
                                 threshold: float = 0.5) -> List[Tuple[int, int, int]]:
    """
    Improved circle detection using gradient information.
    
    Args:
        edge_image: Binary edge image
        gradient_x: X gradient of original image
        gradient_y: Y gradient of original image
        radius_min: Minimum radius
        radius_max: Maximum radius
        threshold: Detection threshold
        
    Returns:
        List of detected circles
    """
    h, w = edge_image.shape
    edge_points = np.argwhere(edge_image > 0)
    
    if len(edge_points) == 0:
        return []
    
    circles = []
    
    for r in range(radius_min, radius_max + 1):
        accumulator = np.zeros((h, w), dtype=np.int64)
        
        for y, x in edge_points:
            # Get gradient direction at edge point
            if 0 <= x < w and 0 <= y < h:
                gx = gradient_x[y, x]
                gy = gradient_y[y, x]
                
                # Normalize gradient
                mag = np.sqrt(gx*gx + gy*gy)
                if mag > 0:
                    gx /= mag
                    gy /= mag
                    
                    # Center lies along gradient direction
                    cx = int(round(x + r * gx))
                    cy = int(round(y + r * gy))
                    
                    if 0 <= cx < w and 0 <= cy < h:
                        accumulator[cy, cx] += 1
                    
                    # Also check opposite direction (circle could be inside-out)
                    cx = int(round(x - r * gx))
                    cy = int(round(y - r * gy))
                    
                    if 0 <= cx < w and 0 <= cy < h:
                        accumulator[cy, cx] += 1
        
        # Find peaks
        max_votes = accumulator.max()
        if max_votes > 0:
            threshold_votes = threshold * max_votes
            centers = np.argwhere(accumulator >= threshold_votes)
            
            for cy, cx in centers:
                circles.append((cx, cy, r))
    
    # Non-maximum suppression
    circles.sort(key=lambda c: accumulator[c[1], c[0]], reverse=True)
    
    final_circles = []
    for c in circles:
        # Check if this circle is too close to any already selected
        too_close = False
        for fc in final_circles:
            dist = np.sqrt((c[0]-fc[0])**2 + (c[1]-fc[1])**2)
            if dist < min(c[2], fc[2]) * 0.5:
                too_close = True
                break
        if not too_close:
            final_circles.append(c)
    
    return final_circles