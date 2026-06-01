#pragma once

#include <glm/vec3.hpp>

class Camera;
struct GLFWwindow;

class DebugUi {
public:
  explicit DebugUi(GLFWwindow *window);
  ~DebugUi();

  DebugUi(const DebugUi &) = delete;
  DebugUi &operator=(const DebugUi &) = delete;

  void beginFrame();

  // Returns true when vsync was toggled.
  bool draw(const Camera &camera, bool &vsyncEnabled, float &clipNear,
            float &clipFar, int &appMode,
            int &selectedPeelLayer, int generatedLayerCount, int &aovDisplayMode,
            glm::vec3 &planeNormal, float &planeOffset, float &planeScale,
            int &transVizMode);

  void endFrame();

  bool wantsMouse() const;
  bool wantsKeyboard() const;

private:
  bool showDemoWindow = false;
};
