#include "debug_ui.hpp"

#include "camera.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

DebugUi::DebugUi(GLFWwindow *window) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ImGui::StyleColorsDark();

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 330");
}

DebugUi::~DebugUi() {
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}

void DebugUi::beginFrame() {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void DebugUi::draw(const Camera &camera, float deltaTime) {
  if (showDemoWindow) {
    ImGui::ShowDemoWindow(&showDemoWindow);
  }

  const ImGuiIO &io = ImGui::GetIO();
  const float imguiFrameTimeMs =
      io.Framerate > 0.0f ? 1000.0f / io.Framerate : 0.0f;

  ImGui::Begin("Debug");
  ImGui::Text("FPS: %.1f", io.Framerate);
  ImGui::Text("Frame time: %.3f ms", imguiFrameTimeMs);
  ImGui::Text("App deltaTime: %.6f s", deltaTime);
  ImGui::Separator();
  ImGui::Text("Camera position: %.2f, %.2f, %.2f", camera.Position.x,
              camera.Position.y, camera.Position.z);
  ImGui::Text("Camera pitch: %.2f deg", camera.Pitch);
  ImGui::Text("Camera yaw: %.2f deg", camera.Yaw);
  ImGui::Text("Camera zoom: %.2f", camera.Zoom);
  ImGui::Checkbox("Show ImGui demo", &showDemoWindow);
  ImGui::End();
}

void DebugUi::endFrame() {
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

bool DebugUi::wantsMouse() const { return ImGui::GetIO().WantCaptureMouse; }

bool DebugUi::wantsKeyboard() const {
  return ImGui::GetIO().WantCaptureKeyboard;
}
