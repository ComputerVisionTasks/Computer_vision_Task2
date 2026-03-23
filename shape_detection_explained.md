# Shape Detection — Algorithm Deep Dive

> This document explains the **Hough Lines** and **Hough Circles** algorithms implemented from scratch in `backend/algorithms/shape_detection.cpp`, walking through the code **line by line**, and explaining every frontend parameter and the mathematical effect it has on detection results.

---

## Table of Contents

- [Part 1 — Hough Line Detection](#part-1--hough-line-detection)
  - [The Idea: Hough Space](#the-idea-hough-space)
  - [Code Walkthrough](#code-walkthrough---hough-lines)
  - [Frontend Parameters](#frontend-parameters---lines)
- [Part 2 — Hough Circle Detection](#part-2--hough-circle-detection)
  - [The Idea: 3D Accumulator → Radius Slices](#the-idea-3d-accumulator--radius-slices)
  - [Code Walkthrough](#code-walkthrough---hough-circles)
  - [Frontend Parameters](#frontend-parameters---circles)
- [Visual Parameter Guide](#visual-parameter-guide)

---

## Part 1 — Hough Line Detection

### The Idea: Hough Space

A line in image space can be described by two numbers: **ρ (rho)** and **θ (theta)**.

```
x·cos(θ) + y·sin(θ) = ρ
```

- **θ** = angle of the line's normal vector (0° to 180°)
- **ρ** = perpendicular distance from the origin to the line (can be negative)

Every **edge pixel** (x, y) votes for ALL possible lines that could pass through it. In (ρ, θ) space, those votes form a sinusoidal curve. Where many sinusoids intersect → many edge pixels agree → a real line exists there.

```
Image Space              Hough Space (ρ, θ)
                          ρ
   edge pixel (x,y)  →   │   ╱╲   ← sinusoid
                          │  ╱  ╲
                          │ ╱    ╲
                          └──────── θ
```

---

### Code Walkthrough — Hough Lines

```cpp
std::vector<HoughLine> hough_lines(const GrayImage& edges,
                                   float thetaRes,   // angular step (degrees)
                                   float rhoRes,     // rho quantization (pixels)
                                   int threshold)    // min votes to be a line
{
```
**Function signature.** Takes a binary edge image (output of Canny) and 3 parameters directly from the frontend sliders.

---

```cpp
    int h = edges.h, w = edges.w;
    int diag = (int)std::ceil(std::sqrt((double)(w * w + h * h)));
```
**`diag`** = the diagonal of the image in pixels. This is the **maximum possible value of ρ** — a line can be at most `diag` pixels away from the origin. ρ ranges from `-diag` to `+diag`.

---

```cpp
    int nTheta = (int)(180.0f / thetaRes);
    int nRho   = (int)(2.0f * diag / rhoRes) + 1;
```
- **`nTheta`** = number of columns in the accumulator = how many distinct angles to test.
  - Default `thetaRes = 1.0°` → `nTheta = 180` columns
  - Lower thetaRes → more columns → finer angular resolution → larger memory
- **`nRho`** = number of rows in the accumulator.
  - Default `rhoRes = 1.0` px → one row per pixel of distance

---

```cpp
    std::vector<float> cos_t(nTheta), sin_t(nTheta);
    for (int i = 0; i < nTheta; i++) {
        float angle = i * thetaRes * PI / 180.0f;
        cos_t[i] = std::cos(angle);
        sin_t[i] = std::sin(angle);
    }
```
**Pre-compute sine and cosine** for every angle in the accumulator. This avoids recomputing `cos/sin` for every pixel × every angle (would be millions of calls). Stored once and reused.

---

```cpp
    std::vector<std::pair<int,int>> edge_pts;
    edge_pts.reserve(w * h / 4);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            if (edges.at(x, y) >= 0.5f)
                edge_pts.push_back({x, y});
```
**Collect all edge pixels** into a vector. `edges.at(x,y) >= 0.5f` means "this pixel is an edge" (Canny output is 0 or 1). Only these pixels vote — blank pixels never vote. The `.reserve()` pre-allocates memory to avoid repeated resizing.

---

```cpp
    std::vector<int> acc(nRho * nTheta, 0);
    for (auto& [x, y] : edge_pts) {
        for (int ti = 0; ti < nTheta; ti++) {
            float rho = x * cos_t[ti] + y * sin_t[ti];
            int ri = (int)std::roundf((rho + diag) / rhoRes);
            if (ri >= 0 && ri < nRho)
                acc[ri * nTheta + ti]++;
        }
    }
```
**The core voting loop.** For every edge pixel `(x, y)` and every angle `ti`:
1. Compute `ρ = x·cos(θ) + y·sin(θ)` — the distance from origin for the line at angle θ passing through (x,y)
2. Convert ρ to a row index `ri` by shifting by `diag` (so ρ = -diag maps to row 0) and dividing by `rhoRes`
3. Increment `acc[ri][ti]` — cast one vote for "a line at angle θ and distance ρ"

After this loop, every cell in `acc` contains the number of edge pixels that agree on that particular line.

---

```cpp
    int suppW = 5;
    std::vector<bool> suppressed(nRho * nTheta, false);
    for (int ri = 0; ri < nRho; ri++) {
        for (int ti = 0; ti < nTheta; ti++) {
            int v = acc[ri * nTheta + ti];
            if (v < threshold) { suppressed[ri * nTheta + ti] = true; continue; }
```
**Step 1 of peak detection.** Immediately suppress any cell below the `threshold`. This is the most important filter — cells with fewer votes than the threshold are discarded entirely.

- **High threshold (150–200):** Only the absolute strongest lines survive. Good for images with 1–3 clean dominant lines (roads, grid lines).
- **Low threshold (10–30):** Many weaker lines also survive. Good for complex images with many faint lines.

---

```cpp
            bool is_max = true;
            for (int dr = -suppW; dr <= suppW && is_max; dr++)
                for (int dt = -suppW; dt <= suppW && is_max; dt++) {
                    if (dr == 0 && dt == 0) continue;
                    int nr = ri + dr;
                    int nt = (ti + dt + nTheta) % nTheta;
                    if (nr < 0 || nr >= nRho) continue;
                    if (acc[nr * nTheta + nt] > v) is_max = false;
                }
            if (!is_max) suppressed[ri * nTheta + ti] = true;
        }
    }
```
**Non-Maximum Suppression (NMS) in a 11×11 window** (`suppW=5` → half-size → full window is `2×5+1 = 11`). A cell only survives if it is the **strict local maximum** in its entire 11×11 neighbourhood in (ρ,θ) space.

This solves the **cluster problem**: a single physical line generates a blob of high votes around its true (ρ, θ), not just one single cell. NMS ensures only the peak of each blob survives, so one physical line = one detected line.

Note `nt = (ti + dt + nTheta) % nTheta` — theta wraps around: 0° and 180° are the same direction.

---

```cpp
    std::vector<HoughLine> lines;
    for (int ri = 0; ri < nRho; ri++)
        for (int ti = 0; ti < nTheta; ti++)
            if (!suppressed[ri * nTheta + ti]) {
                float rho   = ri * rhoRes - diag;
                float theta = ti * thetaRes * PI / 180.0f;
                lines.push_back({ rho, theta });
            }
```
**Convert surviving accumulator indices back to (ρ, θ) values** in physical units:
- `rho = ri × rhoRes - diag` undoes the indexing shift
- `theta = ti × thetaRes × π/180` converts from an integer index to radians

---

```cpp
    std::vector<std::pair<int, int>> indexed;
    for (int i = 0; i < (int)lines.size(); i++) {
        int ri = ...; int ti = ...;
        indexed.push_back({ acc[ri * nTheta + ti], i });
    }
    std::sort(indexed.begin(), indexed.end(),
              [](auto& a, auto& b) { return a.first > b.first; });

    std::vector<HoughLine> sorted_lines;
    for (auto& [v, i] : indexed)
        sorted_lines.push_back(lines[i]);
    return sorted_lines;
```
**Sort surviving lines by vote count (strongest first).** This re-looks up the actual accumulator value for each surviving line, pairs it with the line index, sorts descending, then rebuilds the output vector. Lines with more votes are more confident detections and come first.

---

### Code Walkthrough — Overlay

```cpp
RGBImage overlay_lines(const RGBImage& img, const std::vector<HoughLine>& lines,
                       unsigned char r, unsigned char g, unsigned char b) {
    int span = std::max(img.w, img.h) * 2;
    for (auto& l : lines) {
        float ca = std::cos(l.theta), sa = std::sin(l.theta);
        float x0 = ca * l.rho, y0 = sa * l.rho;         // closest point to origin
        for (int t = -span; t <= span; t++) {
            int px = (int)(x0 - sa * t);
            int py = (int)(y0 + ca * t);
            if (px >= 0 && px < img.w && py >= 0 && py < img.h)
                out.set(px, py, r, g, b);
        }
    }
}
```
Draws each line as an **infinite line** across the image. Starting from `(x0, y0)` (the point on the line closest to the origin), it steps along the perpendicular direction `(-sin θ, cos θ)` by `t` pixels in both directions (up to `2×max(w,h)` pixels), coloring every pixel that lands inside the image.

---

### Frontend Parameters — Lines

| Parameter | Slider ID | Range | Default | Effect on Algorithm |
|---|---|---|---|---|
| **Hough Threshold** | `lineThreshold` | 10 – 200 | **50** | Minimum votes for a line to survive. Higher = fewer but stronger lines. Lower = more lines including weak ones. |
| **Theta Resolution** | `thetaRes` | 0.5° – 5.0° | **1.0°** | Angular step size. `1.0°` → 180 angle bins. Smaller value = finer angle discrimination but slower and uses more memory. Use `0.5°` for near-parallel lines you want to distinguish. Use `5.0°` for speed when angle precision doesn't matter. |
| **Rho Resolution** | `rhoRes` | 0.5 – 3.0 px | **1.0** | Distance quantization step. `1.0` → one bin per pixel of distance. Smaller = finer but larger accumulator. Larger = faster but merges nearby-parallel lines into the same bin. |

#### What each parameter does visually

```
Low Threshold (10–25):        High Threshold (150–200):
Many lines detected,           Only 1–3 dominant lines,
including faint background     very clean output
edges.                         May miss real lines.

Small thetaRes (0.5°):        Large thetaRes (5.0°):
Can tell apart lines at        Lines at 3° apart treated
0.5° angle difference.         as same direction.
Slower, uses ~10× RAM.         Fast, coarse.

Small rhoRes (0.5):           Large rhoRes (3.0):
Lines 0.5px apart treated      Lines 3px apart merged.
as distinct.                   Good for thick/blurry edges.
```

---

## Part 2 — Hough Circle Detection

### The Idea: 3D Accumulator → Radius Slices

A circle in image space needs **3 parameters**: center `(a, b)` and radius `r`.

```
(x - a)² + (y - b)² = r²
```

The naïve approach would be a 3D accumulator `acc[a][b][r]` — extremely expensive. Instead, we run a **separate 2D accumulator for each radius tier:**

1. For each radius `r` in `[rMin, rMax]`:
   - For each edge pixel `(x, y)`:
     - Vote for all centers `(cx, cy)` at exactly distance `r` from `(x, y)`
   - Find peaks in this 2D accumulator = circles of radius `r`

This converts the O(R × W × H × N_ANGLES) problem into O(R) separate O(W × H × N_ANGLES) passes.

---

### Code Walkthrough — Hough Circles

```cpp
std::vector<Circle> hough_circles(const GrayImage& edges,
                                  int rMin, int rMax,    // radius search range
                                  float threshold,       // fraction 0..1
                                  int minAbsVotes)       // hard floor
{
```
**Function signature.** Four parameters — two come directly from frontend sliders, two from sliders as well.

---

```cpp
    const int BORDER = 2;
    for (int y = BORDER; y < h - BORDER; y++)
        for (int x = BORDER; x < w - BORDER; x++)
            if (edges.at(x, y) >= 0.5f)
                edge_pts.push_back({x, y});
```
**Collect edge pixels, skipping a 2-pixel border.** Image borders are always bright in Canny output (JPEG compression artifacts, image frame edges). Without this fix, border pixels would dominate the accumulator and make the algorithm detect fake circles near the image corners instead of real interior shapes.

---

```cpp
    const int N_ANGLES = 360;
    std::vector<float> cos_t(N_ANGLES), sin_t(N_ANGLES);
    for (int t = 0; t < N_ANGLES; t++) {
        float rad = t * PI / 180.0f;
        cos_t[t] = std::cos(rad); sin_t[t] = std::sin(rad);
    }
```
**Pre-compute 360 angles** (every 1°). Same optimization as Hough Lines — avoids re-computing cos/sin millions of times.

---

```cpp
    for (int r = rMin; r <= rMax; r++) {
        std::fill(acc.begin(), acc.end(), 0);
```
**Outer loop: iterate over every radius** in the search range `[rMin, rMax]`. The accumulator is **reset to 0** at the start of each radius tier. Each pass treats only circles of radius `r`.

- **Wide range (rMin=5, rMax=300):** Detects circles of any size, but takes proportionally longer (one pass per pixel in the range).
- **Narrow range (rMin=40, rMax=60):** Only detects coins/circles of that specific size — much faster and fewer false positives.

---

```cpp
        for (auto& [ex, ey] : edge_pts) {
            for (int i = 0; i < N_ANGLES; i += 2) {
                int cx = ex + (int)std::roundf(r * cos_t[i]);
                int cy = ey + (int)std::roundf(r * sin_t[i]);
                if (cx >= BORDER && cx < w - BORDER && cy >= BORDER && cy < h - BORDER)
                    acc[cy * w + cx]++;
            }
        }
```
**The core voting loop.** For each edge pixel `(ex, ey)`:
- Step around a circle of radius `r` centered at `(ex, ey)` in 180 steps (every 2° of 360)
- Each step computes a candidate center `(cx, cy) = (ex + r·cos, ey + r·sin)`
- Increment `acc[cy][cx]`

The **intuition**: if there really is a circle of radius `r` centered at `(cx, cy)`, then ALL edge pixels on that circle's perimeter will independently vote for `(cx, cy)`. The center cell accumulates `≈ 2πr` votes (one from each pixel of the circumference).

The `step = 2` (every 2° instead of 1°) halves computation while still providing 180 votes from a full circle.

Center candidates within the 2-pixel border are also excluded — prevents voting for off-screen centers.

---

```cpp
        int expectedVotes = (int)(2.0f * PI * r);
        int tv = std::max(minAbsVotes, (int)(threshold * expectedVotes));
```
**The threshold calculation — the most critical fix.**

A perfect circle of radius `r` has circumference `2πr` pixels. If all those pixels are edge pixels, the center gets `≈ 2πr/2 = πr` votes (because we step every 2° = half of 360 samples).

The threshold is:
```
tv = max(minAbsVotes, threshold × 2πr)
```

This means:
- For `r = 10`: `tv = max(20, 0.55 × 63) = max(20, 34) = 34` votes required
- For `r = 50`: `tv = max(20, 0.55 × 314) = max(20, 173) = 173` votes required
- For `r = 100`: `tv = max(20, 0.55 × 628) = max(20, 345) = 345` votes required

This **scales naturally with circle size** — small circles need fewer votes, large circles need more, but both need roughly the same **fraction of their circumference** to be covered by edges.

**Why not use `threshold × maxV` (old approach)?** The maximum vote in any radius tier is dominated by wherever the most edge pixels coincidentally point — often an image corner or a textured background region — not an actual circle center. Using the circumference-based expected value instead means a 30% arc coverage (`threshold=0.55 with ~55% of 2πr`) reliably detects real circles regardless of what else is in the image.

---

```cpp
        for (int y = 1; y < h - 1; y++)
            for (int x = 1; x < w - 1; x++) {
                int v = acc[y * w + x];
                if (v < tv) continue;
                bool is_max = true;
                for (int dy = -1; dy <= 1 && is_max; dy++)
                    for (int dx = -1; dx <= 1 && is_max; dx++)
                        if (acc[(y+dy)*w+(x+dx)] > v) is_max = false;
                if (is_max)
                    all.push_back({ x, y, r });
            }
```
**3×3 Non-Maximum Suppression** per radius tier. Among all cells that beat the threshold, only cells that are the strict local maximum in their 3×3 neighbourhood are kept. This prevents the "bullseye" effect where a single circle center generates 9 adjacent winning cells.

---

```cpp
    std::sort(all.begin(), all.end(), [](const Circle& a, const Circle& b){
        return a.r < b.r;
    });

    std::vector<Circle> unique;
    for (auto& c : all) {
        bool dup = false;
        for (auto& u : unique) {
            float dist = std::hypot((float)(c.x - u.x), (float)(c.y - u.y));
            if (dist < (float)std::min(c.r, u.r) && std::abs(c.r - u.r) < 0.3f * u.r) {
                dup = true; break;
            }
        }
        if (!dup) unique.push_back(c);
    }
    return unique;
```
**Duplicate removal across all radius tiers.** After all radii are processed, there may be multiple near-identical circles (e.g., the same coin detected at r=48, r=49, r=50). Two circles are merged if:
1. Their centers are within `min(r1, r2)` pixels of each other (overlapping centers)
2. Their radii differ by less than 30% (`|r1 - r2| < 0.3 × u.r`)

Sorting by radius first ensures we always keep the smaller-radius version when two circles conflict — smaller circles from earlier tiers typically have more precise centers.

---

### Frontend Parameters — Circles

| Parameter | Slider ID | Range | Default | Effect on Algorithm |
|---|---|---|---|---|
| **Min Radius** | `minRadius` | 5 – 100 px | **10** | Start of the radius search range. Set higher to skip detecting tiny dot-like circles. Setting this too low on a complex image → slow (many radius passes) and many small false positives. |
| **Max Radius** | `maxRadius` | 20 – 300 px | **100** | End of the radius search range. Set to roughly the size of the largest circle you expect. Setting too high → slow (many extra passes) with no benefit if no large circles exist. |
| **Threshold (Relative)** | `circleThreshold` | 0.1 – 1.0 | **0.55** | Fraction of the expected circumference that must be covered by edges. `0.55` = 55% of `2πr` votes required. Lower = more circles detected (including partial arcs), Higher = only nearly-complete circles. |
| **Min Absolute Votes** | `minVotes` | 5 – 100 | **20** | Hard floor for the vote count, independent of radius. Prevents tiny circles (where `threshold × 2πr` would be very small) from being accepted with only a handful of votes. Acts like a noise gate. |

#### What each parameter does visually

```
Min/Max Radius defines the SEARCH WINDOW:

minRadius=5, maxRadius=300:    minRadius=40, maxRadius=60:
Finds ALL circles from tiny    Only finds circles ~40–60px
dots to full-image circles.    radius (e.g. coins).
Very slow on large images.     Fast and very specific.

Threshold effect:

threshold=0.2 (low):          threshold=0.9 (high):
Detects partial arcs,          Only full, clean circles.
semicircles, edges of          Misses occluded/partial
large objects.                 circles.
Many false positives.          Very few detections.

Min Absolute Votes:

minVotes=5 (low):             minVotes=80 (high):
Even a few coincident          Requires strong evidence.
edge points can trigger        Tiny r circles need a
a circle detection.            near-complete arc.
Noisy on textured images.
```

---

## Visual Parameter Guide

### Choosing parameters for common scenarios

| Scenario | Recommended Settings |
|---|---|
| **Detecting lane lines in a road image** | Threshold: 80+, thetaRes: 1.0°, rhoRes: 1.0 |
| **Detecting a grid (many parallel lines)** | Threshold: 30–50, thetaRes: 1.0°, rhoRes: 1.0 |
| **Finding 1–2 dominant edges only** | Threshold: 150+, suppression window (hardcoded 5) handles clusters |
| **Coins of similar size** | minRadius: coin_px × 0.8, maxRadius: coin_px × 1.2, threshold: 0.5 |
| **Circles of unknown size** | minRadius: 10, maxRadius: 200, threshold: 0.5, minVotes: 20 |
| **Noisy image → fewer false circles** | Increase threshold to 0.7+, increase minVotes to 40+ |
| **Image with partial circles** | Decrease threshold to 0.3–0.4 |

### Parameter Interaction Summary

```
Lines:

  [Edge Image] ──→ [Accumulator Vote] ──→ [Threshold Filter] ──→ [NMS 11×11] ──→ [Sort by votes]
                                                  ↑
                                           lineThreshold
                          ↑                     ↑
                    thetaRes controls     rhoRes controls
                    number of columns   number of rows
                    in accumulator      in accumulator

Circles:

  [Edge Image, skip border] ──→ [Per-radius Vote Loop]
                                  ↑              ↑
                               minRadius      maxRadius

  ──→ [Absolute threshold] ──→ [NMS 3×3] ──→ [Duplicate merge] ──→ [Return]
        ↑            ↑
   minVotes    circleThreshold × 2πr
```

---

*File: `shape_detection_explained.md` — part of the FromScratchCV project documentation.*
