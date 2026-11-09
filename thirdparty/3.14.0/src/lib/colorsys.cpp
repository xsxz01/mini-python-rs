#include "../cffi.hpp"

#include <cmath>
#include <string>
#include <vector>

namespace minipy {

namespace {

// 构造三元浮点组
// Build a three-float tuple
// ᠭᠤᠷᠪᠠᠨ ᠦᠪᠳᠡᠷᠢᠶ᠎ᠡ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
value makeTriple(cffi& a, double x, double y, double z){
    std::vector<value> items = {a.mkfloat(x), a.mkfloat(y), a.mkfloat(z)};
    return a.vm->makeTuple(items);
}

// 浮点夹取到 [0,1]
// Clamp a float into [0,1]
// ᠦᠪᠳᠡᠷᠢᠶ᠎ᠡ ᠢ [0,1] ᠳᠤ ᠰᠢᠭᠤᠮᠠᠭ᠌᠎ᠠ
double clamp01(double v){
    if (v < 0.0) return 0.0;
    if (v > 1.0) return 1.0;
    return v;
}

// 归一化色相
// Normalize a hue value
// ᠥᠩᠭᠡ ᠶᠢ ᠪᠠᠷᠢᠮᠵᠢᠶ᠎ᠠ
double normHue(double h){
    h = std::fmod(h, 1.0);
    if (h < 0.0) h += 1.0;
    return h;
}

// HLS 单通道转换
// Convert one HLS channel
// HLS ᠨᠢᠭᠡ ᠰᠤᠪᠦᠭ
double hueToRgb(double m1, double m2, double hue){
    hue = normHue(hue);
    if (hue < 1.0 / 6.0) return m1 + (m2 - m1) * hue * 6.0;
    if (hue < 0.5) return m2;
    if (hue < 2.0 / 3.0) return m1 + (m2 - m1) * (2.0 / 3.0 - hue) * 6.0;
    return m1;
}
}

// fn Rgb To Yiq
// fn rgb to yiq
// fn Rgb To Yiq
static value fnRgbToYiq(cffi& a){
    double r = a.arg_float(0), g = a.arg_float(1), b = a.arg_float(2);
    double y = 0.30 * r + 0.59 * g + 0.11 * b;
    double i = 0.74 * (r - y) - 0.27 * (b - y);
    double q = 0.48 * (r - y) + 0.41 * (b - y);
    return makeTriple(a, y, i, q);
}

// fn Yiq To Rgb
// fn yiq to rgb
// fn Yiq To Rgb
static value fnYiqToRgb(cffi& a){
    double y = a.arg_float(0), i = a.arg_float(1), q = a.arg_float(2);
    return makeTriple(a,
                      clamp01(y + 0.948262 * i + 0.624013 * q),
                      clamp01(y - 0.276066 * i - 0.639810 * q),
                      clamp01(y - 1.105450 * i + 1.729860 * q));
}

// fn Rgb To Hls
// fn rgb to hls
// fn Rgb To Hls
static value fnRgbToHls(cffi& a){
    double r = a.arg_float(0), g = a.arg_float(1), b = a.arg_float(2);
    double maxc = std::fmax(std::fmax(r, g), b);
    double minc = std::fmin(std::fmin(r, g), b);
    double h = 0.0, s = 0.0;
    double l = (maxc + minc) / 2.0;
    if (minc != maxc){
        double delta = maxc - minc;
        s = (l <= 0.5) ? delta / (maxc + minc) : delta / (2.0 - maxc - minc);
        double rc = (maxc - r) / delta;
        double gc = (maxc - g) / delta;
        double bc = (maxc - b) / delta;
        if (r == maxc) h = bc - gc;
        else if (g == maxc) h = 2.0 + rc - bc;
        else h = 4.0 + gc - rc;
        h = std::fmod(h / 6.0, 1.0);
    }
    return makeTriple(a, h, l, s);
}

// fn Hls To Rgb
// fn hls to rgb
// fn Hls To Rgb
static value fnHlsToRgb(cffi& a){
    double h = a.arg_float(0), l = a.arg_float(1), s = a.arg_float(2);
    if (s == 0.0) return makeTriple(a, l, l, l);
    double m2 = (l <= 0.5) ? l * (1.0 + s) : l + s - l * s;
    double m1 = 2.0 * l - m2;
    return makeTriple(a,
                      hueToRgb(m1, m2, h + 1.0 / 3.0),
                      hueToRgb(m1, m2, h),
                      hueToRgb(m1, m2, h - 1.0 / 3.0));
}

// fn Rgb To Hsv
// fn rgb to hsv
// fn Rgb To Hsv
static value fnRgbToHsv(cffi& a){
    double r = a.arg_float(0), g = a.arg_float(1), b = a.arg_float(2);
    double maxc = std::fmax(std::fmax(r, g), b);
    double minc = std::fmin(std::fmin(r, g), b);
    double v = maxc;
    if (minc == maxc) return makeTriple(a, 0.0, 0.0, v);
    double s = (maxc - minc) / maxc;
    double delta = maxc - minc;
    double rc = (maxc - r) / delta;
    double gc = (maxc - g) / delta;
    double bc = (maxc - b) / delta;
    double h;
    if (r == maxc) h = bc - gc;
    else if (g == maxc) h = 2.0 + rc - bc;
    else h = 4.0 + gc - rc;
    h = std::fmod(h / 6.0, 1.0);
    return makeTriple(a, h, s, v);
}

// fn Hsv To Rgb
// fn hsv to rgb
// fn Hsv To Rgb
static value fnHsvToRgb(cffi& a){
    double h = a.arg_float(0), s = a.arg_float(1), v = a.arg_float(2);
    if (s == 0.0) return makeTriple(a, v, v, v);
    h = normHue(h) * 6.0;
    int i = static_cast<int>(std::floor(h));
    double f = h - static_cast<double>(i);
    double p = v * (1.0 - s);
    double q = v * (1.0 - s * f);
    double t = v * (1.0 - s * (1.0 - f));
    switch (i % 6){
        case 0: return makeTriple(a, v, t, p);
        case 1: return makeTriple(a, q, v, p);
        case 2: return makeTriple(a, p, v, t);
        case 3: return makeTriple(a, p, q, v);
        case 4: return makeTriple(a, t, p, v);
        default: return makeTriple(a, v, p, q);
    }
}

// cffi colorsys lib
// cffi colorsys lib
// cffi colorsys lib
ffilib cffi_colorsys_lib(){
    return cffi_lib(
        "colorsys",
        {
            {"rgb_to_yiq", (void*)fnRgbToYiq},
            {"yiq_to_rgb", (void*)fnYiqToRgb},
            {"rgb_to_hls", (void*)fnRgbToHls},
            {"hls_to_rgb", (void*)fnHlsToRgb},
            {"rgb_to_hsv", (void*)fnRgbToHsv},
            {"hsv_to_rgb", (void*)fnHsvToRgb}
        });
}

}
