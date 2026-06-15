// ============================================================
//  Highway Racer  –  OpenGL 3.3 Core  |  GLFW + GLM + GLAD
//  Clean rewrite: correct 3D road, proper HUD, full menu UI
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

// Zoom
static float camZoom      = 1.0f;   // 1.0 = normal, <1 = zoomed in, >1 = zoomed out
static float camZoomTarget= 1.0f;

// Manual speed control
static bool  accelOn      = false;  // W / UP  — accelerate forward
static bool  brakeOn      = false;  // S / DOWN — brake / slow down
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

static void spawnTraffic()
{
    TrafficCar c;
    c.isTruck = (rand()%6==0);
    c.hw      = c.isTruck ? 1.2f : 0.65f;
    c.hh      = c.isTruck ? 3.5f : 1.4f;
    int lane  = rand()%NUM_LANES;
    c.x       = laneX(lane);
    c.z       = 80.f + (rand()%120);      // ahead of player
    c.speed   = 2.f + (rand()%35)/10.f;
    float h   = (rand()%360)/360.f;
    int hi=int(h*6); float f=h*6-hi;
    float r,g,b;
    switch(hi%6){
        case 0:r=1;g=f;b=0;break; case 1:r=1-f;g=1;b=0;break;
        case 2:r=0;g=1;b=f;break; case 3:r=0;g=1-f;b=1;break;
        case 4:r=f;g=0;b=1;break; default:r=1;g=0;b=1-f;break;
    }
    c.color = glm::vec3(r,g,b);
    traffic.push_back(c);
}

// ─── Coins ────────────────────────────────────────────────────
struct Coin { float x, z; bool alive; };
static std::vector<Coin> coins;

static void spawnCoin(){
    Coin c;
    c.x = laneX(rand()%NUM_LANES);
    c.z = 60.f + (rand()%100);
    c.alive = true;
    coins.push_back(c);
}

// ─── Trees ────────────────────────────────────────────────────
struct Tree { float x, z, scale; };
static std::vector<Tree> trees;

static void initTrees(){
    trees.clear();
    for(int i=0;i<80;i++){
        Tree t;
        float side=(i%2==0)?1.f:-1.f;
        t.x = side*(ROAD_HALF + 1.5f + (rand()%8));
        t.z = -20.f + (rand()%250);
        t.scale = 0.8f+(rand()%14)/10.f;
        trees.push_back(t);
    }
}

// ─── Reset ────────────────────────────────────────────────────
static void resetGame(){
    traffic.clear(); coins.clear();
    playerX=0.f; playerTargetX=0.f;
    baseSpeed=10.f; distance=0.f; score=0; coinCount=0; lives=3;
    nitroFuel=1.f; nitroOn=false; slowOn=false; slowTimer=0.f;
    camZoom=1.0f; camZoomTarget=1.0f; accelOn=false; brakeOn=false;
    invincible=false; invTimer=0.f; shakeAmt=0.f;
    fogDensity=0.f; wetness=0.f; deathTimer=0.f;
    for(int i=0;i<10;i++) spawnTraffic();
    for(int i=0;i<6;i++)  spawnCoin();
    initTrees();
}

// ─────────────────────────────────────────────────────────────
//  SHADERS
// ─────────────────────────────────────────────────────────────

// ── World geometry (3D perspective) ──────────────────────────
static const char* VS_WORLD = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNorm;
layout(location=2) in vec2 aUV;

uniform mat4 uMVP;
uniform mat4 uModel;
out vec3 vWorldPos;
out vec3 vNorm;
out vec2 vUV;

void main(){
    vec4 wp = uModel * vec4(aPos,1.0);
    vWorldPos = wp.xyz;
    vNorm = mat3(transpose(inverse(uModel))) * aNorm;
    vUV = aUV;
    gl_Position = uMVP * vec4(aPos,1.0);
}
)";

static const char* FS_WORLD = R"(
#version 330 core
in vec3 vWorldPos;
in vec3 vNorm;
in vec2 vUV;

uniform vec4  uColor;
uniform vec3  uLightDir;
uniform vec3  uCamPos;
uniform float uFogDensity;
uniform vec3  uFogColor;
uniform float uWetness;
uniform float uEmissive;
uniform float uRim;

out vec4 FragColor;

void main(){
    vec3 N = normalize(vNorm);
    vec3 L = normalize(uLightDir);
    float diff = max(dot(N,L),0.0)*0.7 + 0.3;

    vec3 col = uColor.rgb * diff;

    // Specular (wet road / shiny cars)
    if(uWetness>0.0 || uRim>0.0){
        vec3 V = normalize(uCamPos - vWorldPos);
        vec3 H = normalize(L+V);
        float sp = pow(max(dot(N,H),0.0),64.0);
        col += uWetness * sp * 0.8;
        // Rim
        float rim = pow(1.0-max(dot(V,N),0.0), 3.0);
        col += rim * uRim * vec3(0.4,0.7,1.0);
    }

    // Emissive
    col = mix(col, uColor.rgb*2.5, uEmissive);

    // Fog
    float dist = length(vWorldPos - uCamPos);
    float fogF = 1.0-exp(-uFogDensity*0.004*dist*dist);
    col = mix(col, uFogColor, clamp(fogF,0.0,1.0));

    FragColor = vec4(col, uColor.a);
}
)";

// ── Sky (simple gradient fullscreen quad) ────────────────────
static const char* VS_SKY = R"(
#version 330 core
layout(location=0) in vec2 aPos;
out float vY;
void main(){ vY=aPos.y; gl_Position=vec4(aPos,0.999,1.0); }
)";
static const char* FS_SKY = R"(
#version 330 core
in float vY;
uniform vec3 uSkyTop;
uniform vec3 uSkyBot;
out vec4 FragColor;
void main(){
    float t = clamp(vY*0.5+0.5, 0.0,1.0);
    FragColor = vec4(mix(uSkyBot,uSkyTop,t),1.0);
}
)";

// ── Post-process passes ───────────────────────────────────────
static const char* VS_QUAD = R"(
#version 330 core
layout(location=0) in vec2 aPos;
out vec2 vUV;
void main(){ vUV=aPos*0.5+0.5; gl_Position=vec4(aPos,0,1); }
)";

static const char* FS_BRIGHT = R"(
#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
uniform float uThresh;
out vec4 FragColor;
void main(){
    vec3 c=texture(uTex,vUV).rgb;
    float l=dot(c,vec3(0.2126,0.7152,0.0722));
    FragColor=vec4(l>uThresh?c:vec3(0.0),1.0);
}
)";

static const char* FS_BLUR = R"(
#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
uniform vec2 uDir;
out vec4 FragColor;
void main(){
    float w[5]=float[](0.227027,0.194595,0.121622,0.054054,0.016216);
    vec3 r=texture(uTex,vUV).rgb*w[0];
    for(int i=1;i<5;i++){
        r+=texture(uTex,vUV+uDir*float(i)).rgb*w[i];
        r+=texture(uTex,vUV-uDir*float(i)).rgb*w[i];
    }
    FragColor=vec4(r,1.0);
}
)";

static const char* FS_COMP = R"(
#version 330 core
in vec2 vUV;
uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform float uBloom_str;
uniform float uVignette;
uniform float uSlowTint;
out vec4 FragColor;
void main(){
    vec3 col=texture(uScene,vUV).rgb + texture(uBloom,vUV).rgb*uBloom_str;
    col=mix(col, col*vec3(0.55,1.0,1.1), uSlowTint);
    // Reinhard
    col=col/(col+1.0);
    // Vignette
    vec2 uv2=vUV-0.5;
    float v=1.0-dot(uv2,uv2)*uVignette;
    col*=clamp(v,0.0,1.0);
    FragColor=vec4(col,1.0);
}
)";

// ── HUD – screen-space 2D ─────────────────────────────────────
// NDC directly from normalised coords [0,1]
static const char* VS_HUD = R"(
#version 330 core
layout(location=0) in vec2 aPos;   // unit quad [-1,1]
uniform vec2 uPos;    // centre in [0,1] NDC space
uniform vec2 uHalf;   // half-size in [0,1]
void main(){
    // aPos is [-1,1]; map to [uPos-uHalf, uPos+uHalf] in [0,1]
    vec2 p = uPos + aPos * uHalf;
    // [0,1] -> NDC [-1,1], flip Y (screen top=0)
    vec2 ndc = p * 2.0 - 1.0;
    ndc.y = -ndc.y;
    gl_Position = vec4(ndc, 0.0, 1.0);
}
)";
static const char* FS_HUD = R"(
#version 330 core
uniform vec4 uColor;
out vec4 FragColor;
void main(){ FragColor=uColor; }
)";

// ─────────────────────────────────────────────────────────────
//  GL helpers
// ─────────────────────────────────────────────────────────────
static GLuint compileShader(GLenum type,const char* src){
    GLuint s=glCreateShader(type);
    glShaderSource(s,1,&src,nullptr);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok){char buf[2048];glGetShaderInfoLog(s,2048,nullptr,buf);
            fprintf(stderr,"SHADER ERR:\n%s\n",buf);}
    return s;
}
static GLuint makeProgram(const char* vs,const char* fs){
    GLuint p=glCreateProgram();
    GLuint v=compileShader(GL_VERTEX_SHADER,vs);
    GLuint f=compileShader(GL_FRAGMENT_SHADER,fs);
    glAttachShader(p,v); glAttachShader(p,f);
    glLinkProgram(p);
    glDeleteShader(v); glDeleteShader(f);
    return p;
}

// ─────────────────────────────────────────────────────────────
//  Geometry buffers
// ─────────────────────────────────────────────────────────────
// Box mesh: 8 verts, 36 indices  (pos+norm+uv)
struct BoxMesh { GLuint vao,vbo,ebo; };

