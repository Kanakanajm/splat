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

#include "env_light.hpp"
#include "ray_model.hpp"
#include "scene.hpp"
#include "scene_config.hpp"
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
  auto light = SceneConfig::load(scenePath).apply(scene);

  PhotonTracer tracer(scene, bvh, light);
  Rng rng(0xDECAFu);
  tracer.trace(/*photon_count=*/10000u, /*max_depth=*/32u, rng);
  scene.upload_geometry();
  scene.upload_points(tracer.points());
  scene.upload_beams(tracer.beams());
  std::cout << "Traced " << tracer.points().size() << " points, " << tracer.beams().size()
            << " beams" << std::endl;

  Shader pointShader        ("shaders/point.vs",          "shaders/point.fs");
  Shader beamShader         ("shaders/beam.vs", "shaders/beam.gs", "shaders/beam.fs");
  Shader geomShader         ("shaders/geom.vs",           "shaders/geom.fs");
  Shader depthPeelInitShader("shaders/geom.vs", "shaders/depth_peel_init.fs");
  Shader depthPeelShader    ("shaders/geom.vs", "shaders/depth_peel.fs");
  Shader quadShader         ("shaders/quad.vs",           "shaders/quad.fs");

  unsigned int emptyVAO = 0;
  glGenVertexArrays(1, &emptyVAO);

  scene.init_depth_peel(framebufferWidth, framebufferHeight);

  unsigned int peelQuery;
  glGenQueries(1, &peelQuery);

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
    const glm::mat4 invViewProj = glm::inverse(projection * view);

    const ViewState& vs = debugUi->viewState();

    // --- Depth peel pass (builds camera-side transmittance maps)
    int peelLayerCount = 1;
    {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);

        glBindFramebuffer(GL_FRAMEBUFFER, scene.peel_fbo());

        // Layer 0: no depth peeling, just record first hit + medium mask.
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  scene.peel_depth_array(), 0, 0);
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                  scene.peel_medium_array(), 0, 0);
        glClear(GL_DEPTH_BUFFER_BIT);
        const GLuint zeroClear[4] = {0u, 0u, 0u, 0u};
        glClearBufferuiv(GL_COLOR, 0, zeroClear);

        setCameraUniforms(depthPeelInitShader, projection, view);
        scene.draw_geometry_peel(depthPeelInitShader);

        // Bind texture arrays for sampling previous layers.
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, scene.peel_depth_array());
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D_ARRAY, scene.peel_medium_array());
        glActiveTexture(GL_TEXTURE0);

        depthPeelShader.use();
        depthPeelShader.setInt("previousDepths", 0);
        depthPeelShader.setInt("previousMedia",  1);
        depthPeelShader.setFloat("peelEpsilon",  1e-5f);

        // Layers 1+: peel past the previous layer.
        for (int layer = 1; layer < scene.peel_max_layers(); ++layer) {
            glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                      scene.peel_depth_array(), 0, layer);
            glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                      scene.peel_medium_array(), 0, layer);
            glClear(GL_DEPTH_BUFFER_BIT);
            glClearBufferuiv(GL_COLOR, 0, zeroClear);

            setCameraUniforms(depthPeelShader, projection, view);
            depthPeelShader.setInt("previousLayerIdx", layer - 1);

            glBeginQuery(GL_ANY_SAMPLES_PASSED, peelQuery);
            scene.draw_geometry_peel(depthPeelShader);
            glEndQuery(GL_ANY_SAMPLES_PASSED);

            GLuint anySamples = GL_FALSE;
            glGetQueryObjectuiv(peelQuery, GL_QUERY_RESULT, &anySamples);
            if (!anySamples) break;
            peelLayerCount = layer + 1;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // Splat mode shows background × T_cam; all other modes use the standard background.
    const bool isSplatMode = vs.showBeams && (vs.beamAov == ViewState::BeamAov::Splat);
    const bool depthMode   = vs.showGeometry && vs.geomAov == ViewState::GeomAov::Depth;
    if (isSplatMode) {
      glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    } else {
      glClearColor(depthMode ? 1.0f : 0.392f,
                   depthMode ? 1.0f : 0.392f,
                   depthMode ? 1.0f : 0.392f, 1.0f);
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // --- Background quad pass (Splat mode: L_bg * T_cam drawn before geometry)
    if (isSplatMode) {
      constexpr int kMaxMedia = 16;
      float mediaSigmaT[kMaxMedia] = {};
      for (uint32_t i = 0; i < std::min(scene.medium_count(), static_cast<uint32_t>(kMaxMedia)); ++i)
          mediaSigmaT[i] = scene.medium(i).sigma_t();

      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D_ARRAY, scene.peel_depth_array());
      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D_ARRAY, scene.peel_medium_array());
      glActiveTexture(GL_TEXTURE0);

      quadShader.use();
      quadShader.setInt("depthMap", 0);
      quadShader.setInt("mediumMap", 1);
      quadShader.setInt("numPeelLayers", peelLayerCount);
      quadShader.setMat4("invViewProj", invViewProj);
      quadShader.setVec3("cameraWorldPos", camera.Position.x, camera.Position.y, camera.Position.z);
      quadShader.setVec2("resolution", static_cast<float>(framebufferWidth),
                                       static_cast<float>(framebufferHeight));
      quadShader.setFloatArray("mediaSigmaT", mediaSigmaT, kMaxMedia);
      const auto* el = std::get_if<EnvLight>(&light);
      const glm::vec3 bgColor = el ? glm::vec3{el->color.x, el->color.y, el->color.z}
                                   : glm::vec3{0.0f};
      quadShader.setVec3("bgColor", bgColor.x, bgColor.y, bgColor.z);

      glDepthMask(GL_FALSE);
      glDepthFunc(GL_LEQUAL);  // quad sits at far-plane depth 1.0

      glBindVertexArray(emptyVAO);
      glDrawArrays(GL_TRIANGLES, 0, 3);
      glBindVertexArray(0);

      glDepthFunc(GL_LESS);
      glDepthMask(GL_TRUE);
    }

    // --- Geometry pass
    if (vs.showGeometry) {
      const int geomAov = static_cast<int>(vs.geomAov);
      setCameraUniforms(geomShader, projection, view);
      geomShader.setInt("aov_mode", geomAov);
      geomShader.setVec3("cameraPos", camera.Position.x, camera.Position.y, camera.Position.z);
      const auto* pl = std::get_if<PointLight>(&light);
      const glm::vec3 lightPos = pl ? glm::vec3{pl->position.x, pl->position.y, pl->position.z}
                                    : glm::vec3{0.0f};
      geomShader.setVec3("lightPos", lightPos.x, lightPos.y, lightPos.z);
      geomShader.setFloat("nearPlane", 0.1f);
      geomShader.setFloat("farPlane", 10.0f);
      geomShader.setInt("attenuateMedium", 0);

      scene.draw_geometry(geomShader, geomAov, vs.instanceVisible, false);
    }

    // --- Photon point pass
    if (vs.showPoints) {
      setCameraUniforms(pointShader, projection, view);
      pointShader.setFloat("pointSize", 3.0f);
      const int bounceFilter = vs.allBounces ? -1 : vs.bounceFilter;
      scene.draw_points(pointShader, static_cast<int>(vs.pointAov), vs.instancePointsVisible, bounceFilter);
    }

    // --- Photon beam pass (skipped in Splat mode — background quad is the visualization)
    if (vs.showBeams && !isSplatMode) {
      setCameraUniforms(beamShader, projection, view);
      beamShader.setVec3("cameraDir", camera.Front.x, camera.Front.y, camera.Front.z);
      beamShader.setFloat("beamRadius", vs.beamRadius);

      constexpr int kMaxMedia = 16;
      float mediaSigmaT[kMaxMedia] = {};
      float mediaSigmaS[kMaxMedia] = {};
      for (uint32_t i = 0; i < std::min(scene.medium_count(), static_cast<uint32_t>(kMaxMedia)); ++i) {
          mediaSigmaT[i] = scene.medium(i).sigma_t();
          mediaSigmaS[i] = scene.medium(i).sigma_s;
      }
      beamShader.setFloatArray("mediaSigmaT", mediaSigmaT, kMaxMedia);
      beamShader.setFloatArray("mediaSigmaS", mediaSigmaS, kMaxMedia);

      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D_ARRAY, scene.peel_depth_array());
      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D_ARRAY, scene.peel_medium_array());
      glActiveTexture(GL_TEXTURE0);
      beamShader.setInt("depthMap", 0);
      beamShader.setInt("mediumMap", 1);
      beamShader.setInt("numPeelLayers", peelLayerCount);
      beamShader.setMat4("invViewProj", invViewProj);
      beamShader.setVec3("cameraWorldPos", camera.Position.x, camera.Position.y, camera.Position.z);
      beamShader.setVec2("resolution", static_cast<float>(framebufferWidth),
                                       static_cast<float>(framebufferHeight));

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

  glDeleteQueries(1, &peelQuery);
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