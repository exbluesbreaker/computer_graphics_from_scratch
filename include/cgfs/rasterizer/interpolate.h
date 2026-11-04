#pragma once

#include <vector>

namespace cgfs {

/// Linear interpolation of a dependent variable d over an integer range of an
/// independent variable i: the values d takes at i0, i0 + 1, ..., i1 when it is d0 at i0
/// and d1 at i1.
///
/// Returns i1 - i0 + 1 values, so values[i - i0] is d at i. The slope
/// a = (d1 - d0) / (i1 - i0) is "how much d changes per unit of i"; starting from d0,
/// every step adds a to a running value, as in the book's listing. That running sum is
/// the whole secret of the rasterizer: a line is one pixel per column (or row) with the
/// other coordinate interpolated this way, and triangles and shading reuse the same
/// function for edges and intensities.
///
/// When i0 == i1 the range is a single point and the result is {d0}; d1 is ignored,
/// which also avoids the division by zero.
///
/// Precondition: i0 <= i1. The callers sort their endpoints first. With i0 > i1 the
/// result is empty (the loop "for i = i0 to i1" runs zero times).
[[nodiscard]] std::vector<double> Interpolate(int i0, double d0, int i1, double d1);

}  // namespace cgfs