static BoxMesh makeCuboid(float hw,float hh,float hd)
{
    // 6 faces × 4 verts, each: pos(3)+norm(3)+uv(2) = 8 floats
    float v[]={
        // +Z face (front)
        -hw,-hh, hd, 0,0,1, 0,0,
         hw,-hh, hd, 0,0,1, 1,0,
         hw, hh, hd, 0,0,1, 1,1,
        -hw, hh, hd, 0,0,1, 0,1,
        // -Z face (back)
        -hw,-hh,-hd, 0,0,-1,1,0,
         hw,-hh,-hd, 0,0,-1,0,0,
         hw, hh,-hd, 0,0,-1,0,1,
        -hw, hh,-hd, 0,0,-1,1,1,
        // +X face
         hw,-hh,-hd, 1,0,0, 0,0,
         hw,-hh, hd, 1,0,0, 1,0,
         hw, hh, hd, 1,0,0, 1,1,
         hw, hh,-hd, 1,0,0, 0,1,
        // -X face
        -hw,-hh,-hd,-1,0,0, 1,0,
        -hw,-hh, hd,-1,0,0, 0,0,
        -hw, hh, hd,-1,0,0, 0,1,
        -hw, hh,-hd,-1,0,0, 1,1,
        // +Y face (top)
        -hw, hh,-hd, 0,1,0, 0,0,
        -hw, hh, hd, 0,1,0, 0,1,
         hw, hh, hd, 0,1,0, 1,1,
         hw, hh,-hd, 0,1,0, 1,0,
        // -Y face (bottom)
        -hw,-hh,-hd, 0,-1,0,0,0,
        -hw,-hh, hd, 0,-1,0,0,1,
         hw,-hh, hd, 0,-1,0,1,1,
         hw,-hh,-hd, 0,-1,0,1,0,
    };
    unsigned idx[36];
    for(int f=0;f<6;f++){
        int b=f*4;
        idx[f*6+0]=b+0; idx[f*6+1]=b+1; idx[f*6+2]=b+2;
        idx[f*6+3]=b+0; idx[f*6+4]=b+2; idx[f*6+5]=b+3;
    }
    BoxMesh m;
    glGenVertexArrays(1,&m.vao); glGenBuffers(1,&m.vbo); glGenBuffers(1,&m.ebo);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER,m.vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(v),v,GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(idx),idx,GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,8*4,(void*)0);    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,8*4,(void*)(3*4));glEnableVertexAttribArray(1);
    glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,8*4,(void*)(6*4));glEnableVertexAttribArray(2);
    return m;
}

// Flat quad (XZ plane) for road surface
static BoxMesh makeRoadQuad(float hw,float hd)
{
    float v[]={
        -hw,0,-hd, 0,1,0, 0,0,
         hw,0,-hd, 0,1,0, 1,0,
         hw,0, hd, 0,1,0, 1,1,
        -hw,0, hd, 0,1,0, 0,1,
    };
    unsigned idx[]={0,1,2,0,2,3};
    BoxMesh m;
    glGenVertexArrays(1,&m.vao); glGenBuffers(1,&m.vbo); glGenBuffers(1,&m.ebo);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER,m.vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(v),v,GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(idx),idx,GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,8*4,(void*)0);    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,8*4,(void*)(3*4));glEnableVertexAttribArray(1);
    glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,8*4,(void*)(6*4));glEnableVertexAttribArray(2);
    return m;
}

// Simple unit [-1,1] quad (2D, for sky + HUD)
static GLuint gQuadVAO,gQuadVBO;
static void initQuad(){
    float q[]={-1,-1, 1,-1, 1,1, -1,-1, 1,1, -1,1};
    glGenVertexArrays(1,&gQuadVAO); glGenBuffers(1,&gQuadVBO);
    glBindVertexArray(gQuadVAO);
    glBindBuffer(GL_ARRAY_BUFFER,gQuadVBO);
    glBufferData(GL_ARRAY_BUFFER,sizeof(q),q,GL_STATIC_DRAW);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,nullptr);
    glEnableVertexAttribArray(0);
}

// ─────────────────────────────────────────────────────────────
//  Framebuffers
// ─────────────────────────────────────────────────────────────
struct FBO { GLuint fbo,tex,rbo; };
static FBO makeFBO(int w,int h){
    FBO f;
    glGenFramebuffers(1,&f.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER,f.fbo);
    glGenTextures(1,&f.tex);
    glBindTexture(GL_TEXTURE_2D,f.tex);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB16F,w,h,0,GL_RGB,GL_FLOAT,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,f.tex,0);
    glGenRenderbuffers(1,&f.rbo);
    glBindRenderbuffer(GL_RENDERBUFFER,f.rbo);
    glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,w,h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,f.rbo);
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    return f;
}

// ─────────────────────────────────────────────────────────────
//  Program handles + shared meshes
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

// ─────────────────────────────────────────────────────────────
//  World draw helpers
// ─────────────────────────────────────────────────────────────
static glm::mat4 gVP;    // View-Projection (updated each frame)
static glm::vec3 gCam;   // camera world pos

static void setWorldUniforms(GLuint prog,
                              glm::vec4 col, float wet=0,
                              float emis=0, float rim=0)
{
    glm::vec3 fogCol = glm::mix(glm::vec3{0.72f,0.85f,0.95f},
                                glm::vec3{0.45f,0.45f,0.50f}, fogDensity);
    glUniform4fv(glGetUniformLocation(prog,"uColor"),1,glm::value_ptr(col));
    glUniform3f(glGetUniformLocation(prog,"uLightDir"), 0.5f,1.0f,0.3f);
    glUniform3fv(glGetUniformLocation(prog,"uCamPos"),1,glm::value_ptr(gCam));
    glUniform1f(glGetUniformLocation(prog,"uFogDensity"),fogDensity);
    glUniform3fv(glGetUniformLocation(prog,"uFogColor"),1,glm::value_ptr(fogCol));
    glUniform1f(glGetUniformLocation(prog,"uWetness"),wet);
    glUniform1f(glGetUniformLocation(prog,"uEmissive"),emis);
    glUniform1f(glGetUniformLocation(prog,"uRim"),rim);
}

static void drawMesh(GLuint prog, const BoxMesh& m,
                     glm::mat4 model, glm::vec4 col,
                     float wet=0,float emis=0,float rim=0)
{
    glUseProgram(prog);
    glm::mat4 mvp = gVP * model;
    glUniformMatrix4fv(glGetUniformLocation(prog,"uMVP"),  1,GL_FALSE,glm::value_ptr(mvp));
    glUniformMatrix4fv(glGetUniformLocation(prog,"uModel"),1,GL_FALSE,glm::value_ptr(model));
    setWorldUniforms(prog,col,wet,emis,rim);
    glBindVertexArray(m.vao);
    glDrawElements(GL_TRIANGLES,36,GL_UNSIGNED_INT,nullptr);
}

static void drawQuadMesh(GLuint prog, const BoxMesh& m,
                         glm::mat4 model, glm::vec4 col,
                         float wet=0)
{
    glUseProgram(prog);
    glm::mat4 mvp = gVP * model;
    glUniformMatrix4fv(glGetUniformLocation(prog,"uMVP"),  1,GL_FALSE,glm::value_ptr(mvp));
    glUniformMatrix4fv(glGetUniformLocation(prog,"uModel"),1,GL_FALSE,glm::value_ptr(model));
    setWorldUniforms(prog,col,wet);
    glBindVertexArray(m.vao);
    glDrawElements(GL_TRIANGLES,6,GL_UNSIGNED_INT,nullptr);
}

// ─────────────────────────────────────────────────────────────
//  Road drawing
// ─────────────────────────────────────────────────────────────
static void drawRoad()
{
    float roadZ = 0.f; // centred on world origin, player moves in Z

    // Asphalt surface (giant XZ quad)
    glm::mat4 M = glm::mat4(1.f);
    drawQuadMesh(pWorld, meshRoad, M,
                 {0.22f+wetness*0.05f, 0.23f, 0.25f, 1.f}, wetness*0.5f);

    // Kerbs left + right  (alternating red/white stripes, scroll with game)
    for(int side=-1;side<=1;side+=2){
        float kx = side*(ROAD_HALF+0.5f);
        for(int i=0;i<24;i++){
            // scroll by using fmod on gTime so they animate
            float kz = fmodf(gTime*baseSpeed*0.5f + i*8.5f, 204.f) - 20.f;
            bool  red=(i%2==0);
            glm::mat4 Mk=glm::translate(glm::mat4(1.f),{kx,0.05f,kz});
            Mk=glm::scale(Mk,{0.5f,0.05f,4.0f});
            drawMesh(pWorld,meshCarBody,Mk,
                     red?glm::vec4{0.90f,0.12f,0.10f,1.f}
                        :glm::vec4{0.92f,0.92f,0.92f,1.f});
        }
    }

    // Yellow centre line
    for(int i=-15;i<15;i++){
        float dz = fmodf(gTime*baseSpeed + i*9.f, 270.f) - 135.f;
        glm::mat4 Md=glm::translate(glm::mat4(1.f),{0.f,0.01f,dz});
        Md=glm::scale(Md,{0.07f,0.01f,3.2f});
        drawMesh(pWorld,meshCarBody,Md,{1.f,0.85f,0.f,1.f});
    }

    // White lane dashes
    for(int lane=1;lane<NUM_LANES;lane++){
        float lx=-ROAD_HALF+lane*LANE_W;
        for(int i=-15;i<15;i++){
            float dz=fmodf(gTime*baseSpeed*0.9f+i*7.f,210.f)-105.f;
            glm::mat4 Md=glm::translate(glm::mat4(1.f),{lx,0.01f,dz});
            Md=glm::scale(Md,{0.04f,0.01f,2.5f});
            drawMesh(pWorld,meshCarBody,Md,{0.85f,0.85f,0.85f,0.85f});
        }
    }

    // Guardrails
    for(int side=-1;side<=1;side+=2){
        float rx=side*(ROAD_HALF+1.1f);
        // horizontal rail
        glm::mat4 Mr=glm::translate(glm::mat4(1.f),{rx,0.5f,0.f});
        Mr=glm::scale(Mr,{0.06f,0.1f,120.f});
        drawMesh(pWorld,meshCarBody,Mr,{0.75f,0.75f,0.78f,1.f},0.3f);
        // posts
        for(int i=-16;i<16;i++){
            float pz=i*7.5f;
            glm::mat4 Mp=glm::translate(glm::mat4(1.f),{rx,0.3f,pz});
            Mp=glm::scale(Mp,{0.08f,0.3f,0.08f});
            drawMesh(pWorld,meshCarBody,Mp,{0.65f,0.65f,0.65f,1.f});
        }
    }
}

