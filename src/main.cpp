#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/detail/qualifier.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include "shader.hpp"
#include "camera.hpp"
#include "debug_ui.hpp"
#include "framebuffer.hpp"
#include "stb_image.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>

void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void mouse_callback(GLFWwindow *window, double xpos, double ypos);
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);
void processInput(GLFWwindow *window, bool uiWantsMouse, bool uiWantsKeyboard);
void drawThreeCubes(Shader &shader, unsigned int VAO,
                    const std::array<int, 3> &mediumIds);
void drawPlane(Shader &shader, unsigned int VAO);
glm::mat4 makePlaneModel(const glm::vec3 &normal, float offset, float scale);
void setMouseLook(GLFWwindow *window, bool enabled);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;
constexpr int maxPeelLayers = 16;
int framebufferWidth = SCR_WIDTH;
int framebufferHeight = SCR_HEIGHT;

// camera
Camera camera(glm::vec3(0.0f, 0.0f, 10.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
bool mouseLookEnabled = false;
bool uiWantsMouse = false;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

float vertices[] = {
    -0.5f, -0.5f, -0.5f, 0.0f, 0.0f,
    0.5f,  0.5f,  -0.5f, 1.0f, 1.0f,
    0.5f,  -0.5f, -0.5f, 1.0f, 0.0f,
    0.5f,  0.5f,  -0.5f, 1.0f, 1.0f,
    -0.5f, -0.5f, -0.5f, 0.0f, 0.0f,
    -0.5f, 0.5f,  -0.5f, 0.0f, 1.0f,

    -0.5f, -0.5f, 0.5f,  0.0f, 0.0f,
    0.5f,  -0.5f, 0.5f,  1.0f, 0.0f,
    0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
    0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
    -0.5f, 0.5f,  0.5f,  0.0f, 1.0f,
    -0.5f, -0.5f, 0.5f,  0.0f, 0.0f,

    -0.5f, 0.5f,  0.5f,  1.0f, 0.0f,
    -0.5f, 0.5f,  -0.5f, 1.0f, 1.0f,
    -0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
    -0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
    -0.5f, -0.5f, 0.5f,  0.0f, 0.0f,
    -0.5f, 0.5f,  0.5f,  1.0f, 0.0f,

    0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
    0.5f,  -0.5f, -0.5f, 0.0f, 1.0f,
    0.5f,  0.5f,  -0.5f, 1.0f, 1.0f,
    0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
    0.5f,  -0.5f, 0.5f,  0.0f, 0.0f,
    0.5f,  -0.5f, -0.5f, 0.0f, 1.0f,

    -0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
    0.5f,  -0.5f, -0.5f, 1.0f, 1.0f,
    0.5f,  -0.5f, 0.5f,  1.0f, 0.0f,
    0.5f,  -0.5f, 0.5f,  1.0f, 0.0f,
    -0.5f, -0.5f, 0.5f,  0.0f, 0.0f,
    -0.5f, -0.5f, -0.5f, 0.0f, 1.0f,

    -0.5f, 0.5f,  -0.5f, 0.0f, 1.0f,
    0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
    0.5f,  0.5f,  -0.5f, 1.0f, 1.0f,
    0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
    -0.5f, 0.5f,  -0.5f, 0.0f, 1.0f,
    -0.5f, 0.5f,  0.5f,  0.0f, 0.0f
    };

glm::vec3 cubePositions[] = {
    glm::vec3(0.0f, 0.0f, 0.0f),    glm::vec3(2.0f, 5.0f, -15.0f),
    glm::vec3(-1.5f, -2.2f, -2.5f), glm::vec3(-3.8f, -2.0f, -12.3f),
    glm::vec3(2.4f, -0.4f, -3.5f),  glm::vec3(-1.7f, 3.0f, -7.5f),
    glm::vec3(1.3f, -2.0f, -2.5f),  glm::vec3(1.5f, 2.0f, -2.5f),
    glm::vec3(1.5f, 0.2f, -1.5f),   glm::vec3(-1.3f, 1.0f, -1.5f)};

struct HomogeneousMedium {
  int id;
  float attenuationCoefficient;
};

float screenQuadVertices[] = {
    // positions   // tex coords
    -1.0f, 1.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, 0.0f,

    -1.0f, 1.0f, 0.0f, 1.0f, 1.0f,  -1.0f, 1.0f, 0.0f, 1.0f, 1.0f,  1.0f, 1.0f,
};

float planeVertices[] = {
    -0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f, 0.5f,  0.5f, 0.0f,

    -0.5f, -0.5f, 0.0f, 0.5f, 0.5f,  0.0f, -0.5f, 0.5f, 0.0f,
};

int main() {
  if (!glfwInit()) {
    std::cerr << "GLFW init failed\n";
    return -1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT,
                                        "Photon Splatter", nullptr, nullptr);
  if (!window) {
    std::cerr << "Window creation failed\n";
    glfwTerminate();
    return -1;
  }

  glfwMakeContextCurrent(window);
  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
  glfwSetCursorPosCallback(window, mouse_callback);
  glfwSetScrollCallback(window, scroll_callback);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cerr << "Failed to initialize GLAD\n";
    glfwDestroyWindow(window);
    glfwTerminate();
    return -1;
  }

  bool vsyncEnabled = true;
  float clipNear = 0.1f;
  float clipFar = 50.0f;
  int selectedPeelLayer = 0;
  int generatedLayerCount = 1;
  int appMode = 0;        // 0 = AOV   1 = Transmittance
  int aovDisplayMode = 0; // 0 = Color  1 = Depth  2 = Medium
  int transVizMode = 0;   // 0 = Transmittance  1 = World Depth  2 = Media Count
  glm::vec3 planeNormal(0.0f, 0.0f, 1.0f);
  float planeOffset = -5.0f;
  float planeScale = 10.0f;
  const std::array<HomogeneousMedium, 3> cubeMedia = {
      HomogeneousMedium{3, 0.25f},
      HomogeneousMedium{2, 0.6f},
      HomogeneousMedium{1, 1.0f},
  };
  const std::array<int, 3> cubeMediumIds = {
      cubeMedia[0].id,
      cubeMedia[1].id,
      cubeMedia[2].id,
  };

  glfwSwapInterval(vsyncEnabled ? 1 : 0);

  glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
  glViewport(0, 0, framebufferWidth, framebufferHeight);

  glEnable(GL_DEPTH_TEST);

  auto debugUi = std::make_unique<DebugUi>(window);

  // Texture arrays
  GLuint colorArray, depthArray, mediumArray;
  glGenTextures(1, &colorArray);
  glBindTexture(GL_TEXTURE_2D_ARRAY, colorArray);
  glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, framebufferWidth,
               framebufferHeight, maxPeelLayers, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               NULL);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  glGenTextures(1, &depthArray);
  glBindTexture(GL_TEXTURE_2D_ARRAY, depthArray);
  glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT24, framebufferWidth,
               framebufferHeight, maxPeelLayers, 0, GL_DEPTH_COMPONENT,
               GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  glGenTextures(1, &mediumArray);
  glBindTexture(GL_TEXTURE_2D_ARRAY, mediumArray);
  glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_R32UI, framebufferWidth,
               framebufferHeight, maxPeelLayers, 0, GL_RED_INTEGER, GL_UNSIGNED_INT,
               NULL);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  // Peel FBO
  unsigned int peelQuery;
  glGenQueries(1, &peelQuery);

  GLuint peelFBO;
  glGenFramebuffers(1, &peelFBO);

  glBindFramebuffer(GL_FRAMEBUFFER, peelFBO);
  const GLenum peelAttachments[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
  glDrawBuffers(2, peelAttachments);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  // Shaders
  Shader ourShader("shaders/shader.vs", "shaders/shader.fs");
  Shader depthPeelShader("shaders/shader.vs", "shaders/depth_peel.fs");
  Shader aovColorShader("shaders/depth.vs", "shaders/aov_color.fs");
  Shader depthShader("shaders/depth.vs", "shaders/depth.fs");
  Shader mediumDebugShader("shaders/depth.vs", "shaders/medium_debug.fs");
  Shader flatColorShader("shaders/flat_color.vs", "shaders/flat_trans.fs");
  Shader wireframeShader("shaders/wireframe.vs", "shaders/wireframe.fs");

  // Screen quad VAO
  unsigned int screenQuadVAO, screenQuadVBO;
  glGenVertexArrays(1, &screenQuadVAO);
  glGenBuffers(1, &screenQuadVBO);
  glBindVertexArray(screenQuadVAO);
  glBindBuffer(GL_ARRAY_BUFFER, screenQuadVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(screenQuadVertices), screenQuadVertices,
               GL_STATIC_DRAW);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                        (void *)(2 * sizeof(float)));
  glEnableVertexAttribArray(1);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);

  // Plane VAO
  unsigned int planeVAO, planeVBO;
  glGenVertexArrays(1, &planeVAO);
  glGenBuffers(1, &planeVBO);
  glBindVertexArray(planeVAO);
  glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), planeVertices,
               GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);

  // Cube VAO
  unsigned int VAO, VBO;
  glGenVertexArrays(1, &VAO);
  glBindVertexArray(VAO);
  glGenBuffers(1, &VBO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);

  stbi_set_flip_vertically_on_load(true);

  unsigned int texture1;
  glGenTextures(1, &texture1);
  glBindTexture(GL_TEXTURE_2D, texture1);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  int width, height, nrChannels;
  unsigned char *data = stbi_load("assets/textures/container.jpg", &width,
                                  &height, &nrChannels, 0);
  if (data) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB,
                 GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
  } else {
    std::cout << "Failed to load texture1\n";
  }
  stbi_image_free(data);

  unsigned int texture2;
  glGenTextures(1, &texture2);
  glBindTexture(GL_TEXTURE_2D, texture2);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  data = stbi_load("assets/textures/awesomeface.png", &width, &height,
                   &nrChannels, 0);
  if (data) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
  } else {
    std::cout << "Failed to load texture2\n";
  }
  stbi_image_free(data);

  ourShader.use();
  ourShader.setInt("texture1", 0);
  ourShader.setInt("texture2", 1);

  depthPeelShader.use();
  depthPeelShader.setInt("texture1", 0);
  depthPeelShader.setInt("texture2", 1);
  depthPeelShader.setInt("previousDepths", 2);
  depthPeelShader.setInt("previousMedia",  3);
  depthPeelShader.setFloat("peelEpsilon", 0.00001f);

  // Permanent texture unit bindings:
  //   0 = texture1 (container)
  //   1 = texture2 (awesomeface)
  //   2 = depthArray   — depth peel read + flat_trans
  //   3 = mediumArray  — depth peel read + flat_trans
  //   4 = colorArray   — AOV color display
  glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texture1);
  glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, texture2);
  glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D_ARRAY, depthArray);
  glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D_ARRAY, mediumArray);
  glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D_ARRAY, colorArray);
  glActiveTexture(GL_TEXTURE0);

  lastFrame = glfwGetTime();

  // MARK: R-LOOP

  while (!glfwWindowShouldClose(window)) {
    float currentFrame = static_cast<float>(glfwGetTime());
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    glfwPollEvents();
    debugUi->beginFrame();
    uiWantsMouse = debugUi->wantsMouse();
    const bool uiWantsKeyboard = debugUi->wantsKeyboard();

    processInput(window, uiWantsMouse, uiWantsKeyboard);

    glViewport(0, 0, framebufferWidth, framebufferHeight);

    const float aspectRatio = framebufferHeight > 0
                                  ? static_cast<float>(framebufferWidth) /
                                        static_cast<float>(framebufferHeight)
                                  : 1.0f;
    glm::mat4 projection = glm::perspective(glm::radians(90.0f),
                                            aspectRatio, clipNear, clipFar);
    glm::mat4 view       = camera.GetViewMatrix();
    glm::mat4 invViewProj = glm::inverse(projection * view);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);

    // ---- Depth peeling (feeds both modes) ----

    glBindFramebuffer(GL_FRAMEBUFFER, peelFBO);

    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, colorArray,  0, 0);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,  depthArray,  0, 0);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, mediumArray, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    int mediumClearColor[4] = {0, 0, 0, 0};
    glClearBufferiv(GL_COLOR, 1, mediumClearColor);

    ourShader.use();
    ourShader.setMat4("projection", projection);
    ourShader.setMat4("view", view);
    drawThreeCubes(ourShader, VAO, cubeMediumIds);

    generatedLayerCount = 1;
    for (int layer = 1; layer < maxPeelLayers; ++layer) {
      glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, colorArray,  0, layer);
      glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,  depthArray,  0, layer);
      glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, mediumArray, 0, layer);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      glClearBufferiv(GL_COLOR, 1, mediumClearColor);

      depthPeelShader.use();
      depthPeelShader.setMat4("projection", projection);
      depthPeelShader.setMat4("view", view);
      depthPeelShader.setInt("previousLayerIdx", layer - 1);

      glBeginQuery(GL_ANY_SAMPLES_PASSED, peelQuery);
      drawThreeCubes(depthPeelShader, VAO, cubeMediumIds);
      glEndQuery(GL_ANY_SAMPLES_PASSED);

      unsigned int anySamplesPassed = GL_FALSE;
      glGetQueryObjectuiv(peelQuery, GL_QUERY_RESULT, &anySamplesPassed);
      if (anySamplesPassed == GL_FALSE)
        break;

      generatedLayerCount = layer + 1;
    }

    // ---- Display ----

    selectedPeelLayer = std::clamp(selectedPeelLayer, 0, generatedLayerCount - 1);

    Framebuffer::bindDefault();
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (appMode == 0) {
      // ---- AOV: inspect one depth-peel layer ----
      glDisable(GL_DEPTH_TEST);

      if (aovDisplayMode == 0) {
        aovColorShader.use();
        aovColorShader.setInt("colorTextures", 4);
        aovColorShader.setInt("layer", selectedPeelLayer);
      } else if (aovDisplayMode == 1) {
        depthShader.use();
        depthShader.setInt("depthTextures", 2);
        depthShader.setInt("layer", selectedPeelLayer);
        depthShader.setFloat("nearPlane", clipNear);
        depthShader.setFloat("farPlane",  clipFar);
      } else {
        mediumDebugShader.use();
        mediumDebugShader.setInt("mediumTextures", 3);
        mediumDebugShader.setInt("layer", selectedPeelLayer);
      }

      glBindVertexArray(screenQuadVAO);
      glDrawArrays(GL_TRIANGLES, 0, 6);
      glBindVertexArray(0);

      glEnable(GL_DEPTH_TEST);

    } else {
      // ---- Transmittance: wireframe + movable cutting plane ----
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
      wireframeShader.use();
      wireframeShader.setMat4("projection", projection);
      wireframeShader.setMat4("view", view);
      wireframeShader.setVec3("color", 1.0f, 0.0f, 0.0f);
      drawThreeCubes(wireframeShader, VAO, cubeMediumIds);
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

      flatColorShader.use();
      flatColorShader.setInt("depthMap",  2);
      flatColorShader.setInt("mediumMap", 3);
      flatColorShader.setInt("numLayers", generatedLayerCount);
      flatColorShader.setMat4("projection",  projection);
      flatColorShader.setMat4("view",        view);
      flatColorShader.setMat4("invViewProj", invViewProj);
      flatColorShader.setVec2("resolution",  framebufferWidth, framebufferHeight);
      flatColorShader.setVec3("cameraWorldPos",
                              camera.Position.x, camera.Position.y, camera.Position.z);
      flatColorShader.setFloat("far", clipFar);
      flatColorShader.setInt("vizMode", transVizMode);
      flatColorShader.setMat4("model", makePlaneModel(planeNormal, planeOffset, planeScale));
      drawPlane(flatColorShader, planeVAO);

      glDisable(GL_BLEND);
    }

    if (debugUi->draw(camera, vsyncEnabled, clipNear, clipFar,
                      appMode, selectedPeelLayer, generatedLayerCount, aovDisplayMode,
                      planeNormal, planeOffset, planeScale, transVizMode)) {
      glfwSwapInterval(vsyncEnabled ? 1 : 0);
    }
    debugUi->endFrame();

    glfwSwapBuffers(window);
  }

  debugUi.reset();

  glDeleteQueries(1, &peelQuery);
  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &VBO);
  glDeleteVertexArrays(1, &screenQuadVAO);
  glDeleteBuffers(1, &screenQuadVBO);
  glDeleteVertexArrays(1, &planeVAO);
  glDeleteBuffers(1, &planeVBO);
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}

