/*
 * image_utils.cpp — Base64, image I/O, and overlay implementations.
 */

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "image_utils.h"

// ========== Base64 ==========

static const std::string B64_CHARS =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64_encode(const unsigned char* data, size_t len) {
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    unsigned int val = 0;
    int bits = -6;
    for (size_t i = 0; i < len; i++) {
        val = (val << 8) + data[i];
        bits += 8;
        while (bits >= 0) {
            out.push_back(B64_CHARS[(val >> bits) & 0x3F]);
            bits -= 6;
        }
    }
    if (bits > -6)
        out.push_back(B64_CHARS[((val << 8) >> (bits + 8)) & 0x3F]);
    while (out.size() % 4)
        out.push_back('=');
    return out;
}

std::vector<unsigned char> base64_decode(const std::string& in) {
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T[B64_CHARS[i]] = i;
    std::vector<unsigned char> out;
    int val = 0, bits = -8;
    for (unsigned char c : in) {
        if (T[c] == -1) continue;
        val = (val << 6) + T[c];
        bits += 6;
        if (bits >= 0) {
            out.push_back((val >> bits) & 0xFF);
            bits -= 8;
        }
    }
    return out;
}

// ========== Image I/O ==========

RGBImage load_image_from_memory(const unsigned char* buf, int len) {
    int w, h, ch;
    unsigned char* img = stbi_load_from_memory(buf, len, &w, &h, &ch, 3);
    RGBImage rgb(w, h);
    if (img) {
        memcpy(rgb.data.data(), img, w * h * 3);
        stbi_image_free(img);
    }
    return rgb;
}

GrayImage to_gray(const RGBImage& rgb) {
    GrayImage g(rgb.w, rgb.h);
    for (int i = 0; i < rgb.w * rgb.h; i++) {
        float r = rgb.data[i * 3] / 255.0f;
        float gr = rgb.data[i * 3 + 1] / 255.0f;
        float b = rgb.data[i * 3 + 2] / 255.0f;
        g.data[i] = 0.299f * r + 0.587f * gr + 0.114f * b;
    }
    return g;
}

RGBImage gray_to_rgb(const GrayImage& g) {
    RGBImage rgb(g.w, g.h);
    for (int i = 0; i < g.w * g.h; i++) {
        unsigned char v = (unsigned char)std::clamp(g.data[i] * 255.0f, 0.0f, 255.0f);
        rgb.data[i * 3] = rgb.data[i * 3 + 1] = rgb.data[i * 3 + 2] = v;
    }
    return rgb;
}

RGBImage resize_image(const RGBImage& img, int maxW, int maxH) {
    if (img.w <= maxW && img.h <= maxH) return img;
    float scale = std::min((float)maxW / img.w, (float)maxH / img.h);
    int nw = (int)(img.w * scale), nh = (int)(img.h * scale);
    if (nw < 1) nw = 1;
    if (nh < 1) nh = 1;
    RGBImage out(nw, nh);
    for (int y = 0; y < nh; y++)
        for (int x = 0; x < nw; x++) {
            int sx = std::min((int)(x / scale), img.w - 1);
            int sy = std::min((int)(y / scale), img.h - 1);
            memcpy(out.pixel(x, y), img.pixel(sx, sy), 3);
        }
    return out;
}

// ========== PNG encoding ==========

struct PngBuffer { std::vector<unsigned char> bytes; };

static void png_write_cb(void* ctx, void* data, int size) {
    auto* buf = (PngBuffer*)ctx;
    auto* src = (unsigned char*)data;
    buf->bytes.insert(buf->bytes.end(), src, src + size);
}

std::string gray_to_base64_png(const GrayImage& img) {
    std::vector<unsigned char> u8(img.w * img.h);
    for (int i = 0; i < img.w * img.h; i++)
        u8[i] = (unsigned char)std::clamp(img.data[i] * 255.0f, 0.0f, 255.0f);
    PngBuffer buf;
    stbi_write_png_to_func(png_write_cb, &buf, img.w, img.h, 1, u8.data(), img.w);
    return "data:image/png;base64," + base64_encode(buf.bytes.data(), buf.bytes.size());
}

std::string rgb_to_base64_png(const RGBImage& img) {
    PngBuffer buf;
    stbi_write_png_to_func(png_write_cb, &buf, img.w, img.h, 3, img.data.data(), img.w * 3);
    return "data:image/png;base64," + base64_encode(buf.bytes.data(), buf.bytes.size());
}

// ========== Overlays ==========

RGBImage overlay_points(const RGBImage& img,
                        const std::vector<std::pair<float,float>>& pts,
                        unsigned char r, unsigned char g, unsigned char b,
                        int radius) {
    RGBImage out = img;
    for (auto& pt : pts) {
        int xi = (int)roundf(pt.first), yi = (int)roundf(pt.second);
        for (int dy = -radius; dy <= radius; dy++)
            for (int dx = -radius; dx <= radius; dx++)
                if (dx*dx + dy*dy <= radius*radius)
                    out.set(xi + dx, yi + dy, r, g, b);
    }
    return out;
}
