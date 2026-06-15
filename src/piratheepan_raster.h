// ============================================================
//  piratheepan_raster.h  —  Raster Graphics, Pixel Processing,
//                           Z-Buffer & Entertainment
//  👤 Piratheepan
//
//  Lecture concepts covered:
//    • Raster graphics    — sceneFBO = framebuffer = grid of pixels in memory
//    • Framebuffer        — makeFBO() creates colour texture + depth renderbuffer
//    • Pixel processing   — each fragment goes through FS_WORLD → colour decision
//    • Z-Buffer/Depth test— glEnable(GL_DEPTH_TEST) — closer object wins per pixel
//    • GL_DEPTH_TEST      — prevents far car showing through near car
//    • Bloom ping-pong    — bloomA ↔ bloomB swap 6 times for Gaussian blur
//    • Pixel font HUD     — 5×7 bitmap font, each character drawn as pixel rects
//    • Screen-space render— HUD uses normalised [0,1] coords — pure 2D raster layer
//    • High colour depth  — GL_RGB16F — 16 bits per channel (beyond 24-bit True Colour)
//    • Real-time 60 FPS   — game loop runs render every frame — entertainment app
//    • Entertainment      — video game = exact entertainment category (lecture slide 24)
// ============================================================
#pragma once

// ── HUD: screen-space 2D shader ──────────────────────────────
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
//  HUD system  (normalised [0,1] screen coords)
// ─────────────────────────────────────────────────────────────
static void hudRect(float x, float y, float w, float h,
                    float r, float g, float b, float a)
{
    glUseProgram(pHud);
    glUniform2f(glGetUniformLocation(pHud,"uPos"),  x+w*0.5f, y+h*0.5f);
    glUniform2f(glGetUniformLocation(pHud,"uHalf"), w*0.5f,   h*0.5f);
    glUniform4f(glGetUniformLocation(pHud,"uColor"),r,g,b,a);
    glBindVertexArray(gQuadVAO);
    glDrawArrays(GL_TRIANGLES,0,6);
}

// ─────────────────────────────────────────────────────────────
//  Pixel Font  —  5×7 bitmap, supports 0-9 and A-Z
//  Each char = 35 bits packed into a uint64_t, row-major top→bot
// ─────────────────────────────────────────────────────────────
static const uint64_t PFONT[36] = {
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
};

