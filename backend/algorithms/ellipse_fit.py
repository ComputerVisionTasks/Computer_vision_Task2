"""
Least-squares ellipse fitting implementation.
Based on: Fitzgibbon, A.W., Pilu, M., and Fisher, R.B., "Direct Least Squares Fitting of Ellipses", IEEE PAMI 1999.
"""

import numpy as np
from typing import List, Tuple, Optional


def fit_ellipse(points: np.ndarray) -> Tuple[float, float, float, float, float]:
    """
    Fit an ellipse to a set of points using least squares.
    
    Args:
        points: Nx2 array of (x, y) points
        
    Returns:
        Ellipse parameters: (x_center, y_center, a, b, angle)
        where a and b are semi-major and semi-minor axes
    """
    if len(points) < 6:
        raise ValueError("At least 6 points required for ellipse fitting")
    
    # Build design matrix for algebraic distance
    # Ellipse equation: ax^2 + bxy + cy^2 + dx + ey + f = 0
    x = points[:, 0].reshape(-1, 1)
    y = points[:, 1].reshape(-1, 1)
    
    D = np.hstack([x*x, x*y, y*y, x, y, np.ones_like(x)])
    
    # Constraint matrix for ellipse (b^2 - 4ac < 0 -> 4ac - b^2 = 1)
    # Using Fitzgibbon's constraint: 4ac - b^2 = 1
    C = np.zeros((6, 6))
    C[0, 2] = C[2, 0] = 2
    C[1, 1] = -1
    
    # Solve generalized eigenvalue problem
    # D^T D a = λ C a
    S = D.T @ D
    
    # Solve using scatter matrix
    try:
        # Solve for eigenvalues and eigenvectors
        eigenvalues, eigenvectors = np.linalg.eig(np.linalg.inv(S) @ C)
        
        # Find the only positive eigenvalue
        pos_eigenvalues = np.where(eigenvalues > 0)[0]
        if len(pos_eigenvalues) > 0:
            # Take the eigenvector corresponding to the smallest positive eigenvalue
            idx = pos_eigenvalues[np.argmin(eigenvalues[pos_eigenvalues])]
            a_params = eigenvectors[:, idx].real
        else:
            # Fallback to least squares
            a_params = np.linalg.lstsq(D, np.ones(len(points)), rcond=None)[0]
    except:
        # Fallback to least squares
        a_params = np.linalg.lstsq(D, np.ones(len(points)), rcond=None)[0]
    
    # Normalize
    a_params = a_params / np.linalg.norm(a_params)
    
    # Extract ellipse parameters
    a, b, c, d, e, f = a_params
    
    # Convert to standard form
    # Center
    denom = 4*a*c - b*b
    if abs(denom) < 1e-10:
        # Not an ellipse, return circle approximation
        return fit_circle(points)
    
    x_center = (b*e - 2*c*d) / denom
    y_center = (b*d - 2*a*e) / denom
    
    # Semi-axes
    # Matrix form: [a, b/2; b/2, c]
    # Eigenvalues give axes lengths
    M = np.array([[a, b/2], [b/2, c]])
    eigenvalues, eigenvectors = np.linalg.eig(M)
    
    # Sort eigenvalues (largest first)
    idx = eigenvalues.argsort()[::-1]
    eigenvalues = eigenvalues[idx]
    eigenvectors = eigenvectors[:, idx]
    
    # Compute axes lengths
    # Formula: 1/λ where λ are eigenvalues of normalized matrix
    F = f + (d*x_center + e*y_center)
    if F > 0:
        eigenvalues = eigenvalues / F
    
    a_axis = np.sqrt(1.0 / abs(eigenvalues[0]))
    b_axis = np.sqrt(1.0 / abs(eigenvalues[1]))
    
    # Orientation angle
    angle = np.arctan2(eigenvectors[1, 0], eigenvectors[0, 0])
    
    return (x_center, y_center, a_axis, b_axis, angle)


def fit_circle(points: np.ndarray) -> Tuple[float, float, float, float, float]:
    """
    Fit a circle to points (fallback for ellipse fitting).
    
    Args:
        points: Nx2 array of points
        
    Returns:
        Circle parameters as (x_center, y_center, radius, radius, 0)
    """
    # Simple least squares circle fit
    x = points[:, 0]
    y = points[:, 1]
    
    # Linear least squares for circle: (x^2 + y^2) + Ax + By + C = 0
    A = np.column_stack([x, y, np.ones(len(x))])
    b = -(x*x + y*y)
    
    try:
        params, _, _, _ = np.linalg.lstsq(A, b, rcond=None)
        x_center = -params[0] / 2
        y_center = -params[1] / 2
        radius = np.sqrt(x_center*x_center + y_center*y_center - params[2])
        return (x_center, y_center, radius, radius, 0)
    except:
        # Return a default circle
        x_center = np.mean(x)
        y_center = np.mean(y)
        radius = np.mean(np.sqrt((x - x_center)**2 + (y - y_center)**2))
        return (x_center, y_center, radius, radius, 0)


def detect_ellipses(edge_image: np.ndarray, min_area: int = 100, 
                   max_area: int = 10000, tolerance: float = 0.1) -> List[Tuple]:
    """
    Detect ellipses in edge image by finding contours and fitting ellipses.
    
    Args:
        edge_image: Binary edge image
        min_area: Minimum contour area
        max_area: Maximum contour area
        tolerance: Fitting tolerance
        
    Returns:
        List of ellipse parameters
    """
    # Find connected components (contours)
    # Simple implementation: find all edge points and group them
    h, w = edge_image.shape
    visited = np.zeros((h, w), dtype=bool)
    ellipses = []
    
    edge_points = np.argwhere(edge_image > 0)
    
    # Simple BFS for connected components
    for start_y, start_x in edge_points:
        if visited[start_y, start_x]:
            continue
        
        # BFS to find connected component
        queue = [(start_y, start_x)]
        component = []
        
        while queue:
            y, x = queue.pop(0)
            if visited[y, x] or edge_image[y, x] == 0:
                continue
            
            visited[y, x] = True
            component.append((x, y))
            
            # Check 8-connected neighbors
            for dy in [-1, 0, 1]:
                for dx in [-1, 0, 1]:
                    ny, nx = y + dy, x + dx
                    if 0 <= ny < h and 0 <= nx < w and not visited[ny, nx] and edge_image[ny, nx] > 0:
                        queue.append((ny, nx))
        
        # Check component size
        if len(component) < 6:  # Need at least 6 points for ellipse
            continue
        
        # Convert to numpy array
        points = np.array(component)
        
        # Check bounding box area
        x_min, y_min = points.min(axis=0)
        x_max, y_max = points.max(axis=0)
        area = (x_max - x_min) * (y_max - y_min)
        
        if min_area <= area <= max_area:
            try:
                ellipse = fit_ellipse(points)
                ellipses.append(ellipse)
            except:
                continue
    
    return ellipses