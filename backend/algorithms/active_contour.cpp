/*
 * active_contour.cpp — Greedy Snake implementation.
 * Uses internal energy (continuity + curvature) and external energy (gradient).
 */

#include "active_contour.h"
#include "edge_detection.h" // for sobel()

void Snake::init(const GrayImage& gray, float a, float b, float g) {
    alpha = a; beta = b; gamma = g;
    winSize = 5; maxIter = 100;

    // External energy = negative gradient magnitude
    GrayImage gx, gy;
    sobel(gray, gx, gy);
    extEnergy = GrayImage(gray.w, gray.h);
    float mx = 0;
    for (int i = 0; i < gray.w * gray.h; i++) {
        extEnergy.data[i] = sqrtf(gx.data[i]*gx.data[i] + gy.data[i]*gy.data[i]);
        mx = std::max(mx, extEnergy.data[i]);
    }
    if (mx > 0) for (auto& v : extEnergy.data) v /= mx;
    for (auto& v : extEnergy.data) v = -v;
}

void Snake::setPoints(const std::vector<std::pair<float,float>>& pts) {
    points = pts;
    original = pts;
}

std::vector<std::vector<std::pair<float,float>>> Snake::evolve() {
    std::vector<std::vector<std::pair<float,float>>> history;
    history.push_back(points);
    int half = winSize / 2;
    int w = extEnergy.w, h = extEnergy.h;
    int n = (int)points.size();

    for (int iter = 0; iter < maxIter; iter++) {
        float maxMove = 0;
        auto newPts = points;

        // Average spacing
        float avgDist = 0;
        for (int i = 0; i < n; i++) {
            int j = (i + 1) % n;
            float dx = points[j].first - points[i].first;
            float dy = points[j].second - points[i].second;
            avgDist += sqrtf(dx*dx + dy*dy);
        }
        avgDist /= n;

        for (int i = 0; i < n; i++) {
            int xi = (int)roundf(points[i].first);
            int yi = (int)roundf(points[i].second);
            float bestE = 1e30f;
            std::pair<float,float> bestPos = points[i];
            int prev = (i - 1 + n) % n, next = (i + 1) % n;

            for (int dy = -half; dy <= half; dy++)
                for (int dx = -half; dx <= half; dx++) {
                    int nx = xi + dx, ny = yi + dy;
                    if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;

                    // Continuity energy
                    float dpx = nx - newPts[prev].first, dpy = ny - newPts[prev].second;
                    float dPrev = sqrtf(dpx*dpx + dpy*dpy);
                    float dnx = newPts[next].first - nx, dny = newPts[next].second - ny;
                    float dNext = sqrtf(dnx*dnx + dny*dny);
                    float cont = fabsf(avgDist - dPrev) + fabsf(avgDist - dNext);

                    // Curvature energy
                    float curv = sqrtf((dpx+dnx)*(dpx+dnx) + (dpy+dny)*(dpy+dny));

                    // External energy
                    float ext = extEnergy.safe(nx, ny);

                    float total = alpha * cont + beta * curv + gamma * ext;
                    if (total < bestE) { bestE = total; bestPos = {(float)nx, (float)ny}; }
                }
            float mvx = bestPos.first - points[i].first;
            float mvy = bestPos.second - points[i].second;
            maxMove = std::max(maxMove, sqrtf(mvx*mvx + mvy*mvy));
            newPts[i] = bestPos;
        }
        points = newPts;
        history.push_back(points);
        if (maxMove < 0.1f) break;
    }
    return history;
}

void Snake::reset() { points = original; }
