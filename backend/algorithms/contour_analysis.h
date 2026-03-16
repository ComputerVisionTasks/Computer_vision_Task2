#pragma once
/*
 * contour_analysis.h — Freeman Chain Code and Contour Analysis
 */

#include "image_utils.h"
#include <vector>
#include <utility>

struct ContourAnalysis {
    std::vector<std::pair<int,int>> boundary;
    std::vector<int> chainCode;
    float perimeter = 0, area = 0;
    int numPoints = 0;
    bool isClosed = false;
};

ContourAnalysis analyze_contour(const GrayImage& edges);

// Freeman visualization functions
RGBImage render_freeman_overlay(const RGBImage& base, const ContourAnalysis& ca);
RGBImage render_freeman_code_image(const ContourAnalysis& ca, int w, int h);
