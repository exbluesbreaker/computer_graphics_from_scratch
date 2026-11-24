#pragma once

#include <cgfs/lighting/light.h>
#include <cgfs/math/vec3.h>

#include <cmath>
#include <span>

namespace cgfs {

// The illumination equation of the book's Light chapter: how much light leaves a point P
// of a surface toward the viewer,
//
//     I_P = I_A + sum over the lights i of
//               I_i * <N, L_i> / (|N| |L_i|)                diffuse reflection
//             + I_i * (<R_i, V> / (|R_i| |V|))^s            specular reflection
//
// with I_A the ambient light, I_i the intensity of the light i, N the normal of the
// surface at P, L_i the direction from P to the light, R_i that direction mirrored about
// the normal, V the direction from P to the viewer and s the surface's specular exponent.
// The result multiplies the surface's color.
//
// The two terms of one light are the first two functions below, and ComputeIllumination
// is the whole sum. Whoever sums the terms decides which lights reach the point at all:
// the ray tracer has a sum of its own that leaves out the lights another sphere blocks,
// which is a shadow; the rasterizer, which casts no shadows, uses the sum here.

/// The `specular` value of a surface with no highlight at all. The book's convention:
/// -1 means "skip the specular term", any other value is the exponent.
inline constexpr double kMatte = -1.0;

/// Diffuse reflection: what a matte surface with normal `normal` sends on, equally in
/// every direction, of a light of intensity `light_intensity` that arrives along
/// `to_light`, the direction from the point to the light:
///
///     I * <N, L> / (|N| |L|) = I * cos(angle between N and L)
///
/// A beam that arrives at an angle is spread over more surface, by 1 / cos, so every
/// part of the surface gets less of it: full intensity where the light is straight above
/// the surface, none where it grazes it. Zero when <N, L> <= 0, the light being behind
/// the surface. Neither vector has to be normalized.
[[nodiscard]] inline double DiffuseReflection(double light_intensity, Vec3 normal, Vec3 to_light) {
    const double n_dot_l = Dot(normal, to_light);
    if (n_dot_l <= 0) {
        return 0.0;
    }
    return light_intensity * n_dot_l / (Length(normal) * Length(to_light));
}

/// Specular reflection: the highlight of the same light on a shiny surface, as seen
/// along `view`, the direction from the point to the viewer:
///
///     I * (<R, V> / (|R| |V|))^s,   R = 2 N <N, L> - L
///
/// A mirror would send the light on in the one direction R, L mirrored about the normal.
/// A shiny surface sends most of it there and less to either side: the highlight is at
/// its strongest where the viewer looks along R and fades as cos^s of the angle between R
/// and V, so a larger `exponent` s gives a smaller, sharper spot. Zero when <R, V> <= 0,
/// the viewer being more than 90 degrees from the mirror direction. As in the book the
/// term is not gated on <N, L> > 0; only the diffuse term is.
///
/// Precondition: |normal| = 1, which Reflect needs. `to_light` and `view` may have any
/// length.
[[nodiscard]] inline double SpecularReflection(double light_intensity, Vec3 normal, Vec3 to_light,
                                               Vec3 view, double exponent) {
    const Vec3 reflected = Reflect(to_light, normal);
    const double r_dot_v = Dot(reflected, view);
    if (r_dot_v <= 0) {
        return 0.0;
    }
    return light_intensity * std::pow(r_dot_v / (Length(reflected) * Length(view)), exponent);
}

/// The illumination equation, summed over `lights`: the intensity of the light that
/// leaves `point` toward the viewer, the book's ComputeIllumination.
///
///     i = the sum of the intensities I of the ambient lights
///     for each point light, with L = position - point,
///     and each directional light, with L = direction:
///         i += DiffuseReflection(I, N, L)
///         i += SpecularReflection(I, N, L, V, s)       unless s is kMatte
///
/// `normal` is the unit normal N of the surface at the point, `view` the direction V
/// from the point to the viewer, of any length, and `specular` the surface's exponent s.
/// The result multiplies the surface's color; it is not clamped and may exceed 1 where
/// several lights add up or a highlight sits on a lit surface.
///
/// Every light reaches the point: nothing is in the way of anything, so there are no
/// shadows. The point, the two vectors and the lights are all in one system of
/// coordinates, whichever it is. Precondition: |normal| = 1, asserted.
[[nodiscard]] double ComputeIllumination(Vec3 point, Vec3 normal, Vec3 view, double specular,
                                         std::span<const Light> lights);

}  // namespace cgfs
