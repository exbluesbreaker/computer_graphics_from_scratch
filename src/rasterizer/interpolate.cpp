#include <cgfs/rasterizer/interpolate.h>

#include <cassert>
#include <cstddef>

namespace cgfs {

std::vector<double> Interpolate(int i0, double d0, int i1, double d1) {
    assert(i0 <= i1 && "Interpolate: the independent variable must not decrease");
    if (i0 == i1) {
        return {d0};
    }
    if (i0 > i1) {
        return {};
    }

    std::vector<double> values;
    values.reserve(static_cast<std::size_t>(i1 - i0) + 1);
    const double a = (d1 - d0) / static_cast<double>(i1 - i0);
    double d = d0;
    for (int i = i0; i <= i1; ++i) {
        values.push_back(d);
        d += a;
    }
    return values;
}

}  // namespace cgfs
