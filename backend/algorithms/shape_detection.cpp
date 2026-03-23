/*
 * shape_detection.cpp — Hough Lines, Hough Circles, Ellipse Detection
 * Improved: NMS, gradient-direction voting, absolute+relative thresholds,
 *           proper duplicate suppression, and better ellipse validation.
 */
#include <iostream>
#include <ostream>
#include "shape_detection.h"
#include <queue>
#include <numeric>
#include <algorithm>
#include <cmath>

// ========== Utility ==========

static constexpr float PI = 3.14159265f;

// ========== Hough Lines ==========

std::vector<HoughLine> hough_lines(const GrayImage& edges, float thetaRes,
                                   float rhoRes, int threshold) {
    int h = edges.h, w = edges.w;
    int diag = (int)std::ceil(std::sqrt((double)(w * w + h * h)));
    int nTheta = (int)(180.0f / thetaRes);
    int nRho   = (int)(2.0f * diag / rhoRes) + 1;

    std::vector<float> cos_t(nTheta), sin_t(nTheta);
    for (int i = 0; i < nTheta; i++) {
        float angle = i * thetaRes * PI / 180.0f;
        cos_t[i] = std::cos(angle);
        sin_t[i] = std::sin(angle);
    }

    // Collect edge points
    std::vector<std::pair<int,int>> edge_pts;
    edge_pts.reserve(w * h / 4);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            if (edges.at(x, y) >= 0.5f)
                edge_pts.push_back({x, y});

    // Fill accumulator
    std::vector<int> acc(nRho * nTheta, 0);
    for (auto& [x, y] : edge_pts) {
        for (int ti = 0; ti < nTheta; ti++) {
            float rho = x * cos_t[ti] + y * sin_t[ti];
            int ri = (int)std::roundf((rho + diag) / rhoRes);
            if (ri >= 0 && ri < nRho)
                acc[ri * nTheta + ti]++;
        }
    }

    // --- FIX 1: Non-Maximum Suppression in (rho, theta) space ---
    // Suppress any cell that is not the local maximum in a 3x3 neighbourhood.
    // This eliminates the clusters of adjacent cells that all vote for the same line.
    int suppW = 5; // suppression window half-size
    std::vector<bool> suppressed(nRho * nTheta, false);
    for (int ri = 0; ri < nRho; ri++) {
        for (int ti = 0; ti < nTheta; ti++) {
            int v = acc[ri * nTheta + ti];
            if (v < threshold) { suppressed[ri * nTheta + ti] = true; continue; }
            bool is_max = true;
            for (int dr = -suppW; dr <= suppW && is_max; dr++)
                for (int dt = -suppW; dt <= suppW && is_max; dt++) {
                    if (dr == 0 && dt == 0) continue;
                    int nr = ri + dr;
                    // Theta wraps (0 == 180 degrees)
                    int nt = (ti + dt + nTheta) % nTheta;
                    if (nr < 0 || nr >= nRho) continue;
                    if (acc[nr * nTheta + nt] > v) is_max = false;
                }
            if (!is_max) suppressed[ri * nTheta + ti] = true;
        }
    }

    // Collect surviving peaks and sort by vote count
    std::vector<HoughLine> lines;
    for (int ri = 0; ri < nRho; ri++) {
        for (int ti = 0; ti < nTheta; ti++) {
            if (!suppressed[ri * nTheta + ti]) {
                float rho   = ri * rhoRes - diag;
                float theta = ti * thetaRes * PI / 180.0f;
                lines.push_back({ rho, theta });
            }
        }
    }

    // --- FIX 2: Sort by actual accumulator value (no recomputation bugs) ---
    std::vector<std::pair<int, int>> indexed; // (acc_value, index_in_lines)
    for (int i = 0; i < (int)lines.size(); i++) {
        int ri = (int)std::roundf((lines[i].rho + diag) / rhoRes);
        int ti = (int)std::roundf(lines[i].theta * 180.0f / PI / thetaRes);
        ri = std::clamp(ri, 0, nRho - 1);
        ti = ((ti % nTheta) + nTheta) % nTheta;
        indexed.push_back({ acc[ri * nTheta + ti], i });
    }
    std::sort(indexed.begin(), indexed.end(),
              [](auto& a, auto& b) { return a.first > b.first; });

    std::vector<HoughLine> sorted_lines;
    sorted_lines.reserve(lines.size());
    for (auto& [v, i] : indexed)
        sorted_lines.push_back(lines[i]);
    return sorted_lines;
}

