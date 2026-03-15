/**
 * FromScratchCV - Operations Page JavaScript
 * Handles image upload, processing, and API communication
 * All components always visible version
 */

// Global state
let currentState = {
    sessionId: generateSessionId(),
    originalImage: null,
    processedImage: null,
    overlayImage: null,
    contourPoints: [],
    snakeInitialized: false,
    history: []
};

// API Base URL - Update this to your backend URL
const API_BASE_URL = 'http://localhost:8000';

// Toast container for notifications
let toastContainer = null;

// Initialize on page load
document.addEventListener('DOMContentLoaded', () => {
    console.log('Operation page initialized');
    initializeEventListeners();
    initializeAOS();
    createToastContainer();
    disableAllOperationButtons(true);
});

// Create toast container
function createToastContainer() {
    toastContainer = document.createElement('div');
    toastContainer.className = 'toast-container';
    document.body.appendChild(toastContainer);
}

// Generate unique session ID
function generateSessionId() {
    return 'session_' + Date.now() + '_' + Math.random().toString(36).substr(2, 9);
}

// Initialize AOS
function initializeAOS() {
    if (typeof AOS !== 'undefined') {
        AOS.init({
            duration: 600,
            easing: 'ease-in-out',
            once: true,
            mirror: false
        });
    }
}

// Initialize event listeners
function initializeEventListeners() {
    console.log('Initializing event listeners');
    
    // Upload area
    const uploadArea = document.getElementById('uploadArea');
    const fileInput = document.getElementById('fileInput');
    const browseBtn = document.getElementById('browseBtn');

    if (uploadArea) {
        uploadArea.addEventListener('click', () => fileInput.click());
        uploadArea.addEventListener('dragover', handleDragOver);
        uploadArea.addEventListener('dragleave', handleDragLeave);
        uploadArea.addEventListener('drop', handleDrop);
        console.log('Upload area listeners attached');
    }
    
    if (browseBtn) {
        browseBtn.addEventListener('click', (e) => {
            e.stopPropagation();
            fileInput.click();
        });
    }
    
    if (fileInput) {
        fileInput.addEventListener('change', handleFileSelect);
    }

    // Image actions
    const undoBtn = document.getElementById('undoBtn');
    const resetBtn = document.getElementById('resetBtn');
    const saveBtn = document.getElementById('saveBtn');
    
    if (undoBtn) undoBtn.addEventListener('click', undoOperation);
    if (resetBtn) resetBtn.addEventListener('click', resetImage);
    if (saveBtn) saveBtn.addEventListener('click', saveImage);

    // Edge detection
    const lowThreshold = document.getElementById('lowThreshold');
    const highThreshold = document.getElementById('highThreshold');
    const sigma = document.getElementById('sigma');
    const runCanny = document.getElementById('runCanny');
    
    if (lowThreshold) lowThreshold.addEventListener('input', updateRangeDisplay);
    if (highThreshold) highThreshold.addEventListener('input', updateRangeDisplay);
    if (sigma) sigma.addEventListener('input', updateRangeDisplay);
    if (runCanny) runCanny.addEventListener('click', runCannyEdge);

    // Shape detection
    const lineThreshold = document.getElementById('lineThreshold');
    const ellipseTolerance = document.getElementById('ellipseTolerance');
    const detectShapes = document.getElementById('detectShapes');
    
    if (lineThreshold) lineThreshold.addEventListener('input', updateRangeDisplay);
    if (ellipseTolerance) ellipseTolerance.addEventListener('input', updateRangeDisplay);
    if (detectShapes) detectShapes.addEventListener('click', detectShapes);

    // Snake
    const alpha = document.getElementById('alpha');
    const beta = document.getElementById('beta');
    const gamma = document.getElementById('gamma');
    const initSnake = document.getElementById('initSnake');
    const runSnake = document.getElementById('runSnake');
    const clearPoints = document.getElementById('clearPoints');
    
    if (alpha) alpha.addEventListener('input', updateRangeDisplay);
    if (beta) beta.addEventListener('input', updateRangeDisplay);
    if (gamma) gamma.addEventListener('input', updateRangeDisplay);
    if (initSnake) initSnake.addEventListener('click', initializeSnake);
    if (runSnake) runSnake.addEventListener('click', runSnake);
    if (clearPoints) clearPoints.addEventListener('click', clearSnakePoints);

    // Analysis
    const analyzeContour = document.getElementById('analyzeContour');
    const exportChainCode = document.getElementById('exportChainCode');
    
    if (analyzeContour) analyzeContour.addEventListener('click', analyzeContour);
    if (exportChainCode) exportChainCode.addEventListener('click', exportResults);

    // Original image click for snake points
    const originalPreview = document.getElementById('originalPreview');
    if (originalPreview) {
        originalPreview.addEventListener('click', handleImageClick);
    }

    console.log('Event listeners initialized');
}

