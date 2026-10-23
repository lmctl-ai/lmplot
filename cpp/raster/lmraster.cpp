// lmraster: anti-aliased software rasterizer for lmplot's png terminal.
// Exact horizontal coverage, 16 vertical sub-scanlines, nonzero/even-odd
// fills, polyline stroker, TrueType outlines via stb_truetype.
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

#include "deflate.hpp"
#include "gp_memory.h"
#include "lmraster.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <vector>

struct lmr {
  int w, h;
  std::vector<uint8_t> px;  // RGBA, straight alpha
};

namespace {

constexpr int SS = 16;  // vertical sub-scanlines per pixel row
constexpr double PI = 3.14159265358979323846;

struct Pt { double x, y; };
struct Edge { double x0, y0, x1, y1, slope; int dir; };

struct Mask {
  int x0 = 0, y0 = 0, w = 0, h = 0;
  std::vector<float> a;
  bool empty() const { return w <= 0 || h <= 0; }
};

struct Shape {
  std::vector<Edge> edges;
  double minx = 1e300, miny = 1e300, maxx = -1e300, maxy = -1e300;
  void polygon(const Pt *p, size_t n) {
    if (n < 3) return;
    for (size_t i = 0; i < n; i++)
      if (!std::isfinite(p[i].x) || !std::isfinite(p[i].y)) return;  // drop the whole shape, never half of it
    for (size_t i = 0; i < n; i++) {
      const Pt &a = p[i], &b = p[(i + 1) % n];
      minx = std::min(minx, a.x); maxx = std::max(maxx, a.x);
      miny = std::min(miny, a.y); maxy = std::max(maxy, a.y);
      if (a.y == b.y) continue;
      Edge e;
      if (a.y < b.y) { e.x0 = a.x; e.y0 = a.y; e.x1 = b.x; e.y1 = b.y; e.dir = 1; }
      else { e.x0 = b.x; e.y0 = b.y; e.x1 = a.x; e.y1 = a.y; e.dir = -1; }
      e.slope = (e.x1 - e.x0) / (e.y1 - e.y0);
      edges.push_back(e);
    }
  }
  void polygon(const std::vector<Pt> &p) { polygon(p.data(), p.size()); }
};

// Scan-convert into a coverage mask clipped to the canvas.
Mask rasterize(Shape &s, int W, int H, bool evenodd, bool crisp = false) {
  Mask m;
  if (s.edges.empty()) return m;
  int bx0 = std::max(0, (int)std::floor(s.minx)), bx1 = std::min(W, (int)std::ceil(s.maxx) + 1);
  int by0 = std::max(0, (int)std::floor(s.miny)), by1 = std::min(H, (int)std::ceil(s.maxy) + 1);
  if (bx1 <= bx0 || by1 <= by0) return m;
  m.x0 = bx0; m.y0 = by0; m.w = bx1 - bx0; m.h = by1 - by0;
  m.a.assign((size_t)m.w * m.h, 0.f);
  std::sort(s.edges.begin(), s.edges.end(), [](const Edge &a, const Edge &b) { return a.y0 < b.y0; });
  std::vector<const Edge *> active;
  size_t next = 0;
  std::vector<float> part(m.w + 2), delta(m.w + 2);
  std::vector<std::pair<double, int>> xs;
  const float wt = 1.f / SS;
  for (int row = 0; row < m.h; row++) {
    double y = m.y0 + row;
    while (next < s.edges.size() && s.edges[next].y0 < y + 1) active.push_back(&s.edges[next++]);
    active.erase(std::remove_if(active.begin(), active.end(), [&](const Edge *e) { return e->y1 <= y; }), active.end());
    if (active.empty()) continue;
    std::fill(part.begin(), part.end(), 0.f);
    std::fill(delta.begin(), delta.end(), 0.f);
    bool any = false;
    const int subs = crisp ? 1 : SS;
    for (int sub = 0; sub < subs; sub++) {
      double sy = y + (sub + 0.5) / subs;
      xs.clear();
      for (const Edge *e : active)
        if (e->y0 <= sy && sy < e->y1) xs.emplace_back(e->x0 + (sy - e->y0) * e->slope, e->dir);
      if (xs.size() < 2) continue;
      std::sort(xs.begin(), xs.end());
      int wind = 0;
      double start = 0;
      for (size_t i = 0; i < xs.size(); i++) {
        bool was = evenodd ? (wind & 1) : wind != 0;
        wind += evenodd ? 1 : xs[i].second;
        bool now = evenodd ? (wind & 1) : wind != 0;
        if (!was && now) start = xs[i].first;
        else if (was && !now) {
          double xa = std::max(start - m.x0, 0.0), xb = std::min(xs[i].first - m.x0, (double)m.w);
          if (crisp) {
            // pixel centres inside [start, end): neighbouring polygons partition the plane, no seams
            int ca = (int)std::ceil(start - m.x0 - 0.5), cb = (int)std::ceil(xs[i].first - m.x0 - 0.5);
            ca = std::max(ca, 0); cb = std::min(cb, m.w);
            if (cb > ca) { any = true; delta[ca] += 1.f; delta[cb] -= 1.f; }
            continue;
          }
          if (xb <= xa) continue;
          any = true;
          int ia = (int)xa, ib = (int)xb;
          if (ia == ib) part[ia] += (float)(xb - xa) * wt;
          else {
            part[ia] += (float)(ia + 1 - xa) * wt;
            if (ib < m.w) part[ib] += (float)(xb - ib) * wt;
            delta[ia + 1] += wt;
            delta[ib] -= wt;
          }
        }
      }
    }
    if (!any) continue;
    float run = 0;
    float *out = &m.a[(size_t)row * m.w];
    for (int x = 0; x < m.w; x++) {
      run += delta[x];
      float v = run + part[x];
      out[x] = v > 1.f ? 1.f : (v < 0.f ? 0.f : v);
    }
  }
  return m;
}

inline void blend(uint8_t *d, unsigned argb, float cov) {
  float sa = ((argb >> 24) & 255) / 255.f * cov;
  if (sa <= 0.f) return;
  float sr = (argb >> 16) & 255, sg = (argb >> 8) & 255, sb = argb & 255;
  float da = d[3] / 255.f;
  float oa = sa + da * (1.f - sa);
  if (oa <= 0.f) return;
  float k = da * (1.f - sa);
  d[0] = (uint8_t)std::lround((sr * sa + d[0] * k) / oa);
  d[1] = (uint8_t)std::lround((sg * sa + d[1] * k) / oa);
  d[2] = (uint8_t)std::lround((sb * sa + d[2] * k) / oa);
  d[3] = (uint8_t)std::lround(oa * 255.f);
}

void composite(lmr *r, const Mask &m, unsigned argb) {
  for (int j = 0; j < m.h; j++) {
    const float *a = &m.a[(size_t)j * m.w];
    uint8_t *d = &r->px[((size_t)(m.y0 + j) * r->w + m.x0) * 4];
    for (int i = 0; i < m.w; i++, d += 4)
      if (a[i] > 0.f) blend(d, argb, a[i]);
  }
}

void fill_shape(lmr *r, Shape &s, unsigned argb, bool evenodd = false, bool crisp = false) {
  Mask m = rasterize(s, r->w, r->h, evenodd, crisp);
  if (!m.empty()) composite(r, m, argb);
}

double signed_area(const std::vector<Pt> &p) {
  double a = 0;
  for (size_t i = 0; i < p.size(); i++) {
    const Pt &u = p[i], &v = p[(i + 1) % p.size()];
    a += u.x * v.y - v.x * u.y;
  }
  return a / 2;
}

// Add a convex piece with consistent orientation so overlapping pieces union under nonzero.
void add_piece(Shape &s, std::vector<Pt> p) {
  if (signed_area(p) < 0) std::reverse(p.begin(), p.end());
  s.polygon(p);
}

void add_circle(Shape &s, Pt c, double r) {
  int n = std::clamp((int)std::ceil(r * 3), 8, 48);
  std::vector<Pt> p(n);
  for (int i = 0; i < n; i++) p[i] = {c.x + r * std::cos(2 * PI * i / n), c.y + r * std::sin(2 * PI * i / n)};
  add_piece(s, p);
}

// Split a polyline into dash runs.
std::vector<std::vector<Pt>> dash_polyline(const std::vector<Pt> &pts, const double *dash, int ndash) {
  std::vector<std::vector<Pt>> out;
  double total = 0;
  for (int i = 0; i < ndash; i++) total += dash[i];
  // a period below a quarter pixel is visually solid and would take forever to walk
  if (ndash < 2 || !(total >= 0.25)) { out.push_back(pts); return out; }
  int idx = 0;
  double left = dash[0];
  bool on = true;
  std::vector<Pt> cur;
  if (!pts.empty()) cur.push_back(pts[0]);
  for (size_t i = 1; i < pts.size(); i++) {
    Pt a = pts[i - 1], b = pts[i];
    double len = std::hypot(b.x - a.x, b.y - a.y);
    double pos = 0;
    while (len - pos > left) {
      pos += left;
      Pt p = {a.x + (b.x - a.x) * pos / len, a.y + (b.y - a.y) * pos / len};
      if (on) { cur.push_back(p); out.push_back(cur); cur.clear(); }
      else { cur.clear(); cur.push_back(p); }
      on = !on;
      idx = (idx + 1) % ndash;
      left = dash[idx];
      if (left <= 0) left = 1e-6;
    }
    left -= len - pos;
    if (on) cur.push_back(b);
  }
  if (on && cur.size() >= 2) out.push_back(cur);
  return out;
}

void stroke_run(Shape &s, const std::vector<Pt> &in, double width, int cap, int join) {
  // Drop repeated points.
  std::vector<Pt> p;
  for (const Pt &q : in)
    if (p.empty() || std::hypot(q.x - p.back().x, q.y - p.back().y) > 1e-9) p.push_back(q);
  double hw = width / 2;
  if (p.size() == 1) {
    if (cap == LMR_CAP_ROUND) add_circle(s, p[0], hw);
    else if (cap == LMR_CAP_SQUARE) add_piece(s, {{p[0].x - hw, p[0].y - hw}, {p[0].x + hw, p[0].y - hw}, {p[0].x + hw, p[0].y + hw}, {p[0].x - hw, p[0].y + hw}});
    return;
  }
  size_t n = p.size();
  std::vector<Pt> nrm(n - 1);
  for (size_t i = 0; i + 1 < n; i++) {
    double dx = p[i + 1].x - p[i].x, dy = p[i + 1].y - p[i].y, l = std::hypot(dx, dy);
    nrm[i] = {-dy / l * hw, dx / l * hw};
    Pt a = p[i], b = p[i + 1];
    if (cap == LMR_CAP_SQUARE) {
      if (i == 0) { a.x -= dx / l * hw; a.y -= dy / l * hw; }
      if (i + 2 == n) { b.x += dx / l * hw; b.y += dy / l * hw; }
    }
    add_piece(s, {{a.x + nrm[i].x, a.y + nrm[i].y}, {b.x + nrm[i].x, b.y + nrm[i].y},
                  {b.x - nrm[i].x, b.y - nrm[i].y}, {a.x - nrm[i].x, a.y - nrm[i].y}});
  }
  if (cap == LMR_CAP_ROUND) { add_circle(s, p[0], hw); add_circle(s, p[n - 1], hw); }
  for (size_t i = 1; i + 1 < n; i++) {
    const Pt &n1 = nrm[i - 1], &n2 = nrm[i];
    double cross = n1.x * n2.y - n1.y * n2.x;
    if (std::fabs(cross) < 1e-9 * hw * hw) continue;
    if (join == LMR_JOIN_ROUND) { if (hw >= 0.75) add_circle(s, p[i], hw); continue; }
    double side = cross > 0 ? -1 : 1;  // outer side of the turn
    Pt a = {p[i].x + side * n1.x, p[i].y + side * n1.y}, b = {p[i].x + side * n2.x, p[i].y + side * n2.y};
    add_piece(s, {p[i], a, b});
    if (join == LMR_JOIN_MITER) {
      // miter tip: intersection of the two outer offset lines, limited to 4 * half width
      double mx = n1.x + n2.x, my = n1.y + n2.y, ml = std::hypot(mx, my);
      if (ml > 1e-12) {
        double cosh = ml / (2 * hw);  // cos(half the turning angle)
        double miter = hw / cosh;
        if (miter <= 4 * hw) {
          Pt tip = {p[i].x + side * mx / ml * miter, p[i].y + side * my / ml * miter};
          add_piece(s, {a, tip, b});
        }
      }
    }
  }
}

// ---------------------------------------------------------------- fonts

struct FontData {
  stbtt_fontinfo info;
  bool ok = false;
  float ascent = 0, descent = 0, upem_scale = 0;  // scale for 1px em
  std::map<int, std::vector<stbtt_vertex>> shapes;
};

FontData *font_for(int family, int bold) {
  static FontData fonts[4];  // sans, sans-bold, serif, mono
  int slot = family == LMR_FAMILY_SERIF ? 2 : family == LMR_FAMILY_MONO ? 3 : (bold ? 1 : 0);
  const unsigned char *data[4] = {lmr_font_sans, lmr_font_sans_bold, lmr_font_serif, lmr_font_mono};
  FontData &f = fonts[slot];
  if (!f.ok) {
    if (stbtt_InitFont(&f.info, data[slot], stbtt_GetFontOffsetForIndex(data[slot], 0))) {
      f.ok = true;
      int a, d, g;
      stbtt_GetFontVMetrics(&f.info, &a, &d, &g);
      f.upem_scale = stbtt_ScaleForMappingEmToPixels(&f.info, 1.0f);
      f.ascent = a * f.upem_scale;
      f.descent = -d * f.upem_scale;
    }
  }
  return f.ok ? &f : nullptr;
}

const std::vector<stbtt_vertex> &glyph_shape(FontData *f, int glyph) {
  auto it = f->shapes.find(glyph);
  if (it != f->shapes.end()) return it->second;
  stbtt_vertex *v = nullptr;
  int n = stbtt_GetGlyphShape(&f->info, glyph, &v);
  std::vector<stbtt_vertex> copy(v, v + n);
  if (v) stbtt_FreeShape(&f->info, v);
  return f->shapes.emplace(glyph, std::move(copy)).first->second;
}

// Adobe Symbol encoding (the letters and a few symbols) -> Unicode, for {/Symbol a}.
int symbol_to_unicode(int c) {
  static const int greek_upper[] = {0x391, 0x392, 0x3A7, 0x394, 0x395, 0x3A6, 0x393, 0x397, 0x399, 0x3D1, 0x39A, 0x39B, 0x39C,
                                    0x39D, 0x39F, 0x3A0, 0x398, 0x3A1, 0x3A3, 0x3A4, 0x3A5, 0x3C2, 0x3A9, 0x39E, 0x3A8, 0x396};
  static const int greek_lower[] = {0x3B1, 0x3B2, 0x3C7, 0x3B4, 0x3B5, 0x3C6, 0x3B3, 0x3B7, 0x3B9, 0x3D5, 0x3BA, 0x3BB, 0x3BC,
                                    0x3BD, 0x3BF, 0x3C0, 0x3B8, 0x3C1, 0x3C3, 0x3C4, 0x3C5, 0x3D6, 0x3C9, 0x3BE, 0x3C8, 0x3B6};
  if (c >= 'A' && c <= 'Z') return greek_upper[c - 'A'];
  if (c >= 'a' && c <= 'z') return greek_lower[c - 'a'];
  switch (c) {
    case 0xB1: return 0xB1;
    case '+': return '+';
    case '-': return 0x2212;
    case '=': return '=';
    case 0xA5: return 0x221E;  // infinity
    case 0xB0: return 0xB0;
    case 0xD7: return 0xD7;
    case 0xB4: return 0xD7;
    case 0xF2: return 0x222B;
    case 0xB6: return 0x2202;
    case 0xD1: return 0x2207;
    case 0xD6: return 0x221A;
    case 0xE5: return 0x2211;
    case 0xD5: return 0x220F;
    default: return c;
  }
}

// Decode UTF-8 (and "&#xHH;" numeric references) into code points.
std::vector<int> decode(const char *s, bool symbol) {
  std::vector<int> out;
  const unsigned char *p = (const unsigned char *)s;
  while (*p) {
    int c = *p;
    if (c == '&' && p[1] == '#' && p[2] == 'x') {
      char *end = nullptr;
      long v = strtol((const char *)p + 3, &end, 16);
      if (end && *end == ';' && v > 0) { out.push_back((int)v); p = (const unsigned char *)end + 1; continue; }
    }
    int len = 1;
    if (c >= 0xF0 && (p[1] & 0xC0) == 0x80) { c = ((c & 7) << 18) | ((p[1] & 63) << 12) | ((p[2] & 63) << 6) | (p[3] & 63); len = 4; }
    else if (c >= 0xE0 && (p[1] & 0xC0) == 0x80) { c = ((c & 15) << 12) | ((p[1] & 63) << 6) | (p[2] & 63); len = 3; }
    else if (c >= 0xC0 && (p[1] & 0xC0) == 0x80) { c = ((c & 31) << 6) | (p[1] & 63); len = 2; }
    p += len;
    out.push_back(symbol && c < 0x100 ? symbol_to_unicode(c) : c);
  }
  return out;
}

void flatten_glyph(Shape &s, const std::vector<stbtt_vertex> &v, double scale, double ox, double oy, double cs, double sn,
                   double x0, double y0, double shear) {
  // glyph space (u right, v up, scaled to px) -> device
  auto map = [&](double gx, double gy) {
    double u = ox + gx * scale + shear * gy * scale, w = gy * scale;
    return Pt{x0 + u * cs - w * sn, y0 - (u * sn + w * cs)};
  };
  std::vector<Pt> contour;
  auto close = [&] { if (contour.size() >= 3) s.polygon(contour); contour.clear(); };
  Pt last{0, 0};
  double lx = 0, ly = 0;
  for (const stbtt_vertex &t : v) {
    switch (t.type) {
      case STBTT_vmove:
        close();
        lx = t.x; ly = t.y;
        last = map(lx, ly);
        contour.push_back(last);
        break;
      case STBTT_vline:
        lx = t.x; ly = t.y;
        contour.push_back(map(lx, ly));
        break;
      case STBTT_vcurve: {
        int steps = std::clamp((int)std::ceil(std::sqrt(std::fabs(t.x - lx) + std::fabs(t.y - ly) + std::fabs(t.cx - lx) + std::fabs(t.cy - ly)) * scale * 0.9), 3, 16);
        for (int i = 1; i <= steps; i++) {
          double q = (double)i / steps, a = (1 - q) * (1 - q), b = 2 * (1 - q) * q, c = q * q;
          contour.push_back(map(a * lx + b * t.cx + c * t.x, a * ly + b * t.cy + c * t.y));
        }
        lx = t.x; ly = t.y;
        break;
      }
      case STBTT_vcubic: {
        int steps = 12;
        for (int i = 1; i <= steps; i++) {
          double q = (double)i / steps, m = 1 - q;
          contour.push_back(map(m * m * m * lx + 3 * m * m * q * t.cx + 3 * m * q * q * t.cx1 + q * q * q * t.x,
                                m * m * m * ly + 3 * m * m * q * t.cy + 3 * m * q * q * t.cy1 + q * q * q * t.y));
        }
        lx = t.x; ly = t.y;
        break;
      }
    }
  }
  close();
}

}  // namespace

