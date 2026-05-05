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
#include <iostream>
#include <memory>
#include <vector>

void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void mouse_callback(GLFWwindow *window, double xpos, double ypos);
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);
void processInput(GLFWwindow *window, bool uiWantsMouse, bool uiWantsKeyboard);
void drawThreeCubes(Shader &shader, unsigned int VAO);
// callback when mouseLookEnabled changes
void setMouseLook(GLFWwindow *window, bool enabled);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;
constexpr int maxPeelLayers = 16;
int framebufferWidth = SCR_WIDTH;
int framebufferHeight = SCR_HEIGHT;

// camera
Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
bool mouseLookEnabled = false;
bool uiWantsMouse = false;

// timing
float deltaTime = 0.0f; // time between current frame and last frame
float lastFrame = 0.0f;

float vertices[] = {
    -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, 0.5f,  -0.5f, -0.5f, 1.0f, 0.0f,
    0.5f,  0.5f,  -0.5f, 1.0f, 1.0f, 0.5f,  0.5f,  -0.5f, 1.0f, 1.0f,
    -0.5f, 0.5f,  -0.5f, 0.0f, 1.0f, -0.5f, -0.5f, -0.5f, 0.0f, 0.0f,

    -0.5f, -0.5f, 0.5f,  0.0f, 0.0f, 0.5f,  -0.5f, 0.5f,  1.0f, 0.0f,
    0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
    -0.5f, 0.5f,  0.5f,  0.0f, 1.0f, -0.5f, -0.5f, 0.5f,  0.0f, 0.0f,

    -0.5f, 0.5f,  0.5f,  1.0f, 0.0f, -0.5f, 0.5f,  -0.5f, 1.0f, 1.0f,
    -0.5f, -0.5f, -0.5f, 0.0f, 1.0f, -0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
    -0.5f, -0.5f, 0.5f,  0.0f, 0.0f, -0.5f, 0.5f,  0.5f,  1.0f, 0.0f,

    0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 0.5f,  0.5f,  -0.5f, 1.0f, 1.0f,
    0.5f,  -0.5f, -0.5f, 0.0f, 1.0f, 0.5f,  -0.5f, -0.5f, 0.0f, 1.0f,
    0.5f,  -0.5f, 0.5f,  0.0f, 0.0f, 0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

    -0.5f, -0.5f, -0.5f, 0.0f, 1.0f, 0.5f,  -0.5f, -0.5f, 1.0f, 1.0f,
    0.5f,  -0.5f, 0.5f,  1.0f, 0.0f, 0.5f,  -0.5f, 0.5f,  1.0f, 0.0f,
    -0.5f, -0.5f, 0.5f,  0.0f, 0.0f, -0.5f, -0.5f, -0.5f, 0.0f, 1.0f,

    -0.5f, 0.5f,  -0.5f, 0.0f, 1.0f, 0.5f,  0.5f,  -0.5f, 1.0f, 1.0f,
    0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
    -0.5f, 0.5f,  0.5f,  0.0f, 0.0f, -0.5f, 0.5f,  -0.5f, 0.0f, 1.0f};

glm::vec3 cubePositions[] = {
    glm::vec3(0.0f, 0.0f, 0.0f),    glm::vec3(2.0f, 5.0f, -15.0f),
    glm::vec3(-1.5f, -2.2f, -2.5f), glm::vec3(-3.8f, -2.0f, -12.3f),
    glm::vec3(2.4f, -0.4f, -3.5f),  glm::vec3(-1.7f, 3.0f, -7.5f),
    glm::vec3(1.3f, -2.0f, -2.5f),  glm::vec3(1.5f, 2.0f, -2.5f),
    glm::vec3(1.5f, 0.2f, -1.5f),   glm::vec3(-1.3f, 1.0f, -1.5f)};

// unsigned int indices[] = {
//     0, 1, 3, // first triangle
//     1, 2, 3  // second triangle
// };

