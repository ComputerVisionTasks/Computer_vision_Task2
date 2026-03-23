/**
 * FromScratchCV - Operations Page JavaScript
 * Handles image upload, processing, and API communication
 * Enhanced with comprehensive shape detection controls
 */

// Global state
let currentState = {
    sessionId: generateSessionId(),
    originalImage: null,
    processedImage: null,
    overlayImage: null,
    contourPoints: [],
    snakeInitialized: false,
    history: [],
    shapeDetection: {
        lines: { count: 0, data: [] },
        circles: { count: 0, data: [] },
        ellipses: { count: 0, data: [] },
        lastParams: null
    }
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
    initShapeDetection();
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
    const detectShapesBtn = document.getElementById('detectShapes');

    if (lineThreshold) lineThreshold.addEventListener('input', updateRangeDisplay);
    if (ellipseTolerance) ellipseTolerance.addEventListener('input', updateRangeDisplay);
    if (detectShapesBtn) detectShapesBtn.addEventListener('click', detectShapes);

    // Snake
    const alpha = document.getElementById('alpha');
    const beta = document.getElementById('beta');
    const iterations = document.getElementById('iterations');
    const gamma = document.getElementById('gamma');
    const initSnakeBtn = document.getElementById('initSnake');
    const runSnakeBtn = document.getElementById('runSnake');

    if (alpha) alpha.addEventListener('input', updateRangeDisplay);
    if (beta) beta.addEventListener('input', updateRangeDisplay);
    if (iterations) iterations.addEventListener('input', updateRangeDisplay);
    if (gamma) gamma.addEventListener('input', updateRangeDisplay);
    if (initSnakeBtn) initSnakeBtn.addEventListener('click', initializeSnake);
    if (runSnakeBtn) runSnakeBtn.addEventListener('click', runSnakeEvolution);

    // Analysis
    const analyzeContourBtn = document.getElementById('analyzeContour');

    if (analyzeContourBtn) analyzeContourBtn.addEventListener('click', analyzeContour);

    // Original image click for snake points
    const originalPreview = document.getElementById('originalPreview');
    if (originalPreview) {
        originalPreview.addEventListener('click', handleImageClick);
    }

    console.log('Event listeners initialized');
}

// Initialize shape detection controls
function initShapeDetection() {
    console.log('Initializing shape detection controls');

    // Shape type toggles
    const shapeInputs = ['detectLines', 'detectCircles', 'detectEllipses'];
    shapeInputs.forEach(id => {
        const input = document.getElementById(id);
        if (input) {
            input.addEventListener('change', updateShapePanels);
        }
    });

    // Line parameters
    const lineParams = ['lineThreshold', 'thetaRes', 'rhoRes'];
    lineParams.forEach(id => {
        const input = document.getElementById(id);
        if (input) {
            input.addEventListener('input', updateRangeDisplay);
            input.addEventListener('change', () => updateShapeParams('lines'));
        }
    });

    // Circle parameters
    const circleParams = ['minRadius', 'maxRadius', 'circleThreshold', 'minVotes', 'centerDist'];
    circleParams.forEach(id => {
        const input = document.getElementById(id);
        if (input) {
            input.addEventListener('input', updateRangeDisplay);
            input.addEventListener('change', () => updateShapeParams('circles'));

            // Special handling for radius relationship
            if (id === 'minRadius' || id === 'maxRadius') {
                input.addEventListener('change', validateRadiusRange);
            }
        }
    });

    // Ellipse parameters
    const ellipseParams = ['minArea', 'maxArea', 'ellipseTolerance', 'minAspect', 'inlierRatio'];
    ellipseParams.forEach(id => {
        const input = document.getElementById(id);
        if (input) {
            input.addEventListener('input', updateRangeDisplay);
            input.addEventListener('change', () => updateShapeParams('ellipses'));

            // Special handling for area relationship
            if (id === 'minArea' || id === 'maxArea') {
                input.addEventListener('change', validateAreaRange);
            }
        }
    });

    // Preset buttons
    const presetDefault = document.getElementById('presetDefault');
    const presetPrecise = document.getElementById('presetPrecise');
    const presetFast = document.getElementById('presetFast');

    if (presetDefault) presetDefault.addEventListener('click', () => loadShapePreset('default'));
    if (presetPrecise) presetPrecise.addEventListener('click', () => loadShapePreset('precise'));
    if (presetFast) presetFast.addEventListener('click', () => loadShapePreset('fast'));

    // Export buttons
    const exportParamsBtn = document.getElementById('exportShapeParams');
    const copyResultsBtn = document.getElementById('copyShapeResults');

    if (exportParamsBtn) exportParamsBtn.addEventListener('click', exportShapeParameters);
    if (copyResultsBtn) copyResultsBtn.addEventListener('click', copyShapeResults);

    // Initial panel state
    updateShapePanels();
}

