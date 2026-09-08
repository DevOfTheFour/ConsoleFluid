#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ------------------------------------------------------------
 * Terminal
 * ------------------------------------------------------------ */

#define SCREEN_W 80
#define SCREEN_H 25

/* ------------------------------------------------------------
 * Simulation
 * ------------------------------------------------------------ */

#define MAX_PARTICLES 3000 // default: 3000

#define SMOOTH_RADIUS       2.20f // default: 2.20f
#define REST_DENSITY        1.40f // default: 1.40f
#define PRESSURE_FACTOR     0.32f // default: 0.32f
#define VISCOSITY_FACTOR    0.12f // default: 0.12f
#define GRAVITY_FACTOR      0.18f // default: 0.18f
#define WALL_REPULSION      1.50f // default: 1.50f

#define TIME_STEP           0.25f // default: 0.25f
#define VELOCITY_DAMPING    0.998f // default: 0.998f
#define MAX_SPEED           2.0f // default: 2.0f

#define PARTICLE_RADIUS 0.32f // default: 0.32f
#define COLLISION_SUBSTEPS 4 // default: 4

#define WALL_BOUNCE   0.10f // default: 0.10f
#define WALL_FRICTION 0.85f // default: 0.85f

/*
 * Spatial acceleration grid.
 *
 * CELL_SIZE >= SMOOTH_RADIUS means that looking through
 * the surrounding 3x3 cells is sufficient.
 */
#define CELL_SIZE 3.0f // default: 3.0f
#define GRID_W 27 // default: 27
#define GRID_H 9 // default: 9

typedef struct Particle {
    float x;
    float y;

    float vx;
    float vy;

    float fx;
    float fy;

    float density;
    float pressure;

    int wall;
} Particle;

static Particle particles[MAX_PARTICLES];
static Particle initial_particles[MAX_PARTICLES];
static unsigned char wall_map[SCREEN_H][SCREEN_W];

static int particle_count = 0;

/*
 * Linked-list spatial grid:
 *
 * grid_head[cell] -> particle index
 * grid_next[i]    -> next particle in same cell
 */
static int grid_head[GRID_W * GRID_H];
static int grid_next[MAX_PARTICLES];


/* ------------------------------------------------------------
 * Utility
 * ------------------------------------------------------------ */

static float clamp_float(float value, float min_value, float max_value)
{
    if (value < min_value) {
        return min_value;
    }

    if (value > max_value) {
        return max_value;
    }

    return value;
}


/* ------------------------------------------------------------
 * Console rendering
 * ------------------------------------------------------------ */

static HANDLE console_output;
static CONSOLE_CURSOR_INFO old_cursor_info;
static WORD old_attributes;

static void console_init(void)
{
    CONSOLE_SCREEN_BUFFER_INFO info;

    console_output = GetStdHandle(STD_OUTPUT_HANDLE);

    if (console_output == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Cannot access Windows console.\n");
        exit(EXIT_FAILURE);
    }

    if (!GetConsoleScreenBufferInfo(console_output, &info)) {
        fprintf(stderr, "stdout is not a Windows console.\n");
        exit(EXIT_FAILURE);
    }

    old_attributes = info.wAttributes;

    GetConsoleCursorInfo(console_output, &old_cursor_info);

    CONSOLE_CURSOR_INFO cursor = old_cursor_info;
    cursor.bVisible = FALSE;

    SetConsoleCursorInfo(console_output, &cursor);

    SetConsoleTitleA(
        "Windows SPH Fluid Simulator | ESC = exit | R = restart"
    );
}

static void console_shutdown(void)
{
    SetConsoleTextAttribute(console_output, old_attributes);
    SetConsoleCursorInfo(console_output, &old_cursor_info);
}

static WORD fluid_color(float density)
{
    if (density < 1.3f) {
        return BACKGROUND_BLUE;
    }

    if (density < 1.8f) {
        return
            BACKGROUND_BLUE |
            BACKGROUND_GREEN;
    }

    if (density < 2.5f) {
        return
            BACKGROUND_BLUE |
            BACKGROUND_GREEN |
            BACKGROUND_INTENSITY;
    }

    return
        BACKGROUND_RED |
        BACKGROUND_GREEN |
        BACKGROUND_BLUE;
}

