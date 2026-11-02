#include <cgfs/demo_scenes.h>

// These scenes are shared by render_demo and the golden-image tests. Changing them
// changes the reference images in tests/golden/.

namespace cgfs::demo {

namespace {

std::vector<Sphere> ThreeSpheres() {
    return {
        {.center = {0.0, -1.0, 3.0}, .radius = 1.0, .color = colors::kRed},
        {.center = {2.0, 0.0, 4.0}, .radius = 1.0, .color = colors::kBlue},
        {.center = {-2.0, 0.0, 4.0}, .radius = 1.0, .color = colors::kGreen},
    };
}

}  // namespace

Scene FlatScene() {
    return {
        .spheres = ThreeSpheres(),
        .lights = {AmbientLight{.intensity = 1.0}},
    };
}

Scene DiffuseScene() {
    Scene scene{
        .spheres = ThreeSpheres(),
        .lights =
            {
                AmbientLight{.intensity = 0.2},
                PointLight{.intensity = 0.6, .position = {2.0, 1.0, 0.0}},
                DirectionalLight{.intensity = 0.2, .direction = {1.0, 4.0, 4.0}},
            },
    };
    // At our scale the surface of this sphere is practically flat: a floor.
    scene.spheres.push_back(
        {.center = {0.0, -5001.0, 0.0}, .radius = 5000.0, .color = colors::kYellow});
    return scene;
}

}  // namespace cgfs::demo
