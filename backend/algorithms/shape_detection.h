#pragma once
/*
 * shape_detection.h
 */
#include "image_utils.h"
#include <vector>

struct HoughLine  { float rho, theta; };
struct Circle     { int x, y, r; };
struct EllipseData{ float x, y, a, b, angle; };

// Hough Lines
std::vector<HoughLine> hough_lines(const GrayImage& edges,
                                   float thetaRes = 1.0f,
                                   float rhoRes   = 1.0f,
                                   int   threshold = 80);

RGBImage overlay_lines(const RGBImage& img,
                       const std::vector<HoughLine>& lines,
                       unsigned char r = 255,
                       unsigned char g = 0,
                       unsigned char b = 0);

// Hough Circles
//   threshold    — fraction of maxV (0..1), e.g. 0.5
//   minAbsVotes  — hard floor; set to ~(0.25 * PI * rMin) or higher
std::vector<Circle> hough_circles(const GrayImage& edges,
                                  int   rMin        = 10,
                                  int   rMax        = 100,
                                  float threshold   = 0.55f,
                                  int   minAbsVotes = 20);

RGBImage overlay_circles(const RGBImage& img,
                         const std::vector<Circle>& circles,
                         unsigned char r = 0,
                         unsigned char g = 255,
                         unsigned char b = 0);

// Ellipse Detection
std::vector<EllipseData> detect_ellipses(const GrayImage& edges,
                                          int minArea = 200,
                                          int maxArea = 50000);

RGBImage overlay_ellipses(const RGBImage& img,
                          const std::vector<EllipseData>& ellipses,
                          unsigned char r = 0,
                          unsigned char g = 0,
                          unsigned char b = 255);