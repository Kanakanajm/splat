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
#include "photon_mapper.hpp"
#include "path_tracer.hpp"
#include "surface_splatter.hpp"
#include "volume_splatter.hpp"
#include "combined_splatter.hpp"
#include "ray_camera.hpp"
#include "random.hpp"
#include "tiny_bvh.h"

#include "tinyexr.h"

#include "capture_utils.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

std::string make_output_dir(const std::string& scene_path) {
    const std::string stem = std::filesystem::path(scene_path).stem().string();
    const std::time_t t    = std::chrono::system_clock::to_time_t(
                                 std::chrono::system_clock::now());
    std::tm tm_buf{};
    localtime_r(&t, &tm_buf);
    char ts[16];
    std::strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", &tm_buf);
    return "output/" + stem + "_" + ts;
}

}  // namespace


void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void mouse_callback(GLFWwindow *window, double xpos, double ypos);
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);
void processInput(GLFWwindow *window, bool uiWantsMouse, bool uiWantsKeyboard);
void setMouseLook(GLFWwindow *window, bool enabled);
void setCameraUniforms(Shader &shader, const glm::mat4 &projection, const glm::mat4 &view);
void recreateHdrFbo(int width, int height);
void recreateAccumFbo(int width, int height);
void recreateSplatFbo(int width, int height);

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
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// HDR FBO for linear-space accumulation
unsigned int hdrFbo = 0, hdrColorTex = 0, hdrDepthRbo = 0;

// Accumulation FBO for multi-pass capture (GL_RGBA32F)
unsigned int accumFbo = 0, accumColorTex = 0, accumDepthRbo = 0;

// Splat HDR FBO for interactive splat mode (GL_RGB16F)
unsigned int splatFbo = 0, splatTex = 0;