// Draw a single character at normalised screen pos (x,y)
static void pfontChar(char c, float x, float y, float cw, float ch,
                      float r, float g, float b, float a=1.f)
{
    int idx = -1;
    if(c>='0'&&c<='9') idx=c-'0';
    else if(c>='A'&&c<='Z') idx=10+(c-'A');
    else if(c>='a'&&c<='z') idx=10+(c-'a');
    if(idx<0) return;

    uint64_t bits = PFONT[idx];
    float pw = cw/5.f;
    float ph = ch/7.f;
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

static void pfontStr(const char* s, float x, float y, float cw, float ch,
                     float r, float g, float b, float a=1.f)
{
    for(int i=0;s[i];i++)
        pfontChar(s[i], x+i*(cw*1.2f), y, cw,ch, r,g,b,a);
}

static void pfontInt(int val, float x, float y, float cw, float ch,
                     float r, float g, float b, float a=1.f)
{
    if(val<0)val=0;
    char buf[16]; snprintf(buf,16,"%d",val);
    pfontStr(buf,x,y,cw,ch,r,g,b,a);
}

static float pfontWidth(int nchars, float cw){ return nchars*cw*1.2f; }

// ─────────────────────────────────────────────────────────────
//  HUD PANELS  (pixel font — readable labels + numbers)
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

    if(nitroOn){
        float g=0.5f+sinf(gTime*15.f)*0.5f;
        hudRect(bx+0.008f,by+0.188f,bw-0.016f,0.042f, 1.f,0.4f+g*0.3f,0.f,0.8f);
        pfontStr("NITRO", bx+0.015f,by+0.198f, 0.012f,0.020f, 1.f,1.f,0.5f);
    }
    if(slowOn){
        float g=0.4f+sinf(gTime*10.f)*0.4f;
        hudRect(bx+0.008f,by+0.188f,bw-0.016f,0.042f, 0.f,g*0.5f,1.f,0.7f);
        pfontStr("SLOW", bx+0.020f,by+0.198f, 0.012f,0.020f, 0.8f,1.f,1.f);
    }
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
    float bx=0.12f, by=0.15f, bw=0.76f, bh=0.260f;
    hudRect(bx-0.005f,by-0.005f,bw+0.010f,bh+0.010f, 0.f,0.4f+pulse*0.3f,1.f,pulse*0.35f);
    hudRect(bx,by,bw,bh, 0.03f,0.06f,0.12f,0.94f);
    hudRect(bx,by,       bw,0.007f, 0.f,0.8f,1.f,0.9f);
    hudRect(bx,by+bh-0.007f,bw,0.007f, 0.f,0.8f,1.f,0.9f);
    hudRect(bx,by,0.010f,bh, 0.f,0.6f,1.f,0.5f);
    hudRect(bx+bw-0.010f,by,0.010f,bh, 0.f,0.6f,1.f,0.5f);

    float cw1=0.040f, ch1=0.100f;
    float tw1=pfontWidth(7,cw1);
    pfontStr("HIGHWAY", bx+(bw-tw1)*0.5f, by+0.025f, cw1,ch1, 0.f,0.75f+pulse*0.25f,1.f);

    float cw2=0.032f, ch2=0.078f;
    float tw2=pfontWidth(5,cw2);
    pfontStr("RACER", bx+(bw-tw2)*0.5f, by+0.155f, cw2,ch2, 1.f,0.72f+pulse*0.2f,0.1f);

    // ── BEST SCORE PANEL ─────────────────────────────────────
    float spx=0.30f, spy=0.455f, spw=0.40f, sph=0.090f;
    hudRect(spx,spy,spw,sph, 0.f,0.f,0.f,0.55f);
    hudRect(spx,spy,spw,0.003f, 0.f,0.6f,1.f,0.7f);
    hudRect(spx,spy+sph-0.003f,spw,0.003f, 0.f,0.6f,1.f,0.7f);
    pfontStr("BEST", spx+0.010f,spy+0.010f, 0.011f,0.018f, 0.5f,0.85f,1.f);
    pfontInt(0,      spx+0.010f,spy+0.038f, 0.016f,0.040f, 0.5f,0.9f,1.f);
    hudRect(spx+spw*0.48f,spy+0.010f,0.003f,sph-0.020f, 0.3f,0.5f,0.7f,0.5f);
    pfontStr("COINS",spx+spw*0.52f,spy+0.010f, 0.011f,0.018f, 1.f,0.75f,0.f);
    pfontInt(0,      spx+spw*0.52f,spy+0.038f, 0.016f,0.040f, 1.f,0.75f,0.f);

    // ── CONTROLS PANEL ───────────────────────────────────────
    float cpx=0.28f, cpy=0.570f, cpw=0.44f, cph=0.110f;
    hudRect(cpx,cpy,cpw,cph, 0.f,0.f,0.f,0.50f);
    hudRect(cpx,cpy,cpw,0.003f, 0.f,0.5f,0.8f,0.6f);
    pfontStr("A D    STEER",  cpx+0.012f,cpy+0.012f, 0.010f,0.018f, 0.6f,0.9f,1.f);
    pfontStr("SHIFT  NITRO", cpx+0.012f,cpy+0.038f, 0.010f,0.016f, 1.f,0.7f,0.2f);
    pfontStr("CTRL S SLOW",  cpx+0.012f,cpy+0.062f, 0.010f,0.016f, 0.4f,0.9f,0.9f);
    pfontStr("SPACE  START", cpx+0.012f,cpy+0.084f, 0.010f,0.016f, 0.7f,0.7f,1.f);

    // ── PRESS SPACE BUTTON ───────────────────────────────────
    float flash=0.5f+sinf(t*2.8f)*0.5f;
    float pbx=0.28f, pby=0.710f, pbw=0.44f, pbh=0.090f;
    hudRect(pbx-0.004f,pby-0.004f,pbw+0.008f,pbh+0.008f, 0.f,flash*0.7f,flash,flash*0.45f);
    hudRect(pbx,pby,pbw,pbh, 0.f,0.12f,0.22f,0.90f);
    hudRect(pbx,pby,pbw,0.008f, flash*0.1f,flash,1.f,0.8f);
    float tw3=pfontWidth(20,0.013f);
    pfontStr("PRESS SPACE TO START",
             pbx+(pbw-tw3)*0.5f, pby+0.028f, 0.013f,0.036f,
             0.85f,0.95f,1.f);
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
    hudRect(0.f,0.f,1.f,1.f, 0.f,0.f,0.f,a*0.78f);
    hudRect(0.f,0.f,1.f,1.f, 0.45f,0.02f,0.02f,a*0.30f);

    float bx=0.26f, by=0.18f, bw=0.48f, bh=0.64f;
    hudRect(bx,by,bw,bh, 0.06f,0.02f,0.02f,a*0.92f);
    hudRect(bx,by,       bw,0.006f, 1.f,0.2f,0.1f,a);
    hudRect(bx,by+bh-0.006f,bw,0.006f, 1.f,0.2f,0.1f,a);
    hudRect(bx,by,0.006f,bh, 1.f,0.2f,0.1f,a);
    hudRect(bx+bw-0.006f,by,0.006f,bh, 1.f,0.2f,0.1f,a);

    hudRect(bx+0.010f,by+0.012f,bw-0.020f,0.085f, 0.6f,0.04f,0.02f,a*0.88f);
    float tw=pfontWidth(9,0.026f);
    pfontStr("GAME OVER", bx+(bw-tw)*0.5f, by+0.025f, 0.026f,0.060f, 1.f,0.30f,0.10f);

    hudRect(bx+0.015f,by+0.108f,bw-0.030f,0.003f, 1.f,0.2f,0.1f,a*0.6f);

    pfontStr("SCORE", bx+0.025f,by+0.125f, 0.013f,0.022f, 1.f,0.60f,0.f);
    pfontInt(score,   bx+0.025f,by+0.158f, 0.022f,0.062f, 1.f,0.35f,0.1f);

    hudRect(bx+0.015f,by+0.235f,bw-0.030f,0.002f, 1.f,0.2f,0.1f,a*0.4f);

    pfontStr("DISTANCE", bx+0.025f,by+0.250f, 0.013f,0.022f, 0.4f,0.7f,1.f);
    char distbuf[16]; snprintf(distbuf,16,"%d M",(int)distance);
    pfontStr(distbuf,  bx+0.025f,by+0.282f, 0.018f,0.052f, 0.5f,0.8f,1.f);

    hudRect(bx+0.015f,by+0.348f,bw-0.030f,0.002f, 1.f,0.2f,0.1f,a*0.4f);

    pfontStr("COINS",  bx+0.025f,by+0.365f, 0.013f,0.022f, 1.f,0.75f,0.f);
    pfontInt(coinCount,bx+0.025f,by+0.396f, 0.018f,0.050f, 1.f,0.78f,0.f);

    hudRect(bx+0.015f,by+0.460f,bw-0.030f,0.002f, 1.f,0.2f,0.1f,a*0.4f);

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
