#pragma once
/*
 * image_utils.h — Shared image types, base64, and I/O utilities.
 */

#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <cstring>

// ========== Grayscale image (float 0..1) ==========

struct GrayImage {
    int w = 0, h = 0;
    std::vector<float> data;
    GrayImage() = default;
    GrayImage(int w_, int h_) : w(w_), h(h_), data(w_ * h_, 0.0f) {}
    float& at(int x, int y) { return data[y * w + x]; }
    float  at(int x, int y) const { return data[y * w + x]; }
    float  safe(int x, int y) const {
        if (x < 0 || x >= w || y < 0 || y >= h) return 0.0f;
        return data[y * w + x];
    }
};

// ========== RGB image ==========

struct RGBImage {
    int w = 0, h = 0;
    std::vector<unsigned char> data; // r,g,b interleaved
    RGBImage() = default;
    RGBImage(int w_, int h_) : w(w_), h(h_), data(w_ * h_ * 3, 0) {}
    unsigned char* pixel(int x, int y) { return &data[(y * w + x) * 3]; }
    const unsigned char* pixel(int x, int y) const { return &data[(y * w + x) * 3]; }
    void set(int x, int y, unsigned char r, unsigned char g, unsigned char b) {
        if (x < 0 || x >= w || y < 0 || y >= h) return;
        auto* p = pixel(x, y);
        p[0] = r; p[1] = g; p[2] = b;
    }
};

// ========== Function declarations ==========

std::string base64_encode(const unsigned char* data, size_t len);
std::vector<unsigned char> base64_decode(const std::string& in);

RGBImage load_image_from_memory(const unsigned char* buf, int len);
GrayImage to_gray(const RGBImage& rgb);
RGBImage  gray_to_rgb(const GrayImage& g);
RGBImage  resize_image(const RGBImage& img, int maxW, int maxH);

std::string gray_to_base64_png(const GrayImage& img);
std::string rgb_to_base64_png(const RGBImage& img);

RGBImage overlay_points(const RGBImage& img,
                        const std::vector<std::pair<float,float>>& pts,
                        unsigned char r, unsigned char g, unsigned char b,
                        int radius = 3);

RGBImage overlay_connected_contour(const RGBImage& img,
                                    const std::vector<std::pair<float,float>>& pts,
                                    unsigned char r, unsigned char g, unsigned char b,
                                    int thickness = 1,
                                    bool show_points = true);
