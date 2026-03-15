#pragma once
/*
 * edge_detection.h — Canny Edge Detector
 */

#include "image_utils.h"

GrayImage gaussian_blur(const GrayImage& img, int ksize, float sigma);
void sobel(const GrayImage& img, GrayImage& gx, GrayImage& gy);
GrayImage canny(const GrayImage& img, float sigma = 1.0f,
                float lowT = 0.05f, float highT = 0.15f);
