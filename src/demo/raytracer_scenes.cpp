#include <cgfs/demo/raytracer_scenes.h>

#include <cmath>
#include <numbers>
#include <vector>

// These scenes are shared by render_demo and the golden-image tests. Changing them
// changes the reference images in tests/demo/golden/.

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

Scene MirrorBallScene() {
    Scene scene{
        .lights =
            {
                AmbientLight{.intensity = 0.2},
                PointLight{.intensity = 0.7, .position = {-4.0, 5.0, 2.0}},   // key, front left
                PointLight{.intensity = 0.5, .position = {5.0, 4.0, 11.0}},   // rim, back right
                DirectionalLight{.intensity = 0.15, .direction = {0.0, 1.0, 0.0}},  // top fill
            },
        .background_color = {50.0, 55.0, 90.0},  // dusk sky
    };

    // A dark polished floor, the same huge sphere as in the book's scene.
    scene.spheres.push_back({.center = {0.0, -5001.0, 0.0},
                             .radius = 5000.0,
                             .color = {90.0, 90.0, 100.0},
                             .specular = 1000.0,
                             .reflective = 0.5});

    // The chrome ball rests on the floor in the middle of the picture.
    const Vec3 ball_center{0.0, 0.5, 7.0};
    scene.spheres.push_back({.center = ball_center,
                             .radius = 1.5,
                             .color = {230.0, 230.0, 240.0},
                             .specular = 1000.0,
                             .reflective = 0.6});

    // Six spheres evenly spaced around the ball, each with its own finish: matte-ish,
    // glossy or mirror-like.
    struct Finish {
        Color color;
        double specular;
        double reflective;
    };
    const Finish ring[] = {
        {{220.0, 30.0, 60.0}, 500.0, 0.3},    // ruby
        {{250.0, 160.0, 30.0}, 100.0, 0.1},   // amber
        {{120.0, 220.0, 60.0}, 1000.0, 0.5},  // lime
        {{30.0, 190.0, 200.0}, 500.0, 0.3},   // teal
        {{40.0, 80.0, 230.0}, 100.0, 0.1},    // sapphire
        {{220.0, 60.0, 200.0}, 1000.0, 0.5},  // magenta
    };
    constexpr double kRingRadius = 3.0;
    constexpr double kSmallRadius = 0.6;
    for (int i = 0; i < 6; ++i) {
        const double angle = i * std::numbers::pi / 3;  // 0, 60, 120, ... degrees
        scene.spheres.push_back({.center = {ball_center.x + kRingRadius * std::cos(angle),
                                            -1.0 + kSmallRadius,  // resting on the floor
                                            ball_center.z + kRingRadius * std::sin(angle)},
                                 .radius = kSmallRadius,
                                 .color = ring[i].color,
                                 .specular = ring[i].specular,
                                 .reflective = ring[i].reflective});
    }

    // Three marbles in the foreground: pearl, gold and black glass.
    constexpr double kMarbleRadius = 0.25;
    constexpr double kMarbleY = -1.0 + kMarbleRadius;
    scene.spheres.push_back({.center = {-1.4, kMarbleY, 3.4},
                             .radius = kMarbleRadius,
                             .color = {240.0, 235.0, 225.0},
                             .specular = 1000.0,
                             .reflective = 0.4});
    scene.spheres.push_back({.center = {0.4, kMarbleY, 3.0},
                             .radius = kMarbleRadius,
                             .color = {250.0, 200.0, 80.0},
                             .specular = 500.0,
                             .reflective = 0.3});
    scene.spheres.push_back({.center = {1.8, kMarbleY, 3.8},
                             .radius = kMarbleRadius,
                             .color = {20.0, 20.0, 20.0},
                             .specular = 1000.0,
                             .reflective = 0.7});

    // Three spheres floating above the scene: they fill the sky, show up in the chrome
    // ball and drop shadows onto the floor.
    scene.spheres.push_back({.center = {-2.5, 2.5, 8.0},
                             .radius = 0.5,
                             .color = {240.0, 235.0, 225.0},
                             .specular = 500.0,
                             .reflective = 0.2});
    scene.spheres.push_back({.center = {2.5, 3.0, 9.0},
                             .radius = 0.7,
                             .color = {250.0, 120.0, 60.0},
                             .specular = 100.0,
                             .reflective = 0.1});
    scene.spheres.push_back({.center = {0.5, 3.8, 12.0},
                             .radius = 0.9,
                             .color = {160.0, 200.0, 240.0},
                             .specular = 1000.0,
                             .reflective = 0.5});
    return scene;
}

}  // namespace cgfs::demo
