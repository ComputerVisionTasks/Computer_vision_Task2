"""
Freeman 8-direction chain code implementation.
Based on: Freeman, H., "On the encoding of arbitrary geometric configurations", IRE Trans. 1961.
"""

import numpy as np
from typing import List, Tuple, Optional


# Direction codes for 8-connected chain
# 3 2 1
# 4   0
# 5 6 7
DIRECTION_CODES = {
    (0, 1): 0,   # East
    (1, 1): 1,   # Southeast
    (1, 0): 2,   # South
    (1, -1): 3,  # Southwest
    (0, -1): 4,  # West
    (-1, -1): 5, # Northwest
    (-1, 0): 6,  # North
    (-1, 1): 7   # Northeast
}

# Reverse mapping
DIRECTION_VECTORS = {v: k for k, v in DIRECTION_CODES.items()}


def trace_boundary(binary_image: np.ndarray, start_point: Optional[Tuple[int, int]] = None) -> List[Tuple[int, int]]:
    """
    Trace the boundary of a connected component.
    
    Args:
        binary_image: Binary image (0 or 255)
        start_point: Starting point for tracing (optional)
        
    Returns:
        List of boundary points in order
    """
    h, w = binary_image.shape
    
    # Find start point if not provided
    if start_point is None:
        points = np.argwhere(binary_image > 0)
        if len(points) == 0:
            return []
        start_point = (points[0, 1], points[0, 0])  # (x, y)
    
    boundary = []
    current = start_point
    prev_direction = 7  # Start with North-West search
    
    # 8-direction search order (clockwise)
    search_order = [0, 1, 2, 3, 4, 5, 6, 7]
    
    while True:
        boundary.append(current)
        
        # Search for next boundary point
        found_next = False
        
        # Determine search order based on previous direction
        # For clockwise tracing, we start from the direction perpendicular to previous
        start_idx = (prev_direction + 5) % 8  # 90 degrees clockwise from prev
        
        for i in range(8):
            direction = (start_idx + i) % 8
            dx, dy = DIRECTION_VECTORS[direction]
            
            nx, ny = current[0] + dx, current[1] + dy
            
            # Check bounds
            if 0 <= nx < w and 0 <= ny < h:
                if binary_image[ny, nx] > 0:
                    # Found next boundary point
                    current = (nx, ny)
                    prev_direction = direction
                    found_next = True
                    break
        
        if not found_next:
            # No neighbors found - single point
            break
        
        # Check if we've completed the loop
        if len(boundary) > 3 and abs(current[0] - start_point[0]) < 2 and abs(current[1] - start_point[1]) < 2:
            # Close enough to start
            break
    
    return boundary


def compute_chain_code(boundary: List[Tuple[int, int]]) -> List[int]:
    """
    Compute Freeman chain code from boundary points.
    
    Args:
        boundary: List of (x, y) points in order
        
    Returns:
        List of direction codes
    """
    if len(boundary) < 2:
        return []
    
    chain_code = []
    
    for i in range(len(boundary) - 1):
        p1 = boundary[i]
        p2 = boundary[i + 1]
        
        dx = p2[0] - p1[0]
        dy = p2[1] - p1[1]
        
        # Find closest direction
        if dx == 0 and dy == 1:
            code = 0  # East
        elif dx == 1 and dy == 1:
            code = 1  # Southeast
        elif dx == 1 and dy == 0:
            code = 2  # South
        elif dx == 1 and dy == -1:
            code = 3  # Southwest
        elif dx == 0 and dy == -1:
            code = 4  # West
        elif dx == -1 and dy == -1:
            code = 5  # Northwest
        elif dx == -1 and dy == 0:
            code = 6  # North
        elif dx == -1 and dy == 1:
            code = 7  # Northeast
        else:
            # Not a valid 8-direction step, interpolate
            # Find closest valid direction
            best_dist = float('inf')
            best_code = 0
            
            for code, (cdx, cdy) in DIRECTION_VECTORS.items():
                dist = (dx - cdx)**2 + (dy - cdy)**2
                if dist < best_dist:
                    best_dist = dist
                    best_code = code
            
            code = best_code
        
        chain_code.append(code)
    
    return chain_code


def compute_perimeter(chain_code: List[int]) -> float:
    """
    Compute perimeter from chain code.
    
    Even steps (0,2,4,6) have length 1
    Odd steps (1,3,5,7) have length √2
    
    Args:
        chain_code: List of direction codes
        
    Returns:
        Perimeter length
    """
    perimeter = 0.0
    
    for code in chain_code:
        if code % 2 == 0:  # Even: horizontal/vertical
            perimeter += 1.0
        else:  # Odd: diagonal
            perimeter += np.sqrt(2)
    
    return perimeter


def compute_area(boundary: List[Tuple[int, int]]) -> float:
    """
    Compute area using shoelace formula.
    
    Args:
        boundary: List of (x, y) points in order
        
    Returns:
        Area
    """
    if len(boundary) < 3:
        return 0.0
    
    # Shoelace formula
    area = 0.0
    n = len(boundary)
    
    for i in range(n):
        x1, y1 = boundary[i]
        x2, y2 = boundary[(i + 1) % n]
        area += x1 * y2 - x2 * y1
    
    return abs(area) / 2.0


def analyze_contour(binary_image: np.ndarray, 
                   start_point: Optional[Tuple[int, int]] = None) -> dict:
    """
    Complete contour analysis.
    
    Args:
        binary_image: Binary image with a single connected component
        start_point: Optional starting point
        
    Returns:
        Dictionary with:
        - boundary: list of points
        - chain_code: list of directions
        - perimeter: float
        - area: float
        - num_points: int
        - is_closed: bool
    """
    # Trace boundary
    boundary = trace_boundary(binary_image, start_point)
    
    if not boundary:
        return {
            'boundary': [],
            'chain_code': [],
            'perimeter': 0.0,
            'area': 0.0,
            'num_points': 0,
            'is_closed': False
        }
    
    # Compute chain code
    chain_code = compute_chain_code(boundary)
    
    # Check if contour is closed
    is_closed = (abs(boundary[0][0] - boundary[-1][0]) < 2 and 
                 abs(boundary[0][1] - boundary[-1][1]) < 2)
    
    # Compute perimeter
    perimeter = compute_perimeter(chain_code)
    
    # Compute area
    area = compute_area(boundary)
    
    return {
        'boundary': boundary,
        'chain_code': chain_code,
        'perimeter': perimeter,
        'area': area,
        'num_points': len(boundary),
        'is_closed': is_closed
    }


def chain_code_to_string(chain_code: List[int]) -> str:
    """Convert chain code list to string."""
    return ''.join(str(code) for code in chain_code)


def string_to_chain_code(code_string: str) -> List[int]:
    """Convert string to chain code list."""
    return [int(c) for c in code_string if c.isdigit()]