// ─────────────────────────────────────────────────────────────
//  Car drawing
// ─────────────────────────────────────────────────────────────
static void drawCar(float cx,float cz,
                    glm::vec3 bodyCol,
                    float hw,float hh,
                    bool isTruck, bool isPlayer)
{
    float bodyH = isTruck? 1.2f : 0.7f;
    float bodyTop= bodyH * 0.5f;

    // Body
    glm::mat4 M=glm::translate(glm::mat4(1.f),{cx, bodyTop, cz});
    M=glm::scale(M,{hw,bodyTop,hh});
    drawMesh(pWorld,meshCarBody,M,
             {bodyCol.r,bodyCol.g,bodyCol.b,1.f}, 0.f,0.f,
             isPlayer?1.0f:0.4f);

    if(isTruck){
        // Cargo box
        glm::mat4 Mc=glm::translate(glm::mat4(1.f),{cx,1.5f,cz-hh*0.3f});
        Mc=glm::scale(Mc,{hw*0.95f,0.9f,hh*1.4f});
        drawMesh(pWorld,meshCarBody,Mc,{0.82f,0.82f,0.80f,1.f});
    } else {
        // Cabin top / roof
        glm::mat4 Mr=glm::translate(glm::mat4(1.f),{cx,bodyH+0.25f,cz+(isPlayer?0.1f:-0.1f)});
        Mr=glm::scale(Mr,{hw*0.75f,0.25f,hh*0.65f});
        drawMesh(pWorld,meshCarBody,Mr,
                 {bodyCol.r*0.7f,bodyCol.g*0.7f,bodyCol.b*0.7f,1.f});
        // Windshield (dark blue transparent-ish)
        glm::mat4 Mw=glm::translate(glm::mat4(1.f),
                    {cx, bodyH+0.12f, cz+(isPlayer?-0.3f:0.3f)});
        Mw=glm::scale(Mw,{hw*0.68f,0.18f,hh*0.28f});
        drawMesh(pWorld,meshCarBody,Mw,{0.15f,0.35f,0.6f,0.9f});
    }

    // Wheels (4 dark cylinders approximated as flat cuboids)
    float wy=0.18f;
    float woff[]={-hw*0.9f, hw*0.9f, -hw*0.9f, hw*0.9f};
    float wzoff[]={-hh*0.7f,-hh*0.7f, hh*0.7f, hh*0.7f};
    for(int i=0;i<4;i++){
        glm::mat4 Mwh=glm::translate(glm::mat4(1.f),{cx+woff[i],wy,cz+wzoff[i]});
        Mwh=glm::scale(Mwh,{0.22f,0.18f,0.32f});
        drawMesh(pWorld,meshCarBody,Mwh,{0.10f,0.10f,0.10f,1.f});
        // Hubcap
        glm::mat4 Mhc=glm::translate(glm::mat4(1.f),{cx+woff[i],wy+0.06f,cz+wzoff[i]});
        Mhc=glm::scale(Mhc,{0.12f,0.12f,0.18f});
        drawMesh(pWorld,meshCarBody,Mhc,{0.65f,0.65f,0.68f,1.f});
    }

    // Tail/headlights
    float ly = bodyTop*0.6f;
    float lz = isPlayer? cz+hh : cz-hh;
    for(int side=-1;side<=1;side+=2){
        glm::mat4 Ml=glm::translate(glm::mat4(1.f),{cx+side*hw*0.72f, ly, lz});
        Ml=glm::scale(Ml,{0.18f,0.12f,0.08f});
        glm::vec4 lc = isPlayer? glm::vec4{1.f,0.15f,0.05f,1.f}
                                : glm::vec4{1.f,0.90f,0.50f,1.f};
        drawMesh(pWorld,meshCarBody,Ml,lc, 0.f,1.f,0.f);
    }

    // Nitro flames (player)
    if(isPlayer && nitroOn){
        float fl=0.5f+sinf(gTime*30.f)*0.3f;
        for(int s=-1;s<=1;s+=2){
            glm::mat4 Mf=glm::translate(glm::mat4(1.f),
                            {cx+s*hw*0.4f, ly, cz+hh+fl*0.5f});
            Mf=glm::scale(Mf,{0.12f,0.12f,fl});
            drawMesh(pWorld,meshCarBody,Mf,
                     {1.f,0.45f+s*0.2f,0.0f,0.9f},0.f,1.f,0.f);
        }
    }
}

// ─────────────────────────────────────────────────────────────
//  Trees
// ─────────────────────────────────────────────────────────────
static void drawTree(float tx,float tz,float sc)
{
    // Trunk
    glm::mat4 Mt=glm::translate(glm::mat4(1.f),{tx,sc*0.7f,tz});
    Mt=glm::scale(Mt,{sc*0.2f,sc*0.7f,sc*0.2f});
    drawMesh(pWorld,meshCarBody,Mt,{0.32f,0.20f,0.10f,1.f});
    // 3 layers of pine foliage
    glm::vec4 gc={0.10f,0.45f,0.12f,1.f};
    float layers[]={1.4f,2.2f,3.0f};
    float widths[]={0.9f,0.7f,0.5f};
    for(int i=0;i<3;i++){
        glm::mat4 Ml=glm::translate(glm::mat4(1.f),{tx,sc*layers[i],tz});
        Ml=glm::scale(Ml,{sc*widths[i],sc*0.65f,sc*widths[i]});
        drawMesh(pWorld,meshCarBody,Ml,gc);
    }
}

// ─────────────────────────────────────────────────────────────
//  HUD system  (normalised [0,1] screen coords)
// ─────────────────────────────────────────────────────────────
static void hudRect(float x,float y,float w,float h,
                    float r,float g,float b,float a)
{
    glUseProgram(pHud);
    // centre + half in [0,1]
    glUniform2f(glGetUniformLocation(pHud,"uPos"),  x+w*0.5f, y+h*0.5f);
    glUniform2f(glGetUniformLocation(pHud,"uHalf"), w*0.5f,   h*0.5f);
    glUniform4f(glGetUniformLocation(pHud,"uColor"),r,g,b,a);
    glBindVertexArray(gQuadVAO);
    glDrawArrays(GL_TRIANGLES,0,6);
}

// 7-segment display in normalised coords
// ─────────────────────────────────────────────────────────────
//  Pixel Font  –  5×7 bitmap, supports 0-9 and A-Z
//  Each char = 35 bits packed into a uint64_t, row-major top→bot
// ─────────────────────────────────────────────────────────────
static const uint64_t PFONT[40] = {
//  0
0b00000'01110'10001'10011'10101'11001'10001'01110ULL,
//  1
0b00000'00100'01100'00100'00100'00100'00100'01110ULL,
//  2
0b00000'01110'10001'00001'00110'01000'10000'11111ULL,
//  3
0b00000'11111'00010'00100'00110'00001'10001'01110ULL,
//  4
0b00000'00010'00110'01010'10010'11111'00010'00010ULL,
//  5
0b00000'11111'10000'11110'00001'00001'10001'01110ULL,
//  6
0b00000'00110'01000'10000'11110'10001'10001'01110ULL,
//  7
0b00000'11111'00001'00010'00100'01000'01000'01000ULL,
//  8
0b00000'01110'10001'10001'01110'10001'10001'01110ULL,
//  9
0b00000'01110'10001'10001'01111'00001'00010'01100ULL,
//  A
0b00000'01110'10001'10001'11111'10001'10001'10001ULL,
//  B
0b00000'11110'10001'10001'11110'10001'10001'11110ULL,
//  C
0b00000'01110'10001'10000'10000'10000'10001'01110ULL,
//  D
0b00000'11100'10010'10001'10001'10001'10010'11100ULL,
//  E
0b00000'11111'10000'10000'11110'10000'10000'11111ULL,
//  F
0b00000'11111'10000'10000'11110'10000'10000'10000ULL,
//  G
0b00000'01110'10001'10000'10111'10001'10001'01111ULL,
//  H
0b00000'10001'10001'10001'11111'10001'10001'10001ULL,
//  I
0b00000'01110'00100'00100'00100'00100'00100'01110ULL,
//  J
0b00000'00111'00010'00010'00010'10010'10010'01100ULL,
//  K
0b00000'10001'10010'10100'11000'10100'10010'10001ULL,
//  L
0b00000'10000'10000'10000'10000'10000'10000'11111ULL,
//  M
0b00000'10001'11011'10101'10001'10001'10001'10001ULL,
//  N
0b00000'10001'11001'10101'10011'10001'10001'10001ULL,
//  O
0b00000'01110'10001'10001'10001'10001'10001'01110ULL,
//  P
0b00000'11110'10001'10001'11110'10000'10000'10000ULL,
//  Q
0b00000'01110'10001'10001'10001'10101'10011'01111ULL,
//  R
0b00000'11110'10001'10001'11110'10100'10010'10001ULL,
//  S
0b00000'01111'10000'10000'01110'00001'00001'11110ULL,
//  T
0b00000'11111'00100'00100'00100'00100'00100'00100ULL,
//  U
0b00000'10001'10001'10001'10001'10001'10001'01110ULL,
//  V
0b00000'10001'10001'10001'10001'10001'01010'00100ULL,
//  W
0b00000'10001'10001'10001'10101'10101'11011'10001ULL,
//  X
0b00000'10001'10001'01010'00100'01010'10001'10001ULL,
//  Y
0b00000'10001'10001'01010'00100'00100'00100'00100ULL,
//  Z
0b00000'11111'00001'00010'00100'01000'10000'11111ULL,
//  ^ (Up Arrow)
0b00000'00100'01110'10101'00100'00100'00100'00100ULL,
//  _ (Down Arrow)
0b00000'00100'00100'00100'00100'10101'01110'00100ULL,
//  < (Left Arrow)
0b00000'00000'00100'01000'11111'01000'00100'00000ULL,
//  > (Right Arrow)
0b00000'00000'00100'00010'11111'00010'00100'00000ULL,
};

