// ─────────────────────────────────────────────────────────────
//  HEPHZIBAH — Animation, Clipping & Computer Vision
// ─────────────────────────────────────────────────────────────
//  Lecture Concepts:
//   Animation (movement) — traffic c.z -= speed * dt every frame
//   Animation (rotation) — coin glm::rotate(gTime * 3.0f) continuous spin
//   Animation (scrolling)— trees reset Z when behind camera — infinite scroll
//   Animation (nitro)    — flame scale pulses with sinf(gTime * 30.f)
//   Camera shake         — camPos += shake * random offset on collision
//   Clipping (frustum)   — glm::perspective defines view volume; outside = not rendered
//   Culling              — traffic.erase when z < -8 — off-screen objects removed
//   Object despawning    — coins erased at z < -5, trees recycled at z < -30
//   CG vs Computer Vision— CG: 3D world -> 2D image. CV: 2D image -> 3D understanding
// ─────────────────────────────────────────────────────────────
#pragma once

// ─────────────────────────────────────────────────────────────
//  Spawning & initialisation
// ─────────────────────────────────────────────────────────────
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

static void spawnCoin(){
    Coin c;
    c.x = laneX(rand()%NUM_LANES);
    c.z = 60.f + (rand()%100);
    c.alive = true;
    coins.push_back(c);
}

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
//  Input handling
// ─────────────────────────────────────────────────────────────
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
    // A / LEFT  -> steer right (+X)
    if(glfwGetKey(win,GLFW_KEY_LEFT)==GLFW_PRESS||glfwGetKey(win,GLFW_KEY_A)==GLFW_PRESS)
        playerTargetX=std::min(playerTargetX+ms*5.f, ROAD_HALF-CAR_HW);
    // D / RIGHT -> steer left  (-X)
    if(glfwGetKey(win,GLFW_KEY_RIGHT)==GLFW_PRESS||glfwGetKey(win,GLFW_KEY_D)==GLFW_PRESS)
        playerTargetX=std::max(playerTargetX-ms*5.f,-ROAD_HALF+CAR_HW);
    if(glfwGetKey(win,GLFW_KEY_HOME)==GLFW_PRESS)
        playerTargetX=-ROAD_HALF+CAR_HW;
    if(glfwGetKey(win,GLFW_KEY_END)==GLFW_PRESS)
        playerTargetX=ROAD_HALF-CAR_HW;

    // Forward accelerate  (W / UP arrow)
    accelOn = (glfwGetKey(win,GLFW_KEY_W)==GLFW_PRESS ||
               glfwGetKey(win,GLFW_KEY_UP)==GLFW_PRESS);

    // Brake / slow down  (S / DOWN arrow)
    brakeOn = (glfwGetKey(win,GLFW_KEY_S)==GLFW_PRESS ||
               glfwGetKey(win,GLFW_KEY_DOWN)==GLFW_PRESS);

    // Nitro  (Shift / N)
    static bool nk=false,sk=false;
    bool nkn=(glfwGetKey(win,GLFW_KEY_LEFT_SHIFT)==GLFW_PRESS||glfwGetKey(win,GLFW_KEY_N)==GLFW_PRESS);
    if(nkn&&nitroFuel>0.05f) nitroOn=true; else nitroOn=false;
    nk=nkn;

    // Slow-mo toggle  (Ctrl / Z)
    bool skn=(glfwGetKey(win,GLFW_KEY_LEFT_CONTROL)==GLFW_PRESS||glfwGetKey(win,GLFW_KEY_Z)==GLFW_PRESS);
    if(skn&&!sk){ slowOn=!slowOn; slowTimer=0.f; }
    sk=skn;

    // Zoom In   (Q / =)
    static bool zin_prev=false, zout_prev=false;
    bool zin =(glfwGetKey(win,GLFW_KEY_Q)==GLFW_PRESS||glfwGetKey(win,GLFW_KEY_EQUAL)==GLFW_PRESS);
    bool zout=(glfwGetKey(win,GLFW_KEY_E)==GLFW_PRESS||glfwGetKey(win,GLFW_KEY_MINUS)==GLFW_PRESS);
    if(zin  && !zin_prev)  camZoomTarget = std::max(camZoomTarget - 0.15f, 0.40f);
    if(zout && !zout_prev) camZoomTarget = std::min(camZoomTarget + 0.15f, 2.20f);
    zin_prev=zin; zout_prev=zout;
    if(glfwGetKey(win,GLFW_KEY_PAGE_UP)  ==GLFW_PRESS) camZoomTarget=std::max(camZoomTarget-0.5f*dt,0.40f);
    if(glfwGetKey(win,GLFW_KEY_PAGE_DOWN)==GLFW_PRESS) camZoomTarget=std::min(camZoomTarget+0.5f*dt,2.20f);

    // F11 — toggle fullscreen
    static bool f11prev=false;
    bool f11=(glfwGetKey(win,GLFW_KEY_F11)==GLFW_PRESS);
    if(f11&&!f11prev){
        gFullscreen=!gFullscreen;
        if(gFullscreen){
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
//  Game update — animation, physics, collision, culling
// ─────────────────────────────────────────────────────────────
static void updateGame(float dt)
{
    if(gState!=GS_PLAYING) return;
    float ts = slowOn?0.35f:(nitroOn?1.65f:1.f);

    playerX+=(playerTargetX-playerX)*10.f*dt;

    // Auto-speed baseline increases with distance
    float autoSpeed = std::min(10.f + distance*0.014f, 32.f);

    if(accelOn && !brakeOn){
        baseSpeed = std::min(baseSpeed + 18.f*dt, 45.f);
    } else if(brakeOn && !accelOn){
        baseSpeed = std::max(baseSpeed - 22.f*dt, 1.0f);
    } else {
        if(baseSpeed < autoSpeed)
            baseSpeed = std::min(baseSpeed + 8.f*dt, autoSpeed);
        else
            baseSpeed = std::max(baseSpeed - 4.f*dt, autoSpeed);
    }

    // Smooth zoom interpolation
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

    // Traffic animation — move toward camera (decrease Z)
    // Culling: traffic.erase when z < -8 (off-screen, outside view frustum)
    for(auto& c:traffic) c.z -= (c.speed+eff)*dt;
    traffic.erase(std::remove_if(traffic.begin(),traffic.end(),
        [](const TrafficCar&c){return c.z<-8.f;}),traffic.end());
    while((int)traffic.size()<12) spawnTraffic();

    // Coin animation — scroll toward camera, despawn at z < -5
    for(auto& c:coins) c.z -= eff*dt;
    for(auto& c:coins){
        if(!c.alive) continue;
        if(fabsf(c.x-playerX)<1.1f&&fabsf(c.z)<1.8f){c.alive=false;coinCount++;}
    }
    coins.erase(std::remove_if(coins.begin(),coins.end(),
        [](const Coin&c){return !c.alive||c.z<-5.f;}),coins.end());
    while((int)coins.size()<6) spawnCoin();

    // Collision detection + camera shake
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

    // Tree scrolling animation — recycle trees that pass behind camera (z < -30)
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
//  Window resize & FBO rebuild
// ─────────────────────────────────────────────────────────────
static void rebuildFBOs()
{
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