// Update shape panels visibility based on checkboxes
function updateShapePanels() {
    const panels = {
        lines: document.getElementById('linesPanel'),
        circles: document.getElementById('circlesPanel'),
        ellipses: document.getElementById('ellipsesPanel')
    };

    const checks = {
        lines: document.getElementById('detectLines')?.checked,
        circles: document.getElementById('detectCircles')?.checked,
        ellipses: document.getElementById('detectEllipses')?.checked
    };

    // Update panel visibility with animation
    Object.keys(panels).forEach(key => {
        if (panels[key]) {
            if (checks[key]) {
                panels[key].style.display = 'block';
                setTimeout(() => panels[key].classList.add('visible'), 10);
            } else {
                panels[key].classList.remove('visible');
                setTimeout(() => panels[key].style.display = 'none', 300);
            }
        }
    });

    // Update shape cards selected state
    document.querySelectorAll('.shape-card').forEach(card => {
        const shape = card.dataset.shape;
        if (checks[shape]) {
            card.classList.add('selected');
        } else {
            card.classList.remove('selected');
        }
    });
}

// Validate min/max radius relationship
function validateRadiusRange() {
    const minInput = document.getElementById('minRadius');
    const maxInput = document.getElementById('maxRadius');

    if (minInput && maxInput) {
        let min = parseInt(minInput.value);
        let max = parseInt(maxInput.value);

        if (min >= max) {
            maxInput.value = min + 10;
            updateRangeDisplay({ target: maxInput });
            showToast('Max radius adjusted to be greater than min radius', 'warning');
        }
    }
}

// Validate min/max area relationship
function validateAreaRange() {
    const minInput = document.getElementById('minArea');
    const maxInput = document.getElementById('maxArea');

    if (minInput && maxInput) {
        let min = parseInt(minInput.value);
        let max = parseInt(maxInput.value);

        if (min >= max) {
            maxInput.value = min * 2;
            updateRangeDisplay({ target: maxInput });
            showToast('Max area adjusted to be greater than min area', 'warning');
        }
    }
}

// Load shape detection preset
function loadShapePreset(preset) {
    switch (preset) {
        case 'default':
            // Default balanced settings
            if (document.getElementById('lineThreshold')) document.getElementById('lineThreshold').value = 50;
            if (document.getElementById('thetaRes')) document.getElementById('thetaRes').value = 1.0;
            if (document.getElementById('rhoRes')) document.getElementById('rhoRes').value = 1.0;

            if (document.getElementById('minRadius')) document.getElementById('minRadius').value = 10;
            if (document.getElementById('maxRadius')) document.getElementById('maxRadius').value = 100;
            if (document.getElementById('circleThreshold')) document.getElementById('circleThreshold').value = 0.55;
            if (document.getElementById('minVotes')) document.getElementById('minVotes').value = 20;
            if (document.getElementById('centerDist')) document.getElementById('centerDist').value = 0.3;

            if (document.getElementById('minArea')) document.getElementById('minArea').value = 200;
            if (document.getElementById('maxArea')) document.getElementById('maxArea').value = 10000;
            if (document.getElementById('ellipseTolerance')) document.getElementById('ellipseTolerance').value = 0.1;
            if (document.getElementById('minAspect')) document.getElementById('minAspect').value = 0.1;
            if (document.getElementById('inlierRatio')) document.getElementById('inlierRatio').value = 0.45;
            break;

        case 'precise':
            // More accurate but slower settings
            if (document.getElementById('lineThreshold')) document.getElementById('lineThreshold').value = 30;
            if (document.getElementById('thetaRes')) document.getElementById('thetaRes').value = 0.5;
            if (document.getElementById('rhoRes')) document.getElementById('rhoRes').value = 0.5;

            if (document.getElementById('minRadius')) document.getElementById('minRadius').value = 5;
            if (document.getElementById('maxRadius')) document.getElementById('maxRadius').value = 150;
            if (document.getElementById('circleThreshold')) document.getElementById('circleThreshold').value = 0.7;
            if (document.getElementById('minVotes')) document.getElementById('minVotes').value = 15;
            if (document.getElementById('centerDist')) document.getElementById('centerDist').value = 0.2;

            if (document.getElementById('minArea')) document.getElementById('minArea').value = 100;
            if (document.getElementById('maxArea')) document.getElementById('maxArea').value = 20000;
            if (document.getElementById('ellipseTolerance')) document.getElementById('ellipseTolerance').value = 0.05;
            if (document.getElementById('minAspect')) document.getElementById('minAspect').value = 0.15;
            if (document.getElementById('inlierRatio')) document.getElementById('inlierRatio').value = 0.6;
            break;

        case 'fast':
            // Faster but less accurate settings
            if (document.getElementById('lineThreshold')) document.getElementById('lineThreshold').value = 80;
            if (document.getElementById('thetaRes')) document.getElementById('thetaRes').value = 2.0;
            if (document.getElementById('rhoRes')) document.getElementById('rhoRes').value = 2.0;

            if (document.getElementById('minRadius')) document.getElementById('minRadius').value = 15;
            if (document.getElementById('maxRadius')) document.getElementById('maxRadius').value = 80;
            if (document.getElementById('circleThreshold')) document.getElementById('circleThreshold').value = 0.4;
            if (document.getElementById('minVotes')) document.getElementById('minVotes').value = 30;
            if (document.getElementById('centerDist')) document.getElementById('centerDist').value = 0.5;

            if (document.getElementById('minArea')) document.getElementById('minArea').value = 500;
            if (document.getElementById('maxArea')) document.getElementById('maxArea').value = 5000;
            if (document.getElementById('ellipseTolerance')) document.getElementById('ellipseTolerance').value = 0.2;
            if (document.getElementById('minAspect')) document.getElementById('minAspect').value = 0.05;
            if (document.getElementById('inlierRatio')) document.getElementById('inlierRatio').value = 0.3;
            break;
    }

    // Update all displays
    document.querySelectorAll('input[type=range]').forEach(input => {
        updateRangeDisplay({ target: input });
    });

    showToast(`Loaded ${preset} preset`, 'success');
}

