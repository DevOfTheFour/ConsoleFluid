# ConsoleFluid

A small real-time fluid simulation for the Windows console.

The simulation uses a simplified SPH-like particle system with pressure,
viscosity, gravity, and solid wall collisions.

Scenes are loaded from simple text files.

## Features

- Real-time particle-based fluid simulation
- Pressure and viscosity
- Gravity
- Solid wall collisions
- Windows console rendering
- Simple text-based scene format
- Several example scenes
- No external libraries required

## Requirements

- Windows 10 or newer
- GCC / MinGW
- C99-compatible compiler

## Build

Using GCC:

```sh
gcc -std=c99 -O2 fluid_win.c -o fluid.exe -lm -luser32
```

Or with the included Makefile:

```sh
make
```

## Run

Pass a scene file as the first argument:

```sh
./fluid.exe scenes/dam.txt
```

For example:

```sh
./fluid.exe scenes/funnel.txt
./fluid.exe scenes/cascade.txt
./fluid.exe scenes/maze.txt
```

## Controls

- `ESC` - exit
- `R` - restart the current scene

## Scene format

Scenes are plain text files.

```text
# = solid wall
o = fluid particle
  = empty space
```

Example:

```text
########################################
#                                      #
#    ooooooooo                         #
#    ooooooooo                         #
#    ooooooooo                         #
#                                      #
#                                      #
########################################
```

Any non-space character other than `#` is currently treated as a fluid
particle.

You can also create your own `.txt` scenes and run them the same way.

## Simulation settings

The main simulation parameters are defined near the top of `fluid_win.c`.

You can change them and rebuild the program to experiment with different fluid
behaviour.

For example:

```c
#define SMOOTH_RADIUS       2.20f
#define REST_DENSITY        1.40f
#define PRESSURE_FACTOR     0.32f
#define VISCOSITY_FACTOR    0.12f
#define GRAVITY_FACTOR      0.18f
#define WALL_REPULSION      1.50f

#define TIME_STEP           0.25f
#define VELOCITY_DAMPING    0.998f
#define MAX_SPEED           2.0f
```

Some useful parameters:

- `PRESSURE_FACTOR` - controls how strongly compressed particles push apart
- `VISCOSITY_FACTOR` - controls how strongly nearby particles try to match velocity
- `GRAVITY_FACTOR` - controls gravity strength
- `SMOOTH_RADIUS` - controls the interaction radius between particles
- `TIME_STEP` - controls simulation step size
- `MAX_SPEED` - limits particle velocity for stability
- `WALL_REPULSION` - controls additional repulsion from solid walls

Changing these values can make the fluid behave more like water, a thick liquid,
or a very unstable high-pressure fluid.

## How it works

The fluid is represented by particles.

Each simulation step calculates:

1. Particle density
2. Pressure forces
3. Viscosity forces
4. Gravity
5. Particle movement
6. Collision with solid walls

A small spatial grid is used to reduce the number of particle-to-particle
checks.

The result is rendered directly to the Windows console using the Win32 API.

## Inspiration

This project was inspired by the fluid simulation approach used in the
2012 IOCCC `endoh1` entry by Yusuke Endoh.

ConsoleFluid is a separate implementation written for readability and
native Windows console support.

## Contributing

Contributions are welcome.

Feel free to open an issue or pull request if you want to:

- add new example scenes
- improve the fluid simulation
- improve collision handling
- add new rendering features
- optimize performance
- fix bugs
- add support for other platforms

Please keep changes simple and readable where possible.

## License

This project is licensed under the MIT License.

See [LICENSE](LICENSE) for details.