#include <trees.h>
#include <iostream>
#include <vector>

using namespace flecs::components;

using Position = transform::Position3;
using Input = input::Input;
using Color = graphics::Rgb;
using Box = geometry::Box;

static const float CameraAcceleration = 0.2;
static const float CameraDeceleration = 0.1;
static const float CameraMaxSpeed = 0.05;
static const float CameraDistance = 8;
static const float CameraHeight = 6.0;

// Components

struct Game {
    flecs::entity tree_prefab;
    flecs::entity pine_prefab;
    flecs::entity window;        
};

struct CameraController {
    CameraController(float ar = 0, float av = 0, float av_h = 0, 
        float ah = CameraHeight, float ad = CameraDistance, float as = 0)
    {
        r = ar;
        v = av;
        v_h = av_h;
        h = ah;
        d = ad;
        shake = as;
    }

    float r;
    float v;
    float v_h;
    float h;
    float d;
    float shake;
};

struct Canopy { };

float randf(float scale) {
    return ((float)rand() / (float)RAND_MAX) * scale;
}

float decelerate_camera(float v, float delta_time, float max_speed) {
    if (v > 0) {
        v = glm_clamp(v - CameraDeceleration * delta_time, 0, v);
    }
    if (v < 0) {
        v = glm_clamp(v + CameraDeceleration * delta_time, v, 0);
    }

    return glm_clamp(v, -max_speed, max_speed);
}

// Camera movement
void MoveCamera(flecs::iter& it, CameraController *ctrl) {
    auto input = it.column<const Input>(2);
    auto camera = it.column<graphics::Camera>(3);

    // Accelerate camera if keys are pressed
    if (input->keys[ECS_KEY_D].state) {
        ctrl->v -= CameraAcceleration * it.delta_time();
    }
    if (input->keys[ECS_KEY_A].state) {
        ctrl->v += CameraAcceleration * it.delta_time();
    }  
    if (input->keys[ECS_KEY_S].state) {
        ctrl->v_h -= CameraAcceleration * it.delta_time();
    }
    if (input->keys[ECS_KEY_W].state) {
        ctrl->v_h += CameraAcceleration * it.delta_time();
    }

    float max_speed = CameraMaxSpeed;
    if (input->keys[ECS_KEY_SHIFT].state) {
        max_speed /= 8;
    }

    // Decelerate camera each frame
    ctrl->v = decelerate_camera(ctrl->v, it.delta_time(), max_speed);
    ctrl->v_h = decelerate_camera(ctrl->v_h, it.delta_time(), max_speed);

    // Update camera spherical coordinates
    ctrl->r += ctrl->v;
    ctrl->h += ctrl->v_h * 2;
    ctrl->d -= ctrl->v_h;

    camera->position[0] = cos(ctrl->r) * ctrl->d;
    camera->position[1] = ctrl->h;
    camera->position[2] = sin(ctrl->r) * ctrl->d + 5;

    // Camera shake
    camera->position[1] += sin(it.world_time() * 50) * ctrl->shake;
    ctrl->shake *= 0.80;

    camera->lookat[0] = 0;
    camera->lookat[2] = 5;

    if (input->keys[ECS_KEY_MINUS].state) {
        it.world().set_time_scale(it.world().get_time_scale() * 0.95);
    }

    if (input->keys[ECS_KEY_PLUS].state) {
        it.world().set_time_scale(it.world().get_time_scale() * 1.05);
    }
}

// Setup window, camera and light
void init_ui(flecs::world& ecs) {
    graphics::Camera camera_data;
    camera_data.set_position(0, CameraHeight, 0);
    camera_data.set_lookat(0, 0, 5);
    auto camera = ecs.entity("Camera")
        .set<graphics::Camera>(camera_data)
        .set<CameraController>({-GLM_PI / 2, 0});

    graphics::DirectionalLight light_data;
    light_data.set_direction(0.6, 0.8, -0.5);
    light_data.set_color(1.05, 1.00, 0.85);
    auto light = ecs.entity("Sun")
        .set<graphics::DirectionalLight>(light_data);

    gui::Window window_data;
    window_data.width = 1024;
    window_data.height = 800;
    window_data.title = "Flecs Trees Example";
    auto window = ecs.entity().set<gui::Window>(window_data);

    gui::Canvas canvas_data;
    canvas_data.background_color = {0.35, 0.45, 0.75};
    canvas_data.ambient_light = {0.5, 0.5, 0.85};
    canvas_data.camera = camera.id();
    canvas_data.directional_light = light.id();
    window.set<gui::Canvas>(canvas_data);
    ecs.patch<Game>([window](Game& g) {
        g.window = window;
    });
}