// Update shape parameters (can be used for real-time preview)
function updateShapeParams(shapeType) {
    if (!currentState.originalImage) return;

    // Could implement real-time parameter preview here
    // For now, just log the changes
    console.log(`Shape params updated for ${shapeType}`);
}

// Disable/enable all operation buttons
function disableAllOperationButtons(disabled) {
    // Operation buttons
    const operationButtons = [
        'runCanny',
        'detectShapes',
        'initSnake',
        'runSnake',
        'analyzeContour'
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

        // Reset shape detection state
        currentState.shapeDetection = {
            lines: { count: 0, data: [] },
            circles: { count: 0, data: [] },
            ellipses: { count: 0, data: [] },
            lastParams: null
        };

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

        // Update shape counts display
        updateShapeResultsUI();

        // Hide shape summary until new detection
        const shapeSummary = document.getElementById('shapeSummary');
        if (shapeSummary) shapeSummary.style.display = 'none';

        const permanentResults = document.getElementById('permanentShapeResults');
        if (permanentResults) permanentResults.style.display = 'none';

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
    let value = parseFloat(e.target.value);

    // Format value appropriately
    let displayValue;
    if (id === 'iterations') {
        displayValue = Math.round(value).toString();
    } else if (id === 'thetaRes' || id === 'rhoRes') {
        displayValue = value.toFixed(1);
    } else {
        displayValue = value.toFixed(2);
    }

    // Look for badge format
    let display = document.getElementById(id + 'ValueBadge');
    if (!display) {
        display = document.getElementById(id + 'Badge');
    }
    if (!display) {
        display = document.getElementById(id + 'Value');
    }

    if (display) {
        display.textContent = displayValue;

        // Add unit if applicable
        if (id === 'thetaRes') {
            display.textContent = displayValue + '°';
        }
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

// Enhanced detect shapes function
async function detectShapes() {
    if (!currentState.originalImage) {
        showToast('Please upload an image first', 'error');
        return;
    }

    const detectLines = document.getElementById('detectLines').checked;
    const detectCircles = document.getElementById('detectCircles').checked;
    const detectEllipses = document.getElementById('detectEllipses').checked;

    if (!detectLines && !detectCircles && !detectEllipses) {
        showToast('Please select at least one shape type to detect', 'error');
        return;
    }

    showLoading('Detecting shapes...');

    try {
        // Store current parameters
        const params = {
            lines: {
                threshold: document.getElementById('lineThreshold')?.value || '50',
                thetaRes: document.getElementById('thetaRes')?.value || '1.0',
                rhoRes: document.getElementById('rhoRes')?.value || '1.0'
            },
            circles: {
                radius_min: document.getElementById('minRadius')?.value || '10',
                radius_max: document.getElementById('maxRadius')?.value || '100',
                threshold: document.getElementById('circleThreshold')?.value || '0.55',
                min_votes: document.getElementById('minVotes')?.value || '20',
                center_dist: document.getElementById('centerDist')?.value || '0.3'
            },
            ellipses: {
                min_area: document.getElementById('minArea')?.value || '200',
                max_area: document.getElementById('maxArea')?.value || '10000',
                tolerance: document.getElementById('ellipseTolerance')?.value || '0.1'
            }
        };

        // Load base image
        const baseImg = new Image();
        await new Promise((resolve, reject) => {
            baseImg.onload = resolve;
            baseImg.onerror = reject;
            baseImg.src = currentState.originalImage;
        });

        const canvas = document.createElement('canvas');
        canvas.width = baseImg.width;
        canvas.height = baseImg.height;
        const ctx = canvas.getContext('2d');
        ctx.drawImage(baseImg, 0, 0);

        // Reset shape detection results
        currentState.shapeDetection = {
            lines: { count: 0, data: [] },
            circles: { count: 0, data: [] },
            ellipses: { count: 0, data: [] },
            lastParams: params
        };

        let shapesDetected = false;

        // Detect Lines
        if (detectLines) {
            const lineFormData = new FormData();
            lineFormData.append('session_id', currentState.sessionId);
            lineFormData.append('threshold', params.lines.threshold);
            lineFormData.append('theta_res', params.lines.thetaRes);
            lineFormData.append('rho_res', params.lines.rhoRes);

            const lineResponse = await fetch(`${API_BASE_URL}/hough-lines`, {
                method: 'POST',
                body: lineFormData
            });

            if (lineResponse.ok) {
                const lineData = await lineResponse.json();
                currentState.shapeDetection.lines = {
                    count: lineData.num_lines || 0,
                    data: lineData.lines || []
                };
                shapesDetected = true;

                // Draw lines in RED
                ctx.strokeStyle = 'rgba(255, 0, 0, 0.8)';
                ctx.lineWidth = 2;
                const span = Math.max(canvas.width, canvas.height) * 2;
                for (const l of lineData.lines || []) {
                    const r = l[0], t = l[1];
                    const ca = Math.cos(t), sa = Math.sin(t);
                    const x0 = ca * r, y0 = sa * r;

                    ctx.beginPath();
                    ctx.moveTo(x0 - sa * span, y0 + ca * span);
                    ctx.lineTo(x0 + sa * span, y0 - ca * span);
                    ctx.stroke();
                }
            }
        }

        // Detect Circles
        if (detectCircles) {
            const circleFormData = new FormData();
            circleFormData.append('session_id', currentState.sessionId);
            circleFormData.append('radius_min', params.circles.radius_min);
            circleFormData.append('radius_max', params.circles.radius_max);
            circleFormData.append('threshold', params.circles.threshold);
            circleFormData.append('min_abs_votes', params.circles.min_votes);
            circleFormData.append('center_dist', params.circles.center_dist);

            const circleResponse = await fetch(`${API_BASE_URL}/hough-circles`, {
                method: 'POST',
                body: circleFormData
            });

            if (circleResponse.ok) {
                const circleData = await circleResponse.json();
                currentState.shapeDetection.circles = {
                    count: circleData.num_circles || 0,
                    data: circleData.circles || []
                };
                shapesDetected = true;

                // Draw circles in GREEN
                ctx.strokeStyle = 'rgba(0, 255, 0, 0.8)';
                ctx.lineWidth = 3;
                for (const c of circleData.circles || []) {
                    ctx.beginPath();
                    ctx.arc(c[0], c[1], c[2], 0, 2 * Math.PI);
                    ctx.stroke();

                    // Draw center point
                    ctx.fillStyle = 'rgba(0, 255, 0, 0.5)';
                    ctx.beginPath();
                    ctx.arc(c[0], c[1], 3, 0, 2 * Math.PI);
                    ctx.fill();
                }
            }
        }

        // Detect Ellipses
        if (detectEllipses) {
            const ellipseFormData = new FormData();
            ellipseFormData.append('session_id', currentState.sessionId);
            ellipseFormData.append('min_area', params.ellipses.min_area);
            ellipseFormData.append('max_area', params.ellipses.max_area);
            ellipseFormData.append('tolerance', params.ellipses.tolerance);
            ellipseFormData.append('inlier_ratio', params.ellipses.inlier_ratio);
            ellipseFormData.append('min_aspect', params.ellipses.min_aspect);

            const ellipseResponse = await fetch(`${API_BASE_URL}/detect-ellipses`, {
                method: 'POST',
                body: ellipseFormData
            });

            if (ellipseResponse.ok) {
                const ellipseData = await ellipseResponse.json();
                currentState.shapeDetection.ellipses = {
                    count: ellipseData.num_ellipses || 0,
                    data: ellipseData.ellipses || []
                };
                shapesDetected = true;

                // Draw ellipses in BLUE
                ctx.strokeStyle = 'rgba(0, 0, 255, 0.8)';
                ctx.lineWidth = 3;
                for (const e of ellipseData.ellipses || []) {
                    ctx.beginPath();
                    ctx.ellipse(e.x, e.y, e.a, e.b, e.angle, 0, 2 * Math.PI);
                    ctx.stroke();

                    // Draw center point
                    ctx.fillStyle = 'rgba(0, 0, 255, 0.5)';
                    ctx.beginPath();
                    ctx.arc(e.x, e.y, 3, 0, 2 * Math.PI);
                    ctx.fill();
                }
            }
        }

        // Update image if we drew anything
        if (shapesDetected) {
            const finalImgData = canvas.toDataURL('image/png');
            currentState.processedImage = finalImgData;
            const processedPreview = document.getElementById('processedPreview');
            if (processedPreview) {
                processedPreview.src = finalImgData;
            }
        }

        // Update UI with results
        updateShapeResultsUI();
        showShapeParameters(params);

        // Show results in permanent display
        displayShapeResults();

        // Build success message
        const parts = [];
        if (currentState.shapeDetection.lines.count > 0) parts.push(`${currentState.shapeDetection.lines.count} lines`);
        if (currentState.shapeDetection.circles.count > 0) parts.push(`${currentState.shapeDetection.circles.count} circles`);
        if (currentState.shapeDetection.ellipses.count > 0) parts.push(`${currentState.shapeDetection.ellipses.count} ellipses`);

        const message = parts.length > 0 ? `Detected: ${parts.join(', ')}` : 'No shapes detected';
        showToast(message, 'success');

    } catch (error) {
        console.error('Shape detection error:', error);
        showToast('Failed to detect shapes: ' + error.message, 'error');
    } finally {
        hideLoading();
    }
}

// Update shape results UI
function updateShapeResultsUI() {
    // Update counts in panels
    const linesCount = document.getElementById('linesCount');
    const circlesCount = document.getElementById('circlesCount');
    const ellipsesCount = document.getElementById('ellipsesCount');

    if (linesCount) linesCount.textContent = currentState.shapeDetection.lines.count;
    if (circlesCount) circlesCount.textContent = currentState.shapeDetection.circles.count;
    if (ellipsesCount) ellipsesCount.textContent = currentState.shapeDetection.ellipses.count;

    // Update preview badges
    const linesPreview = document.getElementById('linesPreview');
    const circlesPreview = document.getElementById('circlesPreview');
    const ellipsesPreview = document.getElementById('ellipsesPreview');

    if (linesPreview) linesPreview.textContent = `${currentState.shapeDetection.lines.count} detected`;
    if (circlesPreview) circlesPreview.textContent = `${currentState.shapeDetection.circles.count} detected`;
    if (ellipsesPreview) ellipsesPreview.textContent = `${currentState.shapeDetection.ellipses.count} detected`;
}

// Show shape parameters
function showShapeParameters(params) {
    const summary = document.getElementById('shapeSummary');
    const content = document.getElementById('shapeSummaryContent');

    if (!summary || !content) return;

    let html = '<div class="row g-2">';

    if (document.getElementById('detectLines').checked) {
        html += `
            <div class="col-12">
                <div class="param-group">
                    <span class="param-group-title"><i class="bi bi-slash-lg"></i> Lines</span>
                    <div class="param-list">
                        <span class="param-item">Threshold: ${params.lines.threshold}</span>
                        <span class="param-item">θ Res: ${params.lines.thetaRes}°</span>
                        <span class="param-item">ρ Res: ${params.lines.rhoRes}</span>
                    </div>
                </div>
            </div>
        `;
    }

    if (document.getElementById('detectCircles').checked) {
        html += `
            <div class="col-12">
                <div class="param-group">
                    <span class="param-group-title"><i class="bi bi-circle"></i> Circles</span>
                    <div class="param-list">
                        <span class="param-item">Radius: ${params.circles.radius_min}-${params.circles.radius_max}</span>
                        <span class="param-item">Threshold: ${params.circles.threshold}</span>
                        <span class="param-item">Min Votes: ${params.circles.min_votes}</span>
                    </div>
                </div>
            </div>
        `;
    }

    if (document.getElementById('detectEllipses').checked) {
        html += `
            <div class="col-12">
                <div class="param-group">
                    <span class="param-group-title"><i class="bi bi-egg"></i> Ellipses</span>
                    <div class="param-list">
                        <span class="param-item">Area: ${params.ellipses.min_area}-${params.ellipses.max_area}</span>
                        <span class="param-item">Tolerance: ${params.ellipses.tolerance}</span>
                    </div>
                </div>
            </div>
        `;
    }

    html += '</div>';

    content.innerHTML = html;
    summary.style.display = 'block';
}

// Display shape results in permanent panel
function displayShapeResults() {
    const permanentResults = document.getElementById('permanentShapeResults');
    const permanentCounts = document.getElementById('permanentShapeCounts');
    const shapeDetails = document.getElementById('shapeDetails');

    if (!permanentResults || !permanentCounts) return;

    let countsHtml = '';
    let detailsHtml = '';

    if (currentState.shapeDetection.lines.count > 0) {
        countsHtml += `<span class="badge shape-badge shape-badge-lines me-1 mb-1">${currentState.shapeDetection.lines.count} Lines</span>`;

        // Show first few lines parameters
        if (currentState.shapeDetection.lines.data.length > 0) {
            detailsHtml += '<div class="lines-details mt-2"><strong>Lines:</strong><br>';
            currentState.shapeDetection.lines.data.slice(0, 3).forEach((line, i) => {
                const angle = (line[1] * 180 / Math.PI).toFixed(1);
                detailsHtml += `<small>Line ${i + 1}: ρ=${line[0].toFixed(1)}, θ=${angle}°</small><br>`;
            });
            if (currentState.shapeDetection.lines.data.length > 3) {
                detailsHtml += `<small>... and ${currentState.shapeDetection.lines.data.length - 3} more</small>`;
            }
            detailsHtml += '</div>';
        }
    }

    if (currentState.shapeDetection.circles.count > 0) {
        countsHtml += `<span class="badge shape-badge shape-badge-circles me-1 mb-1">${currentState.shapeDetection.circles.count} Circles</span>`;

        if (currentState.shapeDetection.circles.data.length > 0) {
            detailsHtml += '<div class="circles-details mt-2"><strong>Circles:</strong><br>';
            currentState.shapeDetection.circles.data.slice(0, 3).forEach((circle, i) => {
                detailsHtml += `<small>Circle ${i + 1}: (${circle[0]},${circle[1]}) r=${circle[2]}</small><br>`;
            });
            if (currentState.shapeDetection.circles.data.length > 3) {
                detailsHtml += `<small>... and ${currentState.shapeDetection.circles.data.length - 3} more</small>`;
            }
            detailsHtml += '</div>';
        }
    }

    if (currentState.shapeDetection.ellipses.count > 0) {
        countsHtml += `<span class="badge shape-badge shape-badge-ellipses me-1 mb-1">${currentState.shapeDetection.ellipses.count} Ellipses</span>`;

        if (currentState.shapeDetection.ellipses.data.length > 0) {
            detailsHtml += '<div class="ellipses-details mt-2"><strong>Ellipses:</strong><br>';
            currentState.shapeDetection.ellipses.data.slice(0, 3).forEach((ellipse, i) => {
                const angle = (ellipse.angle * 180 / Math.PI).toFixed(1);
                detailsHtml += `<small>Ellipse ${i + 1}: a=${ellipse.a.toFixed(1)}, b=${ellipse.b.toFixed(1)}, angle=${angle}°</small><br>`;
            });
            if (currentState.shapeDetection.ellipses.data.length > 3) {
                detailsHtml += `<small>... and ${currentState.shapeDetection.ellipses.data.length - 3} more</small>`;
            }
            detailsHtml += '</div>';
        }
    }

    if (countsHtml) {
        permanentCounts.innerHTML = countsHtml;
        if (shapeDetails) shapeDetails.innerHTML = detailsHtml;
        permanentResults.style.display = 'block';

        // Enable export buttons
        const exportParamsBtn = document.getElementById('exportShapeParams');
        const copyResultsBtn = document.getElementById('copyShapeResults');
        if (exportParamsBtn) exportParamsBtn.disabled = false;
        if (copyResultsBtn) copyResultsBtn.disabled = false;
    } else {
        permanentCounts.innerHTML = '<span class="text-muted">No shapes detected. Try adjusting parameters.</span>';
        if (shapeDetails) shapeDetails.innerHTML = '';
        permanentResults.style.display = 'block';

        // Disable export buttons
        const exportParamsBtn = document.getElementById('exportShapeParams');
        const copyResultsBtn = document.getElementById('copyShapeResults');
        if (exportParamsBtn) exportParamsBtn.disabled = true;
        if (copyResultsBtn) copyResultsBtn.disabled = true;
    }
}

// Export shape parameters and results
function exportShapeParameters() {
    const exportData = {
        timestamp: new Date().toISOString(),
        parameters: currentState.shapeDetection.lastParams,
        results: {
            lines: {
                count: currentState.shapeDetection.lines.count,
                data: currentState.shapeDetection.lines.data
            },
            circles: {
                count: currentState.shapeDetection.circles.count,
                data: currentState.shapeDetection.circles.data
            },
            ellipses: {
                count: currentState.shapeDetection.ellipses.count,
                data: currentState.shapeDetection.ellipses.data
            }
        },
        image_info: {
            width: document.getElementById('originalPreview')?.naturalWidth,
            height: document.getElementById('originalPreview')?.naturalHeight
        }
    };

    const blob = new Blob([JSON.stringify(exportData, null, 2)], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `shape_detection_${Date.now()}.json`;
    a.click();
    URL.revokeObjectURL(url);

    showToast('Shape parameters exported', 'success');
}

// Copy shape results to clipboard
function copyShapeResults() {
    const lines = currentState.shapeDetection.lines.count;
    const circles = currentState.shapeDetection.circles.count;
    const ellipses = currentState.shapeDetection.ellipses.count;

    let text = `Shape Detection Results - ${new Date().toLocaleString()}\n`;
    text += `${'='.repeat(50)}\n\n`;
    text += `Lines: ${lines}\n`;
    text += `Circles: ${circles}\n`;
    text += `Ellipses: ${ellipses}\n\n`;

    if (lines > 0) {
        text += `Line Details:\n`;
        text += `${'-'.repeat(30)}\n`;
        currentState.shapeDetection.lines.data.forEach((line, i) => {
            const angle = (line[1] * 180 / Math.PI).toFixed(1);
            text += `  Line ${i + 1}: ρ=${line[0].toFixed(1)}, θ=${angle}°\n`;
        });
        text += `\n`;
    }

    if (circles > 0) {
        text += `Circle Details:\n`;
        text += `${'-'.repeat(30)}\n`;
        currentState.shapeDetection.circles.data.forEach((circle, i) => {
            text += `  Circle ${i + 1}: center=(${circle[0]},${circle[1]}), radius=${circle[2]}\n`;
        });
        text += `\n`;
    }

    if (ellipses > 0) {
        text += `Ellipse Details:\n`;
        text += `${'-'.repeat(30)}\n`;
        currentState.shapeDetection.ellipses.data.forEach((ellipse, i) => {
            const angle = (ellipse.angle * 180 / Math.PI).toFixed(1);
            text += `  Ellipse ${i + 1}: center=(${ellipse.x.toFixed(1)},${ellipse.y.toFixed(1)}), ` +
                `a=${ellipse.a.toFixed(1)}, b=${ellipse.b.toFixed(1)}, angle=${angle}°\n`;
        });
    }

    navigator.clipboard.writeText(text).then(() => {
        showToast('Results copied to clipboard', 'success');
    }).catch(() => {
        showToast('Failed to copy results', 'error');
    });
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
    img.onload = function () {
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
    formData.append('alpha', document.getElementById('alpha').value || '0.5');
    formData.append('beta', document.getElementById('beta').value || '0.5');
    formData.append('gamma', document.getElementById('gamma').value || '1.0');

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
async function runSnakeEvolution() {
    if (!currentState.snakeInitialized) {
        showToast('Please initialize snake first', 'error');
        return;
    }

    showLoading('Evolving snake...');

    const formData = new FormData();
    formData.append('session_id', currentState.sessionId);
    formData.append('iterations', document.getElementById('iterations').value);
    formData.append('alpha', document.getElementById('alpha').value || '0.5');
    formData.append('beta', document.getElementById('beta').value || '0.5');
    formData.append('gamma', document.getElementById('gamma').value || '1.0');
    formData.append('restart', '1');

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

        const used = data.params || {};
        showToast(
            `Snake evolved (${data.iterations} iterations) - a=${Number(used.alpha ?? 0).toFixed(2)}, b=${Number(used.beta ?? 0).toFixed(2)}, g=${Number(used.gamma ?? 0).toFixed(2)}`,
            'success'
        );
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

        // Update UI with metrics
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
            let chainCodeText = '';
            if (Array.isArray(analysis.chain_code)) {
                chainCodeText = analysis.chain_code.join('');
            } else if (typeof analysis.chain_code === 'string') {
                chainCodeText = analysis.chain_code;
            }

            if (chainCodeText.length > 0) {
                chainCodeBox.textContent = chainCodeText;
            } else if (analysis.chain_code_length && analysis.chain_code_length > 0) {
                chainCodeBox.textContent = `${analysis.chain_code_length} codes`;
            } else {
                chainCodeBox.innerHTML = '<span class="text-muted">No chain code available (Initialize snake first)</span>';
            }
        }

        // Freeman direction image
        if (data.freeman_code_image) {
            const img = document.getElementById('freemanCodeImage');
            if (img) {
                img.src = data.freeman_code_image;
                img.style.display = 'block';
                const parent = img.parentElement;
                if (parent) {
                    Array.from(parent.children).forEach(el => {
                        if (el !== img && el.tagName !== 'IMG') {
                            el.style.display = 'none';
                        }
                    });
                }
            }
        }

        // Also show contour overlay in processed preview for quick feedback
        const processedPreview = document.getElementById('processedPreview');
        if (processedPreview && data.freeman_overlay) {
            processedPreview.src = data.freeman_overlay;
            currentState.processedImage = data.freeman_overlay;
        }

        showToast('Analysis completed', 'success');
    } catch (error) {
        console.error('Analysis error:', error);
        showToast('Failed to analyze contour: ' + error.message, 'error');
    } finally {
        hideLoading();
    }
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

        // Reset shape detection state
        currentState.shapeDetection = {
            lines: { count: 0, data: [] },
            circles: { count: 0, data: [] },
            ellipses: { count: 0, data: [] },
            lastParams: null
        };

        const originalPreview = document.getElementById('originalPreview');
        const processedPreview = document.getElementById('processedPreview');

        if (originalPreview) originalPreview.src = data.original;
        if (processedPreview) processedPreview.src = data.processed;

        // Reset point count display
        const pointCount = document.getElementById('pointCount');
        if (pointCount) {
            pointCount.textContent = '0 points';
        }

        // Hide shape results
        const shapeSummary = document.getElementById('shapeSummary');
        if (shapeSummary) shapeSummary.style.display = 'none';

        const permanentResults = document.getElementById('permanentShapeResults');
        if (permanentResults) permanentResults.style.display = 'none';

        // Reset shape counts in UI
        updateShapeResultsUI();

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

    // Check if we can show this inside the permanent Shape Results box
    const permanentCounts = document.getElementById('permanentShapeCounts');
    const permanentResults = document.getElementById('permanentShapeResults');
    const shapeTabActive = document.getElementById('shape-tab') &&
        document.getElementById('shape-tab').classList.contains('active');

    if (shapeTabActive && permanentCounts && permanentResults) {
        permanentResults.style.display = 'block';
        permanentCounts.innerHTML = `
            <div class="d-flex align-items-center text-info">
                <div class="spinner-border spinner-border-sm me-2" role="status">
                    <span class="visually-hidden">Loading...</span>
                </div>
                <span>${message}</span>
            </div>
        `;
        // We add a marker to know it's a localized loader
        permanentCounts.dataset.isLoading = "true";
        return;
    }

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
    // Hide localized Shape Results loader if active
    const permanentCounts = document.getElementById('permanentShapeCounts');
    if (permanentCounts && permanentCounts.dataset.isLoading === "true") {
        delete permanentCounts.dataset.isLoading;
        // Only reset to "Ready to detect..." if we haven't already populated shapes into it
        if (permanentCounts.innerHTML.includes('Loading...')) {
            permanentCounts.innerHTML = '<span class="text-muted">Ready to detect...</span>';
        }
    }

    // Hide fullscreen overlay if active
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
    switch (type) {
        case 'success':
            title = 'Success';
            break;
        case 'error':
            title = 'Error';
            break;
        case 'warning':
            title = 'Warning';
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
    
    .toast {
        position: relative;
        background: var(--bg-card);
        border-radius: 8px;
        padding: 12px 16px;
        margin-bottom: 10px;
        box-shadow: 0 4px 12px rgba(0, 0, 0, 0.15);
        border-left: 4px solid;
        animation: slideIn 0.3s ease;
        min-width: 280px;
    }
    
    @keyframes slideIn {
        from {
            transform: translateX(100%);
            opacity: 0;
        }
        to {
            transform: translateX(0);
            opacity: 1;
        }
    }
    
    .toast.success { border-left-color: #28a745; }
    .toast.error { border-left-color: #dc3545; }
    .toast.warning { border-left-color: #ffc107; }
    .toast.info { border-left-color: #17a2b8; }
    
    .toast-title {
        font-weight: 600;
        margin-bottom: 4px;
    }
    
    .toast-message {
        font-size: 0.85rem;
        color: var(--text-muted);
    }
    
    .spinner-overlay {
        position: fixed;
        top: 0;
        left: 0;
        width: 100%;
        height: 100%;
        background: rgba(0, 0, 0, 0.7);
        display: flex;
        justify-content: center;
        align-items: center;
        z-index: 9999;
    }
    
    .spinner-content {
        background: var(--bg-card);
        padding: 20px 30px;
        border-radius: 12px;
        display: flex;
        flex-direction: column;
        align-items: center;
        gap: 12px;
    }
    
    .spinner {
        width: 40px;
        height: 40px;
        border: 3px solid rgba(255, 255, 255, 0.3);
        border-top-color: #fff;
        border-radius: 50%;
        animation: spin 0.8s linear infinite;
    }
    
    @keyframes spin {
        to { transform: rotate(360deg); }
    }
    
    .spinner-text {
        color: white;
        font-size: 0.9rem;
    }
`;
document.head.appendChild(style);