// ============================================================
//  pipuni_rendering.h  —  Rendering & Graphics Pipeline
//  👤 Pipuni
//
//  Lecture concepts covered:
//    • Rendering         — full scene drawn to sceneFBO every frame
//    • Graphics pipeline — VS_WORLD → rasterizer → FS_WORLD runs every glDrawElements
//    • Vertex shader     — VS_WORLD transforms aPos using MVP matrix
//    • Fragment shader   — FS_WORLD outputs final pixel colour
//    • Rasterization     — OpenGL converts triangles to pixels automatically
//    • Render loop       — while(!glfwWindowShouldClose) — 60 FPS frame submission
//    • Frame submission  — glfwSwapBuffers(win) — sends frame to screen
// ============================================================
#pragma once

// ── Vertex shader: world geometry ────────────────────────────
// Transforms aPos using MVP, passes world-space data to fragment shader
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

// ── Sky shader: simple gradient fullscreen quad ───────────────
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

// ─────────────────────────────────────────────────────────────
//  Shader compilation and program linking
// ─────────────────────────────────────────────────────────────
static GLuint compileShader(GLenum type, const char* src)
{
    GLuint s=glCreateShader(type);
    glShaderSource(s,1,&src,nullptr);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok){char buf[2048];glGetShaderInfoLog(s,2048,nullptr,buf);
            fprintf(stderr,"SHADER ERR:\n%s\n",buf);}
    return s;
}
static GLuint makeProgram(const char* vs, const char* fs)
{
    GLuint p=glCreateProgram();
    GLuint v=compileShader(GL_VERTEX_SHADER,vs);
    GLuint f=compileShader(GL_FRAGMENT_SHADER,fs);
    glAttachShader(p,v); glAttachShader(p,f);
    glLinkProgram(p);
    glDeleteShader(v); glDeleteShader(f);
    return p;
}

// ─────────────────────────────────────────────────────────────
//  Fullscreen [-1,1] quad VAO (used for sky, post-process, HUD)
// ─────────────────────────────────────────────────────────────
static void initQuad()
{
    float q[]={-1,-1, 1,-1, 1,1, -1,-1, 1,1, -1,1};
    glGenVertexArrays(1,&gQuadVAO); glGenBuffers(1,&gQuadVBO);
    glBindVertexArray(gQuadVAO);
    glBindBuffer(GL_ARRAY_BUFFER,gQuadVBO);
    glBufferData(GL_ARRAY_BUFFER,sizeof(q),q,GL_STATIC_DRAW);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,nullptr);
    glEnableVertexAttribArray(0);
}

// ─────────────────────────────────────────────────────────────
//  render() — full frame submission
//  Stages: scene pass → bloom → composite → HUD
// ─────────────────────────────────────────────────────────────
static void render()
{
    float asp=(float)SCR_W/SCR_H;

    // Camera shake on collision
    float shake=shakeAmt;
    float camShX = shake*((rand()%100-50)/1000.f);
    float camShY = shake*((rand()%100-50)/2000.f);

    // Camera: behind player, looking straight ahead
    glm::vec3 camPos   = { playerX*0.25f + camShX,  5.0f + camShY,  -12.0f };
    glm::vec3 camTarget= { 0.0f,                    0.5f,             80.0f  };
    glm::vec3 up={0.f,1.f,0.f};

    // View + Projection matrices
    glm::mat4 V = glm::lookAt(camPos,camTarget,up);
    glm::mat4 P = glm::perspective(glm::radians(62.f+baseSpeed*0.3f), asp, 0.3f, 400.f);
    gVP  = P * V;
    gCam = camPos;

    // Sky colour: day (blue) → night (orange) with distance
    glm::vec3 skyTop={0.25f,0.50f,0.90f}, skyBot={0.70f,0.85f,0.95f};
    float df=std::min(distance/1800.f,1.f);
    skyTop=glm::mix(skyTop,{0.04f,0.05f,0.18f},df);
    skyBot=glm::mix(skyBot,{0.55f,0.32f,0.12f},df);

    // ── Scene pass: render 3D world into sceneFBO ─────────────
    glBindFramebuffer(GL_FRAMEBUFFER,sceneFBO.fbo);
    glViewport(0,0,SCR_W,SCR_H);
    glEnable(GL_DEPTH_TEST);
    glClearColor(skyBot.r,skyBot.g,skyBot.b,1.f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    // Sky quad (drawn before depth write so it stays in background)
    glDepthMask(GL_FALSE);
    glUseProgram(pSky);
    glUniform3fv(glGetUniformLocation(pSky,"uSkyTop"),1,glm::value_ptr(skyTop));
    glUniform3fv(glGetUniformLocation(pSky,"uSkyBot"),1,glm::value_ptr(skyBot));
    glBindVertexArray(gQuadVAO);
    glDrawArrays(GL_TRIANGLES,0,6);
    glDepthMask(GL_TRUE);

    if(gState==GS_PLAYING||gState==GS_DEAD||gState==GS_WIN){
        drawRoad();

        for(auto& tr:trees) drawTree(tr.x,tr.z,tr.scale);

        for(auto& c:traffic)
            drawCar(c.x,c.z,c.color,c.hw,c.hh,c.isTruck,false);

        // Coins: spinning yellow disc (small cuboid + glm::rotate each frame)
        for(auto& c:coins){
            if(!c.alive) continue;
            float spin=gTime*3.f;
            glm::mat4 Mc=glm::translate(glm::mat4(1.f),{c.x,0.55f,c.z});
            Mc=glm::rotate(Mc,spin,{0.f,1.f,0.f});
            Mc=glm::scale(Mc,{0.4f,0.08f,0.4f});
            drawMesh(pWorld,meshCarBody,Mc,{1.f,0.82f,0.f,1.f},0.f,0.8f,0.f);
        }

        // Player car (blinks while invincible)
        bool drawPlayer=!invincible||(int)(gTime*12)%2==0;
        if(drawPlayer)
            drawCar(playerX, 2.0f, {0.20f,0.92f,0.10f},CAR_HW,CAR_HH,false,true);
    }

    // ── Bloom: bright-pass → 6-pass Gaussian blur (ping-pong) ─
    glDisable(GL_DEPTH_TEST);
    // Bright-pass into bloomA
    glBindFramebuffer(GL_FRAMEBUFFER,bloomA.fbo);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(pBright);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,sceneFBO.tex);
    glUniform1i(glGetUniformLocation(pBright,"uTex"),0);
    glUniform1f(glGetUniformLocation(pBright,"uThresh"),0.70f);
    glBindVertexArray(gQuadVAO); glDrawArrays(GL_TRIANGLES,0,6);
    // 6 separable blur passes: bloomA ↔ bloomB
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

    // ── Composite: scene + bloom → screen (Reinhard + vignette) ─
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

    // ── HUD: 2D raster layer on top (screen-space, blended) ───
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    if(gState==GS_MENU){
        drawMenu();
    } else if(gState==GS_PLAYING){
        drawSpeedometer(baseSpeed*(nitroOn?1.65f:1.f), 35.f);
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