// Draw a single character at normalised screen pos (x,y)
// cw,ch = cell width/height in normalised [0,1] coords
static void pfontChar(char c, float x, float y, float cw, float ch,
                      float r, float g, float b, float a=1.f)
{
    int idx = -1;
    if(c>='0'&&c<='9') idx=c-'0';
    else if(c>='A'&&c<='Z') idx=10+(c-'A');
    else if(c>='a'&&c<='z') idx=10+(c-'a');
    else if(c=='^') idx=36;
    else if(c=='_') idx=37;
    else if(c=='<') idx=38;
    else if(c=='>') idx=39;
    if(idx<0) return;

    uint64_t bits = PFONT[idx];
    float pw = cw/5.f;
    float ph = ch/7.f;
    // bits are stored rows top→bottom, cols left→right
    // bit 34 = top-left, bit 0 = bottom-right
    for(int row=0;row<7;row++){
        for(int col=0;col<5;col++){
            int bit = 34 - (row*5+col);
            if((bits>>bit)&1){
                float px = x + col*pw;
                float py = y + row*ph;
                hudRect(px,py,pw*0.88f,ph*0.88f, r,g,b,a);
            }
        }
    }
}

// Draw a string of characters
static void pfontStr(const char* s, float x, float y, float cw, float ch,
                     float r, float g, float b, float a=1.f)
{
    for(int i=0;s[i];i++)
        pfontChar(s[i], x+i*(cw*1.2f), y, cw,ch, r,g,b,a);
}

// Draw an integer value
static void pfontInt(int val, float x, float y, float cw, float ch,
                     float r, float g, float b, float a=1.f)
{
    if(val<0)val=0;
    char buf[16]; snprintf(buf,16,"%d",val);
    pfontStr(buf,x,y,cw,ch,r,g,b,a);
}

// Measure string width in normalised coords
static float pfontWidth(int nchars, float cw){ return nchars*cw*1.2f; }


// ─────────────────────────────────────────────────────────────
//  HUD PANELS
// ─────────────────────────────────────────────────────────────


// ─────────────────────────────────────────────────────────────
//  HUD PANELS  (pixel font — fully readable labels + numbers)
// ─────────────────────────────────────────────────────────────

// Speedometer  (bottom-left)
static void drawSpeedometer(float spd, float maxSpd)
{
    float bx=0.010f, by=0.740f, bw=0.155f, bh=0.245f;
    hudRect(bx,by,bw,bh, 0.f,0.f,0.f,0.72f);
    hudRect(bx,by,        bw,0.004f, 0.f,0.7f,1.f,1.f);
    hudRect(bx,by+bh-0.004f,bw,0.004f, 0.f,0.7f,1.f,1.f);
    hudRect(bx,by,0.004f,bh, 0.f,0.7f,1.f,0.6f);
    hudRect(bx+bw-0.004f,by,0.004f,bh, 0.f,0.7f,1.f,0.6f);

    pfontStr("SPEED", bx+0.010f,by+0.010f, 0.014f,0.022f, 0.f,0.7f,1.f);

    float frac=spd/maxSpd;
    int N=20;
    float tw=(bw-0.016f)/N;
    for(int i=0;i<N;i++){
        float fi=(float)i/N;
        float fr=fi<frac?(fi<0.5f?0.1f:fi<0.8f?1.f:1.f):0.20f;
        float fg=fi<frac?(fi<0.5f?0.9f:fi<0.8f?0.8f:0.2f):0.20f;
        float fb=fi<frac?(fi<0.5f?0.1f:0.05f):0.22f;
        hudRect(bx+0.008f+i*tw,by+0.038f,tw*0.7f,0.022f, fr,fg,fb,1.f);
    }

    int ispd=(int)spd;
    char buf[8]; snprintf(buf,8,"%d",ispd);
    int nd=(int)strlen(buf);
    float cw=0.022f, ch=0.065f;
    float nx=bx+(bw-pfontWidth(nd,cw))*0.5f;
    pfontInt(ispd, nx, by+0.075f, cw,ch, 1.f,0.90f,0.2f);

    pfontStr("KMH", bx+0.032f,by+0.158f, 0.013f,0.020f, 0.5f,0.7f,0.9f);

    // Active mode indicator
    if(nitroOn){
        float g=0.5f+sinf(gTime*15.f)*0.5f;
        hudRect(bx+0.008f,by+0.186f,bw-0.016f,0.040f, 1.f,0.4f+g*0.3f,0.f,0.85f);
        pfontStr("NITRO", bx+0.015f,by+0.196f, 0.012f,0.020f, 1.f,1.f,0.5f);
    } else if(accelOn){
        float g=0.4f+sinf(gTime*12.f)*0.4f;
        hudRect(bx+0.008f,by+0.186f,bw-0.016f,0.040f, 0.1f,0.7f+g*0.2f,0.1f,0.85f);
        pfontStr("ACCEL", bx+0.012f,by+0.196f, 0.012f,0.020f, 0.5f,1.f,0.5f);
    } else if(brakeOn){
        float g=0.4f+sinf(gTime*10.f)*0.4f;
        hudRect(bx+0.008f,by+0.186f,bw-0.016f,0.040f, 0.8f+g*0.2f,0.1f,0.1f,0.85f);
        pfontStr("BRAKE", bx+0.012f,by+0.196f, 0.012f,0.020f, 1.f,0.4f,0.4f);
    } else if(slowOn){
        float g=0.4f+sinf(gTime*10.f)*0.4f;
        hudRect(bx+0.008f,by+0.186f,bw-0.016f,0.040f, 0.f,g*0.5f,1.f,0.75f);
        pfontStr("SLOW", bx+0.020f,by+0.196f, 0.012f,0.020f, 0.8f,1.f,1.f);
    }

    // Zoom level bar (bottom of panel)
    float zfrac = (camZoom - 0.40f) / (2.20f - 0.40f); // 0=max zoom-in, 1=max zoom-out
    hudRect(bx+0.008f,by+bh-0.030f, bw-0.016f,0.018f, 0.05f,0.05f,0.08f,0.7f);
    hudRect(bx+0.008f,by+bh-0.030f, (bw-0.016f)*zfrac,0.018f, 0.f,0.8f,1.f,0.85f);
    // Zoom label
    pfontStr("Z", bx+0.008f,by+bh-0.048f, 0.008f,0.014f, 0.f,0.7f,1.f);
}


// Nitro fuel bar  (bottom-right)
static void drawNitroBar()
{
    float bx=0.72f, by=0.935f, bw=0.27f, bh=0.052f;
    hudRect(bx,by,bw,bh, 0.f,0.f,0.f,0.70f);
    hudRect(bx,by,bw,0.004f, 0.f,0.7f,1.f,0.9f);
    hudRect(bx,by+bh-0.004f,bw,0.004f, 0.f,0.7f,1.f,0.9f);
    pfontStr("NITRO", bx+0.005f,by+0.013f, 0.011f,0.018f, 0.f,0.7f,1.f);
    float glow=0.55f+sinf(gTime*8.f)*0.45f;
    hudRect(bx+0.070f,by+0.008f,bw-0.078f,bh-0.016f, 0.1f,0.1f,0.15f,0.8f);
    float barW=(bw-0.078f)*nitroFuel;
    if(barW>0.002f)
        hudRect(bx+0.070f,by+0.008f,barW,bh-0.016f, 0.f,glow*0.7f+0.3f,1.f,1.f);
}

// Lives  (top-right)
static void drawLives()
{
    pfontStr("LIVES", 0.870f,0.012f, 0.011f,0.018f, 0.8f,0.3f,0.3f);
    for(int i=0;i<3;i++){
        float hx=0.994f-(i+1)*0.052f, hy=0.036f;
        float hw2=0.044f, hh2=0.052f;
        bool alive=(i<lives);
        hudRect(hx+0.002f,hy+0.002f,hw2,hh2, 0.f,0.f,0.f,0.35f);
        hudRect(hx,hy,hw2,hh2,
                alive?0.85f:0.22f, alive?0.07f:0.10f, alive?0.07f:0.12f, 0.92f);
        if(alive) hudRect(hx+0.005f,hy+0.005f,hw2*0.40f,hh2*0.30f, 1.f,0.5f,0.5f,0.35f);
        hudRect(hx,hy,hw2,0.004f,
                alive?1.f:0.4f, alive?0.3f:0.3f, alive?0.3f:0.4f, 0.85f);
    }
}

