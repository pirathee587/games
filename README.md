# Highway Racer - OpenGL 3.3 Core

A pseudo-3D highway racing game built with C++17, OpenGL 3.3 Core Profile, GLFW, GLAD, and GLM. It uses a perspective camera, directional lighting, bloom, motion blur, fog, and a full HUD.

## Screenshots / Feature Highlights

| Feature          | Details                                                             |
| ---------------- | ------------------------------------------------------------------- |
| Perspective road | `glm::perspective` + `lookAt` camera behind player                  |
| Lighting         | Directional sun, ambient + diffuse + specular (Blinn-Phong)         |
| Wet road         | Specular wetness ramp; road darkens over time                       |
| Bloom            | HDR threshold -> 6-pass Gaussian blur -> additive composite         |
| Motion blur      | Screen-edge vignette darkening at high speed / nitro                |
| Fog              | Exponential depth fog; density grows after 300 m                    |
| Day->Night       | Sky gradient and sun elevation shift as you race further            |
| Nitro boost      | Hold Shift/N - flames + orange motion blur + FOV expansion          |
| Slow-motion      | Hold Ctrl/S - cyan tint, 0.35x time scale (5 s max)                 |
| Coins            | Spinning golden discs; +50 pts each                                 |
| Traffic          | Cars + trucks in 4 lanes; random vivid colours                      |
| Guardrails       | Alternating red/white kerbs + silver rail posts                     |
| Roadside trees   | 3-layer pine silhouettes                                            |
| HUD              | 7-segment speedometer, nitro bar, lives, score, coin count, minimap |
| Camera shake     | On collision (invincibility grace period follows)                   |

## Prerequisites

| Tool         | Version                                                           |
| ------------ | ----------------------------------------------------------------- |
| C++ compiler | GCC >= 11 / Clang >= 14 / MSVC >= 2019                            |
| CMake        | >= 3.16                                                           |
| GLFW         | 3.x static lib (`lib/libglfw3.a` on Windows)                      |
| GLAD         | pre-generated for OpenGL 3.3 Core (`src/glad.c`, `include/glad/`) |
| GLM          | header-only (`include/glm/`)                                      |

## Expected directory layout

```text
OpenGLProject/
├── CMakeLists.txt
├── src/
│   ├── main.cpp       <- this file
│   └── glad.c
├── include/
│   ├── glad/
│   │   └── glad.h
│   ├── KHR/
│   │   └── khrplatform.h
│   ├── GLFW/
│   │   └── glfw3.h
│   └── glm/           <- full GLM header tree
└── lib/
    └── libglfw3.a     (Windows: glfw3.lib)
```

## Build (Windows - MinGW-w64)

```bat
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
mingw32-make -j4
.\OpenGLProject.exe
```

## Build (Linux)

```bash
sudo apt install libglfw3-dev libgl-dev   # Debian/Ubuntu
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
./OpenGLProject
```

## Build (macOS - Homebrew)

```bash
brew install glfw cmake
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.logicalcpu)
./OpenGLProject
```

## Controls

| Key       | Action                                             |
| --------- | -------------------------------------------------- |
| Left / A  | Steer left                                         |
| Right / D | Steer right                                        |
| Shift / N | Nitro boost (hold) - consumes fuel, auto-recharges |
| Ctrl / S  | Slow-motion toggle (5 s max)                       |
| Space     | Start game / restart from menu                     |
| Escape    | Quit                                               |

## Game Rules

- Avoid all traffic cars and trucks - you have 3 lives.
- Collect golden coins scattered across lanes (+50 pts each).
- Speed increases gradually; survive 2000 m to win.
- After 200 m the road gets wet (shiny specular reflections).
- After 300 m fog rolls in, thickening as you go.
- The sky shifts from daytime blue to a sunset/night gradient.

## Architecture Overview

```text
main()
 └─ Game loop
     ├─ handleInput()      keyboard -> playerTargetX, nitro, slowmo flags
     ├─ updateGame()       physics, collision, spawning, weather
     └─ render()
         ├─ Scene pass  ->  sceneFBO   (road, trees, traffic, player, coins)
         ├─ Bloom pass  ->  bloomFBO   (threshold + 6x Gaussian ping-pong)
         ├─ Composite   ->  default FB (bloom add, tone-map, vignette, blur)
         └─ HUD         ->  screen-space quads + 7-segment digit renderer
```

### Shaders

| Name                     | Purpose                                                    |
| ------------------------ | ---------------------------------------------------------- |
| `VS_SCENE / FS_SCENE`    | Main world objects - Blinn-Phong, fog, emissive, rim light |
| `VS_QUAD / FS_THRESHOLD` | Bloom bright-pass extraction                               |
| `VS_QUAD / FS_BLUR`      | Separable Gaussian blur (horizontal / vertical pass)       |
| `VS_QUAD / FS_COMPOSITE` | HDR -> LDR, bloom add, motion blur, slow-mo tint, vignette |
| `VS_HUD / FS_HUD`        | Screen-space pixel-coordinate UI rectangles                |

## Extending the Game

- Textures - add `stb_image.h`, load asphalt/car PNGs, pass via `sampler2D`.
- Sound - integrate OpenAL or miniaudio for engine/coin/crash SFX.
- More lanes - change `NUM_LANES` (road geometry scales automatically).
- Bridges / tunnels - add segment type to road chunks; tunnel uses a box mesh.
- Power-ups - extend the `Coin` struct with a `type` enum (shield, magnet...).
- Leaderboard - write `score` to a local file on game-over.
"# games" 
