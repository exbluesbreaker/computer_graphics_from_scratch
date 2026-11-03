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

Scene SpecularScene() {
    Scene scene = DiffuseScene();
    // The spheres in the order DiffuseScene builds them.
    scene.spheres[0].specular = 500.0;   // red: shiny
    scene.spheres[1].specular = 500.0;   // blue: shiny
    scene.spheres[2].specular = 10.0;    // green: somewhat shiny
    scene.spheres[3].specular = 1000.0;  // yellow floor: very shiny
    return scene;
}

Scene ReflectiveScene() {
    Scene scene = SpecularScene();
    scene.spheres[0].reflective = 0.2;  // red: a bit reflective
    scene.spheres[1].reflective = 0.3;  // blue: a bit more reflective
    scene.spheres[2].reflective = 0.4;  // green: even more reflective
    scene.spheres[3].reflective = 0.5;  // yellow floor: half reflective
    return scene;
}

}  // namespace cgfs::demo