// Disable/enable all operation buttons
function disableAllOperationButtons(disabled) {
    // Operation buttons
    const operationButtons = [
        'runCanny',
        'detectShapes',
        'initSnake',
        'runSnake',
        'clearPoints',
        'analyzeContour',
        'exportChainCode'
    ];
    
    operationButtons.forEach(id => {
        const btn = document.getElementById(id);
        if (btn) btn.disabled = disabled;
    });
    
    // Action buttons
    const actionButtons = ['undoBtn', 'resetBtn', 'saveBtn'];
    actionButtons.forEach(id => {
        const btn = document.getElementById(id);
        if (btn) btn.disabled = disabled;
    });
}

// Handle drag over
function handleDragOver(e) {
    e.preventDefault();
    e.stopPropagation();
    const uploadArea = document.getElementById('uploadArea');
    if (uploadArea) {
        uploadArea.classList.add('dragover');
    }
}

// Handle drag leave
function handleDragLeave(e) {
    e.preventDefault();
    e.stopPropagation();
    const uploadArea = document.getElementById('uploadArea');
    if (uploadArea) {
        uploadArea.classList.remove('dragover');
    }
}

// Handle drop
function handleDrop(e) {
    e.preventDefault();
    e.stopPropagation();
    const uploadArea = document.getElementById('uploadArea');
    if (uploadArea) {
        uploadArea.classList.remove('dragover');
    }
    
    const files = e.dataTransfer.files;
    if (files.length > 0) {
        handleFile(files[0]);
    }
}

// Handle file select
function handleFileSelect(e) {
    const file = e.target.files[0];
    if (file) {
        handleFile(file);
    }
}

// Handle file upload
async function handleFile(file) {
    // Validate file type
    if (!file.type.match('image.*')) {
        showToast('Please select an image file (JPG, PNG, BMP)', 'error');
        return;
    }

    showLoading('Uploading image...');

    const formData = new FormData();
    formData.append('file', file);
    formData.append('session_id', currentState.sessionId);

    try {
        console.log('Uploading file:', file.name);
        
        const response = await fetch(`${API_BASE_URL}/upload`, {
            method: 'POST',
            body: formData
        });

        if (!response.ok) {
            const errorData = await response.json();
            throw new Error(errorData.detail || 'Upload failed');
        }

        const data = await response.json();
        console.log('Upload response:', data);

        // Update state
        currentState.originalImage = data.original;
        currentState.processedImage = data.processed;

        // Update UI - hide placeholders and show images
        const originalPreview = document.getElementById('originalPreview');
        const processedPreview = document.getElementById('processedPreview');
        const originalPlaceholder = document.getElementById('originalPlaceholder');
        const processedPlaceholder = document.getElementById('processedPlaceholder');
        
        if (originalPreview && originalPlaceholder) {
            originalPreview.src = data.original;
            originalPreview.style.display = 'block';
            originalPlaceholder.style.display = 'none';
        }
        
        if (processedPreview && processedPlaceholder) {
            processedPreview.src = data.processed;
            processedPreview.style.display = 'block';
            processedPlaceholder.style.display = 'none';
        }

        // Enable all operation buttons
        disableAllOperationButtons(false);

        showToast('Image uploaded successfully', 'success');
    } catch (error) {
        console.error('Upload error:', error);
        showToast('Failed to upload image: ' + error.message, 'error');
    } finally {
        hideLoading();
    }
}