// Score + Coin panel  (top-centre)
static void drawScorePanel()
{
    float bx=0.32f, by=0.010f, bw=0.36f, bh=0.100f;
    hudRect(bx+0.003f,by+0.003f,bw,bh, 0.f,0.f,0.f,0.4f);
    hudRect(bx,by,bw,bh, 0.04f,0.04f,0.06f,0.82f);
    hudRect(bx,by,        bw,0.005f, 0.9f,0.72f,0.f,0.95f);
    hudRect(bx,by+bh-0.005f,bw,0.005f, 0.9f,0.72f,0.f,0.95f);

    pfontStr("SCORE", bx+0.010f,by+0.010f, 0.011f,0.018f, 0.9f,0.72f,0.f);
    pfontInt(score,   bx+0.010f,by+0.034f, 0.018f,0.050f, 1.f,0.9f,0.2f);

    hudRect(bx+bw*0.50f,by+0.008f,0.003f,bh-0.016f, 0.9f,0.7f,0.f,0.35f);

    pfontStr("COINS", bx+bw*0.53f,by+0.010f, 0.011f,0.018f, 1.f,0.78f,0.f);
    hudRect(bx+bw*0.53f,by+0.036f,0.022f,0.022f, 1.f,0.80f,0.f,1.f);
    hudRect(bx+bw*0.53f+0.004f,by+0.039f,0.010f,0.008f, 1.f,1.f,0.6f,0.4f);
    pfontInt(coinCount, bx+bw*0.53f+0.030f,by+0.034f, 0.016f,0.044f, 1.f,0.80f,0.f);
}

// Distance  (top-right below lives)
static void drawDistance()
{
    float bx=0.840f, by=0.098f, bw=0.155f, bh=0.062f;
    hudRect(bx,by,bw,bh, 0.f,0.f,0.f,0.60f);
    hudRect(bx,by,bw,0.003f, 0.f,0.6f,1.f,0.8f);
    pfontStr("DIST", bx+0.005f,by+0.006f, 0.010f,0.016f, 0.4f,0.7f,1.f);
    char buf[16]; snprintf(buf,16,"%dM",(int)distance);
    pfontStr(buf, bx+0.005f,by+0.030f, 0.014f,0.024f, 0.6f,0.9f,1.f);
}

// Minimap  (top-left)
static void drawMinimap()
{
    float mx=0.010f, my=0.010f, mw=0.082f, mh=0.165f;
    hudRect(mx,my,mw,mh, 0.02f,0.04f,0.07f,0.78f);
    hudRect(mx,my,mw,0.003f, 0.f,0.7f,1.f,0.85f);
    hudRect(mx,my+mh-0.003f,mw,0.003f, 0.f,0.7f,1.f,0.85f);
    hudRect(mx,my,0.003f,mh, 0.f,0.7f,1.f,0.5f);
    hudRect(mx+mw-0.003f,my,0.003f,mh, 0.f,0.7f,1.f,0.5f);
    pfontStr("MAP", mx+0.022f,my+0.003f, 0.009f,0.014f, 0.f,0.6f,0.9f);

    float rs=mx+mw*0.28f, rw=mw*0.44f;
    hudRect(rs,my+0.020f,rw,mh-0.023f, 0.18f,0.18f,0.20f,0.7f);
    for(int l=1;l<NUM_LANES;l++){
        float lx=rs+rw*(float)l/NUM_LANES;
        hudRect(lx,my+0.022f,0.002f,mh-0.025f, 0.5f,0.5f,0.5f,0.35f);
    }
    float pdx=rs+rw*(playerX/(ROAD_HALF*2.f)+0.5f)-0.005f;
    hudRect(pdx,my+mh-0.026f,0.012f,0.016f, 0.2f,1.f,0.4f,1.f);
    for(auto& c:traffic){
        float tx2=rs+rw*(c.x/(ROAD_HALF*2.f)+0.5f)-0.004f;
        float tz=my+mh*(0.85f-(c.z/180.f)*0.75f);
        if(tz<my+0.020f||tz>my+mh-0.003f) continue;
        hudRect(tx2,tz,0.008f,0.009f, c.color.r,c.color.g,c.color.b,0.9f);
    }
}

// Nitro / Slow-mo screen flash
static void drawPowerFlash()
{
    if(nitroOn){
        float a=0.07f+sinf(gTime*20.f)*0.04f;
        hudRect(0.f,0.f,1.f,1.f, 1.f,0.45f,0.f,a);
        for(int i=0;i<10;i++){
            float xi=(float)i/10.f+fmodf(gTime*0.3f,0.1f);
            float len=0.08f+sinf(gTime*15.f+i)*0.07f;
            hudRect(xi,0.25f,0.003f,len, 1.f,0.7f,0.f,0.25f);
        }
    }
    if(slowOn){
        float a=0.06f+sinf(gTime*10.f)*0.03f;
        hudRect(0.f,0.f,1.f,1.f, 0.f,0.6f,1.f,a);
    }
}


// ─────────────────────────────────────────────────────────────
//  MENU SCREEN  — full 3D-style UI
// ─────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────
//  MENU SCREEN
// ─────────────────────────────────────────────────────────────
static void drawMenu()
{
    float t=gTime;
    float pulse=0.5f+sinf(t*1.5f)*0.5f;

    // Dark background
    hudRect(0.f,0.f,1.f,1.f, 0.02f,0.04f,0.08f,0.90f);

    // Animated speed lines
    for(int i=0;i<12;i++){
        float xi=fmodf(t*0.22f*((i%3)+1)+i*0.083f,1.f);
        float len=0.05f+sinf(t*4.f+i)*0.04f;
        float al=0.15f+sinf(t*3.f+i*0.7f)*0.10f;
        hudRect(xi,0.f,0.003f,len, 0.f,0.7f,1.f,al);
        hudRect(xi,1.f-len,0.003f,len, 0.f,0.7f,1.f,al);
    }

    // ── TITLE BANNER ─────────────────────────────────────────
    float bx=0.12f, by=0.120f, bw=0.76f, bh=0.230f;
    // Glow border
    hudRect(bx-0.005f,by-0.005f,bw+0.010f,bh+0.010f, 0.f,0.4f+pulse*0.3f,1.f,pulse*0.35f);
    // Banner bg
    hudRect(bx,by,bw,bh, 0.03f,0.06f,0.12f,0.94f);
    // Accent bars
    hudRect(bx,by,       bw,0.007f, 0.f,0.8f,1.f,0.9f);
    hudRect(bx,by+bh-0.007f,bw,0.007f, 0.f,0.8f,1.f,0.9f);
    hudRect(bx,by,0.010f,bh, 0.f,0.6f,1.f,0.5f);
    hudRect(bx+bw-0.010f,by,0.010f,bh, 0.f,0.6f,1.f,0.5f);

    // "HIGHWAY" — large pixel font, centred
    float cw1=0.040f, ch1=0.100f;
    float tw1=pfontWidth(7,cw1);
    pfontStr("HIGHWAY", bx+(bw-tw1)*0.5f, by+0.025f, cw1,ch1, 0.f,0.75f+pulse*0.25f,1.f);

    // "RACER" — slightly smaller, different colour
    float cw2=0.032f, ch2=0.078f;
    float tw2=pfontWidth(5,cw2);
    pfontStr("RACER", bx+(bw-tw2)*0.5f, by+0.130f, cw2,ch2, 1.f,0.72f+pulse*0.2f,0.1f);

    // ── BEST SCORE PANEL ─────────────────────────────────────
    float spx=0.30f, spy=0.380f, spw=0.40f, sph=0.090f;
    hudRect(spx,spy,spw,sph, 0.f,0.f,0.f,0.55f);
    hudRect(spx,spy,spw,0.003f, 0.f,0.6f,1.f,0.7f);
    hudRect(spx,spy+sph-0.003f,spw,0.003f, 0.f,0.6f,1.f,0.7f);
    // Labels
    pfontStr("BEST", spx+0.010f,spy+0.010f, 0.011f,0.018f, 0.5f,0.85f,1.f);
    pfontInt(0,      spx+0.010f,spy+0.038f, 0.016f,0.040f, 0.5f,0.9f,1.f);
    hudRect(spx+spw*0.48f,spy+0.010f,0.003f,sph-0.020f, 0.3f,0.5f,0.7f,0.5f);
    pfontStr("COINS",spx+spw*0.52f,spy+0.010f, 0.011f,0.018f, 1.f,0.75f,0.f);
    pfontInt(0,      spx+spw*0.52f,spy+0.038f, 0.016f,0.040f, 1.f,0.75f,0.f);

    // ── CONTROLS PANEL ───────────────────────────────────────
    float cpx=0.22f, cpy=0.475f, cpw=0.56f, cph=0.225f;
    hudRect(cpx,cpy,cpw,cph, 0.f,0.f,0.f,0.55f);
    hudRect(cpx,cpy,cpw,0.003f, 0.f,0.5f,0.8f,0.6f);
    hudRect(cpx,cpy+cph-0.003f,cpw,0.003f, 0.f,0.5f,0.8f,0.3f);

    float lh=0.021f; // line height step
    float cx2=cpx+0.012f, cy2=cpy+0.008f, cs=0.009f, ch_ctrl=0.015f;
    // Row 1 — Accelerate (W OR Up Arrow)
    pfontStr("W OR ^",      cx2,cy2,       cs,ch_ctrl, 0.3f,1.f,0.4f);
    pfontStr("ACCEL FORWARD",cx2+0.210f,cy2,cs,ch_ctrl, 0.2f,0.7f,0.3f);
    // Row 2 — Brake (S OR Down Arrow)
    pfontStr("S OR _",      cx2,cy2+lh,    cs,ch_ctrl, 1.f,0.4f,0.3f);
    pfontStr("BRAKE REVERSE",cx2+0.210f,cy2+lh, cs,ch_ctrl, 0.8f,0.3f,0.2f);
    // Row 3 — A/Left Arrow → steer right
    pfontStr("A OR <",      cx2,cy2+lh*2,  cs,ch_ctrl, 0.5f,0.9f,1.f);
    pfontStr("STEER RIGHT",  cx2+0.210f,cy2+lh*2, cs,ch_ctrl, 0.3f,0.6f,0.8f);
    // Row 4 — D/Right Arrow → steer left
    pfontStr("D OR >",      cx2,cy2+lh*3,  cs,ch_ctrl, 0.5f,0.9f,1.f);
    pfontStr("STEER LEFT",   cx2+0.210f,cy2+lh*3, cs,ch_ctrl, 0.3f,0.6f,0.8f);
    // Row 5 — nitro
    pfontStr("SHIFT OR N",  cx2,cy2+lh*4,  cs,ch_ctrl, 1.f,0.75f,0.2f);
    pfontStr("NITRO BOOST", cx2+0.210f,cy2+lh*4, cs,ch_ctrl, 0.8f,0.6f,0.1f);
    // Row 6 — slow-mo
    pfontStr("CTRL OR Z",   cx2,cy2+lh*5,  cs,ch_ctrl, 0.4f,0.9f,1.f);
    pfontStr("SLOW MOTION", cx2+0.210f,cy2+lh*5, cs,ch_ctrl, 0.3f,0.7f,0.9f);
    // Row 7 — zoom keys
    pfontStr("Q E OR EQ MINUS", cx2,cy2+lh*6,  cs,ch_ctrl, 0.7f,0.5f,1.f);
    pfontStr("STEP ZOOM",   cx2+0.210f,cy2+lh*6, cs,ch_ctrl, 0.5f,0.4f,0.9f);
    // Row 8 — zoom continuous / scroll
    pfontStr("SCROLL PGUP DN", cx2,cy2+lh*7,  cs,ch_ctrl, 0.7f,0.5f,1.f);
    pfontStr("SMOOTH ZOOM", cx2+0.210f,cy2+lh*7, cs,ch_ctrl, 0.5f,0.4f,0.9f);
    // Row 9 — Home/End keys
    pfontStr("HOME END",    cx2,cy2+lh*8,  cs,ch_ctrl, 0.5f,0.9f,1.f);
    pfontStr("FAR LEFT RIGHT", cx2+0.210f,cy2+lh*8, cs,ch_ctrl, 0.3f,0.6f,0.8f);
    // Row 10 — fullscreen
    pfontStr("F11",         cx2,cy2+lh*9,  cs,ch_ctrl, 0.7f,0.7f,0.7f);
    pfontStr("FULLSCREEN",  cx2+0.210f,cy2+lh*9, cs,ch_ctrl, 0.5f,0.5f,0.5f);

    // ── PRESS SPACE BUTTON ───────────────────────────────────
    float flash=0.5f+sinf(t*2.8f)*0.5f;
    float pbx=0.28f, pby=0.770f, pbw=0.44f, pbh=0.080f;
    // Glow
    hudRect(pbx-0.004f,pby-0.004f,pbw+0.008f,pbh+0.008f, 0.f,flash*0.7f,flash,flash*0.45f);
    // Body
    hudRect(pbx,pby,pbw,pbh, 0.f,0.12f,0.22f,0.90f);
    hudRect(pbx,pby,pbw,0.008f, flash*0.1f,flash,1.f,0.8f);
    // Text: "PRESS SPACE TO START"
    float tw3=pfontWidth(20,0.013f);
    pfontStr("PRESS SPACE TO START",
             pbx+(pbw-tw3)*0.5f, pby+0.028f, 0.013f,0.036f,
             0.85f,0.95f,1.f);
    // Corner markers
    hudRect(pbx,pby,0.008f,0.008f, 0.f,1.f,1.f,flash);
    hudRect(pbx+pbw-0.008f,pby,0.008f,0.008f, 0.f,1.f,1.f,flash);
    hudRect(pbx,pby+pbh-0.008f,0.008f,0.008f, 0.f,1.f,1.f,flash);
    hudRect(pbx+pbw-0.008f,pby+pbh-0.008f,0.008f,0.008f, 0.f,1.f,1.f,flash);
}

