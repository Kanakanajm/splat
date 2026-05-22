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

#include "ray_model.hpp"
#include "scene.hpp"
#include "scene_config.hpp"
#include "point_light.hpp"
#include "photon_tracer.hpp"
#include "random.hpp"
#include "tiny_bvh.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>


void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void mouse_callback(GLFWwindow *window, double xpos, double ypos);
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);
void processInput(GLFWwindow *window, bool uiWantsMouse, bool uiWantsKeyboard);
// callback when mouseLookEnabled changes
void setMouseLook(GLFWwindow *window, bool enabled);
// set the shared camera matrices on any shader (identity model)
void setCameraUniforms(Shader &shader, const glm::mat4 &projection, const glm::mat4 &view);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;
int framebufferWidth = SCR_WIDTH;
int framebufferHeight = SCR_HEIGHT;

// camera (frames the origin-centered nested cubes; fly with WASD + right-drag)
Camera camera(glm::vec3(0.0f, 0.0f, 2.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
bool mouseLookEnabled = false;
bool uiWantsMouse = false;

// timing
float deltaTime = 0.0f; // time between current frame and last frame
float lastFrame = 0.0f;

int main(int argc, char **argv) {
  // glfw init check
  if (!glfwInit()) {
    std::cerr << "GLFW init failed\n";
    return -1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT,
                                        "Photon Splatter", nullptr, nullptr);

  // window creation check
  if (!window) {
    std::cerr << "Window creation failed\n";
    glfwTerminate();
    return -1;
  }

  glfwMakeContextCurrent(window);
  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
  glfwSetCursorPosCallback(window, mouse_callback);
  glfwSetScrollCallback(window, scroll_callback);

  // glad init check
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cerr << "Failed to initialize GLAD\n";
    glfwDestroyWindow(window);
    glfwTerminate();
    return -1;
  }
  
  bool vsyncEnabled = true;
  glfwSwapInterval(vsyncEnabled ? 1 : 0);
  
  // retrieve framebuffer size
  glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);

  // set view port with the whole frame buffer
  glViewport(0, 0, framebufferWidth, framebufferHeight);

  // opengl will discard fragments that failed depth test
  glEnable(GL_DEPTH_TEST);
  // let the vertex shader control point size via gl_PointSize
  glEnable(GL_PROGRAM_POINT_SIZE);

  // --- Photon scene setup ---------------------------------------------------
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <scene.obj>\n";
    glfwDestroyWindow(window);
    glfwTerminate();
    return -1;
  }

  const std::string scenePath(argv[1]);
  RayModel rayModel(scenePath);
  std::cout << "Scene: " << scenePath << " (" << rayModel.instance_count() << " instances)\n";

  tinybvh::BVH bvh;
  bvh.Build(rayModel.triangles().data(), rayModel.triangle_count());

  Scene scene(rayModel);
  PointLight light = SceneConfig::load(scenePath).apply(scene);

  PhotonTracer tracer(scene, bvh, light);
  Rng rng(0xDECAFu);
  tracer.trace(/*photon_count=*/30000u, /*max_depth=*/64u, rng);
  scene.upload_geometry();
  scene.upload_points(tracer.points());
  scene.upload_beams(tracer.beams());
  std::cout << "Traced " << tracer.points().size() << " points, " << tracer.beams().size()
            << " beams" << std::endl;

  Shader pointShader("shaders/point.vs", "shaders/point.fs");
  Shader beamShader ("shaders/beam.vs",  "shaders/beam.fs");
  Shader geomShader ("shaders/geom.vs",  "shaders/geom.fs");

  auto debugUi = std::make_unique<DebugUi>(window);


  lastFrame = glfwGetTime();

  while (!glfwWindowShouldClose(window)) {
    float currentFrame = static_cast<float>(glfwGetTime());
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    glfwPollEvents();
    debugUi->beginFrame();
    uiWantsMouse = debugUi->wantsMouse();
    const bool uiWantsKeyboard = debugUi->wantsKeyboard();

    processInput(window, uiWantsMouse, uiWantsKeyboard);

    // projection may change every frame (window resize / zoom)
    const float aspectRatio = framebufferHeight > 0
                                  ? static_cast<float>(framebufferWidth) /
                                        static_cast<float>(framebufferHeight)
                                  : 1.0f;
    const glm::mat4 projection =
        glm::perspective(glm::radians(camera.Zoom), aspectRatio, 0.1f, 100.0f);
    const glm::mat4 view = camera.GetViewMatrix();

    const ViewState& vs = debugUi->viewState();

    // Depth AOV uses a white background so far-away geometry reads as white.
    const bool depthMode = vs.showGeometry && vs.geomAov == ViewState::GeomAov::Depth;
    glClearColor(depthMode ? 1.0f : 0.05f,
                 depthMode ? 1.0f : 0.05f,
                 depthMode ? 1.0f : 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // --- Geometry pass
    if (vs.showGeometry) {
      const int geomAov = static_cast<int>(vs.geomAov);
      setCameraUniforms(geomShader, projection, view);
      geomShader.setInt("aov_mode", geomAov);
      geomShader.setVec3("cameraPos", camera.Position.x, camera.Position.y, camera.Position.z);
      geomShader.setVec3("lightPos", light.position.x, light.position.y, light.position.z);
      geomShader.setFloat("nearPlane", 0.1f);
      geomShader.setFloat("farPlane", 10.0f);
      scene.draw_geometry(geomShader, geomAov, vs.instanceVisible);
    }

    // --- Photon point pass
    if (vs.showPoints) {
      setCameraUniforms(pointShader, projection, view);
      pointShader.setFloat("pointSize", 3.0f);
      const int bounceFilter = vs.allBounces ? -1 : vs.bounceFilter;
      scene.draw_points(pointShader, static_cast<int>(vs.pointAov), vs.instancePointsVisible, bounceFilter);
    }

    // --- Photon beam pass
    if (vs.showBeams) {
      setCameraUniforms(beamShader, projection, view);
      scene.draw_beams(beamShader);
    }

    const uint32_t instance_count = rayModel.instance_count();
    const uint32_t medium_count   = 8u;
    if (debugUi->draw(camera, vsyncEnabled, instance_count, medium_count, scene.max_bounce_depth())) {
      glfwSwapInterval(vsyncEnabled ? 1 : 0);
    }
    debugUi->endFrame();

    glfwSwapBuffers(window);
  }

  debugUi.reset();

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this
// frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window, bool uiWantsMouse,
                  bool uiWantsKeyboard) {
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

  // hide cursor in mouse look mode to prevent mouse position failing to update when hitting window border
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

// bind a shader and upload the shared camera matrices (model = identity)
// ----------------------------------------------------------------------
void setCameraUniforms(Shader &shader, const glm::mat4 &projection, const glm::mat4 &view) {
  shader.use();
  shader.setMat4("projection", projection);
  shader.setMat4("view", view);
  shader.setMat4("model", glm::mat4(1.0f));
}