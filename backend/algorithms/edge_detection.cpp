/*
 * edge_detection.cpp — Canny Edge Detector implementation.
 * Gaussian blur → Sobel gradients → Non-maximum suppression → Hysteresis
 */

#include "edge_detection.h"

GrayImage gaussian_blur(const GrayImage& img, int ksize, float sigma) {
    GrayImage out(img.w, img.h);
    int half = ksize / 2;
    std::vector<float> k(ksize);
    float sum = 0;
    for (int i = 0; i < ksize; i++) {
        float x = (float)(i - half);
        k[i] = expf(-(x * x) / (2 * sigma * sigma));
        sum += k[i];
    }
    for (auto& v : k) v /= sum;

    // Separable blur: X pass
    GrayImage tmp(img.w, img.h);
    for (int y = 0; y < img.h; y++)
        for (int x = 0; x < img.w; x++) {
            float s = 0;
            for (int i = 0; i < ksize; i++)
                s += img.safe(x + i - half, y) * k[i];
            tmp.at(x, y) = s;
        }
    // Y pass
    for (int y = 0; y < img.h; y++)
        for (int x = 0; x < img.w; x++) {
            float s = 0;
            for (int i = 0; i < ksize; i++)
                s += tmp.safe(x, y + i - half) * k[i];
            out.at(x, y) = s;
        }
    return out;
}

void sobel(const GrayImage& img, GrayImage& gx, GrayImage& gy) {
    gx = GrayImage(img.w, img.h);
    gy = GrayImage(img.w, img.h);
    for (int y = 1; y < img.h - 1; y++)
        for (int x = 1; x < img.w - 1; x++) {
            gx.at(x, y) = -img.at(x-1,y-1) + img.at(x+1,y-1)
                         - 2*img.at(x-1,y)  + 2*img.at(x+1,y)
                         - img.at(x-1,y+1)  + img.at(x+1,y+1);
            gy.at(x, y) = -img.at(x-1,y-1) - 2*img.at(x,y-1) - img.at(x+1,y-1)
                         + img.at(x-1,y+1) + 2*img.at(x,y+1) + img.at(x+1,y+1);
        }
}

GrayImage canny(const GrayImage& img, float sigma, float lowT, float highT) {
    // 1. Gaussian blur
    GrayImage blurred = gaussian_blur(img, 5, sigma);

    // 2. Sobel gradients
    GrayImage gx, gy;
    sobel(blurred, gx, gy);

    // 3. Magnitude & direction
    GrayImage mag(img.w, img.h);
    std::vector<float> dir(img.w * img.h, 0);
    float maxMag = 0;
    for (int i = 0; i < img.w * img.h; i++) {
        mag.data[i] = sqrtf(gx.data[i]*gx.data[i] + gy.data[i]*gy.data[i]);
        dir[i] = atan2f(gy.data[i], gx.data[i]);
        maxMag = std::max(maxMag, mag.data[i]);
    }
    if (maxMag > 0) for (auto& v : mag.data) v /= maxMag;

    // 4. Non-maximum suppression
    GrayImage nms(img.w, img.h);
    for (int y = 1; y < img.h - 1; y++)
        for (int x = 1; x < img.w - 1; x++) {
            float m = mag.at(x, y);
            if (m == 0) continue;
            float angle = dir[y * img.w + x] * 180.0f / 3.14159265f;
            if (angle < 0) angle += 180;
            float n1, n2;
            if ((angle < 22.5f) || (angle >= 157.5f)) {
                n1 = mag.at(x-1,y); n2 = mag.at(x+1,y);
            } else if (angle < 67.5f) {
                n1 = mag.at(x+1,y-1); n2 = mag.at(x-1,y+1);
            } else if (angle < 112.5f) {
                n1 = mag.at(x,y-1); n2 = mag.at(x,y+1);
            } else {
                n1 = mag.at(x-1,y-1); n2 = mag.at(x+1,y+1);
            }
            if (m >= n1 && m >= n2) nms.at(x, y) = m;
        }

    // 5. Double threshold + hysteresis
    float nmsMax = *std::max_element(nms.data.begin(), nms.data.end());
    float lo = lowT * nmsMax, hi = highT * nmsMax;
    GrayImage edges(img.w, img.h);
    const float STRONG = 1.0f, WEAK = 0.3f;
    for (int i = 0; i < img.w * img.h; i++) {
        if (nms.data[i] >= hi) edges.data[i] = STRONG;
        else if (nms.data[i] >= lo) edges.data[i] = WEAK;
    }
    for (int y = 1; y < img.h - 1; y++)
        for (int x = 1; x < img.w - 1; x++) {
            if (edges.at(x, y) == WEAK) {
                bool strong_neighbor = false;
                for (int dy = -1; dy <= 1 && !strong_neighbor; dy++)
                    for (int dx = -1; dx <= 1 && !strong_neighbor; dx++)
                        if (edges.at(x+dx, y+dy) == STRONG) strong_neighbor = true;
                edges.at(x, y) = strong_neighbor ? STRONG : 0.0f;
            }
        }
    return edges;
}