// ─────────────────────────────────────────────────────────────
//  GAME OVER screen
// ─────────────────────────────────────────────────────────────
static void drawGameOver()
{
    float a=std::min(deathTimer/1.2f,1.f);
    // Full dark overlay
    hudRect(0.f,0.f,1.f,1.f, 0.f,0.f,0.f,a*0.78f);
    hudRect(0.f,0.f,1.f,1.f, 0.45f,0.02f,0.02f,a*0.30f);

    // Centre panel
    float bx=0.26f, by=0.18f, bw=0.48f, bh=0.64f;
    hudRect(bx,by,bw,bh, 0.06f,0.02f,0.02f,a*0.92f);
    // 4-sided border
    hudRect(bx,by,       bw,0.006f, 1.f,0.2f,0.1f,a);
    hudRect(bx,by+bh-0.006f,bw,0.006f, 1.f,0.2f,0.1f,a);
    hudRect(bx,by,0.006f,bh, 1.f,0.2f,0.1f,a);
    hudRect(bx+bw-0.006f,by,0.006f,bh, 1.f,0.2f,0.1f,a);

    // "GAME OVER" title
    hudRect(bx+0.010f,by+0.012f,bw-0.020f,0.085f, 0.6f,0.04f,0.02f,a*0.88f);
    float tw=pfontWidth(9,0.026f);
    pfontStr("GAME OVER", bx+(bw-tw)*0.5f, by+0.025f, 0.026f,0.060f, 1.f,0.30f,0.10f);

    // Divider
    hudRect(bx+0.015f,by+0.108f,bw-0.030f,0.003f, 1.f,0.2f,0.1f,a*0.6f);

    // SCORE row
    pfontStr("SCORE", bx+0.025f,by+0.125f, 0.013f,0.022f, 1.f,0.60f,0.f);
    pfontInt(score,   bx+0.025f,by+0.158f, 0.022f,0.062f, 1.f,0.35f,0.1f);

    // Divider
    hudRect(bx+0.015f,by+0.235f,bw-0.030f,0.002f, 1.f,0.2f,0.1f,a*0.4f);

    // DISTANCE row
    pfontStr("DISTANCE", bx+0.025f,by+0.250f, 0.013f,0.022f, 0.4f,0.7f,1.f);
    char distbuf[16]; snprintf(distbuf,16,"%d M",(int)distance);
    pfontStr(distbuf,  bx+0.025f,by+0.282f, 0.018f,0.052f, 0.5f,0.8f,1.f);

    // Divider
    hudRect(bx+0.015f,by+0.348f,bw-0.030f,0.002f, 1.f,0.2f,0.1f,a*0.4f);

    // COINS row
    pfontStr("COINS",  bx+0.025f,by+0.365f, 0.013f,0.022f, 1.f,0.75f,0.f);
    pfontInt(coinCount,bx+0.025f,by+0.396f, 0.018f,0.050f, 1.f,0.78f,0.f);

    // Divider
    hudRect(bx+0.015f,by+0.460f,bw-0.030f,0.002f, 1.f,0.2f,0.1f,a*0.4f);

    // PRESS SPACE prompt
    float fl=a*(0.5f+sinf(deathTimer*4.f)*0.5f);
    hudRect(bx+0.020f,by+0.540f,bw-0.040f,0.070f, fl*0.4f,0.f,0.f,fl*0.85f);
    hudRect(bx+0.020f,by+0.540f,bw-0.040f,0.004f, 1.f,0.3f,0.1f,fl);
    float tw2=pfontWidth(13,0.014f);
    pfontStr("PRESS SPACE", bx+(bw-tw2)*0.5f,by+0.558f, 0.014f,0.036f, 1.f,0.65f,0.55f);
}

// ─────────────────────────────────────────────────────────────
//  WIN screen
// ─────────────────────────────────────────────────────────────
static void drawWin()
{
    float a=std::min(deathTimer/1.2f,1.f);
    hudRect(0.f,0.f,1.f,1.f, 0.f,0.f,0.f,a*0.75f);
    hudRect(0.f,0.f,1.f,1.f, 0.f,0.35f,0.05f,a*0.28f);

    float bx=0.24f, by=0.18f, bw=0.52f, bh=0.64f;
    hudRect(bx,by,bw,bh, 0.02f,0.06f,0.03f,a*0.92f);
    hudRect(bx,by,       bw,0.006f, 0.1f,1.f,0.4f,a);
    hudRect(bx,by+bh-0.006f,bw,0.006f, 0.1f,1.f,0.4f,a);
    hudRect(bx,by,0.006f,bh, 0.1f,1.f,0.4f,a);
    hudRect(bx+bw-0.006f,by,0.006f,bh, 0.1f,1.f,0.4f,a);

    // "YOU WIN" title
    hudRect(bx+0.010f,by+0.012f,bw-0.020f,0.085f, 0.02f,0.5f,0.08f,a*0.88f);
    float tw=pfontWidth(6,0.030f);
    pfontStr("YOU WIN", bx+(bw-tw)*0.5f-0.010f, by+0.025f, 0.030f,0.062f, 0.1f,1.f,0.4f);

    hudRect(bx+0.015f,by+0.108f,bw-0.030f,0.003f, 0.1f,1.f,0.4f,a*0.6f);

    pfontStr("SCORE",    bx+0.030f,by+0.125f, 0.013f,0.022f, 1.f,0.75f,0.f);
    pfontInt(score,      bx+0.030f,by+0.158f, 0.024f,0.068f, 0.1f,1.f,0.35f);

    hudRect(bx+0.015f,by+0.242f,bw-0.030f,0.002f, 0.1f,0.8f,0.3f,a*0.4f);

    pfontStr("DISTANCE", bx+0.030f,by+0.256f, 0.013f,0.022f, 0.5f,0.9f,1.f);
    char distbuf[16]; snprintf(distbuf,16,"%d M",(int)distance);
    pfontStr(distbuf,    bx+0.030f,by+0.288f, 0.020f,0.055f, 0.5f,0.9f,1.f);

    hudRect(bx+0.015f,by+0.358f,bw-0.030f,0.002f, 0.1f,0.8f,0.3f,a*0.4f);

    pfontStr("COINS",    bx+0.030f,by+0.375f, 0.013f,0.022f, 1.f,0.75f,0.f);
    pfontInt(coinCount,  bx+0.030f,by+0.406f, 0.020f,0.055f, 1.f,0.78f,0.f);

    float fl=a*(0.5f+sinf(deathTimer*4.f)*0.5f);
    hudRect(bx+0.020f,by+0.540f,bw-0.040f,0.070f, 0.f,fl*0.5f,0.f,fl*0.85f);
    hudRect(bx+0.020f,by+0.540f,bw-0.040f,0.004f, 0.1f,1.f,0.4f,fl);
    float tw2=pfontWidth(11,0.014f);
    pfontStr("PRESS SPACE", bx+(bw-tw2)*0.5f,by+0.558f, 0.014f,0.036f, 0.6f,1.f,0.7f);
}