// Pointer to scene for depth-peel resize in the framebuffer callback
Scene* g_scene = nullptr;

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

  glViewport(0, 0, framebufferWidth, framebufferHeight);

  // HDR FBO: accumulate radiance in linear float16 space.
  recreateHdrFbo(framebufferWidth, framebufferHeight);
  recreateAccumFbo(framebufferWidth, framebufferHeight);

  // Empty VAO for the fullscreen triangle draw (no attributes, positions
  // are generated from gl_VertexID in hdr.vs).
  unsigned int screenVao = 0;
  glGenVertexArrays(1, &screenVao);

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
  auto lights = SceneConfig::load(scenePath).apply(scene);

  PhotonTracer tracer(scene, bvh, lights);
  Rng rng(0xDECAFu);
  tracer.trace(photonCount, /*max_depth=*/20u, rng);
  scene.upload_geometry();
  scene.upload_points(tracer.points());
  scene.upload_splats(tracer.points());
  scene.upload_beams(tracer.beams());
  std::cout << "Traced " << tracer.points().size() << " points, " << tracer.beams().size()
            << " beams" << std::endl;

  // --- Power sanity check 1: stored beam/point power sum vs Phi_total (CPU) ---
  {
      tinybvh::bvhvec3 phi{};
      for (const auto& l : lights) {
          const auto p = std::visit([](const auto& ll) { return ll.total_power(); }, l);
          phi.x += p.x; phi.y += p.y; phi.z += p.z;
      }
      tinybvh::bvhvec3 beam_sum{}, pt_sum{};
      for (const auto& b : tracer.beams())  { beam_sum.x += b.power.x; beam_sum.y += b.power.y; beam_sum.z += b.power.z; }
      for (const auto& p : tracer.points()) { pt_sum.x  += p.power.x;  pt_sum.y  += p.power.y;  pt_sum.z  += p.power.z;  }
      const float combined = beam_sum.x + pt_sum.x;
      std::cout << "[Power check 1 - CPU, R channel]\n"
                << "  Phi_total  = " << phi.x      << "\n"
                << "  beam_sum   = " << beam_sum.x << "  ratio = " << beam_sum.x / phi.x << "\n"
                << "  point_sum  = " << pt_sum.x   << "  ratio = " << pt_sum.x   / phi.x << "\n"
                << "  combined   = " << combined   << "  ratio = " << combined    / phi.x << "\n"
                << std::flush;
  }

  Shader pointShader        ("shaders/point.vs",          "shaders/point.fs");
  Shader splatShader        ("shaders/splat.vs", "shaders/splat.gs", "shaders/splat.fs");
  Shader beamShader         ("shaders/beam.vs", "shaders/beam.gs", "shaders/beam.fs");
  Shader volSplatShader     ("shaders/beam.vs", "shaders/beam.gs", "shaders/vol_splat.fs");
  Shader geomShader         ("shaders/geom.vs",           "shaders/geom.fs");
  Shader depthPeelInitShader("shaders/geom.vs", "shaders/depth_peel_init.fs");
  Shader depthPeelShader    ("shaders/geom.vs", "shaders/depth_peel.fs");
  Shader quadShader         ("shaders/quad.vs",           "shaders/quad.fs");
  Shader presentShader      ("shaders/quad.vs",           "shaders/present.fs");

  unsigned int emptyVAO = 0;
  glGenVertexArrays(1, &emptyVAO);

  scene.init_depth_peel(framebufferWidth, framebufferHeight);
  g_scene = &scene;

  recreateSplatFbo(framebufferWidth, framebufferHeight);

  unsigned int peelQuery;
  glGenQueries(1, &peelQuery);

  auto debugUi = std::make_unique<DebugUi>(window);
  {
    const std::string stem = std::filesystem::path(scenePath).stem().string();
    std::filesystem::create_directories("output");
    std::snprintf(debugUi->captureState().output_path,
                  sizeof(debugUi->captureState().output_path),
                  "output/%s.exr", stem.c_str());
  }

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

    const bool isSplatMode = vs.showBeams && (vs.beamAov == ViewState::BeamAov::Splat);
    const bool depthMode   = vs.showGeometry && vs.geomAov == ViewState::GeomAov::Depth;

    // --- Multi-pass capture (triggered by "Render" button in debug UI) --------
    CaptureState& cs = debugUi->captureState();
    if (cs.triggered && !cs.is_running) {
      cs.triggered  = false;
      cs.is_running = true;

      if (cs.use_path_tracer) {
        // --- Path tracer capture ---
        const int W = framebufferWidth, H = framebufferHeight;
        PinholeCamera pt_cam{
            .eye    = {camera.Position.x, camera.Position.y, camera.Position.z},
            .target = {camera.Position.x + camera.Front.x,
                       camera.Position.y + camera.Front.y,
                       camera.Position.z + camera.Front.z},
            .up     = {camera.Up.x, camera.Up.y, camera.Up.z},
            .fov_y  = glm::radians(camera.Zoom),
            .width  = static_cast<uint32_t>(W),
            .height = static_cast<uint32_t>(H),
        };

        if (cs.compare_reference > 0) {
          // --- Checkpointed render with reference comparison ---
          const std::string out_dir = make_output_dir(scenePath);
          std::filesystem::create_directories(out_dir);

          pt_cam.save_json(out_dir + "/camera.json");
          std::cout << "Output dir: " << out_dir << "\n";

          const int  n_cp        = std::max(1, std::min(cs.pt_num_checkpoints, cs.pt_spp));
          const auto checkpoints = generate_checkpoints(cs.pt_spp, n_cp);
          PathTracer pt{scene, bvh, lights, cs.pt_max_depth, cs.pt_spp};
          std::cout << "Path tracer (checkpointed): max_depth=" << cs.pt_max_depth << "\n";

          pt.render_checkpointed(W, H, pt_cam, checkpoints,
            [&](int spp, const std::vector<float>& buf) {
              char fname[512];
              std::snprintf(fname, sizeof(fname), "%s/pt_spp%04d.exr", out_dir.c_str(), spp);
              const char* exrErr = nullptr;
              if (SaveEXR(buf.data(), W, H, 3, 0, fname, &exrErr) != TINYEXR_SUCCESS) {
                std::cerr << "EXR save failed: " << (exrErr ? exrErr : "unknown") << "\n";
                FreeEXRErrorMessage(exrErr);
              } else {
                std::cout << "Saved: " << fname << "\n";
              }
            });

          // Build comma-separated SPP list for Python scripts
          std::string spp_list;
          for (std::size_t i = 0; i < checkpoints.size(); ++i) {
            if (i > 0) spp_list += ',';
            spp_list += std::to_string(checkpoints[i]);
          }

          // Use the venv python by symlink path (do NOT canonicalize — it resolves to system python)
          const std::string venv_py = (
              std::filesystem::path(argv[0]).parent_path() / "../.venv/bin/python").string();

          auto run = [](const std::string& cmd) {
            std::cout << "$ " << cmd << "\n";
            if (std::system(cmd.c_str()) != 0)
              std::cerr << "[warning] command returned non-zero\n";
          };

          if (cs.compare_reference == 1) {
            run(venv_py + " tools/export_mitsuba.py " + scenePath +
                " " + out_dir + "/camera.json" +
                " --output-dir " + out_dir +
                " --spp " + std::to_string(cs.pt_spp) +
                " --max-depth " + std::to_string(cs.pt_max_depth));

            run(venv_py + " tools/run_mitsuba.py " + out_dir + "/scene_mitsuba.xml" +
                " --spp-list " + spp_list +
                " --output-dir " + out_dir);

            run(venv_py + " tools/convergence_compare.py " + out_dir);
          }

        } else {
          // --- Single render (original path) ---
          PathTracer pt{scene, bvh, lights, cs.pt_max_depth, cs.pt_spp};
          std::vector<float> pt_rgb;
          std::cout << "Path tracer: " << cs.pt_spp << " spp, max_depth=" << cs.pt_max_depth << "\n";
          pt.render(pt_rgb, W, H, pt_cam);

          const char* exrErr = nullptr;
          if (SaveEXR(pt_rgb.data(), W, H, 3, 0, cs.output_path, &exrErr) != TINYEXR_SUCCESS) {
            std::cerr << "EXR save failed: " << (exrErr ? exrErr : "unknown") << "\n";
            FreeEXRErrorMessage(exrErr);
          } else {
            std::cout << "Saved: " << cs.output_path << "\n";
          }
        }

        cs.is_running = false;
      } else if (cs.use_photon_mapper) {
        // --- Photon Mapper capture ---
        const int W = framebufferWidth, H = framebufferHeight;
        PinholeCamera pm_cam{
            .eye    = {camera.Position.x, camera.Position.y, camera.Position.z},
            .target = {camera.Position.x + camera.Front.x,
                       camera.Position.y + camera.Front.y,
                       camera.Position.z + camera.Front.z},
            .up     = {camera.Up.x, camera.Up.y, camera.Up.z},
            .fov_y  = glm::radians(camera.Zoom),
            .width  = static_cast<uint32_t>(W),
            .height = static_cast<uint32_t>(H),
        };

        const std::string venv_py = (
            std::filesystem::path(argv[0]).parent_path() / "../.venv/bin/python").string();
        auto run = [](const std::string& cmd) {
          std::cout << "$ " << cmd << "\n";
          if (std::system(cmd.c_str()) != 0)
            std::cerr << "[warning] command returned non-zero\n";
        };

        auto save_exr = [&](const std::vector<float>& buf, const char* path) {
          const char* exrErr = nullptr;
          if (SaveEXR(buf.data(), W, H, 3, 0, path, &exrErr) != TINYEXR_SUCCESS) {
            std::cerr << "EXR save failed: " << (exrErr ? exrErr : "unknown") << "\n";
            FreeEXRErrorMessage(exrErr);
          } else {
            std::cout << "Saved: " << path << "\n";
          }
        };

        if (cs.compare_reference > 0) {
          // --- Checkpointed PM render (SPP is the convergence axis, n_photons fixed) ---
          const std::string out_dir = make_output_dir(scenePath);
          std::filesystem::create_directories(out_dir);
          pm_cam.save_json(out_dir + "/camera.json");
          std::cout << "Output dir: " << out_dir << "\n";

          const int  n_cp        = std::max(1, std::min(cs.pm_num_checkpoints, cs.pm_spp));
          const auto checkpoints = generate_checkpoints(cs.pm_spp, n_cp);

          PhotonMapper pm{scene, bvh, lights,
                          cs.pm_n_photons, cs.pm_r_surf, cs.pm_r_vol,
                          cs.pm_max_cam_depth, cs.pm_max_emit_depth};
          std::cout << "[PM] n_photons=" << cs.pm_n_photons
                    << "  max_cam=" << cs.pm_max_cam_depth
                    << "  max_emit=" << cs.pm_max_emit_depth << "\n";

          pm.render_checkpointed(W, H, pm_cam, checkpoints,
            [&](int spp, const std::vector<float>& buf) {
              char fname[512];
              std::snprintf(fname, sizeof(fname), "%s/pm_spp%04d.exr", out_dir.c_str(), spp);
              const char* exrErr = nullptr;
              if (SaveEXR(buf.data(), W, H, 3, 0, fname, &exrErr) != TINYEXR_SUCCESS) {
                std::cerr << "EXR save failed: " << (exrErr ? exrErr : "unknown") << "\n";
                FreeEXRErrorMessage(exrErr);
              } else {
                std::cout << "Saved: " << fname << "\n";
              }
            });

          // Build comma-separated SPP list for Python scripts
          std::string spp_list;
          for (std::size_t i = 0; i < checkpoints.size(); ++i) {
            if (i > 0) spp_list += ',';
            spp_list += std::to_string(checkpoints[i]);
          }

          // Generate reference checkpoints at the same SPP values.
          // compare_depth = pm_max_emit_depth + pm_max_cam_depth (user-editable in UI)
          const int cmp_depth = std::max(1, cs.pm_compare_depth);
          if (cs.compare_reference == 1) {
            run(venv_py + " tools/export_mitsuba.py " + scenePath +
                " " + out_dir + "/camera.json" +
                " --output-dir " + out_dir +
                " --spp " + std::to_string(cs.pm_spp) +
                " --max-depth " + std::to_string(cmp_depth));
            run(venv_py + " tools/run_mitsuba.py " + out_dir + "/scene_mitsuba.xml" +
                " --spp-list " + spp_list +
                " --output-dir " + out_dir);
          } else {
            const int cmp_depth_pt = std::max(1, cs.pm_compare_depth);
            PathTracer pt_ref{scene, bvh, lights, cmp_depth_pt, cs.pm_spp};
            std::cout << "[PT ref] " << cs.pm_spp << " spp, max_depth=" << cmp_depth_pt << "\n";
            pt_ref.render_checkpointed(W, H, pm_cam, checkpoints,
              [&](int spp, const std::vector<float>& buf) {
                char fname[512];
                std::snprintf(fname, sizeof(fname), "%s/pt_spp%04d.exr", out_dir.c_str(), spp);
                const char* exrErr = nullptr;
                if (SaveEXR(buf.data(), W, H, 3, 0, fname, &exrErr) != TINYEXR_SUCCESS) {
                  std::cerr << "EXR save failed: " << (exrErr ? exrErr : "unknown") << "\n";
                  FreeEXRErrorMessage(exrErr);
                } else {
                  std::cout << "Saved: " << fname << "\n";
                }
              });
          }

          run(venv_py + " tools/convergence_compare.py " + out_dir);

        } else {
          // --- Single PM render ---
          PhotonMapper pm{scene, bvh, lights,
                          cs.pm_n_photons, cs.pm_r_surf, cs.pm_r_vol, cs.pm_max_cam_depth};
          std::vector<float> pm_rgb;
          std::cout << "[PM] n_photons=" << cs.pm_n_photons << "\n";
          pm.render(pm_rgb, W, H, pm_cam, 0u, cs.pm_spp);
          save_exr(pm_rgb, cs.output_path);
        }

        cs.is_running = false;
      } else if (cs.use_surface_splatter) {
        // --- Surface Splat capture ---
        const int W = framebufferWidth, H = framebufferHeight;
        PinholeCamera ss_cam{
            .eye    = {camera.Position.x, camera.Position.y, camera.Position.z},
            .target = {camera.Position.x + camera.Front.x,
                       camera.Position.y + camera.Front.y,
                       camera.Position.z + camera.Front.z},
            .up     = {camera.Up.x, camera.Up.y, camera.Up.z},
            .fov_y  = glm::radians(camera.Zoom),
            .width  = static_cast<uint32_t>(W),
            .height = static_cast<uint32_t>(H),
        };

        SurfaceSplatter ss{scene, bvh, lights,
                           cs.total_photons, cs.photons_per_pass,
                           cs.ss_max_emit_depth};
        std::cout << "[SS] n_photons=" << cs.total_photons
                  << " per_pass=" << cs.photons_per_pass
                  << " h=" << cs.ss_h << "\n";

        if (cs.ss_compare_pm) {
          // --- Checkpointed SS + PM reference comparison ---
          const std::string out_dir = make_output_dir(scenePath);
          std::filesystem::create_directories(out_dir);
          ss_cam.save_json(out_dir + "/camera.json");
          std::cout << "Output dir: " << out_dir << "\n";

          const std::string venv_py = (
              std::filesystem::path(argv[0]).parent_path() / "../.venv/bin/python").string();
          auto run = [](const std::string& cmd) {
            std::cout << "$ " << cmd << "\n";
            if (std::system(cmd.c_str()) != 0)
              std::cerr << "[warning] command returned non-zero\n";
          };
          auto save_exr = [&](const std::vector<float>& buf, const char* path) {
            const char* exrErr = nullptr;
            if (SaveEXR(buf.data(), W, H, 3, 0, path, &exrErr) != TINYEXR_SUCCESS) {
              std::cerr << "EXR save failed: " << (exrErr ? exrErr : "unknown") << "\n";
              FreeEXRErrorMessage(exrErr);
            } else {
              std::cout << "Saved: " << path << "\n";
            }
          };

          // SS checkpoints: evenly spaced pass indices up to K.
          const int K = (cs.total_photons + cs.photons_per_pass - 1) / cs.photons_per_pass;
          const int n_ss_cp = std::max(1, std::min(cs.ss_num_checkpoints, K));
          const auto ss_checkpoints = generate_checkpoints(K, n_ss_cp);

          ss.render_checkpointed(W, H, ss_cam, geomShader, splatShader, accumFbo,
            ss_checkpoints,
            [&](int pass, const std::vector<float>& buf) {
              char fname[512];
              std::snprintf(fname, sizeof(fname), "%s/ss_pass%04d.exr", out_dir.c_str(), pass);
              save_exr(buf, fname);
            }, cs.ss_h, cs.ss_exposure);

          // PM reference: same camera, r_surf = ss_h, spp = ss_pm_spp.
          const auto pm_checkpoints = cs.ss_pm_save_checkpoints
              ? generate_checkpoints(cs.ss_pm_spp,
                                     std::max(1, std::min(cs.ss_num_checkpoints, cs.ss_pm_spp)))
              : std::vector<int>{cs.ss_pm_spp};

          PhotonMapper pm_ref{scene, bvh, lights,
                              cs.pm_n_photons, cs.ss_h, cs.pm_r_vol,
                              /*max_cam_depth=*/1, cs.ss_max_emit_depth};
          std::cout << "[PM ref] spp=" << cs.ss_pm_spp
                    << " n_photons=" << cs.pm_n_photons
                    << " r_surf=" << cs.ss_h
                    << " max_cam=1 max_emit=" << cs.ss_max_emit_depth << "\n";

          pm_ref.render_checkpointed(W, H, ss_cam, pm_checkpoints,
            [&](int spp, const std::vector<float>& buf) {
              char fname[512];
              std::snprintf(fname, sizeof(fname), "%s/pm_spp%04d.exr", out_dir.c_str(), spp);
              save_exr(buf, fname);
            });

          run(venv_py + " tools/convergence_compare.py " + out_dir);

        } else {
          // --- Single SS render ---
          std::vector<float> ss_rgb;
          ss.render(ss_rgb, W, H, ss_cam, geomShader, splatShader, accumFbo, cs.ss_h, cs.ss_exposure);

          const char* exrErr = nullptr;
          if (SaveEXR(ss_rgb.data(), W, H, 3, 0, cs.output_path, &exrErr) != TINYEXR_SUCCESS) {
            std::cerr << "EXR save failed: " << (exrErr ? exrErr : "unknown") << "\n";
            FreeEXRErrorMessage(exrErr);
          } else {
            std::cout << "Saved: " << cs.output_path << "\n";
          }
        }

        // Restore interactive beams.
        Rng restore_rng(0xDECAFu);
        tracer.trace(photonCount, /*max_depth=*/20u, restore_rng);
        scene.upload_beams(tracer.beams());

        cs.is_running = false;
      } else if (cs.use_volume_splatter) {
        // --- Volume Splat capture ---
        const int W = framebufferWidth, H = framebufferHeight;
        PinholeCamera vs_cam{
            .eye    = {camera.Position.x, camera.Position.y, camera.Position.z},
            .target = {camera.Position.x + camera.Front.x,
                       camera.Position.y + camera.Front.y,
                       camera.Position.z + camera.Front.z},
            .up     = {camera.Up.x, camera.Up.y, camera.Up.z},
            .fov_y  = glm::radians(camera.Zoom),
            .width  = static_cast<uint32_t>(W),
            .height = static_cast<uint32_t>(H),
        };

        VolumeSplatter vs{scene, bvh, lights,
                          cs.total_photons, cs.photons_per_pass,
                          cs.vs_max_emit_depth};
        std::cout << "[VS] n_photons=" << cs.total_photons
                  << " per_pass=" << cs.photons_per_pass
                  << " beam_radius=" << cs.vs_beam_radius << "\n";

        if (cs.vs_compare_pm) {
          const std::string out_dir = make_output_dir(scenePath);
          std::filesystem::create_directories(out_dir);
          vs_cam.save_json(out_dir + "/camera.json");
          std::cout << "Output dir: " << out_dir << "\n";

          const std::string venv_py = (
              std::filesystem::path(argv[0]).parent_path() / "../.venv/bin/python").string();
          auto run = [](const std::string& cmd) {
            std::cout << "$ " << cmd << "\n";
            if (std::system(cmd.c_str()) != 0)
              std::cerr << "[warning] command returned non-zero\n";
          };
          auto save_exr = [&](const std::vector<float>& buf, const char* path) {
            const char* exrErr = nullptr;
            if (SaveEXR(buf.data(), W, H, 3, 0, path, &exrErr) != TINYEXR_SUCCESS) {
              std::cerr << "EXR save failed: " << (exrErr ? exrErr : "unknown") << "\n";
              FreeEXRErrorMessage(exrErr);
            } else {
              std::cout << "Saved: " << path << "\n";
            }
          };

          const int K = (cs.total_photons + cs.photons_per_pass - 1) / cs.photons_per_pass;
          const int n_vs_cp = std::max(1, std::min(cs.vs_num_checkpoints, K));
          const auto vs_checkpoints = generate_checkpoints(K, n_vs_cp);

          vs.render_checkpointed(W, H, vs_cam,
            depthPeelInitShader, depthPeelShader, quadShader, volSplatShader, accumFbo,
            vs_checkpoints,
            [&](int pass, const std::vector<float>& buf) {
              char fname[512];
              std::snprintf(fname, sizeof(fname), "%s/vs_pass%04d.exr", out_dir.c_str(), pass);
              save_exr(buf, fname);
            }, cs.vs_beam_radius, cs.vs_exposure);

          const auto pm_checkpoints = cs.vs_pm_save_checkpoints
              ? generate_checkpoints(cs.vs_pm_spp,
                                     std::max(1, std::min(cs.vs_num_checkpoints, cs.vs_pm_spp)))
              : std::vector<int>{cs.vs_pm_spp};

          PhotonMapper pm_ref{scene, bvh, lights,
                              cs.pm_n_photons, cs.pm_r_surf, cs.pm_r_vol,
                              /*max_cam_depth=*/3, cs.vs_max_emit_depth};
          std::cout << "[PM ref] spp=" << cs.vs_pm_spp
                    << " n_photons=" << cs.pm_n_photons
                    << " r_vol=" << cs.pm_r_vol
                    << " max_cam=3 max_emit=" << cs.vs_max_emit_depth << "\n";

          pm_ref.render_checkpointed(W, H, vs_cam, pm_checkpoints,
            [&](int spp, const std::vector<float>& buf) {
              char fname[512];
              std::snprintf(fname, sizeof(fname), "%s/pm_spp%04d.exr", out_dir.c_str(), spp);
              save_exr(buf, fname);
            });

          run(venv_py + " tools/convergence_compare.py " + out_dir);
        } else {
          std::vector<float> vs_rgb;
          vs.render(vs_rgb, W, H, vs_cam,
            depthPeelInitShader, depthPeelShader, quadShader, volSplatShader, accumFbo,
            cs.vs_beam_radius, cs.vs_exposure);

          const char* exrErr = nullptr;
          if (SaveEXR(vs_rgb.data(), W, H, 3, 0, cs.output_path, &exrErr) != TINYEXR_SUCCESS) {
            std::cerr << "EXR save failed: " << (exrErr ? exrErr : "unknown") << "\n";
            FreeEXRErrorMessage(exrErr);
          } else {
            std::cout << "Saved: " << cs.output_path << "\n";
          }
        }

        // Restore interactive beams.
        Rng restore_rng(0xDECAFu);
        tracer.trace(photonCount, /*max_depth=*/20u, restore_rng);
        scene.upload_beams(tracer.beams());

        cs.is_running = false;
      } else if (cs.use_combined_splatter) {
        // --- Combined (PVS) Splat capture ---
        const int W = framebufferWidth, H = framebufferHeight;
        PinholeCamera pvs_cam{
            .eye    = {camera.Position.x, camera.Position.y, camera.Position.z},
            .target = {camera.Position.x + camera.Front.x,
                       camera.Position.y + camera.Front.y,
                       camera.Position.z + camera.Front.z},
            .up     = {camera.Up.x, camera.Up.y, camera.Up.z},
            .fov_y  = glm::radians(camera.Zoom),
            .width  = static_cast<uint32_t>(W),
            .height = static_cast<uint32_t>(H),
        };

        CombinedSplatter pvs{scene, bvh, lights,
                             cs.total_photons, cs.photons_per_pass,
                             cs.pvs_max_emit_depth};
        std::cout << "[PVS] n_photons=" << cs.total_photons
                  << " per_pass=" << cs.photons_per_pass
                  << " h=" << cs.pvs_h
                  << " beam_radius=" << cs.pvs_beam_radius << "\n";

        const std::string out_dir = make_output_dir(scenePath);
        std::filesystem::create_directories(out_dir);
        pvs_cam.save_json(out_dir + "/camera.json");
        std::cout << "Output dir: " << out_dir << "\n";

        auto save_exr = [&](const std::vector<float>& buf, const char* path) {
          const char* exrErr = nullptr;
          if (SaveEXR(buf.data(), W, H, 3, 0, path, &exrErr) != TINYEXR_SUCCESS) {
            std::cerr << "EXR save failed: " << (exrErr ? exrErr : "unknown") << "\n";
            FreeEXRErrorMessage(exrErr);
          } else {
            std::cout << "Saved: " << path << "\n";
          }
        };

        const int K = (cs.total_photons + cs.photons_per_pass - 1) / cs.photons_per_pass;
        const int n_pvs_cp = std::max(1, std::min(cs.pvs_num_checkpoints, K));
        const auto pvs_checkpoints = generate_checkpoints(K, n_pvs_cp);

        pvs.render_checkpointed(W, H, pvs_cam,
          geomShader, depthPeelInitShader, depthPeelShader,
          quadShader, splatShader, volSplatShader, accumFbo,
          pvs_checkpoints,
          [&](int pass, const std::vector<float>& buf) {
            char fname[512];
            std::snprintf(fname, sizeof(fname), "%s/pvs_pass%04d.exr", out_dir.c_str(), pass);
            save_exr(buf, fname);
          },
          cs.pvs_h, cs.pvs_beam_radius, cs.pvs_exposure);

        // Restore interactive beams.
        Rng restore_rng(0xDECAFu);
        tracer.trace(photonCount, /*max_depth=*/20u, restore_rng);
        scene.upload_beams(tracer.beams());

        cs.is_running = false;
      } else {

      const uint32_t N_total    = static_cast<uint32_t>(cs.total_photons);
      const uint32_t N_per_pass = static_cast<uint32_t>(cs.photons_per_pass);
      const uint32_t K          = (N_total + N_per_pass - 1) / N_per_pass;

      constexpr int kMaxMedia = 16;
      float mediaSigmaT[kMaxMedia] = {};
      float mediaSigmaS[kMaxMedia] = {};
      for (uint32_t mi = 0; mi < std::min(scene.medium_count(), static_cast<uint32_t>(kMaxMedia)); ++mi) {
          mediaSigmaT[mi] = scene.medium(mi).sigma_t();
          mediaSigmaS[mi] = scene.medium(mi).sigma_s;
      }

      glBindFramebuffer(GL_FRAMEBUFFER, accumFbo);
      glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      // Bind peel textures once — used by background, surface, and beam passes.
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D_ARRAY, scene.peel_depth_array());
      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D_ARRAY, scene.peel_medium_array());
      glActiveTexture(GL_TEXTURE0);

      // --- 1. Background pass: bgColor * T_cam(cam -> inf), no depth test/write ---
      glDisable(GL_DEPTH_TEST);
      glDepthMask(GL_FALSE);
      glDisable(GL_BLEND);
      quadShader.use();
      quadShader.setInt("depthMap", 0);
      quadShader.setInt("mediumMap", 1);
      quadShader.setInt("numPeelLayers", peelLayerCount);
      quadShader.setMat4("invViewProj", invViewProj);
      quadShader.setVec3("cameraWorldPos", camera.Position.x, camera.Position.y, camera.Position.z);
      quadShader.setVec2("resolution", static_cast<float>(framebufferWidth),
                                       static_cast<float>(framebufferHeight));
      quadShader.setFloatArray("mediaSigmaT", mediaSigmaT, kMaxMedia);
      {
          glm::vec3 bgColor{0.0f};
          for (const auto& l : lights)
              if (const auto* el = std::get_if<EnvLight>(&l))
                  { bgColor = {el->color.x, el->color.y, el->color.z}; break; }
          quadShader.setVec3("bgColor", bgColor.x, bgColor.y, bgColor.z);
      }
      glBindVertexArray(emptyVAO);
      glDrawArrays(GL_TRIANGLES, 0, 3);
      glBindVertexArray(0);

      // --- 2. Opaque surface pass: L_surface * T_cam(cam -> surface), with depth write ---
      glEnable(GL_DEPTH_TEST);
      glDepthFunc(GL_LESS);
      glDepthMask(GL_TRUE);
      glDisable(GL_BLEND);
      setCameraUniforms(geomShader, projection, view);
      geomShader.setInt("aov_mode", 1); // Diffuse
      geomShader.setVec3("cameraPos", camera.Position.x, camera.Position.y, camera.Position.z);
      {
          glm::vec3 lightPos{0.0f};
          for (const auto& l : lights)
              if (const auto* pl = std::get_if<PointLight>(&l))
                  { lightPos = {pl->position.x, pl->position.y, pl->position.z}; break; }
          geomShader.setVec3("lightPos", lightPos.x, lightPos.y, lightPos.z);
      }
      geomShader.setFloat("nearPlane", 0.1f);
      geomShader.setFloat("farPlane", 10.0f);
      geomShader.setInt("attenuateMedium", 1);
      geomShader.setInt("depthMap", 0);
      geomShader.setInt("mediumMap", 1);
      geomShader.setInt("numPeelLayers", peelLayerCount);
      geomShader.setMat4("invViewProj", invViewProj);
      geomShader.setVec3("cameraWorldPos", camera.Position.x, camera.Position.y, camera.Position.z);
      geomShader.setVec2("resolution", static_cast<float>(framebufferWidth),
                                       static_cast<float>(framebufferHeight));
      geomShader.setFloatArray("mediaSigmaT", mediaSigmaT, kMaxMedia);
      scene.draw_geometry(geomShader, 1, {}, false);

      // --- 3. Beam passes: additive, depth-tested against opaque surface depth ---
      glDepthMask(GL_FALSE);

      for (uint32_t pass = 0; pass < K; ++pass) {
        Rng pass_rng(0xDECAFu + pass);
        tracer.trace(N_per_pass, /*max_depth=*/20u, pass_rng, N_total);
        scene.upload_beams(tracer.beams());
        tracer.release_cpu_memory();

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        setCameraUniforms(beamShader, projection, view);
        beamShader.setVec3("cameraDir", camera.Front.x, camera.Front.y, camera.Front.z);
        beamShader.setFloat("beamRadius", vs.beamRadius);
        beamShader.setFloat("exposure", vs.beamExposure);
        beamShader.setFloatArray("mediaSigmaT", mediaSigmaT, kMaxMedia);
        beamShader.setFloatArray("mediaSigmaS", mediaSigmaS, kMaxMedia);
        beamShader.setInt("depthMap", 0);
        beamShader.setInt("mediumMap", 1);
        beamShader.setInt("numPeelLayers", peelLayerCount);
        beamShader.setMat4("invViewProj", invViewProj);
        beamShader.setVec3("cameraWorldPos", camera.Position.x, camera.Position.y, camera.Position.z);
        beamShader.setVec2("resolution", static_cast<float>(framebufferWidth),
                                         static_cast<float>(framebufferHeight));
        scene.draw_beams(beamShader, static_cast<int>(ViewState::BeamAov::Splat), vs.mediumBeamsVisible);
        glDisable(GL_BLEND);
        glFinish();
        std::cout << "Capture pass " << (pass + 1) << "/" << K << "\n";
      }

      glDepthMask(GL_TRUE);
      glEnable(GL_DEPTH_TEST);

      // Readback accumulated radiance, flip Y (GL origin is bottom-left), strip alpha.
      const int W = framebufferWidth, H = framebufferHeight;
      std::vector<float> raw(W * H * 4);
      glBindFramebuffer(GL_FRAMEBUFFER, accumFbo);
      glReadPixels(0, 0, W, H, GL_RGBA, GL_FLOAT, raw.data());
      glBindFramebuffer(GL_FRAMEBUFFER, 0);

      std::vector<float> rgb(W * H * 3);
      for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
          const int src = ((H - 1 - y) * W + x) * 4;
          const int dst = (y * W + x) * 3;
          rgb[dst + 0] = raw[src + 0];
          rgb[dst + 1] = raw[src + 1];
          rgb[dst + 2] = raw[src + 2];
        }
      }

      const char* exrErr = nullptr;
      if (SaveEXR(rgb.data(), W, H, 3, /*fp16=*/0, cs.output_path, &exrErr) != TINYEXR_SUCCESS) {
        std::cerr << "EXR save failed: " << (exrErr ? exrErr : "unknown") << "\n";
        FreeEXRErrorMessage(exrErr);
      } else {
        std::cout << "Saved: " << cs.output_path << "\n";
      }

      // Restore interactive beams with the original photon count.
      Rng restore_rng(0xDECAFu);
      tracer.trace(photonCount, /*max_depth=*/20u, restore_rng);
      scene.upload_beams(tracer.beams());

      cs.is_running = false;
      }  // end else (beam capture)
    }

    // Default framebuffer clear (splat result is composited later).
    glClearColor(depthMode ? 1.0f : 0.392f,
                 depthMode ? 1.0f : 0.392f,
                 depthMode ? 1.0f : 0.392f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // --- Splat mode: accumulate L_bg*T_cam + beam splats into linear HDR buffer,
    //     then gamma-correct to the default framebuffer.
    if (isSplatMode) {
      constexpr int kMaxMedia = 16;
      float mediaSigmaT[kMaxMedia] = {};
      float mediaSigmaS[kMaxMedia] = {};
      for (uint32_t i = 0; i < std::min(scene.medium_count(), static_cast<uint32_t>(kMaxMedia)); ++i) {
          mediaSigmaT[i] = scene.medium(i).sigma_t();
          mediaSigmaS[i] = scene.medium(i).sigma_s;
      }

      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D_ARRAY, scene.peel_depth_array());
      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D_ARRAY, scene.peel_medium_array());
      glActiveTexture(GL_TEXTURE0);

      // -- Accumulate into linear HDR FBO --
      glBindFramebuffer(GL_FRAMEBUFFER, splatFbo);
      glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
      glClear(GL_COLOR_BUFFER_BIT);
      glDisable(GL_DEPTH_TEST);
      glDisable(GL_BLEND);

      // Background: L_bg * T_cam
      quadShader.use();
      quadShader.setInt("depthMap", 0);
      quadShader.setInt("mediumMap", 1);
      quadShader.setInt("numPeelLayers", peelLayerCount);
      quadShader.setMat4("invViewProj", invViewProj);
      quadShader.setVec3("cameraWorldPos", camera.Position.x, camera.Position.y, camera.Position.z);
      quadShader.setVec2("resolution", static_cast<float>(framebufferWidth),
                                       static_cast<float>(framebufferHeight));
      quadShader.setFloatArray("mediaSigmaT", mediaSigmaT, kMaxMedia);
      glm::vec3 bgColor{0.0f};
      for (const auto& l : lights)
          if (const auto* el = std::get_if<EnvLight>(&l))
              { bgColor = {el->color.x, el->color.y, el->color.z}; break; }
      quadShader.setVec3("bgColor", bgColor.x, bgColor.y, bgColor.z);
      glBindVertexArray(emptyVAO);
      glDrawArrays(GL_TRIANGLES, 0, 3);
      glBindVertexArray(0);

      // Beam splats: additive contributions in linear space
      glEnable(GL_BLEND);
      glBlendFunc(GL_ONE, GL_ONE);
      setCameraUniforms(beamShader, projection, view);
      beamShader.setVec3("cameraDir", camera.Front.x, camera.Front.y, camera.Front.z);
      beamShader.setFloat("beamRadius", vs.beamRadius);
      beamShader.setFloat("exposure", vs.beamExposure);
      beamShader.setFloatArray("mediaSigmaT", mediaSigmaT, kMaxMedia);
      beamShader.setFloatArray("mediaSigmaS", mediaSigmaS, kMaxMedia);
      beamShader.setInt("depthMap", 0);
      beamShader.setInt("mediumMap", 1);
      beamShader.setInt("numPeelLayers", peelLayerCount);
      beamShader.setMat4("invViewProj", invViewProj);
      beamShader.setVec3("cameraWorldPos", camera.Position.x, camera.Position.y, camera.Position.z);
      beamShader.setVec2("resolution", static_cast<float>(framebufferWidth),
                                       static_cast<float>(framebufferHeight));
      const int beamBounceFilter = vs.allBeamBounces ? -1 : vs.beamBounceFilter;
      scene.draw_beams(beamShader, static_cast<int>(ViewState::BeamAov::Splat),
                       vs.mediumBeamsVisible, beamBounceFilter);
      glDisable(GL_BLEND);

      // --- Power sanity check 2: splat buffer pixel sum (GPU, linear, first frame only) ---
      {
          static bool done = false;
          if (!done) {
              done = true;
              std::vector<float> px(framebufferWidth * framebufferHeight * 3);
              glBindTexture(GL_TEXTURE_2D, splatTex);
              glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_FLOAT, px.data());
              glBindTexture(GL_TEXTURE_2D, 0);
              double sum = 0.0;
              for (float v : px) sum += v;
              const double avg = sum / (framebufferWidth * framebufferHeight);
              std::cout << "[Power check 2 - GPU splat buffer, R+G+B sum]\n"
                        << "  pixel RGB sum = " << sum << "\n"
                        << "  avg per pixel = " << avg << "\n"
                        << std::flush;
          }
      }

      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      glEnable(GL_DEPTH_TEST);

      // Present: gamma-correct HDR buffer to default framebuffer
      glDisable(GL_DEPTH_TEST);
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, splatTex);
      presentShader.use();
      presentShader.setInt("splatBuffer", 0);
      presentShader.setVec2("resolution", static_cast<float>(framebufferWidth),
                                          static_cast<float>(framebufferHeight));
      glBindVertexArray(emptyVAO);
      glDrawArrays(GL_TRIANGLES, 0, 3);
      glBindVertexArray(0);
      glEnable(GL_DEPTH_TEST);
    }

    // --- Geometry pass
    if (vs.showGeometry) {
      const int geomAov = static_cast<int>(vs.geomAov);
      setCameraUniforms(geomShader, projection, view);
      geomShader.setInt("aov_mode", geomAov);
      geomShader.setVec3("cameraPos", camera.Position.x, camera.Position.y, camera.Position.z);
      glm::vec3 lightPos{0.0f};
      for (const auto& l : lights)
          if (const auto* pl = std::get_if<PointLight>(&l))
              { lightPos = {pl->position.x, pl->position.y, pl->position.z}; break; }
      geomShader.setVec3("lightPos", lightPos.x, lightPos.y, lightPos.z);
      geomShader.setFloat("nearPlane", 0.1f);
      geomShader.setFloat("farPlane", 10.0f);
      geomShader.setInt("attenuateMedium", 0);

      scene.draw_geometry(geomShader, geomAov, vs.instanceVisible, false);
    }

    // --- Photon point pass
    if (vs.showPoints) {
      const int bounceFilter = vs.allBounces ? -1 : vs.bounceFilter;
      if (vs.showSplatTriangle) {
        // Depth prepass when geometry is hidden so splat triangles are occluded correctly.
        if (!vs.showGeometry) {
          glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
          setCameraUniforms(geomShader, projection, view);
          geomShader.setInt("aov_mode", 1);
          scene.draw_geometry(geomShader, 1, vs.instanceVisible, false);
          glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        }
        setCameraUniforms(splatShader, projection, view);
        scene.draw_splats(splatShader, vs.splatH, vs.exposure, static_cast<int>(vs.splatAov));
      } else {
        setCameraUniforms(pointShader, projection, view);
        pointShader.setFloat("pointSize", 3.0f);
        scene.draw_points(pointShader, static_cast<int>(vs.pointAov), vs.instancePointsVisible, bounceFilter);
      }
    }

    // --- Photon beam pass (diagnostic AOV modes only; splat mode handled above)
    if (vs.showBeams && !isSplatMode) {
      setCameraUniforms(beamShader, projection, view);
      beamShader.setVec3("cameraDir", camera.Front.x, camera.Front.y, camera.Front.z);
      beamShader.setFloat("beamRadius", vs.beamRadius);
      beamShader.setFloat("exposure", vs.beamExposure);

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

    // Apply camera load requested from the debug UI
    CaptureState& cs_ui = debugUi->captureState();
    if (cs_ui.pending_camera_load) {
      cs_ui.pending_camera_load = false;
      try {
        const PinholeCamera pc = PinholeCamera::load_json(cs_ui.camera_json_path);
        camera.Position = {pc.eye.x, pc.eye.y, pc.eye.z};
        const float dx = pc.target.x - pc.eye.x;
        const float dy = pc.target.y - pc.eye.y;
        const float dz = pc.target.z - pc.eye.z;
        const float len = std::sqrt(dx*dx + dy*dy + dz*dz);
        if (len > 1e-6f) {
          camera.Pitch = glm::degrees(std::asin(dy / len));
          camera.Yaw   = glm::degrees(std::atan2(dz, dx));
          camera.Zoom  = glm::degrees(pc.fov_y);
          camera.ProcessMouseMovement(0.0f, 0.0f, false);
        }
        std::cout << "Camera loaded from: " << cs_ui.camera_json_path << "\n";
      } catch (const std::exception& e) {
        std::cerr << "Camera load failed: " << e.what() << "\n";
      }
    }

    debugUi->endFrame();

    glfwSwapBuffers(window);
  }

  debugUi.reset();

  glDeleteFramebuffers(1, &splatFbo);
  glDeleteTextures(1, &splatTex);
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
void recreateHdrFbo(int width, int height) {
  if (hdrFbo) {
    glDeleteFramebuffers(1, &hdrFbo);
    glDeleteTextures(1, &hdrColorTex);
    glDeleteRenderbuffers(1, &hdrDepthRbo);
  }
  glGenFramebuffers(1, &hdrFbo);
  glGenTextures(1, &hdrColorTex);
  glGenRenderbuffers(1, &hdrDepthRbo);

  glBindTexture(GL_TEXTURE_2D, hdrColorTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);

  glBindRenderbuffer(GL_RENDERBUFFER, hdrDepthRbo);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);

  glBindFramebuffer(GL_FRAMEBUFFER, hdrFbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, hdrColorTex, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, hdrDepthRbo);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void recreateAccumFbo(int width, int height) {
  if (accumFbo) {
    glDeleteFramebuffers(1, &accumFbo);
    glDeleteTextures(1, &accumColorTex);
    glDeleteRenderbuffers(1, &accumDepthRbo);
  }
  glGenFramebuffers(1, &accumFbo);
  glGenTextures(1, &accumColorTex);
  glGenRenderbuffers(1, &accumDepthRbo);

  glBindTexture(GL_TEXTURE_2D, accumColorTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);

  glBindRenderbuffer(GL_RENDERBUFFER, accumDepthRbo);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);

  glBindFramebuffer(GL_FRAMEBUFFER, accumFbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, accumColorTex, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, accumDepthRbo);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void recreateSplatFbo(int width, int height) {
  if (splatFbo) {
    glDeleteFramebuffers(1, &splatFbo);
    glDeleteTextures(1, &splatTex);
  }
  glGenTextures(1, &splatTex);
  glBindTexture(GL_TEXTURE_2D, splatTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);
  glGenFramebuffers(1, &splatFbo);
  glBindFramebuffer(GL_FRAMEBUFFER, splatFbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, splatTex, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
  framebufferWidth = width;
  framebufferHeight = height;
  glViewport(0, 0, width, height);
  recreateHdrFbo(width, height);
  recreateAccumFbo(width, height);
  recreateSplatFbo(width, height);
  if (g_scene) g_scene->init_depth_peel(width, height);
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