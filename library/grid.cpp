#include "grid.hpp"

#include <algorithm>

namespace ls {

void bit_plane::reset(int W, int H) {
    w = W;
    h = H;
    int n_bits  = W * H;
    int n_words = (n_bits + 63) / 64;
    words.assign(n_words, 0);
}

bool bit_plane::test(int x, int y) const {
    int idx = index(x, y);
    return (words[idx / 64] >> (idx % 64)) & 1;
}

void bit_plane::set(int x, int y) {
    int idx = index(x, y);
    words[idx / 64] |= uint64_t(1) << (idx % 64);
}

void bit_plane::clear(int x, int y) {
    int idx = index(x, y);
    words[idx / 64] &= ~(uint64_t(1) << (idx % 64));
}

void bit_plane::clear_all() {
    std::fill(words.begin(), words.end(), uint64_t(0));
}

grid::grid(int W, int H) : w(W), h(H), values(W * H, 0) {
    occupied.reset(W, H);
}

void grid::set(int x, int y, int64_t v) {
    values[y * w + x] = v;
    occupied.set(x, y);
}

void grid::clear(int x, int y) {
    occupied.clear(x, y);
}

bool grid::in_bounds(int x, int y) const {
    return x >= 0 && x < w && y >= 0 && y < h;
}

void grid::resize(int new_w, int new_h) {
    grid next(new_w, new_h);
    int copy_w = std::min(w, new_w);
    int copy_h = std::min(h, new_h);
    for (int y = 0; y < copy_h; ++y)
        for (int x = 0; x < copy_w; ++x)
            if (!empty(x, y))
                next.set(x, y, get(x, y));
    *this = std::move(next);
}

void grid::upscale(int sx, int sy) {
    grid next(w * sx, h * sy);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            if (!empty(x, y)) {
                int64_t v = get(x, y);
                for (int dy = 0; dy < sy; ++dy)
                    for (int dx = 0; dx < sx; ++dx)
                        next.set(x * sx + dx, y * sy + dy, v);
            }
    *this = std::move(next);
}

} // namespace ls
