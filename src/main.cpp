// ============================================================
//  Highway Racer  —  OpenGL 3.3 Core  |  GLFW + GLM + GLAD
//
//  Group presentation split:
//    globals.h            — shared state, constants, data structures
//    tharusha_lighting.h  — Lighting & Photoreal Rendering    (👤 Tharusha)
//    dulsi_modelling.h    — Modelling & Transformation         (👤 Dulsi)
//    piratheepan_raster.h — Raster Graphics, Z-Buffer, HUD    (👤 Piratheepan)
//    hephzibah_animation.h— Animation, Clipping & CV          (👤 Hephzibah)
//    pipuni_rendering.h   — Rendering & Graphics Pipeline      (👤 Pipuni)
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

// ── Member files (included in dependency order) ──────────────
#include "globals.h"             // shared globals — must be first
#include "tharusha_lighting.h"   // drawMesh / setWorldUniforms — before dulsi
#include "dulsi_modelling.h"     // drawRoad / drawCar / drawTree — uses drawMesh
#include "piratheepan_raster.h"  // HUD, pixel font, screen overlays
#include "hephzibah_animation.h" // handleInput / updateGame / spawn
#include "pipuni_rendering.h"    // render() + pipeline shaders — last (calls all above)

// ─────────────────────────────────────────────────────────────
//  Window resize callback
// ─────────────────────────────────────────────────────────────
static void onResize(GLFWwindow*, int w, int h)
{
    if(w<1)w=1; if(h<1)h=1;
    SCR_W=w; SCR_H=h;
    glViewport(0,0,w,h);
}

// ─────────────────────────────────────────────────────────────
//  Entry point
// ─────────────────────────────────────────────────────────────
int main()
{
    srand((unsigned)time(nullptr));
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES,4);

    GLFWwindow* win=glfwCreateWindow(SCR_W,SCR_H,"Highway Racer",nullptr,nullptr);
    if(!win){fprintf(stderr,"GLFW failed\n");return 1;}
    glfwMakeContextCurrent(win);
    glfwSetFramebufferSizeCallback(win,onResize);
    glfwSwapInterval(1);

    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)){
        fprintf(stderr,"GLAD failed\n");return 1;}
    glEnable(GL_MULTISAMPLE);

    // Compile shader programs
    pWorld  = makeProgram(VS_WORLD,  FS_WORLD);
    pSky    = makeProgram(VS_SKY,    FS_SKY);
    pBright = makeProgram(VS_QUAD,   FS_BRIGHT);
    pBlur   = makeProgram(VS_QUAD,   FS_BLUR);
    pComp   = makeProgram(VS_QUAD,   FS_COMP);
    pHud    = makeProgram(VS_HUD,    FS_HUD);

    // Create geometry buffers
    initQuad();
    meshRoad      = makeRoadQuad(ROAD_HALF, 200.f);
    meshCarBody   = makeCuboid(1.f,1.f,1.f);  // unit cuboid — scaled per draw
    meshWheel     = makeCuboid(1.f,1.f,1.f);
    meshDash      = makeCuboid(1.f,1.f,1.f);
    meshKerb      = makeCuboid(1.f,1.f,1.f);
    meshTreeTrunk = makeCuboid(1.f,1.f,1.f);
    meshTreeLeaves= makeCuboid(1.f,1.f,1.f);
    meshRail      = makeCuboid(1.f,1.f,1.f);

    // Create HDR framebuffers
    sceneFBO = makeFBO(SCR_W,SCR_H);
    bloomA   = makeFBO(SCR_W,SCR_H);
    bloomB   = makeFBO(SCR_W,SCR_H);

    double prev=glfwGetTime();

    // ── Render loop — 60 FPS frame submission ─────────────────
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

        glfwSwapBuffers(win);  // send completed frame to screen
    }
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
