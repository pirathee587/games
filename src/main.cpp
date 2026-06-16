// ============================================================
//  Highway Racer  –  OpenGL 3.3 Core  |  GLFW + GLM + GLAD
//  Clean rewrite: correct 3D road, proper HUD, full menu UI
// ============================================================
//
//  Source split by member topic:
//   tharusha.h    — Lighting & Photoreal Rendering
//   dulsi.h       — Modelling & Transformation
//   hephzibah.h   — Animation, Clipping & Computer Vision
//   piratheepan.h — Raster Graphics, Pixel Processing, Z-Buffer
//   pipuni.h      — Rendering & Graphics Pipeline
//
// ============================================================
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
static bool gFullscreen  = false;
static int  gWinX=100,gWinY=100,gWinW=1280,gWinH=720; // saved windowed pos/size
static GLFWwindow* gWin  = nullptr;

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

static float camZoom      = 1.0f;
static float camZoomTarget= 1.0f;

static bool  accelOn      = false;
static bool  brakeOn      = false;
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

static float fogDensity   = 0.0f;
static float wetness      = 0.0f;

// ─── Game object structs ──────────────────────────────────────
struct TrafficCar {
    float x, z;       // X = lateral,  Z = depth (positive = ahead)
    float speed;
    glm::vec3 color;
    float hw, hh;
    bool  isTruck;
};
static std::vector<TrafficCar> traffic;

struct Coin { float x, z; bool alive; };
static std::vector<Coin> coins;

struct Tree { float x, z, scale; };
static std::vector<Tree> trees;

// ─── Framebuffer object ───────────────────────────────────────
struct FBO { GLuint fbo,tex,rbo; };
static FBO sceneFBO, bloomA, bloomB;

// ─── Box mesh ─────────────────────────────────────────────────
struct BoxMesh { GLuint vao,vbo,ebo; };

// ─── GL program handles ───────────────────────────────────────
static GLuint pWorld, pSky, pBright, pBlur, pComp, pHud;

// ─── Shared meshes ────────────────────────────────────────────
static BoxMesh meshRoad;          // giant flat quad
static BoxMesh meshCarBody;       // unit 1x1x1 cuboid (scaled per draw)
static BoxMesh meshWheel;
static BoxMesh meshDash;          // lane dash strip
static BoxMesh meshKerb;
static BoxMesh meshTreeTrunk;
static BoxMesh meshTreeLeaves;
static BoxMesh meshRail;

// ─── Screen quad (sky + post-process) ────────────────────────
static GLuint gQuadVAO, gQuadVBO;

// ─── Per-frame matrices ───────────────────────────────────────
static glm::mat4 gVP;    // View-Projection (updated each frame)
static glm::vec3 gCam;   // camera world pos

// =============================================================
//  MEMBER FILES  (included in dependency order)
// =============================================================
#include "tharusha.h"     // FS_WORLD + bloom shaders + makeFBO() + setWorldUniforms()
#include "dulsi.h"        // VS_WORLD + makeCuboid() + drawMesh() + drawRoad/Car/Tree()
#include "hephzibah.h"    // spawn*() + resetGame() + handleInput() + updateGame()
#include "piratheepan.h"  // VS_HUD/FS_HUD + PFONT + hudRect() + drawMenu/HUD panels
#include "pipuni.h"       // VS_SKY/VS_QUAD + compileShader() + makeProgram() + render()

// ─────────────────────────────────────────────────────────────
//  MAIN
// ─────────────────────────────────────────────────────────────
int main()
{
    srand((unsigned)time(nullptr));
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES,4);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWwindow* win=glfwCreateWindow(SCR_W,SCR_H,"Highway Racer",nullptr,nullptr);
    if(!win){fprintf(stderr,"GLFW failed\n");return 1;}
    gWin=win;
    glfwMakeContextCurrent(win);
    glfwSetFramebufferSizeCallback(win,onResize);
    glfwSetScrollCallback(win, onScroll);
    glfwSwapInterval(1);

    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)){
        fprintf(stderr,"GLAD failed\n");return 1;}
    glEnable(GL_MULTISAMPLE);

    // Compile GPU programs
    pWorld  = makeProgram(VS_WORLD,  FS_WORLD);
    pSky    = makeProgram(VS_SKY,    FS_SKY);
    pBright = makeProgram(VS_QUAD,   FS_BRIGHT);
    pBlur   = makeProgram(VS_QUAD,   FS_BLUR);
    pComp   = makeProgram(VS_QUAD,   FS_COMP);
    pHud    = makeProgram(VS_HUD,    FS_HUD);

    // Create geometry
    initQuad();
    meshRoad     = makeRoadQuad(ROAD_HALF, 200.f);
    meshCarBody  = makeCuboid(1.f,1.f,1.f);  // unit cuboid — scaled per draw
    meshWheel    = makeCuboid(1.f,1.f,1.f);
    meshDash     = makeCuboid(1.f,1.f,1.f);
    meshKerb     = makeCuboid(1.f,1.f,1.f);
    meshTreeTrunk= makeCuboid(1.f,1.f,1.f);
    meshTreeLeaves=makeCuboid(1.f,1.f,1.f);
    meshRail     = makeCuboid(1.f,1.f,1.f);

    // Create framebuffers
    sceneFBO = makeFBO(SCR_W,SCR_H);
    bloomA   = makeFBO(SCR_W,SCR_H);
    bloomB   = makeFBO(SCR_W,SCR_H);

    double prev=glfwGetTime();

    // ── Main render loop — 60 FPS frame submission ────────────
    while(!glfwWindowShouldClose(win)){
        glfwPollEvents();
        double now=glfwGetTime();
        float dt=(float)(now-prev); if(dt>0.05f)dt=0.05f;
        prev=now;
        gTime=(float)now;
        gDt=dt;

        handleInput(win,dt);
        updateGame(dt);
        render();

        glfwSwapBuffers(win);   // send completed frame to screen
    }
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