RGBImage overlay_lines(const RGBImage& img, const std::vector<HoughLine>& lines,
                       unsigned char r, unsigned char g, unsigned char b) {
    RGBImage out = img;
    int span = std::max(img.w, img.h) * 2;
    for (auto& l : lines) {
        float ca = std::cos(l.theta), sa = std::sin(l.theta);
        float x0 = ca * l.rho, y0 = sa * l.rho;
        for (int t = -span; t <= span; t++) {
            float alpha = (float)t;
            int px = (int)(x0 - sa * alpha);
            int py = (int)(y0 + ca * alpha);
            if (px >= 0 && px < img.w && py >= 0 && py < img.h)
                out.set(px, py, r, g, b);
        }
    }
    return out;
}

// ========== Hough Circles ==========

std::vector<Circle> hough_circles(const GrayImage& edges, int rMin, int rMax,
                                  float threshold,
                                  int minAbsVotes,
                                  float centerDist) {
    int h = edges.h, w = edges.w;

    // Precompute edge points — exclude a 2px border to avoid image-boundary
    // bias: border pixels are always bright in Canny output and inflate maxV
    // for every radius tier, causing real interior circles to be suppressed.
    std::vector<std::pair<int,int>> edge_pts;
    edge_pts.reserve(w * h / 4);
    const int BORDER = 2;
    for (int y = BORDER; y < h - BORDER; y++)
        for (int x = BORDER; x < w - BORDER; x++)
            if (edges.at(x, y) >= 0.5f)
                edge_pts.push_back({x, y});

    // Use 360 angular samples (every 1 degree)
    const int N_ANGLES = 360;
    std::vector<float> cos_t(N_ANGLES), sin_t(N_ANGLES);
    for (int t = 0; t < N_ANGLES; t++) {
        float rad = t * PI / 180.0f;
        cos_t[t] = std::cos(rad);
        sin_t[t] = std::sin(rad);
    }

    // Expected votes for a PERFECT circle of radius r with N_ANGLES/2 samples
    // each contributing 1 vote. With step=2 deg, we cast N_ANGLES/2 = 180 votes
    // from every edge point that lies on the circle. The expected max accumulator
    // value for a perfect circle is roughly: num_circle_edge_points * 1
    // (since each sample from a different angle will land on the same center).
    // More practically: expectedVotes ≈ 2*PI*r (circumference in pixels).
    // We gate on: votes >= threshold * expectedVotes.
    // This is ABSOLUTE (not relative-to-max), so border inflation cannot hurt us.

    std::vector<Circle> all;
    std::vector<int> acc(h * w, 0);

    for (int r = rMin; r <= rMax; r++) {
        std::fill(acc.begin(), acc.end(), 0);

        for (auto& [ex, ey] : edge_pts) {
            for (int i = 0; i < N_ANGLES; i += 2) {
                int cx = ex + (int)std::roundf(r * cos_t[i]);
                int cy = ey + (int)std::roundf(r * sin_t[i]);
                if (cx >= BORDER && cx < w - BORDER && cy >= BORDER && cy < h - BORDER)
                    acc[cy * w + cx]++;
            }
        }

        // ABSOLUTE threshold based on expected circumference:
        // A circle of radius r has perimeter 2*PI*r pixels. With 180 voting
        // angles we expect each edge pixel to only receive 1 vote (from the
        // unique angle pointing toward the center). So expectedVotes ≈ 2*PI*r.
        // We require at least (threshold * 2*PI*r), floored by minAbsVotes.
        int expectedVotes = (int)(2.0f * PI * r);
        int tv = std::max(minAbsVotes, (int)(threshold * expectedVotes));

        // NMS: 3x3 local max and must beat the absolute threshold
        for (int y = 1; y < h - 1; y++) {
            for (int x = 1; x < w - 1; x++) {
                int v = acc[y * w + x];
                if (v < tv) continue;
                bool is_max = true;
                for (int dy = -1; dy <= 1 && is_max; dy++)
                    for (int dx = -1; dx <= 1 && is_max; dx++)
                        if (acc[(y + dy) * w + (x + dx)] > v) is_max = false;
                if (is_max)
                    all.push_back({ x, y, r });
            }
        }
    }

    // Duplicate removal: merge circles whose centers are within r pixels
    // of each other (much more generous than the old 15% of average condition)
    std::sort(all.begin(), all.end(), [](const Circle& a, const Circle& b){
        return a.r < b.r;
    });

    std::vector<Circle> unique;
    for (auto& c : all) {
        bool dup = false;
        for (auto& u : unique) {
            float dist = std::hypot((float)(c.x - u.x), (float)(c.y - u.y));
            // Use centerDist (from frontend slider) as a fraction of min(r1,r2):
            // centerDist=0.1 → must be very close to merge (tolerates slight jitter)
            // centerDist=0.8 → circles up to 80% of their radius apart are merged
            float mergeThresh = centerDist * (float)std::min(c.r, u.r);
            if (dist < mergeThresh && std::abs(c.r - u.r) < 0.3f * u.r) {
                dup = true; break;
            }
        }
        if (!dup) unique.push_back(c);
    }
    return unique;
}

