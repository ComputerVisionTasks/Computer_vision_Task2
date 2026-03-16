/*
 * active_contour.cpp — Greedy Snake implementation (fixed).
 */

#include "active_contour.h"
#include "edge_detection.h"

// Gaussian blur helper for smoothing the external energy field
static GrayImage gaussian_blur_energy(const GrayImage& src, int ksize, float sigma) {
    GrayImage tmp(src.w, src.h), out(src.w, src.h);
    int half = ksize / 2;

    // Build 1-D kernel
    std::vector<float> k(ksize);
    float sum = 0;
    for (int i = 0; i < ksize; i++) {
        float x = i - half;
        k[i] = expf(-x * x / (2.0f * sigma * sigma));
        sum += k[i];
    }
    for (auto& v : k) v /= sum;

    // Horizontal pass
    for (int y = 0; y < src.h; y++)
        for (int x = 0; x < src.w; x++) {
            float acc = 0;
            for (int i = 0; i < ksize; i++)
                acc += k[i] * src.safe(x + i - half, y);
            tmp.at(x, y) = acc;
        }

    // Vertical pass
    for (int y = 0; y < src.h; y++)
        for (int x = 0; x < src.w; x++) {
            float acc = 0;
            for (int i = 0; i < ksize; i++)
                acc += k[i] * tmp.safe(x, y + i - half);
            out.at(x, y) = acc;
        }

    return out;
}

void Snake::init(const GrayImage& gray, float a, float b, float g) {
    alpha = a; beta = b; gamma = g;
    winSize = 7;   // FIX: larger window for better pull range
    maxIter = 100;

    // Build gradient magnitude
    GrayImage gx(gray.w, gray.h), gy(gray.w, gray.h);
    sobel(gray, gx, gy);

    GrayImage mag(gray.w, gray.h);
    float mx = 0;
    for (int i = 0; i < gray.w * gray.h; i++) {
        mag.data[i] = sqrtf(gx.data[i] * gx.data[i] + gy.data[i] * gy.data[i]);
        mx = std::max(mx, mag.data[i]);
    }
    if (mx > 0)
        for (auto& v : mag.data) v /= mx;  // normalize to [0, 1]

    // FIX: blur the gradient field so snake can "feel" edges from further away
    GrayImage blurred = gaussian_blur_energy(mag, 15, 3.0f);

    // FIX: store as negative (minimizer will be drawn TO edges, not away)
    extEnergy = GrayImage(gray.w, gray.h);
    for (int i = 0; i < gray.w * gray.h; i++)
        extEnergy.data[i] = -blurred.data[i];
}

void Snake::setPoints(const std::vector<std::pair<float, float>>& pts) {
    points = pts;
    original = pts;
}

std::vector<std::vector<std::pair<float, float>>> Snake::evolve(int iterations) {
    std::vector<std::vector<std::pair<float, float>>> history;
    history.push_back(points);

    int half = winSize / 2;
    int w = extEnergy.w, h = extEnergy.h;
    int n = (int)points.size();
    int iterLimit = (iterations > 0) ? iterations : maxIter;

    for (int iter = 0; iter < iterLimit; iter++) {
        float maxMove = 0;
        auto newPts = points;

        // FIX: compute average spacing once per iteration, used for
        // continuity — keeps points evenly distributed along contour
        float avgDist = 0;
        for (int i = 0; i < n; i++) {
            int j = (i + 1) % n;
            float dx = points[j].first  - points[i].first;
            float dy = points[j].second - points[i].second;
            avgDist += sqrtf(dx * dx + dy * dy);
        }
        avgDist /= n;

        for (int i = 0; i < n; i++) {
            int xi = (int)roundf(points[i].first);
            int yi = (int)roundf(points[i].second);
            float bestE = 1e30f;
            std::pair<float, float> bestPos = points[i];

            int prev = (i - 1 + n) % n;
            int next = (i + 1) % n;

            for (int dy = -half; dy <= half; dy++) {
                for (int dx = -half; dx <= half; dx++) {
                    int nx = xi + dx, ny = yi + dy;
                    if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;

                    // --- Continuity energy ---
                    // FIX: only penalize distance from THIS point to prev neighbor
                    // (not both prev and next — that double-counts and destabilizes)
                    float dpx = (float)nx - points[prev].first;
                    float dpy = (float)ny - points[prev].second;
                    float dPrev = sqrtf(dpx * dpx + dpy * dpy);
                    float cont = (dPrev - avgDist) * (dPrev - avgDist);

                    // --- Curvature energy ---
                    // Second-difference: prev - 2*candidate + next
                    float cx = points[prev].first  - 2.0f * (float)nx + points[next].first;
                    float cy = points[prev].second - 2.0f * (float)ny + points[next].second;
                    float curv = cx * cx + cy * cy;  // FIX: skip sqrt, squared is fine

                    // --- External (gradient) energy ---
                    float ext = extEnergy.safe(nx, ny);

                    // FIX: normalize each term so alpha/beta/gamma are
                    // truly comparable weights in [0,1] range
                    float total = alpha * cont / (avgDist * avgDist + 1e-6f)
                                + beta  * curv / (4.0f * avgDist * avgDist + 1e-6f)
                                + gamma * ext;

                    if (total < bestE) {
                        bestE = total;
                        bestPos = {(float)nx, (float)ny};
                    }
                }
            }

            float mvx = bestPos.first  - points[i].first;
            float mvy = bestPos.second - points[i].second;
            maxMove = std::max(maxMove, sqrtf(mvx * mvx + mvy * mvy));
            newPts[i] = bestPos;
        }

        points = newPts;
        history.push_back(points);

        // FIX: tighter convergence threshold
        if (maxMove < 0.5f) break;
    }

    return history;
}

void Snake::reset() {
    points = original;
}