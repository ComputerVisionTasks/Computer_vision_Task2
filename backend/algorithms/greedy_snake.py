"""
Greedy Snake (Active Contour) implementation.
Based on: Williams, D.J. and Shah, M., "A Fast Algorithm for Active Contours", CVGIP 1992.
"""

import numpy as np
from typing import List, Tuple, Optional


class GreedySnake:
    """
    Greedy algorithm for active contour evolution.
    
    This implementation uses:
    - Internal energy: continuity (elasticity) and curvature (smoothness)
    - External energy: image gradient (edge attraction)
    - Greedy optimization at each point
    """
    
    def __init__(self, image: np.ndarray, alpha: float = 0.5, beta: float = 0.5, 
                 gamma: float = 1.0, iterations: int = 100, 
                 window_size: int = 5, convergence_threshold: float = 0.1):
        """
        Initialize snake.
        
        Args:
            image: Grayscale image (0-255)
            alpha: Elasticity weight (continuity)
            beta: Smoothness weight (curvature)
            gamma: Step size (external energy weight)
            iterations: Maximum iterations
            window_size: Neighborhood size for point movement
            convergence_threshold: Minimum movement for convergence
        """
        self.alpha = alpha
        self.beta = beta
        self.gamma = gamma
        self.iterations = iterations
        self.window_size = window_size
        self.convergence_threshold = convergence_threshold
        
        # Precompute external energy (negative of gradient magnitude)
        self.compute_external_energy(image)
        
        self.points = None
        self.original_points = None
    
    def compute_external_energy(self, image: np.ndarray):
        """Compute external energy from image gradients."""
        # Simple gradient using Sobel
        from scipy.ndimage import sobel
        
        self.image = image.astype(np.float32)
        h, w = image.shape
        
        grad_x = sobel(image, axis=1)
        grad_y = sobel(image, axis=0)
        
        # Gradient magnitude
        grad_mag = np.sqrt(grad_x**2 + grad_y**2)
        
        # Normalize to [0, 1]
        if grad_mag.max() > 0:
            grad_mag = grad_mag / grad_mag.max()
        
        # External energy is negative gradient (snake attracted to edges)
        self.external_energy = -grad_mag
        
        # Pad for neighborhood search
        pad = self.window_size // 2
        self.padded_energy = np.pad(self.external_energy, pad, mode='edge')
    
    def initialize_contour(self, points: List[Tuple[int, int]]):
        """Initialize snake contour with points."""
        self.points = np.array(points, dtype=np.float32)
        self.original_points = self.points.copy()
    
    def compute_continuity_energy(self, points: np.ndarray, idx: int, 
                                  candidate: Tuple[int, int]) -> float:
        """
        Compute continuity energy (encourages evenly spaced points).
        
        This measures how well the candidate maintains average spacing.
        """
        n = len(points)
        prev_idx = (idx - 1) % n
        next_idx = (idx + 1) % n
        
        # Average distance between consecutive points
        distances = []
        for i in range(n):
            p1 = points[i]
            p2 = points[(i + 1) % n]
            distances.append(np.linalg.norm(p1 - p2))
        
        avg_dist = np.mean(distances)
        
        # Distance from candidate to neighbors
        d_prev = np.linalg.norm(candidate - points[prev_idx])
        d_next = np.linalg.norm(points[next_idx] - candidate)
        
        # Energy is deviation from average spacing
        continuity_energy = abs(avg_dist - d_prev) + abs(avg_dist - d_next)
        
        return continuity_energy
    
    def compute_curvature_energy(self, points: np.ndarray, idx: int,
                                 candidate: Tuple[int, int]) -> float:
        """
        Compute curvature energy (encourages smooth contour).
        
        Uses second derivative approximation.
        """
        n = len(points)
        prev_idx = (idx - 1) % n
        next_idx = (idx + 1) % n
        
        # Current and previous vectors
        v_prev = points[prev_idx] - np.array(candidate)
        v_next = points[next_idx] - np.array(candidate)
        
        # Normalize
        norm_prev = np.linalg.norm(v_prev)
        norm_next = np.linalg.norm(v_next)
        
        if norm_prev > 0 and norm_next > 0:
            v_prev = v_prev / norm_prev
            v_next = v_next / norm_next
        
        # Curvature energy is magnitude of difference between unit vectors
        curvature_energy = np.linalg.norm(v_prev - v_next)
        
        return curvature_energy
    
    def compute_external_energy_at(self, x: int, y: int) -> float:
        """Get external energy at a point."""
        pad = self.window_size // 2
        return self.padded_energy[y + pad, x + pad]
    
    def evolve(self) -> List[List[Tuple[int, int]]]:
        """
        Evolve the snake using greedy algorithm.
        
        Returns:
            List of contour positions at each iteration
        """
        if self.points is None:
            raise ValueError("Contour not initialized")
        
        history = [self.points.copy()]
        h, w = self.image.shape
        half_window = self.window_size // 2
        
        for iteration in range(self.iterations):
            max_movement = 0
            new_points = self.points.copy()
            
            # Process each point
            for i in range(len(self.points)):
                x, y = self.points[i]
                x_int, y_int = int(round(x)), int(round(y))
                
                best_energy = float('inf')
                best_pos = (x, y)
                
                # Search in neighborhood
                for dy in range(-half_window, half_window + 1):
                    for dx in range(-half_window, half_window + 1):
                        nx = x_int + dx
                        ny = y_int + dy
                        
                        # Check bounds
                        if 0 <= nx < w and 0 <= ny < h:
                            # Compute total energy
                            external = self.compute_external_energy_at(nx, ny)
                            
                            # For internal energies, use candidate position
                            candidate = np.array([nx, ny])
                            continuity = self.compute_continuity_energy(new_points, i, candidate)
                            curvature = self.compute_curvature_energy(new_points, i, candidate)
                            
                            total_energy = (self.alpha * continuity + 
                                           self.beta * curvature + 
                                           self.gamma * external)
                            
                            if total_energy < best_energy:
                                best_energy = total_energy
                                best_pos = (float(nx), float(ny))
                
                # Update point
                movement = np.linalg.norm(np.array(best_pos) - self.points[i])
                max_movement = max(max_movement, movement)
                new_points[i] = best_pos
            
            self.points = new_points
            history.append(self.points.copy())
            
            # Check convergence
            if max_movement < self.convergence_threshold:
                break
        
        return [p.tolist() for p in history]
    
    def get_contour(self) -> List[Tuple[int, int]]:
        """Get current contour as integer coordinates."""
        return [(int(round(x)), int(round(y))) for x, y in self.points]
    
    def reset(self):
        """Reset to original contour."""
        if self.original_points is not None:
            self.points = self.original_points.copy()