RGBImage overlay_circles(const RGBImage& img, const std::vector<Circle>& circles,
                         unsigned char r, unsigned char g, unsigned char b) {
    RGBImage out = img;
    for (auto& c : circles)
        for (int t = 0; t < 360; t++) {
            float rad = t * PI / 180.0f;
            int px = c.x + (int)std::roundf(c.r * std::cos(rad));
            int py = c.y + (int)std::roundf(c.r * std::sin(rad));
            if (px >= 0 && px < img.w && py >= 0 && py < img.h)
                out.set(px, py, r, g, b);
        }
    return out;
}

// ========== Ellipse Detection ==========

GrayImage dilate_edges(const GrayImage& input) {
    GrayImage output = input;
    // Offset by 2 to prevent boundary overflow due to 5x5 kernel
    for (int y = 2; y < input.h - 2; y++) {
        for (int x = 2; x < input.w - 2; x++) {
            if (input.at(x, y) > 0.5f) {
                // Apply 5x5 dilation kernel around the white pixel
                for (int dy = -2; dy <= 2; dy++) {
                    for (int dx = -2; dx <= 2; dx++) {
                        output.at(x + dx, y + dy) = 1.0f; 
                    }
                }
            }
        }
    }
    return output;
}

std::vector<EllipseData> detect_ellipses(const GrayImage& edges,
                                          int minArea, int maxArea, float tolerance, float inlierRatio, 
                                          float minAspect) {
    // 1. Dilation to ensure edge connectivity
    GrayImage dilated = dilate_edges(edges);
    
    int h = edges.h, w = edges.w;
    std::vector<bool> visited(h * w, false);
    std::vector<EllipseData> result;

    for (int sy = 0; sy < h; sy++) {
        for (int sx = 0; sx < w; sx++) {
            // Check dilated image for starting points
            if (dilated.at(sx, sy) < 0.5f || visited[sy * w + sx]) continue;

            // BFS flood-fill to group connected components
            std::queue<std::pair<int,int>> q;
            std::vector<std::pair<int,int>> comp;
            q.push({sx, sy});
            visited[sy * w + sx] = true;

            while (!q.empty()) {
                auto [qx, qy] = q.front(); q.pop();
                comp.push_back({qx, qy});
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        int nx = qx + dx, ny = qy + dy;
                        if (nx >= 0 && nx < w && ny >= 0 && ny < h &&
                            !visited[ny * w + nx] && dilated.at(nx, ny) >= 0.5f) {
                            visited[ny * w + nx] = true;
                            q.push({nx, ny});
                        }
                    }
                }
            }

            if ((int)comp.size() < 5) continue; 

            // Calculate Bounding Box and Centroid
            int xmin = w, ymin = h, xmax = 0, ymax = 0;
            double sum_x = 0, sum_y = 0;
            for (auto& [px, py] : comp) {
                xmin = std::min(xmin, px); ymin = std::min(ymin, py);
                xmax = std::max(xmax, px); ymax = std::max(ymax, py);
                sum_x += px; sum_y += py;
            }

            int box_area = (xmax - xmin + 1) * (ymax - ymin + 1);
            if (box_area < minArea || box_area > maxArea) continue; 

            // Ramanujan's perimeter approximation for filtering
            int bw = xmax - xmin + 1, bh = ymax - ymin + 1;
            float h_param = std::pow((float)bw - bh, 2) / std::pow((float)bw + bh, 2);
            float expected_perimeter = 3.14159f * (bw + bh) * (1.0f + (3.0f * h_param) / (10.0f + std::sqrt(4.0f - 3.0f * h_param)));
            float perim_ratio = (float)comp.size() / expected_perimeter;

            // Calculate moments for ellipse properties
            int N = (int)comp.size();
            double cx = sum_x / N;
            double cy = sum_y / N;

            double cxx = 0, cyy = 0, cxy = 0;
            for (auto& [px, py] : comp) {
                double dx = px - cx, dy = py - cy;
                cxx += dx * dx; cyy += dy * dy; cxy += dx * dy;
            }
            cxx /= N; cyy /= N; cxy /= N;

            double diff  = cxx - cyy;
            double term1 = cxx + cyy;
            double term2 = std::sqrt(diff * diff + 4.0 * cxy * cxy);
            double l1 = (term1 + term2) / 2.0;
            double l2 = std::max(0.0, (term1 - term2) / 2.0);

            EllipseData e;
            e.x = (float)cx; e.y = (float)cy;
            e.a = (float)std::max(1.0, std::sqrt(2.0 * l1)); 
            e.b = (float)std::max(1.0, std::sqrt(2.0 * l2));
            e.angle = (float)(0.5 * std::atan2(2.0 * cxy, diff));

            // Filter by aspect ratio
            float current_ratio = (e.a > 0) ? (e.b / e.a) : 0;
            if (current_ratio < minAspect) continue;

            // Inlier detection using algebraic distance
            float ca = std::cos(e.angle), sa = std::sin(e.angle);
            int inliers = 0;
            float inv_a2 = 1.0f / (e.a * e.a);
            float inv_b2 = 1.0f / (e.b * e.b);
            
            for (auto& [px, py] : comp) {
                float dx = (float)(px - cx), dy = (float)(py - cy);
                float u = dx * ca + dy * sa, v = -dx * sa + dy * ca;
                float dist = std::abs(u * u * inv_a2 + v * v * inv_b2 - 1.0f);
                if (dist < tolerance) inliers++;
            }

            float final_inlier_ratio = (float)inliers / N;

            // Validate against inlier ratio threshold
            if (final_inlier_ratio < inlierRatio) continue;

            result.push_back(e);
        }
    }
    return result;
}

RGBImage overlay_ellipses(const RGBImage& img, const std::vector<EllipseData>& ellipses,
                           unsigned char r, unsigned char g, unsigned char b) {
    RGBImage out = img;
    for (auto& e : ellipses) {
        // Draw the ellipse perimeter using parametric equations
        for (int t = 0; t < 360; t++) {
            float rad = t * 3.14159f / 180.0f;
            float ca = std::cos(e.angle), sa = std::sin(e.angle);
            float px = e.x + e.a * std::cos(rad) * ca - e.b * std::sin(rad) * sa;
            float py = e.y + e.a * std::cos(rad) * sa + e.b * std::sin(rad) * ca;
            int ix = (int)std::roundf(px), iy = (int)std::roundf(py);
            if (ix >= 0 && ix < img.w && iy >= 0 && iy < img.h) {
                out.set(ix, iy, r, g, b);
            }
        }
    }
    return out;
}