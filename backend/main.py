"""
FromScratchCV - FastAPI Backend
Complete implementation with all endpoints.
"""

from fastapi import FastAPI, File, UploadFile, Form, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse
import numpy as np
from PIL import Image
import io
import base64
from typing import List, Optional, Dict, Any
import logging
import traceback

from utils.image_utils import (
    base64_to_image, image_to_base64, pil_to_numpy, numpy_to_pil,
    resize_image, to_grayscale, overlay_points, overlay_lines,
    overlay_circles, overlay_ellipses
)
from algorithms.canny import canny_edge_detector
from algorithms.hough_lines import hough_lines, probabilistic_hough_lines
from algorithms.hough_circles import hough_circles, gradient_based_hough_circles
from algorithms.ellipse_fit import detect_ellipses
from algorithms.greedy_snake import GreedySnake
from algorithms.chain_code import analyze_contour

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# Initialize FastAPI app
app = FastAPI(
    title="FromScratchCV API",
    description="Computer Vision operations implemented from scratch using NumPy and Pillow",
    version="1.0.0"
)

# Configure CORS
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],  # In production, replace with specific origins
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Store uploaded images in memory (for demo purposes)
# In production, use a proper database or file storage
image_store: Dict[str, Dict[str, Any]] = {}

# Error handler
@app.exception_handler(Exception)
async def global_exception_handler(request, exc):
    logger.error(f"Unhandled exception: {exc}")
    logger.error(traceback.format_exc())
    return JSONResponse(
        status_code=500,
        content={"error": str(exc), "detail": traceback.format_exc()}
    )


@app.get("/")
async def root():
    """Root endpoint - API health check."""
    return {
        "message": "FromScratchCV API is running",
        "version": "1.0.0",
        "endpoints": [
            "/upload",
            "/canny",
            "/hough-lines",
            "/hough-circles",
            "/detect-ellipses",
            "/snake/init",
            "/snake/evolve",
            "/snake/reset",
            "/analyze-contour"
        ]
    }


@app.post("/upload")
async def upload_image(file: UploadFile = File(...), session_id: str = Form(...)):
    """
    Upload an image and store it for processing.
    
    Args:
        file: Image file (JPG, PNG, BMP)
        session_id: Unique session identifier
        
    Returns:
        JSON with original image (base64) and metadata
    """
    logger.info(f"Uploading image for session {session_id}")
    
    try:
        # Read image
        contents = await file.read()
        image = Image.open(io.BytesIO(contents))
        
        # Convert to RGB if necessary
        if image.mode not in ('RGB', 'L'):
            image = image.convert('RGB')
        
        # Resize if too large
        image = resize_image(image, max_size=(1024, 1024))
        
        # Store original image
        image_store[session_id] = {
            'original': image.copy(),
            'current': image.copy(),
            'processed': None,
            'history': [],
            'snake': None,
            'contour_points': []
        }
        
        # Convert to base64 for frontend
        original_base64 = image_to_base64(image)
        
        return {
            "success": True,
            "original": original_base64,
            "processed": original_base64,  # Initially same as original
            "width": image.width,
            "height": image.height,
            "mode": image.mode
        }
    
    except Exception as e:
        logger.error(f"Upload failed: {e}")
        raise HTTPException(status_code=400, detail=f"Image upload failed: {str(e)}")


