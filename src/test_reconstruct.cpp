#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "shader.hpp"
#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

// Run from the project root: ./build/TestReconstruct
//
// Tests that reconstructWorldPos(gl_FragCoord.z) in the fragment shader
// returns the correct world position, and attributes any residual error
// to the floor step in rasterisation.
//
// Strategy
// --------
// For each test point P (ground truth, world space):
//
//   1. Render P as a GL_POINT; fragment shader writes reconstructWorldPos to
//      attachment 0.
//
//   2. Project P on the CPU to get the EXACT continuous window coordinate
//      (wx, wy).  Invert that exact coordinate back through invViewProj:
//      this "ideal reconstruction" should recover P to float precision,
//      proving the formula is correct when given exact coords.
//
//   3. The rasteriser floors (wx,wy) to a discrete pixel (px,py) and the
//      fragment centre lands at (px+0.5, py+0.5).  The difference between
//      using that centre vs the exact (wx,wy) is the floor quantisation
//      error.  We show that this accounts for the full GPU vs ground-truth
//      error.

static const int W = 800;
static const int H = 600;

static std::string fmt3(const glm::vec3& v) {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "(% .5f, % .5f, % .5f)", v.x, v.y, v.z);
    return buf;
}

static glm::vec3 reconstructCPU(float fragX, float fragY, float fragZ,
                                 float w, float h,
                                 const glm::mat4& invViewProj) {
    glm::vec2 ndc_xy = (glm::vec2(fragX, fragY) / glm::vec2(w, h)) * 2.0f - 1.0f;
    float z_ndc = fragZ * 2.0f - 1.0f;
    glm::vec4 clip(ndc_xy.x, ndc_xy.y, z_ndc, 1.0f);
    glm::vec4 world = invViewProj * clip;
    return glm::vec3(world) / world.w;
}