static void render(void)
{
    static CHAR_INFO screen[SCREEN_W * SCREEN_H];

    static int fluid_count[SCREEN_W * SCREEN_H];
    static float fluid_density[SCREEN_W * SCREEN_H];

    int i;

    memset(fluid_count, 0, sizeof(fluid_count));
    memset(fluid_density, 0, sizeof(fluid_density));

    /*
     * Clear framebuffer.
     */
    for (i = 0; i < SCREEN_W * SCREEN_H; ++i) {
        screen[i].Char.AsciiChar = ' ';
        screen[i].Attributes = 0;
    }

    /*
     * First pass: walls.
     */
    for (i = 0; i < particle_count; ++i) {
        Particle *p = &particles[i];

        if (!p->wall) {
            continue;
        }

        int x = (int)p->x;
        int y = (int)p->y;

        if (
            x >= 0 &&
            x < SCREEN_W &&
            y >= 0 &&
            y < SCREEN_H
        ) {
            int index = y * SCREEN_W + x;

            screen[index].Char.AsciiChar = '#';

            screen[index].Attributes =
                FOREGROUND_RED |
                FOREGROUND_GREEN |
                FOREGROUND_BLUE |
                FOREGROUND_INTENSITY;
        }
    }

    /*
     * Accumulate fluid particles into console cells.
     */
    for (i = 0; i < particle_count; ++i) {
        Particle *p = &particles[i];

        if (p->wall) {
            continue;
        }

        int x = (int)p->x;
        int y = (int)p->y;

        if (
            x >= 0 &&
            x < SCREEN_W &&
            y >= 0 &&
            y < SCREEN_H
        ) {
            int index = y * SCREEN_W + x;

            fluid_count[index]++;

            if (p->density > fluid_density[index]) {
                fluid_density[index] = p->density;
            }
        }
    }

    /*
     * Render fluid as colored background.
     */
    for (i = 0; i < SCREEN_W * SCREEN_H; ++i) {
        if (fluid_count[i] == 0) {
            continue;
        }

        /*
         * Do not overwrite wall characters.
         */
        if (screen[i].Char.AsciiChar == '#') {
            continue;
        }

        screen[i].Char.AsciiChar = ' ';
        screen[i].Attributes = fluid_color(fluid_density[i]);
    }

    COORD buffer_size = {
        SCREEN_W,
        SCREEN_H
    };

    COORD buffer_position = {
        0,
        0
    };

    SMALL_RECT destination = {
        0,
        0,
        SCREEN_W - 1,
        SCREEN_H - 1
    };

    WriteConsoleOutputA(
        console_output,
        screen,
        buffer_size,
        buffer_position,
        &destination
    );
}


/* ------------------------------------------------------------
 * Input
 * ------------------------------------------------------------ */

static void add_particle(float x, float y, int wall)
{
    if (particle_count >= MAX_PARTICLES) {
        return;
    }

    Particle *p = &particles[particle_count++];

    memset(p, 0, sizeof(*p));

    p->x = x;
    p->y = y;
    p->wall = wall;

    p->density = 1.0f;
}

static int load_scene(FILE *file)
{
    int x = 0;
    int y = 0;
    int ch;

    particle_count = 0;

    memset(wall_map, 0, sizeof(wall_map));

    while ((ch = fgetc(file)) != EOF) {
        if (ch == '\r') {
            continue;
        }

        if (ch == '\n') {
            x = 0;
            y++;

            if (y >= SCREEN_H) {
                break;
            }

            continue;
        }

        if (x >= SCREEN_W) {
            x++;
            continue;
        }

        if (ch == '#') {
            wall_map[y][x] = 1;

            add_particle(
                (float)x + 0.5f,
                (float)y + 0.5f,
                1
            );
        }
        else if (ch != ' ' && ch != '\t') {
            add_particle(
                (float)x + 0.5f,
                (float)y + 0.5f,
                0
            );
        }

        x++;
    }

    memcpy(
        initial_particles,
        particles,
        sizeof(Particle) * particle_count
    );

    return particle_count;
}

static void restart_scene(void)
{
    memcpy(
        particles,
        initial_particles,
        sizeof(Particle) * particle_count
    );
}


/* ------------------------------------------------------------
 * Spatial grid
 * ------------------------------------------------------------ */

static int grid_x(float x)
{
    int gx = (int)(x / CELL_SIZE);

    if (gx < 0) {
        gx = 0;
    }

    if (gx >= GRID_W) {
        gx = GRID_W - 1;
    }

    return gx;
}