static void handleInput(GLFWwindow* win, float dt)
{
    if(gState==GS_MENU){
        if(glfwGetKey(win,GLFW_KEY_SPACE)==GLFW_PRESS){ resetGame(); gState=GS_PLAYING; }
        return;
    }
    if(gState!=GS_PLAYING) {
        if(glfwGetKey(win,GLFW_KEY_SPACE)==GLFW_PRESS) gState=GS_MENU;
        return;
    }
    float ms=12.f*dt;
    // A / LEFT  → steer right (+X)
    if(glfwGetKey(win,GLFW_KEY_LEFT)==GLFW_PRESS||glfwGetKey(win,GLFW_KEY_A)==GLFW_PRESS)
        playerTargetX=std::min(playerTargetX+ms*5.f, ROAD_HALF-CAR_HW);
    // D / RIGHT → steer left  (-X)
    if(glfwGetKey(win,GLFW_KEY_RIGHT)==GLFW_PRESS||glfwGetKey(win,GLFW_KEY_D)==GLFW_PRESS)
        playerTargetX=std::max(playerTargetX-ms*5.f,-ROAD_HALF+CAR_HW);
    if(glfwGetKey(win,GLFW_KEY_HOME)==GLFW_PRESS)
        playerTargetX=-ROAD_HALF+CAR_HW;
    if(glfwGetKey(win,GLFW_KEY_END)==GLFW_PRESS)
        playerTargetX=ROAD_HALF-CAR_HW;

    // ── Forward accelerate  (W / UP arrow) ───────────────────
    accelOn = (glfwGetKey(win,GLFW_KEY_W)==GLFW_PRESS ||
               glfwGetKey(win,GLFW_KEY_UP)==GLFW_PRESS);

    // ── Brake / slow down  (S / DOWN arrow) ──────────────────
    brakeOn = (glfwGetKey(win,GLFW_KEY_S)==GLFW_PRESS ||
               glfwGetKey(win,GLFW_KEY_DOWN)==GLFW_PRESS);

    // ── Nitro  (Shift / N) ────────────────────────────────────
    static bool nk=false,sk=false;
    bool nkn=(glfwGetKey(win,GLFW_KEY_LEFT_SHIFT)==GLFW_PRESS||glfwGetKey(win,GLFW_KEY_N)==GLFW_PRESS);
    if(nkn&&nitroFuel>0.05f) nitroOn=true; else nitroOn=false;
    nk=nkn;

    // ── Slow-mo toggle  (Ctrl / Z) ────────────────────────────
    bool skn=(glfwGetKey(win,GLFW_KEY_LEFT_CONTROL)==GLFW_PRESS||glfwGetKey(win,GLFW_KEY_Z)==GLFW_PRESS);
    if(skn&&!sk){ slowOn=!slowOn; slowTimer=0.f; }
    sk=skn;

    // ── Zoom In   (Q / =) ─────────────────────────────────────
    static bool zin_prev=false, zout_prev=false;
    bool zin =(glfwGetKey(win,GLFW_KEY_Q)==GLFW_PRESS||glfwGetKey(win,GLFW_KEY_EQUAL)==GLFW_PRESS);
    bool zout=(glfwGetKey(win,GLFW_KEY_E)==GLFW_PRESS||glfwGetKey(win,GLFW_KEY_MINUS)==GLFW_PRESS);
    if(zin  && !zin_prev)  camZoomTarget = std::max(camZoomTarget - 0.15f, 0.40f); // zoom in
    if(zout && !zout_prev) camZoomTarget = std::min(camZoomTarget + 0.15f, 2.20f); // zoom out
    zin_prev=zin; zout_prev=zout;
    // Also support mouse scroll via continuous hold
    if(glfwGetKey(win,GLFW_KEY_PAGE_UP)  ==GLFW_PRESS) camZoomTarget=std::max(camZoomTarget-0.5f*dt,0.40f);
    if(glfwGetKey(win,GLFW_KEY_PAGE_DOWN)==GLFW_PRESS) camZoomTarget=std::min(camZoomTarget+0.5f*dt,2.20f);

    // F11 — toggle fullscreen
    static bool f11prev=false;
    bool f11=(glfwGetKey(win,GLFW_KEY_F11)==GLFW_PRESS);
    if(f11&&!f11prev){
        gFullscreen=!gFullscreen;
        if(gFullscreen){
            // Save windowed geometry
            glfwGetWindowPos(win,&gWinX,&gWinY);
            glfwGetWindowSize(win,&gWinW,&gWinH);
            GLFWmonitor* mon=glfwGetPrimaryMonitor();
            const GLFWvidmode* vm=glfwGetVideoMode(mon);
            glfwSetWindowMonitor(win,mon,0,0,vm->width,vm->height,vm->refreshRate);
        } else {
            glfwSetWindowMonitor(win,nullptr,gWinX,gWinY,gWinW,gWinH,0);
        }
    }
    f11prev=f11;

    if(glfwGetKey(win,GLFW_KEY_ESCAPE)==GLFW_PRESS) glfwSetWindowShouldClose(win,1);
}

// ─────────────────────────────────────────────────────────────
//  Update
// ─────────────────────────────────────────────────────────────
static void updateGame(float dt)
{
    if(gState!=GS_PLAYING) return;
    float ts = slowOn?0.35f:(nitroOn?1.65f:1.f);

    playerX+=(playerTargetX-playerX)*10.f*dt;

    // ── Manual speed control ──────────────────────────────────
    // Auto-speed baseline increases with distance
    float autoSpeed = std::min(10.f + distance*0.014f, 32.f);

    if(accelOn && !brakeOn){
        // W/UP: accelerate beyond auto-speed, up to 45
        baseSpeed = std::min(baseSpeed + 18.f*dt, 45.f);
    } else if(brakeOn && !accelOn){
        // S/DOWN: brake hard — can slow to near-stop (min 1.0)
        baseSpeed = std::max(baseSpeed - 22.f*dt, 1.0f);
    } else {
        // No input: drift back toward auto-speed
        if(baseSpeed < autoSpeed)
            baseSpeed = std::min(baseSpeed + 8.f*dt, autoSpeed);
        else
            baseSpeed = std::max(baseSpeed - 4.f*dt, autoSpeed);
    }

    // ── Smooth zoom interpolation ─────────────────────────────
    camZoom += (camZoomTarget - camZoom) * 6.f*dt;

    float eff = baseSpeed * ts;
    distance += eff*dt;
    score = (int)(distance*2.f) + coinCount*50;

    if(nitroOn){ nitroFuel=std::max(nitroFuel-dt*0.22f,0.f); if(nitroFuel<=0.f)nitroOn=false; }
    else        nitroFuel=std::min(nitroFuel+dt*0.08f,1.f);
    if(slowOn){ slowTimer+=dt; if(slowTimer>5.f){slowOn=false;slowTimer=0.f;} }
    if(invincible){ invTimer+=dt; if(invTimer>2.f)invincible=false; }
    shakeAmt*=0.82f;

    // Weather
    if(distance>250.f) fogDensity=std::min(fogDensity+dt*0.008f,0.85f);
    if(distance>150.f) wetness=std::min(wetness+dt*0.006f,1.f);

    // Traffic (move toward camera = decrease Z)
    for(auto& c:traffic) c.z -= (c.speed+eff)*dt;  // approaching
    traffic.erase(std::remove_if(traffic.begin(),traffic.end(),
        [](const TrafficCar&c){return c.z<-8.f;}),traffic.end());
    while((int)traffic.size()<12) spawnTraffic();

    // Coins
    for(auto& c:coins) c.z -= eff*dt;
    for(auto& c:coins){
        if(!c.alive) continue;
        if(fabsf(c.x-playerX)<1.1f&&fabsf(c.z)<1.8f){c.alive=false;coinCount++;}
    }
    coins.erase(std::remove_if(coins.begin(),coins.end(),
        [](const Coin&c){return !c.alive||c.z<-5.f;}),coins.end());
    while((int)coins.size()<6) spawnCoin();

    // Collision
    if(!invincible){
        for(auto& c:traffic){
            if(fabsf(c.x-playerX)<(CAR_HW+c.hw)*0.82f&&
               fabsf(c.z)<(CAR_HH+c.hh)*0.75f){
                lives--;
                shakeAmt=1.f; invincible=true; invTimer=0.f;
                if(lives<=0){gState=GS_DEAD;deathTimer=0.f;return;}
            }
        }
    }

    // Trees scroll
    for(auto& tr:trees){
        tr.z-=eff*dt;
        if(tr.z<-30.f){
            tr.z=180.f+(rand()%60);
            tr.x=(rand()%2==0?1.f:-1.f)*(ROAD_HALF+1.5f+(rand()%8));
        }
    }

    if(distance>=2000.f){gState=GS_WIN;deathTimer=0.f;}
}

