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
  bool draw(const Camera &camera, bool &vsyncEnabled, float &clipNear,
            float &clipFar, int &selectedPeelLayer, int generatedLayerCount,
            int &displayMode, glm::vec3 &planeNormal, float &planeOffset,
            float &planeScale);
  void endFrame();

  bool wantsMouse() const;
  bool wantsKeyboard() const;

private:
  bool showDemoWindow = false;
};
