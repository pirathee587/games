// ============================================================
//  hephzibah_animation.h  —  Animation, Clipping & Computer Vision
//  👤 Hephzibah
//
//  Lecture concepts covered:
//    • Animation (movement)    — traffic c.z -= speed * dt every frame
//    • Animation (rotation)    — coin glm::rotate(gTime * 3.0f) continuous spin
//    • Animation (scrolling)   — trees reset Z when behind camera = infinite scroll
//    • Animation (nitro flame) — scale pulses with sinf(gTime * 30.f)
//    • Camera shake            — camPos += shake * random offset on collision
//    • Clipping (view volume)  — glm::perspective defines frustum; outside not rendered
//    • Culling                 — traffic.erase when z < -8 (off-screen removal)
//    • Object despawning       — coins erased at z < -5, trees recycled at z < -30
//    • CG vs Computer Vision   — CG: 3D world → 2D image
//                                CV: 2D image → 3D understanding (opposite direction)
// ============================================================
#pragma once

// ─────────────────────────────────────────────────────────────
//  Object spawning
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

static void spawnCoin()
{
    Coin c;
    c.x = laneX(rand()%NUM_LANES);
    c.z = 60.f + (rand()%100);
    c.alive = true;
    coins.push_back(c);
}

static void initTrees()
{
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

static void resetGame()
{
    traffic.clear(); coins.clear();
    playerX=0.f; playerTargetX=0.f;
    baseSpeed=10.f; distance=0.f; score=0; coinCount=0; lives=3;
    nitroFuel=1.f; nitroOn=false; slowOn=false; slowTimer=0.f;
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
    if(glfwGetKey(win,GLFW_KEY_LEFT)==GLFW_PRESS||glfwGetKey(win,GLFW_KEY_A)==GLFW_PRESS)
        playerTargetX=std::min(playerTargetX+ms*5.f, ROAD_HALF-CAR_HW);
    if(glfwGetKey(win,GLFW_KEY_RIGHT)==GLFW_PRESS||glfwGetKey(win,GLFW_KEY_D)==GLFW_PRESS)
        playerTargetX=std::max(playerTargetX-ms*5.f,-ROAD_HALF+CAR_HW);

    static bool nk=false,sk=false;
    bool nkn=(glfwGetKey(win,GLFW_KEY_LEFT_SHIFT)==GLFW_PRESS||glfwGetKey(win,GLFW_KEY_N)==GLFW_PRESS);
    if(nkn&&nitroFuel>0.05f) nitroOn=true; else nitroOn=false;
    nk=nkn;
    bool skn=(glfwGetKey(win,GLFW_KEY_LEFT_CONTROL)==GLFW_PRESS||glfwGetKey(win,GLFW_KEY_S)==GLFW_PRESS);
    if(skn&&!sk){ slowOn=!slowOn; slowTimer=0.f; }
    sk=skn;

    if(glfwGetKey(win,GLFW_KEY_ESCAPE)==GLFW_PRESS) glfwSetWindowShouldClose(win,1);
}

// ─────────────────────────────────────────────────────────────
//  Game update — all animation happens here each frame
// ─────────────────────────────────────────────────────────────
static void updateGame(float dt)
{
    if(gState!=GS_PLAYING) return;
    float ts = slowOn?0.35f:(nitroOn?1.65f:1.f);
    float rdt=dt*ts;

    // Player lateral movement (smooth lerp toward target)
    playerX+=(playerTargetX-playerX)*10.f*dt;

    float eff=baseSpeed*ts;
    distance+=eff*dt;
    score=(int)(distance*2.f)+coinCount*50;
    baseSpeed=std::min(10.f+distance*0.014f,32.f);

    // Nitro fuel drain / recharge
    if(nitroOn){ nitroFuel=std::max(nitroFuel-dt*0.22f,0.f); if(nitroFuel<=0.f)nitroOn=false; }
    else        nitroFuel=std::min(nitroFuel+dt*0.08f,1.f);
    if(slowOn){ slowTimer+=dt; if(slowTimer>5.f){slowOn=false;slowTimer=0.f;} }
    if(invincible){ invTimer+=dt; if(invTimer>2.f)invincible=false; }
    shakeAmt*=0.82f;

    // Weather ramp (fog and wet road grow with distance)
    if(distance>250.f) fogDensity=std::min(fogDensity+dt*0.008f,0.85f);
    if(distance>150.f) wetness=std::min(wetness+dt*0.006f,1.f);

    // Traffic animation: move toward camera (Z decreases each frame)
    for(auto& c:traffic) c.z -= (c.speed+eff)*dt;
    // Culling: erase off-screen traffic (z < -8 = behind camera)
    traffic.erase(std::remove_if(traffic.begin(),traffic.end(),
        [](const TrafficCar&c){return c.z<-8.f;}),traffic.end());
    while((int)traffic.size()<12) spawnTraffic();

    // Coin animation: translate toward player
    for(auto& c:coins) c.z -= eff*dt;
    // Coin collection
    for(auto& c:coins){
        if(!c.alive) continue;
        if(fabsf(c.x-playerX)<1.1f&&fabsf(c.z)<1.8f){c.alive=false;coinCount++;}
    }
    // Despawn passed/collected coins (z < -5)
    coins.erase(std::remove_if(coins.begin(),coins.end(),
        [](const Coin&c){return !c.alive||c.z<-5.f;}),coins.end());
    while((int)coins.size()<6) spawnCoin();

    // Collision detection
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

    // Tree infinite scroll: recycle when z < -30 (behind camera)
    for(auto& tr:trees){
        tr.z-=eff*dt;
        if(tr.z<-30.f){
            tr.z=180.f+(rand()%60);
            tr.x=(rand()%2==0?1.f:-1.f)*(ROAD_HALF+1.5f+(rand()%8));
        }
    }

    if(distance>=2000.f){gState=GS_WIN;deathTimer=0.f;}
}