static int grid_y(float y)
{
    int gy = (int)(y / CELL_SIZE);

    if (gy < 0) {
        gy = 0;
    }

    if (gy >= GRID_H) {
        gy = GRID_H - 1;
    }

    return gy;
}

static void build_grid(void)
{
    int i;

    for (i = 0; i < GRID_W * GRID_H; ++i) {
        grid_head[i] = -1;
    }

    for (i = 0; i < particle_count; ++i) {
        int gx = grid_x(particles[i].x);
        int gy = grid_y(particles[i].y);

        int cell = gy * GRID_W + gx;

        grid_next[i] = grid_head[cell];
        grid_head[cell] = i;
    }
}


/* ------------------------------------------------------------
 * SPH density
 * ------------------------------------------------------------ */

static void calculate_density(void)
{
    const float radius_squared =
        SMOOTH_RADIUS * SMOOTH_RADIUS;

    int i;

    for (i = 0; i < particle_count; ++i) {
        Particle *a = &particles[i];

        float density = 1.0f;

        int cx = grid_x(a->x);
        int cy = grid_y(a->y);

        int yy;
        int xx;

        for (yy = cy - 1; yy <= cy + 1; ++yy) {
            if (yy < 0 || yy >= GRID_H) {
                continue;
            }

            for (xx = cx - 1; xx <= cx + 1; ++xx) {
                if (xx < 0 || xx >= GRID_W) {
                    continue;
                }

                int j = grid_head[yy * GRID_W + xx];

                while (j != -1) {
                    if (j != i) {
                        Particle *b = &particles[j];

                        float dx = b->x - a->x;
                        float dy = b->y - a->y;

                        float distance_squared =
                            dx * dx + dy * dy;

                        if (
                            distance_squared <
                            radius_squared
                        ) {
                            float distance =
                                sqrtf(distance_squared);

                            float q =
                                1.0f -
                                distance /
                                SMOOTH_RADIUS;

                            density += q * q;
                        }
                    }

                    j = grid_next[j];
                }
            }
        }

        a->density = density;

        a->pressure =
            (density - REST_DENSITY) *
            PRESSURE_FACTOR;

        if (a->pressure < 0.0f) {
            a->pressure = 0.0f;
        }
    }
}


/* ------------------------------------------------------------
 * Pressure + viscosity + gravity
 * ------------------------------------------------------------ */

static void calculate_forces(void)
{
    const float radius_squared =
        SMOOTH_RADIUS * SMOOTH_RADIUS;

    int i;

    for (i = 0; i < particle_count; ++i) {
        Particle *a = &particles[i];

        if (a->wall) {
            a->fx = 0.0f;
            a->fy = 0.0f;
            continue;
        }

        float fx = 0.0f;
        float fy =
            GRAVITY_FACTOR * a->density;

        int cx = grid_x(a->x);
        int cy = grid_y(a->y);

        int yy;
        int xx;

        for (yy = cy - 1; yy <= cy + 1; ++yy) {
            if (yy < 0 || yy >= GRID_H) {
                continue;
            }

            for (xx = cx - 1; xx <= cx + 1; ++xx) {
                if (xx < 0 || xx >= GRID_W) {
                    continue;
                }

                int j = grid_head[yy * GRID_W + xx];

                while (j != -1) {
                    if (j != i) {
                        Particle *b = &particles[j];

                        float dx = b->x - a->x;
                        float dy = b->y - a->y;

                        float distance_squared =
                            dx * dx + dy * dy;

                        if (
                            distance_squared <
                            radius_squared
                        ) {
                            float distance =
                                sqrtf(distance_squared);

                            float nx;
                            float ny;

                            if (distance > 0.0001f) {
                                nx = dx / distance;
                                ny = dy / distance;
                            }
                            else {
                                /*
                                 * Avoid division by zero if two
                                 * particles occupy same position.
                                 */
                                nx =
                                    (i < j)
                                        ? -1.0f
                                        : 1.0f;

                                ny = 0.0f;
                                distance = 0.0001f;
                            }

                            float q =
                                1.0f -
                                distance /
                                SMOOTH_RADIUS;

                            /*
                             * Pressure pushes nearby particles apart.
                             */
                            float pressure_force =
                                (
                                    a->pressure +
                                    b->pressure
                                ) *
                                0.5f *
                                q;

                            fx -= nx * pressure_force;
                            fy -= ny * pressure_force;

                            /*
                             * Viscosity tries to equalize velocity
                             * between nearby fluid particles.
                             */
                            if (!b->wall) {
                                fx +=
                                    (b->vx - a->vx) *
                                    VISCOSITY_FACTOR *
                                    q;

                                fy +=
                                    (b->vy - a->vy) *
                                    VISCOSITY_FACTOR *
                                    q;
                            }

                            /*
                             * Extra collision force against walls.
                             *
                             * This makes ASCII '#' walls much harder
                             * to penetrate than ordinary particles.
                             */
                            if (
                                b->wall &&
                                distance < 1.10f
                            ) {
                                float wall_force =
                                    WALL_REPULSION *
                                    (1.10f - distance);

                                fx -= nx * wall_force;
                                fy -= ny * wall_force;
                            }
                        }
                    }

                    j = grid_next[j];
                }
            }
        }

        a->fx = fx;
        a->fy = fy;
    }
}