// ---------------------------------------------------------------- C API

// The C API is called from gnuplot's C code, which an exception must not unwind (the memory
// budget's Exceeded is a std::bad_alloc). Each allocating entry point catches it, notes the
// failure for the host (gp_memory.h) and returns as if nothing was drawn.
#define LMR_TRY try {
#define LMR_CATCH(ret)                                    \
  }                                                       \
  catch (const std::exception &e) {                       \
    lmplot_mem_note_failure(e.what());                    \
    return ret;                                           \
  }                                                       \
  catch (...) {                                           \
    lmplot_mem_note_failure("png terminal: internal error"); \
    return ret;                                           \
  }

extern "C" {

lmr *lmr_create(int w, int h, unsigned bg) { LMR_TRY
  if (w < 1 || h < 1 || (long long)w * h > 100000000LL) return nullptr;
  std::unique_ptr<lmr> r(new lmr);  // freed if the pixel buffer does not fit the budget
  r->w = w; r->h = h;
  r->px.resize((size_t)w * h * 4);
  for (size_t i = 0; i < (size_t)w * h; i++) {
    r->px[i * 4] = (bg >> 16) & 255; r->px[i * 4 + 1] = (bg >> 8) & 255; r->px[i * 4 + 2] = bg & 255; r->px[i * 4 + 3] = (bg >> 24) & 255;
  }
  return r.release();
  LMR_CATCH(nullptr)
}
void lmr_destroy(lmr *r) { delete r; }
int lmr_width(const lmr *r) { return r->w; }
int lmr_height(const lmr *r) { return r->h; }
unsigned lmr_get_pixel(const lmr *r, int x, int y) {
  if (x < 0 || y < 0 || x >= r->w || y >= r->h) return 0;
  const uint8_t *d = &r->px[((size_t)y * r->w + x) * 4];
  return ((unsigned)d[3] << 24) | (d[0] << 16) | (d[1] << 8) | d[2];
}
void lmr_set_pixel(lmr *r, int x, int y, unsigned argb) {
  if (x < 0 || y < 0 || x >= r->w || y >= r->h) return;
  blend(&r->px[((size_t)y * r->w + x) * 4], argb, 1.f);
}

void lmr_fill_polygon(lmr *r, const double *xy, int n, unsigned argb) { LMR_TRY
  Shape s;
  std::vector<Pt> p(n);
  for (int i = 0; i < n; i++) p[i] = {xy[2 * i], xy[2 * i + 1]};
  s.polygon(p);
  fill_shape(r, s, argb);
  LMR_CATCH()
}

void lmr_fill_polygon_crisp(lmr *r, const double *xy, int n, unsigned argb) { LMR_TRY
  Shape s;
  std::vector<Pt> p(n);
  for (int i = 0; i < n; i++) p[i] = {xy[2 * i], xy[2 * i + 1]};
  s.polygon(p);
  fill_shape(r, s, argb, false, true);
  LMR_CATCH()
}

void lmr_fill_polygon_pattern(lmr *r, const double *xy, int n, unsigned argb, unsigned bg, int pattern) { LMR_TRY
  Shape s;
  std::vector<Pt> p(n);
  for (int i = 0; i < n; i++) p[i] = {xy[2 * i], xy[2 * i + 1]};
  s.polygon(p);
  Mask poly = rasterize(s, r->w, r->h, false);
  if (poly.empty()) return;
  if ((bg >> 24) != 0) composite(r, poly, bg);
  if (pattern == 3) { composite(r, poly, argb); return; }
  // line families kx*x + ky*y = c with spacing `step`
  struct Fam { double kx, ky, step; };
  std::vector<Fam> fams;
  switch (pattern) {
    case 1: fams = {{1, -1, 8}, {1, 1, 8}}; break;
    case 2: fams = {{1, -1, 5}, {1, 1, 5}}; break;
    case 4: fams = {{1, -1, 8}}; break;
    case 5: fams = {{1, 1, 8}}; break;
    case 6: fams = {{7, -3, 28}}; break;
    case 7: fams = {{7, 3, 28}}; break;
    case 8: fams = {{3, -7, 28}}; break;
    case 9: fams = {{3, 7, 28}}; break;
    default: return;
  }
  Shape h;
  double x0 = poly.x0, y0 = poly.y0, x1 = poly.x0 + poly.w, y1 = poly.y0 + poly.h;
  double cx = (x0 + x1) / 2, cy = (y0 + y1) / 2, diag = std::hypot(x1 - x0, y1 - y0);
  for (const Fam &f : fams) {
    double kn = std::hypot(f.kx, f.ky);
    double cmid = f.kx * cx + f.ky * cy, span = diag * kn / 2 + f.step;
    double dx = f.ky / kn, dy = -f.kx / kn;  // along the line
    for (double c = std::floor((cmid - span) / f.step) * f.step; c <= cmid + span; c += f.step) {
      double t = (c - cmid) / (kn * kn);  // offset along the normal
      Pt base = {cx + f.kx * t, cy + f.ky * t};
      std::vector<Pt> seg = {{base.x - dx * diag, base.y - dy * diag}, {base.x + dx * diag, base.y + dy * diag}};
      stroke_run(h, seg, 1.0, LMR_CAP_BUTT, LMR_JOIN_BEVEL);
    }
  }
  Mask hm = rasterize(h, r->w, r->h, false);
  if (hm.empty()) return;
  for (int j = 0; j < poly.h; j++)
    for (int i = 0; i < poly.w; i++) {
      int hx = poly.x0 + i - hm.x0, hy = poly.y0 + j - hm.y0;
      float cov = (hx < 0 || hy < 0 || hx >= hm.w || hy >= hm.h) ? 0.f : hm.a[(size_t)hy * hm.w + hx];
      poly.a[(size_t)j * poly.w + i] *= cov;
    }
  composite(r, poly, argb);
  LMR_CATCH()
}

void lmr_stroke(lmr *r, const double *xy, int n, double width, unsigned argb, int cap, int join, const double *dash,
                int ndash) { LMR_TRY
  if (n < 1 || width <= 0) return;
  std::vector<Pt> p(n);
  for (int i = 0; i < n; i++) p[i] = {xy[2 * i], xy[2 * i + 1]};
  Shape s;
  for (const auto &run : dash_polyline(p, dash, ndash)) stroke_run(s, run, width, cap, join);
  fill_shape(r, s, argb);
  LMR_CATCH()
}

void lmr_draw_image(lmr *r, const unsigned *argb, int w, int h, double x0, double y0, double x1, double y1, double x2,
                    double y2) {
  if (w < 1 || h < 1) return;
  double ux = (x1 - x0) / w, uy = (y1 - y0) / w, vx = (x2 - x0) / h, vy = (y2 - y0) / h;
  double det = ux * vy - uy * vx;
  if (std::fabs(det) < 1e-12) return;
  double minx = std::min({x0, x1, x2, x1 + x2 - x0}), maxx = std::max({x0, x1, x2, x1 + x2 - x0});
  double miny = std::min({y0, y1, y2, y1 + y2 - y0}), maxy = std::max({y0, y1, y2, y1 + y2 - y0});
  int ix0 = std::max(0, (int)std::floor(minx)), ix1 = std::min(r->w, (int)std::ceil(maxx));
  int iy0 = std::max(0, (int)std::floor(miny)), iy1 = std::min(r->h, (int)std::ceil(maxy));
  for (int j = iy0; j < iy1; j++)
    for (int i = ix0; i < ix1; i++) {
      double px = i + 0.5 - x0, py = j + 0.5 - y0;
      double u = (px * vy - py * vx) / det, v = (ux * py - uy * px) / det;
      if (u < 0 || v < 0 || u >= w || v >= h) continue;
      unsigned c = argb[(size_t)(int)v * w + (int)u];
      blend(&r->px[((size_t)j * r->w + i) * 4], c, 1.f);
    }
}

void lmr_font_metrics(lmr_font font, double px, double *ascent, double *descent) {
  FontData *f = font_for(font.family, font.bold);
  *ascent = f ? f->ascent * px : px * 0.93;
  *descent = f ? f->descent * px : px * 0.24;
}

double lmr_text_width(lmr_font font, double px, const char *utf8) { LMR_TRY
  FontData *f = font_for(font.family == LMR_FAMILY_SYMBOL ? LMR_FAMILY_SANS : font.family, font.bold);
  if (!f) return 0;
  double w = 0;
  for (int cp : decode(utf8, font.family == LMR_FAMILY_SYMBOL)) {
    int adv, lsb;
    stbtt_GetCodepointHMetrics(&f->info, cp, &adv, &lsb);
    w += adv * f->upem_scale * px;
  }
  return w;
  LMR_CATCH(0)
}

void lmr_text_draw(lmr *r, lmr_font font, double px, const char *utf8, double x, double y, double angle, unsigned argb) { LMR_TRY
  FontData *f = font_for(font.family == LMR_FAMILY_SYMBOL ? LMR_FAMILY_SANS : font.family, font.bold);
  if (!f || px <= 0) return;
  double cs = std::cos(angle), sn = std::sin(angle), scale = f->upem_scale * px, pen = 0;
  Shape s;
  for (int cp : decode(utf8, font.family == LMR_FAMILY_SYMBOL)) {
    int g = stbtt_FindGlyphIndex(&f->info, cp);
    flatten_glyph(s, glyph_shape(f, g), scale, pen, 0, cs, sn, x, y, font.italic ? 0.2 : 0.0);
    int adv, lsb;
    stbtt_GetGlyphHMetrics(&f->info, g, &adv, &lsb);
    pen += adv * scale;
  }
  Mask m = rasterize(s, r->w, r->h, false);
  if (m.empty()) return;
  // light stem darkening: unhinted outlines read thin at small sizes
  for (float &a : m.a) a = std::min(1.f, a * 1.12f);
  composite(r, m, argb);
  LMR_CATCH()
}

int lmr_content_bbox(const lmr *r, unsigned bg, int *bx0, int *by0, int *bx1, int *by1) {
  int x0 = r->w, y0 = r->h, x1 = -1, y1 = -1;
  for (int y = 0; y < r->h; y++)
    for (int x = 0; x < r->w; x++)
      if (lmr_get_pixel(r, x, y) != bg) { x0 = std::min(x0, x); x1 = std::max(x1, x); y0 = std::min(y0, y); y1 = std::max(y1, y); }
  if (x1 < 0) return 0;
  *bx0 = x0; *by0 = y0; *bx1 = x1; *by1 = y1;
  return 1;
}

lmr *lmr_crop(const lmr *r, int x0, int y0, int x1, int y1) {
  lmr *c = lmr_create(x1 - x0 + 1, y1 - y0 + 1, 0);
  if (!c) return nullptr;
  for (int y = y0; y <= y1; y++)
    std::memcpy(&c->px[(size_t)(y - y0) * c->w * 4], &r->px[((size_t)y * r->w + x0) * 4], (size_t)c->w * 4);
  return c;
}

static void put32(std::vector<unsigned char> &o, uint32_t v) { o.push_back(v >> 24); o.push_back(v >> 16); o.push_back(v >> 8); o.push_back(v); }
static uint32_t crc32_(const unsigned char *p, size_t n) {
  static uint32_t table[256];
  static bool init = false;
  if (!init) {
    for (uint32_t i = 0; i < 256; i++) { uint32_t c = i; for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xedb88320u & (uint32_t)(-(int)(c & 1))); table[i] = c; }
    init = true;
  }
  uint32_t c = 0xffffffffu;
  for (size_t i = 0; i < n; i++) c = table[(c ^ p[i]) & 255] ^ (c >> 8);
  return c ^ 0xffffffffu;
}
static void chunk(std::vector<unsigned char> &out, const char *type, const std::vector<unsigned char> &data) {
  put32(out, (uint32_t)data.size());
  size_t begin = out.size();
  for (int i = 0; i < 4; i++) out.push_back((unsigned char)type[i]);
  out.insert(out.end(), data.begin(), data.end());
  put32(out, crc32_(out.data() + begin, out.size() - begin));
}