// Update range input displays
function updateRangeDisplay(e) {
    const id = e.target.id;
    const value = e.target.value;
    const displayId = id + 'Value';
    const display = document.getElementById(displayId);
    if (display) {
        display.textContent = value;
    }
}

// Run Canny edge detection
async function runCannyEdge() {
    if (!currentState.originalImage) {
        showToast('Please upload an image first', 'error');
        return;
    }

    showLoading('Running Canny edge detection...');

    const formData = new FormData();
    formData.append('session_id', currentState.sessionId);
    formData.append('low_threshold', document.getElementById('lowThreshold').value);
    formData.append('high_threshold', document.getElementById('highThreshold').value);
    formData.append('sigma', document.getElementById('sigma').value);

    try {
        console.log('Running Canny with params:', {
            low: document.getElementById('lowThreshold').value,
            high: document.getElementById('highThreshold').value,
            sigma: document.getElementById('sigma').value
        });

        const response = await fetch(`${API_BASE_URL}/canny`, {
            method: 'POST',
            body: formData
        });

        if (!response.ok) {
            const errorData = await response.json();
            throw new Error(errorData.detail || 'Canny edge detection failed');
        }

        const data = await response.json();
        console.log('Canny response:', data);

        // Update processed image
        const showOverlay = document.getElementById('showEdgeOverlay').checked;
        currentState.processedImage = showOverlay && data.overlay ? data.overlay : data.processed;
        
        const processedPreview = document.getElementById('processedPreview');
        if (processedPreview) {
            processedPreview.src = currentState.processedImage;
        }

        // Add to history
        currentState.history.push('canny');

        showToast('Edge detection completed', 'success');
    } catch (error) {
        console.error('Canny error:', error);
        showToast('Failed to run edge detection: ' + error.message, 'error');
    } finally {
        hideLoading();
    }
}

