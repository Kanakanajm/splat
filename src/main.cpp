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
  glfwWindowHint(GLFW_STENCIL_BITS, 8);

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
    std::cerr << "Usage: " << argv[0] << " <scene.obj> [photon_count]\n";
    glfwDestroyWindow(window);
    glfwTerminate();
    return -1;
  }

  const std::string scenePath(argv[1]);
  const uint32_t photonCount = argc >= 3 ? static_cast<uint32_t>(std::stoul(argv[2])) : 30000u;

  RayModel rayModel(scenePath);
  std::cout << "Scene: " << scenePath << " (" << rayModel.instance_count() << " instances)\n";

  tinybvh::BVH bvh;
  bvh.Build(rayModel.triangles().data(), rayModel.triangle_count());

  Scene scene(rayModel);
  PointLight light = SceneConfig::load(scenePath).apply(scene);

  PhotonTracer tracer(scene, bvh, light);
  Rng rng(0xDECAFu);
  tracer.trace(photonCount, /*max_depth=*/64u, rng);
  scene.upload_geometry();
  scene.upload_points(tracer.points());
  scene.upload_beams(tracer.beams());
  scene.upload_splats(tracer.points());
  std::cout << "Traced " << tracer.points().size() << " points, " << tracer.beams().size()
            << " beams" << std::endl;

  Shader pointShader ("shaders/point.vs",  "shaders/point.fs");
  Shader beamShader  ("shaders/beam.vs",   "shaders/beam.fs");
  Shader geomShader  ("shaders/geom.vs",   "shaders/geom.fs");
  Shader splatShader ("shaders/splat.vs",  "shaders/splat.gs", "shaders/splat.fs");
  Shader shadowShader("shaders/shadow.vs", "shaders/shadow.gs", "shaders/shadow.fs");

  // --- Point light shadow map (depth cubemap, rendered once since scene is static) ---
  const int   SHADOW_RES = 1024;
  const float SHADOW_FAR = 25.0f;
  unsigned int shadowFBO = 0, depthCubemap = 0;
  {
    glGenTextures(1, &depthCubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap);
    for (int i = 0; i < 6; ++i)
      glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT,
                   SHADOW_RES, SHADOW_RES, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &shadowFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthCubemap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }
  {
    const glm::vec3 lp(light.position.x, light.position.y, light.position.z);
    const glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, SHADOW_FAR);
    const glm::mat4 shadowMatrices[6] = {
      shadowProj * glm::lookAt(lp, lp + glm::vec3( 1, 0, 0), glm::vec3(0,-1, 0)),
      shadowProj * glm::lookAt(lp, lp + glm::vec3(-1, 0, 0), glm::vec3(0,-1, 0)),
      shadowProj * glm::lookAt(lp, lp + glm::vec3( 0, 1, 0), glm::vec3(0, 0, 1)),
      shadowProj * glm::lookAt(lp, lp + glm::vec3( 0,-1, 0), glm::vec3(0, 0,-1)),
      shadowProj * glm::lookAt(lp, lp + glm::vec3( 0, 0, 1), glm::vec3(0,-1, 0)),
      shadowProj * glm::lookAt(lp, lp + glm::vec3( 0, 0,-1), glm::vec3(0,-1, 0)),
    };

    glViewport(0, 0, SHADOW_RES, SHADOW_RES);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
    glClear(GL_DEPTH_BUFFER_BIT);
    shadowShader.use();
    for (int i = 0; i < 6; ++i)
      shadowShader.setMat4("shadowMatrices[" + std::to_string(i) + "]", shadowMatrices[i]);
    shadowShader.setVec3("lightPos", light.position.x, light.position.y, light.position.z);
    shadowShader.setFloat("farPlane", SHADOW_FAR);
    shadowShader.setMat4("model", glm::mat4(1.0f));
    scene.draw_geometry(shadowShader, /*aov_mode=*/1, {});
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, framebufferWidth, framebufferHeight);
  }

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
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // --- Geometry pass
    if (vs.showGeometry) {
      const int geomAov = static_cast<int>(vs.geomAov);
      setCameraUniforms(geomShader, projection, view);
      geomShader.setInt("aov_mode", geomAov);
      geomShader.setVec3("cameraPos", camera.Position.x, camera.Position.y, camera.Position.z);
      geomShader.setVec3("lightPos", light.position.x, light.position.y, light.position.z);
      geomShader.setFloat("nearPlane", 0.1f);
      geomShader.setFloat("farPlane", 10.0f);
      geomShader.setInt("shadowMap", 1);
      geomShader.setFloat("shadowFarPlane", SHADOW_FAR);
      geomShader.setBool("useShadow", vs.useShadow);
      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap);
      scene.draw_geometry(geomShader, geomAov, vs.instanceVisible);
    }

    // --- Depth prepass (when geometry is hidden but splat needs a filled depth buffer)
    if (vs.showSplat && !vs.showGeometry) {
      glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
      setCameraUniforms(geomShader, projection, view);
      scene.draw_geometry(geomShader, /*aov_mode=*/1, vs.instanceVisible);
      glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }

    // --- Splat pass (after geometry so depth buffer is populated)
    if (vs.showSplat) {
      setCameraUniforms(splatShader, projection, view);
      scene.draw_splats(splatShader, vs.splatH, vs.exposure, static_cast<int>(vs.splatAov));
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
      const int beamBounceFilter = vs.allBeamBounces ? -1 : vs.beamBounceFilter;
      scene.draw_beams(beamShader, static_cast<int>(vs.beamAov), vs.mediumBeamsVisible, beamBounceFilter);
    }

    // Pixel picker: sample on left-click edge, not consumed by ImGui.
    // glReadPixels reads the back buffer after all scene draws.
    {
        static bool sPrevLeftDown = false;
        const bool leftDown = !uiWantsMouse &&
            glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        if (leftDown && !sPrevLeftDown) {
            double mx, my;
            glfwGetCursorPos(window, &mx, &my);
            int winW, winH;
            glfwGetWindowSize(window, &winW, &winH);
            const int fbX = static_cast<int>(mx * framebufferWidth  / winW);
            const int fbY = framebufferHeight - 1 -
                            static_cast<int>(my * framebufferHeight / winH);
            float px[3]{};
            glReadPixels(fbX, fbY, 1, 1, GL_RGB, GL_FLOAT, px);
            debugUi->pick(px[0], px[1], px[2]);
        }
        sPrevLeftDown = leftDown;
    }

    const uint32_t instance_count = rayModel.instance_count();
    const uint32_t medium_count   = scene.medium_count();
    if (debugUi->draw(camera, vsyncEnabled, instance_count, medium_count,
                      scene.max_bounce_depth(), scene.beam_max_bounce())) {
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