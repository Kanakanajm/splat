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

bool DebugUi::draw(const Camera &camera, bool &vsyncEnabled) {
  if (showDemoWindow) {
    ImGui::ShowDemoWindow(&showDemoWindow);
  }

  const ImGuiIO &io = ImGui::GetIO();

  ImGui::Begin("Debug");
  ImGui::Text("FPS: %.1f", io.Framerate);
  const bool vsyncChanged = ImGui::Checkbox("VSync", &vsyncEnabled);
  ImGui::Separator();
  ImGui::Text("Camera position: %.2f, %.2f, %.2f", camera.Position.x,
              camera.Position.y, camera.Position.z);
  ImGui::Text("Camera pitch & yaw: %.2f, %.2f deg", camera.Pitch,  camera.Yaw);
  ImGui::Text("Camera zoom: %.2f", camera.Zoom);
  ImGui::Checkbox("Show ImGui demo", &showDemoWindow);
  ImGui::End();

  return vsyncChanged;
}

void DebugUi::endFrame() {
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

bool DebugUi::wantsMouse() const { return ImGui::GetIO().WantCaptureMouse; }

bool DebugUi::wantsKeyboard() const {
  return ImGui::GetIO().WantCaptureKeyboard;
}