// Detect shapes
async function detectShapes() {
    if (!currentState.originalImage) {
        showToast('Please upload an image first', 'error');
        return;
    }

    showLoading('Detecting shapes...');

    const detectLines = document.getElementById('detectLines').checked;
    const detectCircles = document.getElementById('detectCircles').checked;
    const detectEllipses = document.getElementById('detectEllipses').checked;

    if (!detectLines && !detectCircles && !detectEllipses) {
        showToast('Please select at least one shape type to detect', 'error');
        hideLoading();
        return;
    }

    try {
        let overlayImage = null;
        const results = {};

        // Detect Lines
        if (detectLines) {
            const lineFormData = new FormData();
            lineFormData.append('session_id', currentState.sessionId);
            lineFormData.append('threshold', document.getElementById('lineThreshold').value);

            console.log('Detecting lines...');
            const lineResponse = await fetch(`${API_BASE_URL}/hough-lines`, {
                method: 'POST',
                body: lineFormData
            });

            if (lineResponse.ok) {
                const lineData = await lineResponse.json();
                overlayImage = lineData.overlay;
                results.lines = lineData.num_lines;
                console.log(`Found ${lineData.num_lines} lines`);
            }
        }

        // Detect Circles
        if (detectCircles) {
            const circleFormData = new FormData();
            circleFormData.append('session_id', currentState.sessionId);
            circleFormData.append('radius_min', document.getElementById('minRadius').value);
            circleFormData.append('radius_max', document.getElementById('maxRadius').value);

            console.log('Detecting circles...');
            const circleResponse = await fetch(`${API_BASE_URL}/hough-circles`, {
                method: 'POST',
                body: circleFormData
            });

            if (circleResponse.ok) {
                const circleData = await circleResponse.json();
                overlayImage = circleData.overlay;
                results.circles = circleData.num_circles;
                console.log(`Found ${circleData.num_circles} circles`);
            }
        }

        // Detect Ellipses
        if (detectEllipses) {
            const ellipseFormData = new FormData();
            ellipseFormData.append('session_id', currentState.sessionId);
            ellipseFormData.append('tolerance', document.getElementById('ellipseTolerance').value);
            ellipseFormData.append('min_area', 100);
            ellipseFormData.append('max_area', 10000);

            console.log('Detecting ellipses...');
            const ellipseResponse = await fetch(`${API_BASE_URL}/detect-ellipses`, {
                method: 'POST',
                body: ellipseFormData
            });

            if (ellipseResponse.ok) {
                const ellipseData = await ellipseResponse.json();
                overlayImage = ellipseData.overlay;
                results.ellipses = ellipseData.num_ellipses;
                console.log(`Found ${ellipseData.num_ellipses} ellipses`);
            }
        }

        // Update image
        if (overlayImage) {
            currentState.processedImage = overlayImage;
            const processedPreview = document.getElementById('processedPreview');
            if (processedPreview) {
                processedPreview.src = overlayImage;
            }
        }

        // Show results message
        let message = 'Detection completed: ';
        const parts = [];
        if (results.lines) parts.push(`${results.lines} lines`);
        if (results.circles) parts.push(`${results.circles} circles`);
        if (results.ellipses) parts.push(`${results.ellipses} ellipses`);
        message += parts.join(', ');
        
        showToast(message, 'success');

    } catch (error) {
        console.error('Shape detection error:', error);
        showToast('Failed to detect shapes: ' + error.message, 'error');
    } finally {
        hideLoading();
    }
}

// Handle image click for snake points
function handleImageClick(e) {
    if (!currentState.originalImage) {
        showToast('Please upload an image first', 'error');
        return;
    }

    const img = e.target;
    const rect = img.getBoundingClientRect();
    
    // Calculate click coordinates relative to image
    const scaleX = img.naturalWidth / rect.width;
    const scaleY = img.naturalHeight / rect.height;
    
    const x = Math.round((e.clientX - rect.left) * scaleX);
    const y = Math.round((e.clientY - rect.top) * scaleY);

    // Ensure coordinates are within image bounds
    if (x >= 0 && x < img.naturalWidth && y >= 0 && y < img.naturalHeight) {
        // Add point
        currentState.contourPoints.push([x, y]);

        // Draw point on image
        drawContourPoints();

        // Update point count
        const pointCount = document.getElementById('pointCount');
        if (pointCount) {
            pointCount.textContent = `${currentState.contourPoints.length} points`;
        }

        showToast(`Point added (${currentState.contourPoints.length} total)`, 'info');
    }
}

// Draw contour points on image
function drawContourPoints() {
    if (!currentState.originalImage || currentState.contourPoints.length === 0) return;

    const img = new Image();
    img.onload = function() {
        const canvas = document.createElement('canvas');
        canvas.width = img.width;
        canvas.height = img.height;
        const ctx = canvas.getContext('2d');
        
        ctx.drawImage(img, 0, 0);
        
        // Draw points
        ctx.fillStyle = '#00ff00';
        ctx.strokeStyle = '#ffffff';
        ctx.lineWidth = 2;
        
        currentState.contourPoints.forEach(point => {
            ctx.beginPath();
            ctx.arc(point[0], point[1], 5, 0, 2 * Math.PI);
            ctx.fillStyle = '#00ff00';
            ctx.fill();
            ctx.strokeStyle = '#ffffff';
            ctx.stroke();
        });

        // Draw lines between points
        if (currentState.contourPoints.length > 1) {
            ctx.beginPath();
            ctx.strokeStyle = '#00ff00';
            ctx.lineWidth = 2;
            ctx.moveTo(currentState.contourPoints[0][0], currentState.contourPoints[0][1]);
            for (let i = 1; i < currentState.contourPoints.length; i++) {
                ctx.lineTo(currentState.contourPoints[i][0], currentState.contourPoints[i][1]);
            }
            // Close the loop if more than 2 points
            if (currentState.contourPoints.length > 2) {
                ctx.lineTo(currentState.contourPoints[0][0], currentState.contourPoints[0][1]);
            }
            ctx.stroke();
        }

        const originalPreview = document.getElementById('originalPreview');
        if (originalPreview) {
            originalPreview.src = canvas.toDataURL();
        }
    };
    img.src = currentState.originalImage;
}