int main() {
    if (!glfwInit()) { std::cerr << "GLFW init failed\n"; return 1; }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(W, H, "test_reconstruct", nullptr, nullptr);
    if (!window) { std::cerr << "Window creation failed\n"; glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "GLAD init failed\n";
        glfwDestroyWindow(window); glfwTerminate(); return 1;
    }

    glViewport(0, 0, W, H);

    // FBO: two RGBA32F colour attachments + depth
    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    GLuint colTex[2];
    glGenTextures(2, colTex);
    for (int i = 0; i < 2; ++i) {
        glBindTexture(GL_TEXTURE_2D, colTex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, W, H, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, colTex[i], 0);
    }

    GLuint depthRBO;
    glGenRenderbuffers(1, &depthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, depthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, W, H);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRBO);

    const GLenum drawBufs[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, drawBufs);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "FBO incomplete\n"; return 1;
    }

    const std::vector<glm::vec3> testPoints = {
        { 0.0f,  0.0f,  0.0f},   // projects to exact screen centre (pixel boundary)
        { 1.0f,  0.0f,  0.0f},
        { 0.0f,  1.0f,  0.0f},
        { 2.0f, -1.0f,  3.0f},
        {-3.0f,  2.0f,  5.0f},
        { 0.7f,  0.3f, -2.1f},
        {-1.4f, -0.9f,  6.5f},
    };

    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(testPoints.size() * sizeof(glm::vec3)),
                 testPoints.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    const glm::vec3 camPos(0.0f, 0.0f, 10.0f);
    const glm::mat4 view = glm::lookAt(camPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 proj = glm::perspective(glm::radians(90.0f),
                                             static_cast<float>(W) / static_cast<float>(H),
                                             0.1f, 50.0f);
    const glm::mat4 invViewProj = glm::inverse(proj * view);

    Shader shader("shaders/test_reconstruct.vs", "shaders/test_reconstruct.fs");

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    const float zerof[4] = {};
    glClearBufferfv(GL_COLOR, 1, zerof);

    glPointSize(1.0f);
    shader.use();
    shader.setMat4("view",        view);
    shader.setMat4("projection",  proj);
    shader.setMat4("invViewProj", invViewProj);
    shader.setVec2("resolution",  static_cast<float>(W), static_cast<float>(H));

    glBindVertexArray(vao);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(testPoints.size()));
    glBindVertexArray(0);
    glFinish();

    std::vector<float> buf0(W * H * 4, 0.0f);  // GPU reconstructed
    std::vector<float> buf1(W * H * 4, 0.0f);  // vertex world pos

    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(0, 0, W, H, GL_RGBA, GL_FLOAT, buf0.data());
    glReadBuffer(GL_COLOR_ATTACHMENT1);
    glReadPixels(0, 0, W, H, GL_RGBA, GL_FLOAT, buf1.data());

    std::printf("%-32s  px   py   %-32s  ideal_err   floor_err   gpu_err   explained\n",
                "ground truth (P)", "GPU reconstructed");
    std::printf("%s\n", std::string(140, '-').c_str());

    int passCount = 0, failCount = 0;

    for (const auto& P : testPoints) {
        // --- CPU projection of ground truth ---
        const glm::vec4 clip = proj * view * glm::vec4(P, 1.0f);
        const glm::vec3 ndc  = glm::vec3(clip) / clip.w;
        const float fragZ    = ndc.z * 0.5f + 0.5f;

        // Exact continuous window coordinate (not yet floored)
        const float wx = (ndc.x * 0.5f + 0.5f) * static_cast<float>(W);
        const float wy = (ndc.y * 0.5f + 0.5f) * static_cast<float>(H);

        // "Ideal" reconstruction: invert the EXACT projected coords back.
        // Should recover P to float precision, proving the formula is correct.
        const glm::vec3 ideal = reconstructCPU(wx, wy, fragZ,
                                               static_cast<float>(W), static_cast<float>(H),
                                               invViewProj);
        const float idealErr = glm::length(ideal - P);

        if ((int)wx < 0 || (int)wx >= W || (int)wy < 0 || (int)wy >= H) {
            std::printf("SKIP  %-32s  off-screen\n", fmt3(P).c_str());
            continue;
        }

        // Find the actual rasterised pixel (scan 3x3 for non-zero alpha)
        auto pixIdx = [&](int x, int y) { return (y * W + x) * 4; };
        int px = (int)wx, py = (int)wy;
        bool found = false;
        int idx = pixIdx(px, py);
        for (int dy = -1; dy <= 1 && !found; ++dy)
            for (int dx = -1; dx <= 1 && !found; ++dx) {
                int nx = px+dx, ny = py+dy;
                if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
                if (buf1[pixIdx(nx,ny)+3] > 0.5f) {
                    idx = pixIdx(nx, ny); px = nx; py = ny; found = true;
                }
            }

        if (!found) {
            std::printf("SKIP  %-32s  pixel not rasterised\n", fmt3(P).c_str());
            continue;
        }

        const glm::vec3 gpuReconstructed(buf0[idx], buf0[idx+1], buf0[idx+2]);
        const float gpuErr = glm::length(gpuReconstructed - P);

        // "Floor reconstruction": what you get when you use the pixel centre
        // (px+0.5, py+0.5) instead of the exact (wx, wy).
        // This is exactly what gl_FragCoord.xy is, so it predicts the GPU error.
        const glm::vec3 floorRecon = reconstructCPU(px + 0.5f, py + 0.5f, fragZ,
                                                     static_cast<float>(W), static_cast<float>(H),
                                                     invViewProj);
        const float floorErr = glm::length(floorRecon - P);

        // Does the floor offset fully explain the GPU error?
        const float unexplained = glm::length(gpuReconstructed - floorRecon);
        const bool explained = unexplained < 1e-3f;

        std::printf("%s  %-32s  %3d  %3d  %-32s  %.2e    %.5f    %.5f  %s\n",
                    explained ? "PASS" : "FAIL",
                    fmt3(P).c_str(), px, py,
                    fmt3(gpuReconstructed).c_str(),
                    idealErr, floorErr, gpuErr,
                    explained ? "yes" : "NO");

        if (explained) ++passCount; else ++failCount;
    }

    std::printf("%s\n", std::string(140, '-').c_str());
    std::printf("%d passed, %d failed\n\n", passCount, failCount);
    std::printf("ideal_err  : error when inverting the EXACT projected coords  (proves formula is correct)\n");
    std::printf("floor_err  : error from using pixel centre instead of exact coords  (rasterisation quantisation)\n");
    std::printf("gpu_err    : actual GPU reconstruction error vs ground truth\n");
    std::printf("explained  : gpu_err == floor_err  (floor step fully accounts for the error)\n");

    glDeleteTextures(2, colTex);
    glDeleteRenderbuffers(1, &depthRBO);
    glDeleteFramebuffers(1, &fbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glfwDestroyWindow(window);
    glfwTerminate();

    return failCount > 0 ? 1 : 0;
}