static void collide_with_wall_cell(
    Particle *p,
    int cell_x,
    int cell_y
)
{
    float left   = (float)cell_x;
    float right  = (float)cell_x + 1.0f;
    float top    = (float)cell_y;
    float bottom = (float)cell_y + 1.0f;

    /*
     * The point on the AABB closest to the particle's center.
     */
    float nearest_x =
        clamp_float(p->x, left, right);

    float nearest_y =
        clamp_float(p->y, top, bottom);

    float dx = p->x - nearest_x;
    float dy = p->y - nearest_y;

    float distance_squared =
        dx * dx + dy * dy;

    float radius_squared =
        PARTICLE_RADIUS * PARTICLE_RADIUS;

    if (distance_squared >= radius_squared) {
        return;
    }

    float nx;
    float ny;
    float penetration;

    /*
     * Typical case:
     * The particle's center is outside the block.
     */
    if (distance_squared > 0.000001f) {
        float distance = sqrtf(distance_squared);

        nx = dx / distance;
        ny = dy / distance;

        penetration =
            PARTICLE_RADIUS - distance;
    }
    else {
        /*
         * The center is already inside the wall cell.
         *
         * We select the nearest face and push
         * the particle out.
         */
        float dl = p->x - left;
        float dr = right - p->x;
        float dt = p->y - top;
        float db = bottom - p->y;

        nx = -1.0f;
        ny = 0.0f;

        float minimum = dl;

        if (dr < minimum) {
            minimum = dr;
            nx = 1.0f;
            ny = 0.0f;
        }

        if (dt < minimum) {
            minimum = dt;
            nx = 0.0f;
            ny = -1.0f;
        }

        if (db < minimum) {
            minimum = db;
            nx = 0.0f;
            ny = 1.0f;
        }

        penetration =
            minimum + PARTICLE_RADIUS;
    }

    /*
     * Geometrically push the particle.
     */
    p->x += nx * penetration;
    p->y += ny * penetration;

    /*
     * Velocity relative to the wall normal.
     */
    float normal_velocity =
        p->vx * nx +
        p->vy * ny;

    /*
     * Reflect only if the particle is moving TOWARD the wall.
     */
    if (normal_velocity < 0.0f) {
        p->vx -=
            (1.0f + WALL_BOUNCE) *
            normal_velocity *
            nx;

        p->vy -=
            (1.0f + WALL_BOUNCE) *
            normal_velocity *
            ny;
    }

    /*
     * Slight friction along the wall.
     */
    float vn =
        p->vx * nx +
        p->vy * ny;

    float tx =
        p->vx - vn * nx;

    float ty =
        p->vy - vn * ny;

    p->vx =
        vn * nx +
        tx * WALL_FRICTION;

    p->vy =
        vn * ny +
        ty * WALL_FRICTION;
}

static void resolve_wall_collisions(Particle *p)
{
    int min_x =
        (int)floorf(p->x - PARTICLE_RADIUS) - 1;

    int max_x =
        (int)floorf(p->x + PARTICLE_RADIUS) + 1;

    int min_y =
        (int)floorf(p->y - PARTICLE_RADIUS) - 1;

    int max_y =
        (int)floorf(p->y + PARTICLE_RADIUS) + 1;

    int y;
    int x;

    if (min_x < 0) {
        min_x = 0;
    }

    if (min_y < 0) {
        min_y = 0;
    }

    if (max_x >= SCREEN_W) {
        max_x = SCREEN_W - 1;
    }

    if (max_y >= SCREEN_H) {
        max_y = SCREEN_H - 1;
    }

    /*
     * Multiple passes are useful in corners,
     * where a particle touches two walls at the same time.
     */
    int iteration;

    for (iteration = 0; iteration < 3; ++iteration) {
        for (y = min_y; y <= max_y; ++y) {
            for (x = min_x; x <= max_x; ++x) {
                if (wall_map[y][x]) {
                    collide_with_wall_cell(
                        p,
                        x,
                        y
                    );
                }
            }
        }
    }
}

