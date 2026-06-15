// ============================================================
//  tharusha_lighting.h  —  Lighting & Photoreal Rendering
//  👤 Tharusha
//
//  Lecture concepts covered:
//    • Illumination model  — Blinn-Phong (ambient + diffuse + specular) in FS_WORLD
//    • Directional light   — uLightDir = (0.5, 1.0, 0.3) simulates sunlight
//    • Specular highlights — pow(dot(N,H), 64.0) — shiny surfaces
//    • Wet road reflections— uWetness uniform grows after 150 m
//    • Fog                 — fogF = 1 - exp(-density * dist²) in fragment shader
//    • Day→Night transition— glm::mix(skyBlue, skyOrange, distanceFraction)
//    • HDR Tone mapping    — color = color / (color + 1.0) — Reinhard in FS_COMP
//    • Bloom (glow effect) — Bright-pass → 6-pass Gaussian blur → additive composite
// ============================================================
#pragma once

// ── Fragment shader: world geometry (Blinn-Phong lighting + fog) ──
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

// ── Post-process passes ───────────────────────────────────────
static const char* VS_QUAD = R"(
#version 330 core
layout(location=0) in vec2 aPos;
out vec2 vUV;
void main(){ vUV=aPos*0.5+0.5; gl_Position=vec4(aPos,0,1); }
)";

// Bright-pass: extract pixels above threshold for bloom
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

// Gaussian blur: 5-tap separable kernel (horizontal or vertical)
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

// Composite: scene + bloom, Reinhard HDR tone-map, vignette, slow-mo tint
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
    // Reinhard HDR tone-mapping
    col=col/(col+1.0);
    // Vignette
    vec2 uv2=vUV-0.5;
    float v=1.0-dot(uv2,uv2)*uVignette;
    col*=clamp(v,0.0,1.0);
    FragColor=vec4(col,1.0);
}
)";

// ─────────────────────────────────────────────────────────────
//  Framebuffer Object: GL_RGB16F colour + depth renderbuffer
//  (16 bits per channel — beyond 24-bit True Colour for HDR)
// ─────────────────────────────────────────────────────────────
static FBO makeFBO(int w, int h)
{
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
//  Upload lighting uniforms (called before every draw call)
// ─────────────────────────────────────────────────────────────
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

// ─────────────────────────────────────────────────────────────
//  Draw a box mesh (6 faces × 2 triangles = 36 indices)
// ─────────────────────────────────────────────────────────────
static void drawMesh(GLuint prog, const BoxMesh& m,
                     glm::mat4 model, glm::vec4 col,
                     float wet=0, float emis=0, float rim=0)
{
    glUseProgram(prog);
    glm::mat4 mvp = gVP * model;
    glUniformMatrix4fv(glGetUniformLocation(prog,"uMVP"),  1,GL_FALSE,glm::value_ptr(mvp));
    glUniformMatrix4fv(glGetUniformLocation(prog,"uModel"),1,GL_FALSE,glm::value_ptr(model));
    setWorldUniforms(prog,col,wet,emis,rim);
    glBindVertexArray(m.vao);
    glDrawElements(GL_TRIANGLES,36,GL_UNSIGNED_INT,nullptr);
}

// Draw a road quad mesh (2 triangles = 6 indices)
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