int lmr_write_png(const lmr *r, FILE *fp) { LMR_TRY
  bool opaque = true;
  for (size_t i = 3; i < r->px.size(); i += 4) if (r->px[i] != 255) { opaque = false; break; }
  int bpp = opaque ? 3 : 4;
  size_t stride = (size_t)r->w * bpp;
  std::vector<unsigned char> raw;
  raw.reserve((stride + 1) * r->h);
  std::vector<unsigned char> prev(stride, 0), cur(stride), best(stride), tmp(stride);
  for (int y = 0; y < r->h; y++) {
    for (int x = 0; x < r->w; x++)
      for (int c = 0; c < bpp; c++) cur[(size_t)x * bpp + c] = r->px[((size_t)y * r->w + x) * 4 + c];
    int bestf = 0;
    long bestsum = -1;
    for (int f = 0; f < 5; f++) {
      long sum = 0;
      for (size_t i = 0; i < stride; i++) {
        int a = i >= (size_t)bpp ? cur[i - bpp] : 0, b = prev[i], c = i >= (size_t)bpp ? prev[i - bpp] : 0, pred = 0;
        switch (f) {
          case 1: pred = a; break;
          case 2: pred = b; break;
          case 3: pred = (a + b) / 2; break;
          case 4: { int p = a + b - c, pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c); pred = (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c); break; }
          default: pred = 0;
        }
        tmp[i] = (unsigned char)(cur[i] - pred);
        sum += tmp[i] < 128 ? tmp[i] : 256 - tmp[i];
      }
      if (bestsum < 0 || sum < bestsum) { bestsum = sum; bestf = f; best = tmp; }
    }
    raw.push_back((unsigned char)bestf);
    raw.insert(raw.end(), best.begin(), best.end());
    prev = cur;
  }
  std::vector<unsigned char> out = {137, 80, 78, 71, 13, 10, 26, 10}, ihdr;
  put32(ihdr, r->w); put32(ihdr, r->h);
  ihdr.insert(ihdr.end(), {8, (unsigned char)(opaque ? 2 : 6), 0, 0, 0});
  chunk(out, "IHDR", ihdr);
  chunk(out, "sRGB", {0});
  chunk(out, "IDAT", lmraster::deflate_zlib(raw));
  chunk(out, "IEND", {});
  return fwrite(out.data(), 1, out.size(), fp) == out.size() ? 0 : 1;
  LMR_CATCH(1)
}

}  // extern "C"