/* ------------------------------------------------------------
 * Integration
 * ------------------------------------------------------------ */

static void integrate(void)
{
    int i;

    for (i = 0; i < particle_count; ++i) {
        Particle *p = &particles[i];

        if (p->wall) {
            continue;
        }

        float inverse_density =
            1.0f /
            (
                p->density > 0.001f
                    ? p->density
                    : 0.001f
            );

        /*
         * First, we update the speed.
         */
        p->vx +=
            p->fx *
            inverse_density *
            TIME_STEP;

        p->vy +=
            p->fy *
            inverse_density *
            TIME_STEP;

        p->vx *= VELOCITY_DAMPING;
        p->vy *= VELOCITY_DAMPING;

        /*
         * Maximum speed limit.
         */
        float speed_squared =
            p->vx * p->vx +
            p->vy * p->vy;

        if (speed_squared > MAX_SPEED * MAX_SPEED) {
            float speed = sqrtf(speed_squared);

            p->vx =
                p->vx /
                speed *
                MAX_SPEED;

            p->vy =
                p->vy /
                speed *
                MAX_SPEED;
        }

        /*
         * A very important point:
         *
         * Break the movement down into several small
         * steps so that the particle cannot "teleport"
         * through a thin wall.
         */
        float sub_dt =
            TIME_STEP /
            (float)COLLISION_SUBSTEPS;

        int step;

        for (
            step = 0;
            step < COLLISION_SUBSTEPS;
            ++step
        ) {
            p->x += p->vx * sub_dt;
            p->y += p->vy * sub_dt;

            resolve_wall_collisions(p);

            /*
             * Additional screen edge protection.
             */
            if (p->x < PARTICLE_RADIUS) {
                p->x = PARTICLE_RADIUS;

                if (p->vx < 0.0f) {
                    p->vx *= -WALL_BOUNCE;
                }
            }

            if (
                p->x >
                SCREEN_W - PARTICLE_RADIUS
            ) {
                p->x =
                    SCREEN_W - PARTICLE_RADIUS;

                if (p->vx > 0.0f) {
                    p->vx *= -WALL_BOUNCE;
                }
            }

            if (p->y < PARTICLE_RADIUS) {
                p->y = PARTICLE_RADIUS;

                if (p->vy < 0.0f) {
                    p->vy *= -WALL_BOUNCE;
                }
            }

            if (
                p->y >
                SCREEN_H - PARTICLE_RADIUS
            ) {
                p->y =
                    SCREEN_H - PARTICLE_RADIUS;

                if (p->vy > 0.0f) {
                    p->vy *= -WALL_BOUNCE;
                }
            }
        }
    }
}


/* ------------------------------------------------------------
 * Simulation step
 * ------------------------------------------------------------ */

static void simulation_step(void)
{
    build_grid();
    calculate_density();
    calculate_forces();
    integrate();
}


/* ------------------------------------------------------------
 * Main
 * ------------------------------------------------------------ */

int main(int argc, char **argv)
{
    FILE *input = stdin;

    if (argc >= 2) {
        input = fopen(argv[1], "rb");

        if (!input) {
            fprintf(
                stderr,
                "Cannot open file: %s\n",
                argv[1]
            );

            return EXIT_FAILURE;
        }
    }

    int loaded = load_scene(input);

    if (input != stdin) {
        fclose(input);
    }

    if (loaded == 0) {
        fprintf(
            stderr,
            "No particles found in input.\n"
            "\n"
            "Use '#' for walls and any other "
            "non-space character for fluid.\n"
        );

        return EXIT_FAILURE;
    }

    console_init();

    int previous_r = 0;

    for (;;) {
        /*
         * ESC exits.
         */
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
            break;
        }

        /*
         * R restarts simulation.
         */
        int current_r =
            (GetAsyncKeyState('R') & 0x8000) != 0;

        if (current_r && !previous_r) {
            restart_scene();
        }

        previous_r = current_r;

        simulation_step();
        render();

        /*
         * Roughly 60 FPS.
         */
        Sleep(16);
    }

    console_shutdown();

    return EXIT_SUCCESS;
}