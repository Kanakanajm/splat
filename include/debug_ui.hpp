#pragma once

class Camera;
struct GLFWwindow;

class DebugUi {
public:
  explicit DebugUi(GLFWwindow *window);
  ~DebugUi();

  DebugUi(const DebugUi &) = delete;
  DebugUi &operator=(const DebugUi &) = delete;

  void beginFrame();
  void draw(const Camera &camera, float deltaTime);
  void endFrame();

  bool wantsMouse() const;
  bool wantsKeyboard() const;

private:
  bool showDemoWindow = false;
};