// Init level
void init_level(flecs::world& ecs) {
    Game *g = ecs.get_mut<Game>();

    // Grass
    ecs.entity()
        .set<Position>({0, -0.25, 5})
        .set<Color>({0.25, 0.4, 0.18})
        .set<Box>({12, 0.5, 12});

    // Water
    ecs.entity()
        .set<Position>({0, -0.25, 5})
        .set<Color>({0.1, 0.27, 0.42})
        .set<Box>({15, 0.25, 15});        

    // Trees
    for (int x = 0; x < 12; x ++) {
        for (int z = 0; z < 12; z ++) {
            if (randf(1) > 0.6) {
                if (randf(1.0) > 0.5) {
                    ecs.entity().add_instanceof(g->pine_prefab)
                        .set<Position>({x - 5.0f - 0.5f, 0.0, z - 0.5f});
                } else {
                    ecs.entity().add_instanceof(g->tree_prefab)
                        .set<Position>({x - 5.0f - 0.5f, 0.0, z - 0.5f});
                }
            }
        }
    }                                  
}

// Init prefabs
void init_prefabs(flecs::world& ecs) {
    Game *g = ecs.get_mut<Game>();

    auto trunk = ecs.prefab("PTrunk")
        .set<Position>({0, 0.25, 0})
        .set<Box>({0.4, 0.5, 0.4})
        .set<Color>({0.25, 0.2, 0.1});

    auto canopy = ecs.prefab("PCanopy")
        .add<Canopy>()
        .set<Color>({0.35, 0.25, 0.0});

    auto pine_canopy = ecs.prefab("PPineCanopy")
        .add_instanceof(canopy)
        .set<Color>({0.2, 0.3, 0.15});        
    
    g->tree_prefab = ecs.prefab("PTree");
        ecs.prefab()
            .add_childof(g->tree_prefab)
            .add_instanceof(trunk);

        ecs.prefab()
            .add_childof(g->tree_prefab)
            .add_instanceof(canopy)
            .set<Position>({0, 0.9, 0})
            .set<Box>({0.8, 0.8, 0.8});

    g->pine_prefab = ecs.prefab("PPine");
        ecs.prefab()
            .add_childof(g->pine_prefab)
            .add_instanceof(trunk);

        ecs.prefab()
            .add_childof(g->pine_prefab)
            .add_instanceof(pine_canopy)
            .set<Position>({0, 0.6, 0})
            .set<Box>({0.8, 0.4, 0.8});

        ecs.prefab()
            .add_childof(g->pine_prefab)
            .add_instanceof(pine_canopy)
            .set<Position>({0, 1.0, 0})
            .set<Box>({0.6, 0.4, 0.6});

        ecs.prefab()
            .add_childof(g->pine_prefab)
            .add_instanceof(pine_canopy)
            .set<Position>({0, 1.4, 0})
            .set<Box>({0.4, 0.4, 0.4});

    ecs.system<Position, Box>("RandomizeCanopy", "SHARED:Canopy, PARENT:INSTANCEOF|PTree")
        .kind(flecs::OnSet)
        .each([](flecs::entity e, Position &p, Box &b) {
            float h = randf(1.0) + 0.8;
            b.height = h;
            p.y = h / 2.0 + 0.5;
        });
}

void init_systems(flecs::world& ecs) {
    ecs.system<CameraController>(
        "MoveCamera", "$Input, [inout] Camera:flecs.components.graphics.Camera")
        .iter(MoveCamera);
}

int main(int argc, char *argv[]) {
    flecs::world ecs;

    ecs.import<flecs::components::transform>();
    ecs.import<flecs::components::graphics>();
    ecs.import<flecs::components::geometry>();
    ecs.import<flecs::components::gui>();
    ecs.import<flecs::components::input>();
    ecs.import<flecs::systems::transform>();
    ecs.import<flecs::systems::sdl2>();
    ecs.import<flecs::systems::sokol>();

    // Add aliases for components from modules for more readable string queries
    ecs.use<Input>();
    ecs.use<Position>("Position");
    ecs.use<Color>("Color");
    ecs.use<Box>();

    init_ui(ecs);
    init_prefabs(ecs);
    init_level(ecs);
    init_systems(ecs);

    ecs.set_target_fps(60);

    /* Run systems */
    while (ecs.progress()) { }
}