// Clear snake points
function clearSnakePoints() {
    if (!currentState.originalImage) {
        showToast('Please upload an image first', 'error');
        return;
    }
    
    currentState.contourPoints = [];
    currentState.snakeInitialized = false;
    
    // Restore original image
    const originalPreview = document.getElementById('originalPreview');
    if (originalPreview && currentState.originalImage) {
        originalPreview.src = currentState.originalImage;
    }
    
    // Update point count
    const pointCount = document.getElementById('pointCount');
    if (pointCount) {
        pointCount.textContent = '0 points';
    }
    
    showToast('Points cleared', 'info');
}

// Initialize snake
async function initializeSnake() {
    if (currentState.contourPoints.length < 3) {
        showToast('Please select at least 3 points', 'error');
        return;
    }

    showLoading('Initializing snake...');

    const formData = new FormData();
    formData.append('session_id', currentState.sessionId);
    formData.append('points', JSON.stringify(currentState.contourPoints));
    formData.append('alpha', document.getElementById('alpha').value);
    formData.append('beta', document.getElementById('beta').value);
    formData.append('gamma', document.getElementById('gamma').value);

    try {
        console.log('Initializing snake with', currentState.contourPoints.length, 'points');
        
        const response = await fetch(`${API_BASE_URL}/snake/init`, {
            method: 'POST',
            body: formData
        });

        if (!response.ok) {
            const errorData = await response.json();
            throw new Error(errorData.detail || 'Snake initialization failed');
        }

        const data = await response.json();
        console.log('Snake init response:', data);
        
        currentState.snakeInitialized = true;
        
        const originalPreview = document.getElementById('originalPreview');
        if (originalPreview && data.overlay) {
            originalPreview.src = data.overlay;
        }
        
        showToast('Snake initialized', 'success');
    } catch (error) {
        console.error('Snake init error:', error);
        showToast('Failed to initialize snake: ' + error.message, 'error');
    } finally {
        hideLoading();
    }
}

// Run snake evolution
async function runSnake() {
    if (!currentState.snakeInitialized) {
        showToast('Please initialize snake first', 'error');
        return;
    }

    showLoading('Evolving snake...');

    const formData = new FormData();
    formData.append('session_id', currentState.sessionId);
    formData.append('iterations', document.getElementById('iterations').value);

    try {
        console.log('Running snake evolution...');
        
        const response = await fetch(`${API_BASE_URL}/snake/evolve`, {
            method: 'POST',
            body: formData
        });

        if (!response.ok) {
            const errorData = await response.json();
            throw new Error(errorData.detail || 'Snake evolution failed');
        }

        const data = await response.json();
        console.log('Snake evolution response:', data);
        
        const processedPreview = document.getElementById('processedPreview');
        if (processedPreview) {
            processedPreview.src = data.overlay;
        }
        
        currentState.contourPoints = data.contour;
        
        showToast(`Snake evolved (${data.iterations} iterations)`, 'success');
    } catch (error) {
        console.error('Snake evolution error:', error);
        showToast('Failed to evolve snake: ' + error.message, 'error');
    } finally {
        hideLoading();
    }
}