void processInput(GLFWwindow *window, bool uiWantsMouse, bool uiWantsKeyboard) {
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);

  bool wantsMouseLook =
      !uiWantsMouse &&
      glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
  if (wantsMouseLook != mouseLookEnabled)
    setMouseLook(window, wantsMouseLook);

  if (uiWantsMouse || uiWantsKeyboard)
    return;

  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    camera.ProcessKeyboard(FORWARD, deltaTime);
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    camera.ProcessKeyboard(BACKWARD, deltaTime);
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    camera.ProcessKeyboard(LEFT, deltaTime);
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    camera.ProcessKeyboard(RIGHT, deltaTime);
}

void drawThreeCubes(Shader &shader, unsigned int VAO,
                    const std::array<int, 3> &mediumIds) {
  glBindVertexArray(VAO);

  glm::mat4 model = glm::mat4(1.0f);
  model = glm::scale(model, glm::vec3(10.0f));
  shader.setMat4("model", model);
  shader.setInt("mediumId", mediumIds[0]);
  glDrawArrays(GL_TRIANGLES, 0, 36);

  model = glm::mat4(1.0f);
  model = glm::scale(model, glm::vec3(5.0f));
  shader.setMat4("model", model);
  shader.setInt("mediumId", mediumIds[1]);
  glDrawArrays(GL_TRIANGLES, 0, 36);

  model = glm::mat4(1.0f);
  model = glm::scale(model, glm::vec3(2.0f));
  shader.setMat4("model", model);
  shader.setInt("mediumId", mediumIds[2]);
  glDrawArrays(GL_TRIANGLES, 0, 36);

  glBindVertexArray(0);
}