float screenQuadVertices[] = {
    // positions   // tex coords
    -1.0f, 1.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, 0.0f,

    -1.0f, 1.0f, 0.0f, 1.0f, 1.0f,  -1.0f, 1.0f, 0.0f, 1.0f, 1.0f,  1.0f, 1.0f,
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
  float clipFar = 10.0f;
  int selectedPeelLayer = 0;
  int generatedLayerCount = 1;
  bool showDepth = false;

  glfwSwapInterval(vsyncEnabled ? 1 : 0);

  glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
  glViewport(0, 0, framebufferWidth, framebufferHeight);

  glEnable(GL_DEPTH_TEST);

  auto debugUi = std::make_unique<DebugUi>(window);

  std::vector<Framebuffer> peelLayers;
  peelLayers.reserve(maxPeelLayers);
  for (int i = 0; i < maxPeelLayers; ++i) {
    peelLayers.emplace_back(framebufferWidth, framebufferHeight);
  }

  unsigned int peelQuery;
  glGenQueries(1, &peelQuery);

  Shader ourShader("shaders/shader.vs", "shaders/shader.fs");
  Shader depthPeelShader("shaders/shader.vs", "shaders/depth_peel.fs");
  Shader depthShader("shaders/depth.vs", "shaders/depth.fs");

  unsigned int screenQuadVAO;
  unsigned int screenQuadVBO;

  glGenVertexArrays(1, &screenQuadVAO);
  glGenBuffers(1, &screenQuadVBO);

  glBindVertexArray(screenQuadVAO);

  glBindBuffer(GL_ARRAY_BUFFER, screenQuadVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(screenQuadVertices), screenQuadVertices,
               GL_STATIC_DRAW);

  // location 0: vec2 position
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  // location 1: vec2 tex coord
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                        (void *)(2 * sizeof(float)));
  glEnableVertexAttribArray(1);

  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glBindVertexArray(0);

  unsigned int VAO;
  glGenVertexArrays(1, &VAO);
  glBindVertexArray(VAO);

  // unsigned int EBO;
  // glGenBuffers(1, &EBO);
  // glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
  // glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
  // GL_STATIC_DRAW);

  unsigned int VBO;
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

  // flip y for textures
  stbi_set_flip_vertically_on_load(true);

  // texture1: container
  unsigned int texture1;
  glGenTextures(1, &texture1);
  glBindTexture(GL_TEXTURE_2D, texture1);
  // set the texture wrapping/filtering options (on the currently bound texture
  // object)
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  // load and generate the texture
  int width, height, nrChannels;
  unsigned char *data = stbi_load("assets/textures/container.jpg", &width,
                                  &height, &nrChannels, 0);
  if (data) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB,
                 GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
  } else {
    std::cout << "Failed to load texture1" << std::endl;
  }
  stbi_image_free(data);

  // texture2: awesomeface
  unsigned int texture2;
  glGenTextures(1, &texture2);
  glBindTexture(GL_TEXTURE_2D, texture2);
  // set the texture wrapping/filtering options (on the currently bound texture
  // object)
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  // load and generate the texture
  data = stbi_load("assets/textures/awesomeface.png", &width, &height,
                   &nrChannels, 0);
  if (data) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
  } else {
    std::cout << "Failed to load texture2" << std::endl;
  }
  stbi_image_free(data);

  ourShader.use();
  ourShader.setInt("texture1", 0);
  ourShader.setInt("texture2", 1);

  depthPeelShader.use();
  depthPeelShader.setInt("texture1", 0);
  depthPeelShader.setInt("texture2", 1);
  depthPeelShader.setInt("previousDepth", 2);

  // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  // glPointSize(50.0f);

  lastFrame = glfwGetTime();
  float modelRotationAngle = 0.0f;

  // MARK: R-LOOP

  while (!glfwWindowShouldClose(window)) {
    float currentFrame = static_cast<float>(glfwGetTime());
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    glfwPollEvents();
    debugUi->beginFrame();
    uiWantsMouse = debugUi->wantsMouse();
    const bool uiWantsKeyboard = debugUi->wantsKeyboard();

    modelRotationAngle += glm::radians(40.0f) * deltaTime;

    processInput(window, uiWantsMouse, uiWantsKeyboard);

    for (Framebuffer &layer : peelLayers) {
      layer.resize(framebufferWidth, framebufferHeight);
    }
    glViewport(0, 0, framebufferWidth, framebufferHeight);

    const float aspectRatio = framebufferHeight > 0
                                  ? static_cast<float>(framebufferWidth) /
                                        static_cast<float>(framebufferHeight)
                                  : 1.0f;
    glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom),
                                            aspectRatio, clipNear, clipFar);
    glm::mat4 view = camera.GetViewMatrix();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);

    // Layer 0: render the closest visible layer.
    peelLayers[0].bind();

    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture1);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, texture2);

    ourShader.use();
    ourShader.setMat4("projection", projection);
    ourShader.setMat4("view", view);
    drawThreeCubes(ourShader, VAO);

    generatedLayerCount = 1;
    for (int layer = 1; layer < maxPeelLayers; ++layer) {
      peelLayers[layer].bind();

      glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, texture1);
      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D, texture2);
      glActiveTexture(GL_TEXTURE2);
      glBindTexture(GL_TEXTURE_2D, peelLayers[layer - 1].depthTexture());

      depthPeelShader.use();
      depthPeelShader.setFloat("peelEpsilon", 0.00001f);
      depthPeelShader.setMat4("projection", projection);
      depthPeelShader.setMat4("view", view);

      glBeginQuery(GL_ANY_SAMPLES_PASSED, peelQuery);
      drawThreeCubes(depthPeelShader, VAO);
      glEndQuery(GL_ANY_SAMPLES_PASSED);

      unsigned int anySamplesPassed = GL_FALSE;
      glGetQueryObjectuiv(peelQuery, GL_QUERY_RESULT, &anySamplesPassed);
      if (anySamplesPassed == GL_FALSE) {
        break;
      }

      generatedLayerCount = layer + 1;
    }

    selectedPeelLayer =
        std::clamp(selectedPeelLayer, 0, generatedLayerCount - 1);
    const Framebuffer &displayFbo = peelLayers[selectedPeelLayer];

    Framebuffer::bindDefault();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (showDepth) {
      glDisable(GL_DEPTH_TEST);

      depthShader.use();
      depthShader.setInt("depthTex", 0);
      depthShader.setFloat("nearPlane", clipNear);
      depthShader.setFloat("farPlane", clipFar);

      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, displayFbo.depthTexture());

      glBindVertexArray(screenQuadVAO);
      glDrawArrays(GL_TRIANGLES, 0, 6);
      glBindVertexArray(0);

      glEnable(GL_DEPTH_TEST);
    } else {
      glBindFramebuffer(GL_READ_FRAMEBUFFER, displayFbo.id());
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
      glBlitFramebuffer(0, 0, displayFbo.width(), displayFbo.height(), 0, 0,
                        framebufferWidth, framebufferHeight,
                        GL_COLOR_BUFFER_BIT, GL_NEAREST);
      Framebuffer::bindDefault();
    }

    if (debugUi->draw(camera, vsyncEnabled, clipNear, clipFar,
                      selectedPeelLayer, generatedLayerCount, showDepth)) {
      glfwSwapInterval(vsyncEnabled ? 1 : 0);
    }
    debugUi->endFrame();

    glfwSwapBuffers(window);
  }

  debugUi.reset();

  glDeleteQueries(1, &peelQuery);
  for (Framebuffer &layer : peelLayers) {
    layer.destroy();
  }
  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &VBO);
  glDeleteVertexArrays(1, &screenQuadVAO);
  glDeleteBuffers(1, &screenQuadVBO);
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this
// frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window, bool uiWantsMouse, bool uiWantsKeyboard) {
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);

  // user may hold down right click to enable camera look around
  bool wantsMouseLook =
      !uiWantsMouse &&
      glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
  if (wantsMouseLook != mouseLookEnabled) {
    setMouseLook(window, wantsMouseLook);
  }

  if (uiWantsMouse || uiWantsKeyboard) {
    return;
  }

  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    camera.ProcessKeyboard(FORWARD, deltaTime);
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    camera.ProcessKeyboard(BACKWARD, deltaTime);
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    camera.ProcessKeyboard(LEFT, deltaTime);
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    camera.ProcessKeyboard(RIGHT, deltaTime);
}

