// ============================================================
//  dulsi_modelling.h  —  Modelling & Transformation
//  👤 Dulsi
//
//  Lecture concepts covered:
//    • Geometric modelling         — every car, tree, road = cuboid mesh
//                                    (8 vertices, 6 faces) via makeCuboid()
//    • Object representation       — vertices + mesh (VAO/VBO/EBO) with
//                                    position + normal + UV per vertex
//    • Translation                 — glm::translate() moves every object to
//                                    its world position
//    • Rotation                    — glm::rotate(gTime) spins coins each frame
//    • Scaling                     — glm::scale() resizes the same unit cube
//                                    into car body, wheels, tree trunk, etc.
//    • Camera View matrix          — glm::lookAt(camPos, camTarget, up)
//    • Projection matrix           — glm::perspective(vfov, asp, 0.3f, 400.f)
// ============================================================
#pragma once

// ─────────────────────────────────────────────────────────────
//  Box mesh: 6 faces × 4 verts → 24 verts, 36 indices
//  Each vertex: pos(3) + norm(3) + uv(2) = 8 floats
// ─────────────────────────────────────────────────────────────
static BoxMesh makeCuboid(float hw, float hh, float hd)
{
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

// Flat quad (XZ plane) for road surface — 4 verts, 6 indices
static BoxMesh makeRoadQuad(float hw, float hd)
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

// ─────────────────────────────────────────────────────────────
//  Road drawing — all pieces assembled via translate + scale
// ─────────────────────────────────────────────────────────────
static void drawRoad()
{
    // Asphalt surface (giant XZ quad)
    glm::mat4 M = glm::mat4(1.f);
    drawQuadMesh(pWorld, meshRoad, M,
                 {0.22f+wetness*0.05f, 0.23f, 0.25f, 1.f}, wetness*0.5f);

    // Kerbs left + right (alternating red/white stripes, scroll with game)
    for(int side=-1;side<=1;side+=2){
        float kx = side*(ROAD_HALF+0.5f);
        for(int i=0;i<24;i++){
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
//  Car drawing — body, cabin, wheels, lights, nitro flame
//  Each part = same unit cuboid scaled + translated into place
// ─────────────────────────────────────────────────────────────
static void drawCar(float cx, float cz,
                    glm::vec3 bodyCol,
                    float hw, float hh,
                    bool isTruck, bool isPlayer)
{
    float bodyH  = isTruck? 1.2f : 0.7f;
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

    // Nitro flames (player only) — scale pulses with sinf(gTime*30)
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
//  Tree drawing — trunk + 3 stacked pine foliage layers
//  glm::scale() resizes same unit cube into trunk vs leaves
// ─────────────────────────────────────────────────────────────
static void drawTree(float tx, float tz, float sc)
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