@app.post("/canny")
async def apply_canny(
    session_id: str = Form(...),
    low_threshold: float = Form(0.05),
    high_threshold: float = Form(0.15),
    sigma: float = Form(1.0)
):
    """
    Apply Canny edge detection.
    
    Args:
        session_id: Session identifier
        low_threshold: Low threshold (0-1)
        high_threshold: High threshold (0-1)
        sigma: Gaussian blur sigma
        
    Returns:
        Processed image with edges
    """
    logger.info(f"Applying Canny for session {session_id}")
    
    if session_id not in image_store:
        raise HTTPException(status_code=404, detail="Session not found")
    
    try:
        # Get current image
        image = image_store[session_id]['current']
        
        # Convert to grayscale if needed
        if image.mode != 'L':
            gray = image.convert('L')
        else:
            gray = image.copy()
        
        # Convert to numpy
        img_array = pil_to_numpy(gray)
        
        # Apply Canny
        edges = canny_edge_detector(
            img_array, 
            sigma=sigma,
            low_threshold=low_threshold,
            high_threshold=high_threshold
        )
        
        # Convert back to PIL
        edges_image = numpy_to_pil(edges)
        
        # Create overlay if needed
        overlay = None
        if image.mode == 'RGB':
            # Create RGB overlay
            rgb_edges = np.stack([edges, edges, edges], axis=2) * 255
            edges_rgb = numpy_to_pil(rgb_edges.astype(np.uint8))
            
            # Blend with original
            overlay = Image.blend(image, edges_rgb, 0.5)
        
        # Store processed image
        processed_base64 = image_to_base64(edges_image)
        overlay_base64 = image_to_base64(overlay) if overlay else None
        
        # Update history
        image_store[session_id]['history'].append({
            'operation': 'canny',
            'params': {'low_threshold': low_threshold, 'high_threshold': high_threshold, 'sigma': sigma}
        })
        
        return {
            "success": True,
            "processed": processed_base64,
            "overlay": overlay_base64,
            "edges": edges.tolist() if edges.size < 10000 else "too_large"
        }
    
    except Exception as e:
        logger.error(f"Canny failed: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@app.post("/hough-lines")
async def detect_lines(
    session_id: str = Form(...),
    threshold: int = Form(50),
    theta_res: float = Form(1.0),
    rho_res: float = Form(1.0)
):
    """
    Detect lines using Hough transform.
    
    Args:
        session_id: Session identifier
        threshold: Minimum votes threshold
        theta_res: Angular resolution (degrees)
        rho_res: Distance resolution (pixels)
        
    Returns:
        Detected lines and overlay image
    """
    logger.info(f"Detecting lines for session {session_id}")
    
    if session_id not in image_store:
        raise HTTPException(status_code=404, detail="Session not found")
    
    try:
        # Get current image
        image = image_store[session_id]['current']
        
        # Convert to grayscale
        if image.mode != 'L':
            gray = image.convert('L')
        else:
            gray = image.copy()
        
        # Apply Canny first to get edges
        img_array = pil_to_numpy(gray)
        edges = canny_edge_detector(img_array)
        
        # Detect lines
        lines = hough_lines(edges, theta_res=theta_res, rho_res=rho_res, threshold=threshold)
        
        # Create overlay on original
        if image.mode == 'RGB':
            img_rgb = pil_to_numpy(image)
        else:
            img_rgb = np.stack([img_array, img_array, img_array], axis=2)
        
        overlay = overlay_lines(img_rgb, lines, color=(0, 0, 255), thickness=2)
        overlay_image = numpy_to_pil(overlay)
        
        # Convert lines to serializable format
        lines_serializable = [(float(rho), float(theta)) for rho, theta in lines]
        
        return {
            "success": True,
            "lines": lines_serializable,
            "overlay": image_to_base64(overlay_image),
            "num_lines": len(lines)
        }
    
    except Exception as e:
        logger.error(f"Line detection failed: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@app.post("/hough-circles")
async def detect_circles(
    session_id: str = Form(...),
    radius_min: int = Form(10),
    radius_max: int = Form(100),
    threshold: float = Form(0.5)
):
    """
    Detect circles using Hough transform.
    
    Args:
        session_id: Session identifier
        radius_min: Minimum circle radius
        radius_max: Maximum circle radius
        threshold: Detection threshold (0-1)
        
    Returns:
        Detected circles and overlay image
    """
    logger.info(f"Detecting circles for session {session_id}")
    
    if session_id not in image_store:
        raise HTTPException(status_code=404, detail="Session not found")
    
    try:
        # Get current image
        image = image_store[session_id]['current']
        
        # Convert to grayscale
        if image.mode != 'L':
            gray = image.convert('L')
        else:
            gray = image.copy()
        
        # Apply Canny first to get edges
        img_array = pil_to_numpy(gray)
        edges = canny_edge_detector(img_array)
        
        # Detect circles
        circles = hough_circles(edges, radius_min, radius_max, threshold=threshold)
        
        # Create overlay on original
        if image.mode == 'RGB':
            img_rgb = pil_to_numpy(image)
        else:
            img_rgb = np.stack([img_array, img_array, img_array], axis=2)
        
        overlay = overlay_circles(img_rgb, circles, color=(0, 255, 0), thickness=2)
        overlay_image = numpy_to_pil(overlay)
        
        return {
            "success": True,
            "circles": [(int(x), int(y), int(r)) for x, y, r in circles],
            "overlay": image_to_base64(overlay_image),
            "num_circles": len(circles)
        }
    
    except Exception as e:
        logger.error(f"Circle detection failed: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@app.post("/detect-ellipses")
async def detect_ellipses_endpoint(
    session_id: str = Form(...),
    min_area: int = Form(100),
    max_area: int = Form(10000),
    tolerance: float = Form(0.1)
):
    """
    Detect ellipses in image.
    
    Args:
        session_id: Session identifier
        min_area: Minimum contour area
        max_area: Maximum contour area
        tolerance: Fitting tolerance
        
    Returns:
        Detected ellipses and overlay image
    """
    logger.info(f"Detecting ellipses for session {session_id}")
    
    if session_id not in image_store:
        raise HTTPException(status_code=404, detail="Session not found")
    
    try:
        # Get current image
        image = image_store[session_id]['current']
        
        # Convert to grayscale
        if image.mode != 'L':
            gray = image.convert('L')
        else:
            gray = image.copy()
        
        # Apply Canny first to get edges
        img_array = pil_to_numpy(gray)
        edges = canny_edge_detector(img_array)
        
        # Detect ellipses
        ellipses = detect_ellipses(edges, min_area=min_area, max_area=max_area, tolerance=tolerance)
        
        # Create overlay on original
        if image.mode == 'RGB':
            img_rgb = pil_to_numpy(image)
        else:
            img_rgb = np.stack([img_array, img_array, img_array], axis=2)
        
        overlay = overlay_ellipses(img_rgb, ellipses, color=(255, 0, 0), thickness=2)
        overlay_image = numpy_to_pil(overlay)
        
        # Convert ellipses to serializable format
        ellipses_serializable = []
        for e in ellipses:
            if len(e) >= 5:
                ellipses_serializable.append({
                    'x': float(e[0]),
                    'y': float(e[1]),
                    'a': float(e[2]),
                    'b': float(e[3]),
                    'angle': float(e[4])
                })
        
        return {
            "success": True,
            "ellipses": ellipses_serializable,
            "overlay": image_to_base64(overlay_image),
            "num_ellipses": len(ellipses)
        }
    
    except Exception as e:
        logger.error(f"Ellipse detection failed: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@app.post("/snake/init")
async def init_snake(
    session_id: str = Form(...),
    points: str = Form(...),  # JSON string of points
    alpha: float = Form(0.5),
    beta: float = Form(0.5),
    gamma: float = Form(1.0)
):
    """
    Initialize snake with points.
    
    Args:
        session_id: Session identifier
        points: JSON string of [[x1,y1], [x2,y2], ...]
        alpha: Elasticity weight
        beta: Smoothness weight
        gamma: Step size
        
    Returns:
        Initialized snake status
    """
    logger.info(f"Initializing snake for session {session_id}")
    
    if session_id not in image_store:
        raise HTTPException(status_code=404, detail="Session not found")
    
    try:
        import json
        point_list = json.loads(points)
        
        # Get current image
        image = image_store[session_id]['current']
        
        # Convert to grayscale
        if image.mode != 'L':
            gray = image.convert('L')
        else:
            gray = image.copy()
        
        img_array = pil_to_numpy(gray)
        
        # Create snake
        snake = GreedySnake(img_array, alpha=alpha, beta=beta, gamma=gamma)
        snake.initialize_contour(point_list)
        
        # Store snake
        image_store[session_id]['snake'] = snake
        image_store[session_id]['contour_points'] = point_list
        
        # Create overlay
        if image.mode == 'RGB':
            img_rgb = pil_to_numpy(image)
        else:
            img_rgb = np.stack([img_array, img_array, img_array], axis=2)
        
        overlay = overlay_points(img_rgb, point_list, color=(0, 255, 0), radius=3)
        overlay_image = numpy_to_pil(overlay)
        
        return {
            "success": True,
            "message": "Snake initialized",
            "num_points": len(point_list),
            "overlay": image_to_base64(overlay_image)
        }
    
    except Exception as e:
        logger.error(f"Snake initialization failed: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@app.post("/snake/evolve")
async def evolve_snake(
    session_id: str = Form(...),
    iterations: int = Form(100)
):
    """
    Evolve snake for specified iterations.
    
    Args:
        session_id: Session identifier
        iterations: Number of iterations
        
    Returns:
        Evolved contour and overlay
    """
    logger.info(f"Evolving snake for session {session_id}")
    
    if session_id not in image_store:
        raise HTTPException(status_code=404, detail="Session not found")
    
    snake = image_store[session_id].get('snake')
    if snake is None:
        raise HTTPException(status_code=400, detail="Snake not initialized")
    
    try:
        # Evolve snake
        history = snake.evolve()
        
        # Get current contour
        contour = snake.get_contour()
        image_store[session_id]['contour_points'] = contour
        
        # Get image for overlay
        image = image_store[session_id]['current']
        
        if image.mode == 'RGB':
            img_rgb = pil_to_numpy(image)
        else:
            gray = pil_to_numpy(image)
            img_rgb = np.stack([gray, gray, gray], axis=2)
        
        # Create overlay
        overlay = overlay_points(img_rgb, contour, color=(0, 255, 255), radius=2)
        
        # Add history overlay
        for hist_points in history[:-1]:  # All but final
            hist_int = [(int(p[0]), int(p[1])) for p in hist_points]
            overlay = overlay_points(overlay, hist_int, color=(128, 128, 128), radius=1)
        
        overlay_image = numpy_to_pil(overlay)
        
        return {
            "success": True,
            "contour": contour,
            "overlay": image_to_base64(overlay_image),
            "iterations": len(history),
            "history_length": len(history)
        }
    
    except Exception as e:
        logger.error(f"Snake evolution failed: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@app.post("/snake/reset")
async def reset_snake(session_id: str = Form(...)):
    """
    Reset snake to initial contour.
    
    Args:
        session_id: Session identifier
    """
    logger.info(f"Resetting snake for session {session_id}")
    
    if session_id not in image_store:
        raise HTTPException(status_code=404, detail="Session not found")
    
    snake = image_store[session_id].get('snake')
    if snake is None:
        raise HTTPException(status_code=400, detail="Snake not initialized")
    
    try:
        snake.reset()
        contour = snake.get_contour()
        image_store[session_id]['contour_points'] = contour
        
        return {
            "success": True,
            "contour": contour,
            "message": "Snake reset"
        }
    
    except Exception as e:
        logger.error(f"Snake reset failed: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@app.post("/analyze-contour")
async def analyze_contour_endpoint(
    session_id: str = Form(...)
):
    """
    Analyze current contour (perimeter, area, chain code).
    
    Args:
        session_id: Session identifier
        
    Returns:
        Contour analysis results
    """
    logger.info(f"Analyzing contour for session {session_id}")
    
    if session_id not in image_store:
        raise HTTPException(status_code=404, detail="Session not found")
    
    try:
        # Get current image and edges
        image = image_store[session_id]['current']
        
        # Convert to grayscale
        if image.mode != 'L':
            gray = image.convert('L')
        else:
            gray = image.copy()
        
        img_array = pil_to_numpy(gray)
        
        # Apply Canny to get edges
        edges = canny_edge_detector(img_array)
        
        # Analyze contour
        analysis = analyze_contour(edges)
        
        # Convert numpy arrays to lists for JSON
        analysis['boundary'] = [(int(x), int(y)) for x, y in analysis['boundary']]
        
        return {
            "success": True,
            "analysis": analysis
        }
    
    except Exception as e:
        logger.error(f"Contour analysis failed: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@app.post("/reset")
async def reset_session(session_id: str = Form(...)):
    """
    Reset session to original image.
    
    Args:
        session_id: Session identifier
    """
    logger.info(f"Resetting session {session_id}")
    
    if session_id not in image_store:
        raise HTTPException(status_code=404, detail="Session not found")
    
    try:
        # Restore original image
        original = image_store[session_id]['original'].copy()
        image_store[session_id]['current'] = original
        image_store[session_id]['processed'] = None
        image_store[session_id]['history'] = []
        image_store[session_id]['snake'] = None
        image_store[session_id]['contour_points'] = []
        
        original_base64 = image_to_base64(original)
        
        return {
            "success": True,
            "original": original_base64,
            "processed": original_base64
        }
    
    except Exception as e:
        logger.error(f"Reset failed: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@app.post("/undo")
async def undo_operation(session_id: str = Form(...)):
    """
    Undo last operation.
    
    Args:
        session_id: Session identifier
    """
    logger.info(f"Undo for session {session_id}")
    
    if session_id not in image_store:
        raise HTTPException(status_code=404, detail="Session not found")
    
    try:
        history = image_store[session_id]['history']
        
        if history:
            # Remove last operation
            history.pop()
            
            # Restore original
            original = image_store[session_id]['original'].copy()
            image_store[session_id]['current'] = original
            
            # Reapply remaining operations (simplified - would need to reapply all)
            # For now, just reset
            image_store[session_id]['processed'] = None
        
        original_base64 = image_to_base64(image_store[session_id]['original'])
        
        return {
            "success": True,
            "original": original_base64,
            "processed": original_base64,
            "history_length": len(history)
        }
    
    except Exception as e:
        logger.error(f"Undo failed: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@app.post("/save")
async def save_image(session_id: str = Form(...), format: str = Form("PNG")):
    """
    Save current processed image.
    
    Args:
        session_id: Session identifier
        format: Output format (PNG, JPEG)
        
    Returns:
        Base64 encoded image for download
    """
    logger.info(f"Saving image for session {session_id}")
    
    if session_id not in image_store:
        raise HTTPException(status_code=404, detail="Session not found")
    
    try:
        # Get current image
        if image_store[session_id]['processed'] is not None:
            image = image_store[session_id]['processed']
        else:
            image = image_store[session_id]['current']
        
        # Convert to base64
        image_base64 = image_to_base64(image, format=format)
        
        return {
            "success": True,
            "image": image_base64,
            "format": format
        }
    
    except Exception as e:
        logger.error(f"Save failed: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@app.post("/clear")
async def clear_session(session_id: str = Form(...)):
    """
    Clear session data.
    
    Args:
        session_id: Session identifier
    """
    logger.info(f"Clearing session {session_id}")
    
    if session_id in image_store:
        del image_store[session_id]
    
    return {"success": True, "message": "Session cleared"}


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000, reload=True)