void drawThreeCubes(Shader &shader, unsigned int VAO) {
  glBindVertexArray(VAO);

  glm::mat4 model = glm::mat4(1.0f);
  model = glm::scale(model, glm::vec3(2.0f));
  shader.setMat4("model", model);
  glDrawArrays(GL_TRIANGLES, 0, 36);

  model = glm::mat4(1.0f);
  shader.setMat4("model", model);
  glDrawArrays(GL_TRIANGLES, 0, 36);

  model = glm::mat4(1.0f);
  model = glm::scale(model, glm::vec3(0.5f));
  shader.setMat4("model", model);
  glDrawArrays(GL_TRIANGLES, 0, 36);

  glBindVertexArray(0);
}

// glfw: whenever the window size changed (by OS or user resize) this callback
// function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
  framebufferWidth = width;
  framebufferHeight = height;
  // make sure the viewport matches the new window dimensions; note that width
  // and height will be significantly larger than specified on retina displays.
  glViewport(0, 0, width, height);
}

void setMouseLook(GLFWwindow *window, bool enabled) {
  mouseLookEnabled = enabled;
  firstMouse = true;

  // hide cursor in mouse look mode to prevent mouse position failing to update
  // when hitting window border
  glfwSetInputMode(window, GLFW_CURSOR,
                   enabled ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);

  if (glfwRawMouseMotionSupported()) {
    glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION,
                     enabled ? GLFW_TRUE : GLFW_FALSE);
  }
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_callback(GLFWwindow *window, double xposIn, double yposIn) {
  float xpos = static_cast<float>(xposIn);
  float ypos = static_cast<float>(yposIn);

  if (firstMouse) {
    lastX = xpos;
    lastY = ypos;
    firstMouse = false;
  }

  float xoffset = xpos - lastX;
  float yoffset =
      lastY - ypos; // reversed since y-coordinates go from bottom to top

  lastX = xpos;
  lastY = ypos;

  if (!mouseLookEnabled) {
    return;
  }

  camera.ProcessMouseMovement(xoffset, yoffset);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
  if (uiWantsMouse) {
    return;
  }

  camera.ProcessMouseScroll(static_cast<float>(yoffset));
}