// Analyze contour
async function analyzeContour() {
    if (!currentState.originalImage) {
        showToast('Please upload an image first', 'error');
        return;
    }

    showLoading('Analyzing contour...');

    const formData = new FormData();
    formData.append('session_id', currentState.sessionId);

    try {
        console.log('Analyzing contour...');
        
        const response = await fetch(`${API_BASE_URL}/analyze-contour`, {
            method: 'POST',
            body: formData
        });

        if (!response.ok) {
            const errorData = await response.json();
            throw new Error(errorData.detail || 'Analysis failed');
        }

        const data = await response.json();
        console.log('Analysis response:', data);
        
        const analysis = data.analysis;

        // Update UI
        const perimeterValue = document.getElementById('perimeterValue');
        const areaValue = document.getElementById('areaValue');
        const numPointsValue = document.getElementById('numPointsValue');
        const closedValue = document.getElementById('closedValue');
        const chainCodeBox = document.getElementById('chainCodeBox');
        
        if (perimeterValue) perimeterValue.textContent = analysis.perimeter.toFixed(2);
        if (areaValue) areaValue.textContent = analysis.area.toFixed(2);
        if (numPointsValue) numPointsValue.textContent = analysis.num_points;
        if (closedValue) closedValue.textContent = analysis.is_closed ? 'Yes' : 'No';
        
        if (chainCodeBox) {
            const chainCode = analysis.chain_code.join('');
            chainCodeBox.innerHTML = chainCode || '<span class="text-muted">No chain code available</span>';
        }

        showToast('Analysis completed', 'success');
    } catch (error) {
        console.error('Analysis error:', error);
        showToast('Failed to analyze contour: ' + error.message, 'error');
    } finally {
        hideLoading();
    }
}