// ─────────────────────────────────────────────────────────────
//  Render
// ─────────────────────────────────────────────────────────────
static void render()
{
    // Always recalculate aspect from current framebuffer size
    float asp = (SCR_H > 0) ? (float)SCR_W / (float)SCR_H : 16.f/9.f;

    // Camera
    float shake=shakeAmt;
    float camShX = shake*((rand()%100-50)/1000.f);
    float camShY = shake*((rand()%100-50)/2000.f);

    // Camera is ALWAYS at road centre (X=0), elevated and pulled back.
    // Only the target X shifts slightly with player for steering feel.
    // This guarantees the road is centred at every aspect ratio.
    glm::vec3 camPos    = { camShX,              6.0f + camShY,  -16.0f };
    glm::vec3 camTarget = { playerX * 0.15f,     0.0f,            80.0f  };
    glm::vec3 up        = { 0.f, 1.f, 0.f };

    // Vertical FOV — camZoom: <1 = zoom in (narrow FOV), >1 = zoom out (wide FOV)
    float vfov = glm::radians((55.f + baseSpeed * 0.20f) * camZoom);
    glm::mat4 V = glm::lookAt(camPos, camTarget, up);
    glm::mat4 P = glm::perspective(vfov, asp, 0.3f, 400.f);
    gVP  = P * V;
    gCam = camPos;

    // Sky gradient colours
    glm::vec3 skyTop={0.25f,0.50f,0.90f},skyBot={0.70f,0.85f,0.95f};
    float df=std::min(distance/1800.f,1.f);
    skyTop=glm::mix(skyTop,{0.04f,0.05f,0.18f},df);
    skyBot=glm::mix(skyBot,{0.55f,0.32f,0.12f},df);

    // ── Scene pass ─────────────────────────────────────────────
    glBindFramebuffer(GL_FRAMEBUFFER,sceneFBO.fbo);
    glViewport(0,0,SCR_W,SCR_H);
    glEnable(GL_DEPTH_TEST);
    glClearColor(skyBot.r,skyBot.g,skyBot.b,1.f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    // Sky quad (drawn before depth write)
    glDepthMask(GL_FALSE);
    glUseProgram(pSky);
    glUniform3fv(glGetUniformLocation(pSky,"uSkyTop"),1,glm::value_ptr(skyTop));
    glUniform3fv(glGetUniformLocation(pSky,"uSkyBot"),1,glm::value_ptr(skyBot));
    glBindVertexArray(gQuadVAO);
    glDrawArrays(GL_TRIANGLES,0,6);
    glDepthMask(GL_TRUE);

    if(gState==GS_PLAYING||gState==GS_DEAD||gState==GS_WIN){
        // Road
        drawRoad();

        // Trees
        for(auto& tr:trees) drawTree(tr.x,tr.z,tr.scale);

        // Traffic
        for(auto& c:traffic)
            drawCar(c.x,c.z,c.color,c.hw,c.hh,c.isTruck,false);

        // Coins (glowing discs = small yellow cuboid)
        for(auto& c:coins){
            if(!c.alive) continue;
            float spin=gTime*3.f;
            glm::mat4 Mc=glm::translate(glm::mat4(1.f),{c.x,0.55f,c.z});
            Mc=glm::rotate(Mc,spin,{0.f,1.f,0.f});
            Mc=glm::scale(Mc,{0.4f,0.08f,0.4f});
            drawMesh(pWorld,meshCarBody,Mc,{1.f,0.82f,0.f,1.f},0.f,0.8f,0.f);
        }

        // Player car at world Z=0 (camera is at Z=-12, so player is ahead)
        bool drawPlayer=!invincible||(int)(gTime*12)%2==0;
        if(drawPlayer)
            drawCar(playerX, 4.0f, {0.20f,0.92f,0.10f},CAR_HW,CAR_HH,false,true);
    }

    // ── Bloom ──────────────────────────────────────────────────
    glDisable(GL_DEPTH_TEST);
    // bright pass
    glBindFramebuffer(GL_FRAMEBUFFER,bloomA.fbo);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(pBright);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,sceneFBO.tex);
    glUniform1i(glGetUniformLocation(pBright,"uTex"),0);
    glUniform1f(glGetUniformLocation(pBright,"uThresh"),0.70f);
    glBindVertexArray(gQuadVAO); glDrawArrays(GL_TRIANGLES,0,6);
    // blur passes
    bool horiz=true;
    for(int i=0;i<6;i++){
        glBindFramebuffer(GL_FRAMEBUFFER, horiz?bloomB.fbo:bloomA.fbo);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(pBlur);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, horiz?bloomA.tex:bloomB.tex);
        glUniform1i(glGetUniformLocation(pBlur,"uTex"),0);
        glUniform2f(glGetUniformLocation(pBlur,"uDir"),
                    horiz?1.f/SCR_W:0.f, horiz?0.f:1.f/SCR_H);
        glBindVertexArray(gQuadVAO); glDrawArrays(GL_TRIANGLES,0,6);
        horiz=!horiz;
    }

    // ── Composite ──────────────────────────────────────────────
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    glViewport(0,0,SCR_W,SCR_H);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(pComp);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,sceneFBO.tex);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D,bloomA.tex);
    glUniform1i(glGetUniformLocation(pComp,"uScene"),0);
    glUniform1i(glGetUniformLocation(pComp,"uBloom"),1);
    glUniform1f(glGetUniformLocation(pComp,"uBloom_str"),0.45f);
    glUniform1f(glGetUniformLocation(pComp,"uVignette"),1.6f+(nitroOn?1.2f:0.f));
    glUniform1f(glGetUniformLocation(pComp,"uSlowTint"),slowOn?0.35f:0.f);
    glBindVertexArray(gQuadVAO); glDrawArrays(GL_TRIANGLES,0,6);

    // ── HUD ────────────────────────────────────────────────────
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    if(gState==GS_MENU){
        drawMenu();
    } else if(gState==GS_PLAYING){
        drawSpeedometer(baseSpeed*(nitroOn?1.65f:1.f), 45.f);
        drawNitroBar();
        drawLives();
        drawScorePanel();
        drawDistance();
        drawMinimap();
        drawPowerFlash();
    } else if(gState==GS_DEAD){
        deathTimer+=gDt;
        drawGameOver();
    } else if(gState==GS_WIN){
        deathTimer+=gDt;
        drawWin();
    }

    glDisable(GL_BLEND);
}

// ─────────────────────────────────────────────────────────────
//  Resize callback
// ─────────────────────────────────────────────────────────────
static void rebuildFBOs()
{
    // Delete old FBOs
    glDeleteFramebuffers(1,&sceneFBO.fbo); glDeleteTextures(1,&sceneFBO.tex); glDeleteRenderbuffers(1,&sceneFBO.rbo);
    glDeleteFramebuffers(1,&bloomA.fbo);   glDeleteTextures(1,&bloomA.tex);   glDeleteRenderbuffers(1,&bloomA.rbo);
    glDeleteFramebuffers(1,&bloomB.fbo);   glDeleteTextures(1,&bloomB.tex);   glDeleteRenderbuffers(1,&bloomB.rbo);
    sceneFBO = makeFBO(SCR_W, SCR_H);
    bloomA   = makeFBO(SCR_W, SCR_H);
    bloomB   = makeFBO(SCR_W, SCR_H);
}

static void onScroll(GLFWwindow*, double /*x*/, double y){
    // Scroll up = zoom in (smaller FOV), scroll down = zoom out
    camZoomTarget = std::clamp((float)(camZoomTarget - y*0.12f), 0.40f, 2.20f);
}

static void onResize(GLFWwindow*,int w,int h){
    if(w<1)w=1; if(h<1)h=1;
    SCR_W=w; SCR_H=h;
    glViewport(0,0,w,h);
    rebuildFBOs();
}

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

    // Compile programs
    pWorld  = makeProgram(VS_WORLD,  FS_WORLD);
    pSky    = makeProgram(VS_SKY,    FS_SKY);
    pBright = makeProgram(VS_QUAD,   FS_BRIGHT);
    pBlur   = makeProgram(VS_QUAD,   FS_BLUR);
    pComp   = makeProgram(VS_QUAD,   FS_COMP);
    pHud    = makeProgram(VS_HUD,    FS_HUD);

    // Geometry
    initQuad();
    meshRoad     = makeRoadQuad(ROAD_HALF, 200.f);
    meshCarBody  = makeCuboid(1.f,1.f,1.f);  // unit cuboid — scaled per draw
    meshWheel    = makeCuboid(1.f,1.f,1.f);
    meshDash     = makeCuboid(1.f,1.f,1.f);
    meshKerb     = makeCuboid(1.f,1.f,1.f);
    meshTreeTrunk= makeCuboid(1.f,1.f,1.f);
    meshTreeLeaves=makeCuboid(1.f,1.f,1.f);
    meshRail     = makeCuboid(1.f,1.f,1.f);

    // FBOs
    sceneFBO = makeFBO(SCR_W,SCR_H);
    bloomA   = makeFBO(SCR_W,SCR_H);
    bloomB   = makeFBO(SCR_W,SCR_H);

    double prev=glfwGetTime();

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

        glfwSwapBuffers(win);
    }
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
