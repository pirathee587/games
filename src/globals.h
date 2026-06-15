// ============================================================
//  globals.h  —  Shared state, constants, and data structures
//  All global variables visible to every member file.
// ============================================================
#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>
#include <algorithm>
#include <string>

// ─── Window ───────────────────────────────────────────────────
static int SCR_W = 1280, SCR_H = 720;

// ─── Road constants ───────────────────────────────────────────
static const float ROAD_HALF   = 7.0f;      // road half-width in world X
static const int   NUM_LANES   = 4;
static const float LANE_W      = (ROAD_HALF*2.f)/NUM_LANES;
static const float CAR_HW      = 0.55f;     // player half-width
static const float CAR_HH      = 1.1f;      // player half-height (depth)

inline float laneX(int l){ return -ROAD_HALF + LANE_W*0.5f + l*LANE_W; }

// ─── Game state ───────────────────────────────────────────────
enum GameState { GS_MENU, GS_PLAYING, GS_DEAD, GS_WIN };
static GameState gState = GS_MENU;

static float playerX      = 0.f;
static float playerTargetX= 0.f;
static float baseSpeed    = 10.f;
static float distance     = 0.f;
static int   score        = 0;
static int   coinCount    = 0;
static int   lives        = 3;

static float nitroFuel    = 1.f;
static bool  nitroOn      = false;
static bool  slowOn       = false;
static float slowTimer    = 0.f;

static bool  invincible   = false;
static float invTimer     = 0.f;
static float shakeAmt     = 0.f;

static float deathTimer   = 0.f;
static float gDt          = 0.016f;
static float gTime        = 0.f;

// weather
static float fogDensity   = 0.0f;
static float wetness      = 0.0f;

// ─── Traffic ──────────────────────────────────────────────────
struct TrafficCar {
    float x, z;       // X = lateral,  Z = depth (positive = ahead)
    float speed;
    glm::vec3 color;
    float hw, hh;
    bool  isTruck;
};
static std::vector<TrafficCar> traffic;

// ─── Coins ────────────────────────────────────────────────────
struct Coin { float x, z; bool alive; };
static std::vector<Coin> coins;

// ─── Trees ────────────────────────────────────────────────────
struct Tree { float x, z, scale; };
static std::vector<Tree> trees;

// ─────────────────────────────────────────────────────────────
//  Geometry buffer handle
// ─────────────────────────────────────────────────────────────
struct BoxMesh { GLuint vao, vbo, ebo; };

// ─────────────────────────────────────────────────────────────
//  Framebuffer handle
// ─────────────────────────────────────────────────────────────
struct FBO { GLuint fbo, tex, rbo; };

// ─────────────────────────────────────────────────────────────
//  Program handles + shared meshes (defined in main, used everywhere)
// ─────────────────────────────────────────────────────────────
static GLuint pWorld, pSky, pBright, pBlur, pComp, pHud;
static FBO sceneFBO, bloomA, bloomB;

// Shared meshes
static BoxMesh meshRoad;          // giant flat quad
static BoxMesh meshCarBody;       // unit 1×1×1 cuboid (scaled per draw)
static BoxMesh meshWheel;
static BoxMesh meshDash;          // lane dash strip
static BoxMesh meshKerb;
static BoxMesh meshTreeTrunk;
static BoxMesh meshTreeLeaves;
static BoxMesh meshRail;

// ─── View-Projection and camera (updated each frame in render) ─
static glm::mat4 gVP;
static glm::vec3 gCam;

// ─── 2D quad VAO used for sky, post-process, and HUD ──────────
static GLuint gQuadVAO, gQuadVBO;
