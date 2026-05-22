#pragma once

class Camera;
struct GLFWwindow;

class DebugUi {
public:
  enum class VizMode { Points, Beams, Both };

  explicit DebugUi(GLFWwindow *window);
  ~DebugUi();

  DebugUi(const DebugUi &) = delete;
  DebugUi &operator=(const DebugUi &) = delete;

  void beginFrame();
  bool draw(const Camera &camera, bool &vsyncEnabled);
  void endFrame();

  bool wantsMouse() const;
  bool wantsKeyboard() const;

  VizMode vizMode() const { return vizMode_; }

private:
  bool showDemoWindow = false;
  VizMode vizMode_ = VizMode::Beams;
};
