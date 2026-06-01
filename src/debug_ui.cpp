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

bool DebugUi::draw(const Camera &camera, bool &vsyncEnabled, float &clipNear,
                   float &clipFar, int &appMode,
                   int &selectedPeelLayer, int generatedLayerCount,
                   int &aovDisplayMode,
                   glm::vec3 &planeNormal, float &planeOffset,
                   float &planeScale, int &transVizMode, int &transTestCase) {
  if (showDemoWindow)
    ImGui::ShowDemoWindow(&showDemoWindow);

  const ImGuiIO &io = ImGui::GetIO();

  ImGui::Begin("Debug");

  ImGui::Text("FPS: %.1f", io.Framerate);
  const bool vsyncChanged = ImGui::Checkbox("VSync", &vsyncEnabled);
  ImGui::Separator();

  ImGui::Text("Camera position: %.2f, %.2f, %.2f",
              camera.Position.x, camera.Position.y, camera.Position.z);
  ImGui::Text("Camera pitch & yaw: %.2f, %.2f deg", camera.Pitch, camera.Yaw);
  ImGui::Text("Camera zoom: %.2f", camera.Zoom);
  ImGui::Separator();

  ImGui::SliderFloat("Near", &clipNear, 0.01f, 10.0f);
  ImGui::SliderFloat("Far",  &clipFar,  1.0f,  200.0f);
  ImGui::Separator();

  ImGui::Text("Mode");
  ImGui::RadioButton("AOV",            &appMode, 0); ImGui::SameLine();
  ImGui::RadioButton("Transmittance",  &appMode, 1);
  ImGui::Separator();

  // Shared helper: one color swatch + label on a single line.
  auto swatchRow = [](const char *id, float r, float g, float b, const char *label) {
    ImGui::ColorButton(id, ImVec4(r, g, b, 1.0f),
                       ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker,
                       ImVec2(16, 16));
    ImGui::SameLine();
    ImGui::Text("%s", label);
  };

  if (appMode == 0) {
    // ---- AOV ----
    ImGui::Text("Generated layers: %d", generatedLayerCount);
    if (generatedLayerCount > 0)
      ImGui::SliderInt("Layer", &selectedPeelLayer, 0, generatedLayerCount - 1);
    const char *aovModes[] = {"Color", "Depth", "Medium"};
    ImGui::Combo("Display", &aovDisplayMode, aovModes, IM_ARRAYSIZE(aovModes));

    if (aovDisplayMode == 2) {
      ImGui::Spacing();
      ImGui::Text("Medium ID");
      swatchRow("##vac", 0.08f, 0.08f, 0.08f, "Vacuum (-1)");
      swatchRow("##m1",  0.95f, 0.35f, 0.35f, "Medium 1");
      swatchRow("##m2",  0.30f, 0.85f, 0.45f, "Medium 2");
      swatchRow("##m3",  0.25f, 0.55f, 0.95f, "Medium 3");
      swatchRow("##m4",  0.90f, 0.80f, 0.30f, "Medium 4");
    }
  } else {
    // ---- Transmittance ----
    const char *testCaseNames[] = {
        "Encapsulated (3 nested)",
        "Separate (3 in a line)",
        "2 encapsulated + 1 behind",
        "Encapsulated (reversed IDs)",
        "2 encapsulated",

    };
    ImGui::Combo("Test case", &transTestCase, testCaseNames, IM_ARRAYSIZE(testCaseNames));
    ImGui::Separator();
    ImGui::SliderFloat3("Plane normal", &planeNormal.x, -1.0f,  1.0f);
    ImGui::SliderFloat("Plane offset",  &planeOffset,  -20.0f, 20.0f);
    ImGui::SliderFloat("Plane scale",   &planeScale,    0.1f,  20.0f);
    const char *vizModes[] = {"Transmittance", "World Depth", "Media Count"};
    ImGui::Combo("Visualize", &transVizMode, vizModes, IM_ARRAYSIZE(vizModes));

    if (transVizMode == 2) {
      ImGui::Spacing();
      ImGui::Text("Media count");
      swatchRow("##c0", 0.05f, 0.05f, 0.05f, "0 (vacuum only)");
      swatchRow("##c1", 0.95f, 0.35f, 0.35f, "1");
      swatchRow("##c2", 0.30f, 0.85f, 0.45f, "2");
      swatchRow("##c3", 0.25f, 0.55f, 0.95f, "3");
      swatchRow("##c4", 0.90f, 0.80f, 0.30f, "4");
      swatchRow("##c5", 1.00f, 0.00f, 1.00f, "5+");
    }
  }

  ImGui::Separator();
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