void drawPlane(Shader &shader, unsigned int VAO) {
  glBindVertexArray(VAO);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glBindVertexArray(0);
}

glm::mat4 makePlaneModel(const glm::vec3 &normal, float offset, float scale) {
  glm::vec3 n = glm::length(normal) < 0.0001f
                    ? glm::vec3(0.0f, 0.0f, 1.0f)
                    : glm::normalize(normal);

  const glm::vec3 helper = std::abs(n.y) < 0.99f ? glm::vec3(0.0f, 1.0f, 0.0f)
                                                 : glm::vec3(1.0f, 0.0f, 0.0f);
  const glm::vec3 tangent   = glm::normalize(glm::cross(helper, n));
  const glm::vec3 bitangent = glm::cross(n, tangent);
  const glm::vec3 position  = n * offset;

  glm::mat4 model(1.0f);
  model[0] = glm::vec4(tangent   * scale, 0.0f);
  model[1] = glm::vec4(bitangent * scale, 0.0f);
  model[2] = glm::vec4(n, 0.0f);
  model[3] = glm::vec4(position, 1.0f);
  return model;
}

void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
  framebufferWidth  = width;
  framebufferHeight = height;
  glViewport(0, 0, width, height);
}

void setMouseLook(GLFWwindow *window, bool enabled) {
  mouseLookEnabled = enabled;
  firstMouse = true;

  glfwSetInputMode(window, GLFW_CURSOR,
                   enabled ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);

  if (glfwRawMouseMotionSupported()) {
    glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION,
                     enabled ? GLFW_TRUE : GLFW_FALSE);
  }
}

void mouse_callback(GLFWwindow *window, double xposIn, double yposIn) {
  float xpos = static_cast<float>(xposIn);
  float ypos = static_cast<float>(yposIn);

  if (firstMouse) {
    lastX = xpos;
    lastY = ypos;
    firstMouse = false;
  }

  float xoffset = xpos - lastX;
  float yoffset = lastY - ypos;

  lastX = xpos;
  lastY = ypos;

  if (!mouseLookEnabled)
    return;

  camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
  if (uiWantsMouse)
    return;
  camera.ProcessMouseScroll(static_cast<float>(yoffset));
}
