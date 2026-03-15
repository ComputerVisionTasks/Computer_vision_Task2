#pragma once
/*
 * shape_detection.h — Hough Lines, Hough Circles, Ellipse Detection
 */

#include "image_utils.h"
#include <vector>

// ========== Hough Lines ==========

struct HoughLine { float rho, theta; };

std::vector<HoughLine> hough_lines(const GrayImage& edges, float thetaRes = 1.0f,
                                   float rhoRes = 1.0f, int threshold = 50);

RGBImage overlay_lines(const RGBImage& img, const std::vector<HoughLine>& lines,
                       unsigned char r, unsigned char g, unsigned char b);

// ========== Hough Circles ==========

struct Circle { int x, y, r; };

std::vector<Circle> hough_circles(const GrayImage& edges, int rMin, int rMax,
                                  float threshold = 0.5f);

RGBImage overlay_circles(const RGBImage& img, const std::vector<Circle>& circles,
                         unsigned char r, unsigned char g, unsigned char b);

// ========== Ellipse Detection ==========

struct EllipseData { float x, y, a, b, angle; };

std::vector<EllipseData> detect_ellipses(const GrayImage& edges,
                                         int minArea = 100, int maxArea = 10000);

RGBImage overlay_ellipses(const RGBImage& img, const std::vector<EllipseData>& ellipses,
                          unsigned char r, unsigned char g, unsigned char b);