// Export results
function exportResults() {
    const perimeter = document.getElementById('perimeterValue').textContent;
    const area = document.getElementById('areaValue').textContent;
    const chainCodeBox = document.getElementById('chainCodeBox');
    const chainCode = chainCodeBox ? chainCodeBox.textContent : '';

    if (perimeter === '-' || area === '-') {
        showToast('Please analyze contour first', 'error');
        return;
    }

    const results = {
        perimeter: parseFloat(perimeter),
        area: parseFloat(area),
        chain_code: chainCode,
        timestamp: new Date().toISOString()
    };

    const blob = new Blob([JSON.stringify(results, null, 2)], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `contour_analysis_${Date.now()}.json`;
    a.click();
    URL.revokeObjectURL(url);

    showToast('Results exported', 'success');
}

// Undo last operation
async function undoOperation() {
    if (!currentState.originalImage) {
        showToast('Please upload an image first', 'error');
        return;
    }
    
    if (currentState.history.length === 0) {
        showToast('Nothing to undo', 'error');
        return;
    }

    showLoading('Undoing...');

    const formData = new FormData();
    formData.append('session_id', currentState.sessionId);

    try {
        const response = await fetch(`${API_BASE_URL}/undo`, {
            method: 'POST',
            body: formData
        });

        if (!response.ok) {
            const errorData = await response.json();
            throw new Error(errorData.detail || 'Undo failed');
        }

        const data = await response.json();
        console.log('Undo response:', data);

        currentState.originalImage = data.original;
        currentState.processedImage = data.processed;
        
        const originalPreview = document.getElementById('originalPreview');
        const processedPreview = document.getElementById('processedPreview');
        
        if (originalPreview) originalPreview.src = data.original;
        if (processedPreview) processedPreview.src = data.processed;
        
        currentState.history.pop();

        showToast('Undo successful', 'success');
    } catch (error) {
        console.error('Undo error:', error);
        showToast('Failed to undo: ' + error.message, 'error');
    } finally {
        hideLoading();
    }
}

// Reset image
async function resetImage() {
    if (!currentState.originalImage) {
        showToast('Please upload an image first', 'error');
        return;
    }

    showLoading('Resetting...');

    const formData = new FormData();
    formData.append('session_id', currentState.sessionId);

    try {
        const response = await fetch(`${API_BASE_URL}/reset`, {
            method: 'POST',
            body: formData
        });

        if (!response.ok) {
            const errorData = await response.json();
            throw new Error(errorData.detail || 'Reset failed');
        }

        const data = await response.json();
        console.log('Reset response:', data);

        currentState.originalImage = data.original;
        currentState.processedImage = data.processed;
        currentState.contourPoints = [];
        currentState.snakeInitialized = false;
        currentState.history = [];
        
        const originalPreview = document.getElementById('originalPreview');
        const processedPreview = document.getElementById('processedPreview');
        
        if (originalPreview) originalPreview.src = data.original;
        if (processedPreview) processedPreview.src = data.processed;

        // Reset point count display
        const pointCount = document.getElementById('pointCount');
        if (pointCount) {
            pointCount.textContent = '0 points';
        }

        showToast('Image reset', 'success');
    } catch (error) {
        console.error('Reset error:', error);
        showToast('Failed to reset image: ' + error.message, 'error');
    } finally {
        hideLoading();
    }
}

// Save image
async function saveImage() {
    if (!currentState.processedImage) {
        showToast('No processed image to save', 'error');
        return;
    }

    showLoading('Saving...');

    const formData = new FormData();
    formData.append('session_id', currentState.sessionId);
    formData.append('format', 'PNG');

    try {
        const response = await fetch(`${API_BASE_URL}/save`, {
            method: 'POST',
            body: formData
        });

        if (!response.ok) {
            const errorData = await response.json();
            throw new Error(errorData.detail || 'Save failed');
        }

        const data = await response.json();
        console.log('Save response received');

        // Download image
        const a = document.createElement('a');
        a.href = data.image;
        a.download = `processed_image_${Date.now()}.png`;
        a.click();

        showToast('Image saved', 'success');
    } catch (error) {
        console.error('Save error:', error);
        showToast('Failed to save image: ' + error.message, 'error');
    } finally {
        hideLoading();
    }
}

// Show loading overlay
function showLoading(message = 'Loading...') {
    // Remove existing overlay if any
    hideLoading();
    
    const overlay = document.createElement('div');
    overlay.className = 'spinner-overlay';
    overlay.id = 'loadingOverlay';
    overlay.innerHTML = `
        <div class="spinner-content">
            <div class="spinner"></div>
            <div class="spinner-text">${message}</div>
        </div>
    `;
    document.body.appendChild(overlay);
}

// Hide loading overlay
function hideLoading() {
    const overlay = document.getElementById('loadingOverlay');
    if (overlay) {
        overlay.remove();
    }
}

// Show toast notification
function showToast(message, type = 'info') {
    if (!toastContainer) {
        createToastContainer();
    }

    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    
    let title = '';
    switch(type) {
        case 'success':
            title = 'Success';
            break;
        case 'error':
            title = 'Error';
            break;
        default:
            title = 'Info';
    }
    
    toast.innerHTML = `
        <div class="toast-title">${title}</div>
        <div class="toast-message">${message}</div>
    `;
    
    toastContainer.appendChild(toast);

    // Auto remove after 3 seconds
    setTimeout(() => {
        toast.style.animation = 'slideOut 0.3s ease';
        setTimeout(() => {
            if (toast.parentNode) {
                toast.remove();
            }
        }, 300);
    }, 3000);
}

// Add slideOut animation
const style = document.createElement('style');
style.textContent = `
    @keyframes slideOut {
        from {
            transform: translateX(0);
            opacity: 1;
        }
        to {
            transform: translateX(100%);
            opacity: 0;
        }
    }
`;
document.head.appendChild(style);