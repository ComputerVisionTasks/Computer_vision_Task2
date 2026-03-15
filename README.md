# FromScratchCV - Computer Vision from Scratch

A production-ready web application that implements core computer vision algorithms from scratch using only NumPy and Pillow. No OpenCV, scikit-image, or other CV libraries are used.

## Features

### Task 1 - Edge Detection & Shape Recognition
- **Canny Edge Detector** - Full implementation including Gaussian blur, Sobel gradients, non-maximum suppression, and hysteresis thresholding
- **Hough Line Transform** - Detects straight lines in edge images
- **Hough Circle Transform** - Finds circular shapes with variable radii
- **Ellipse Fitting** - Least squares ellipse fitting on edge points

### Task 2 - Active Contour (Snake)
- **Greedy Snake Algorithm** - Energy-minimizing contour evolution
- **Freeman Chain Code** - 8-directional chain encoding of contours
- **Shape Analysis** - Perimeter and area calculation (shoelace formula)

## Algorithm References

1. **Canny Edge Detector** - Canny, J., "A Computational Approach to Edge Detection", IEEE PAMI 1986
2. **Hough Transform** - Duda, R. O. and P. E. Hart, "Use of the Hough Transformation to Detect Lines and Curves in Pictures", 1972
3. **Circle Detection** - Kimme, C., Ballard, D., and Sklansky, J. "Finding Circles by an Array of Accumulators", 1975
4. **Ellipse Fitting** - Fitzgibbon, A.W., Pilu, M., and Fisher, R.B. "Direct Least Square Fitting of Ellipses", IEEE PAMI 1999
5. **Greedy Snake** - Williams, D.J. and Shah, M. "A Fast Algorithm for Active Contours and Curvature Estimation", CVGIP 1992
6. **Chain Code** - Freeman, H. "On the Encoding of Arbitrary Geometric Configurations", IRE Trans. 1961

## Project Structure
