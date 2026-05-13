#pragma once

class Framebuffer {
public:
  Framebuffer() = default;
  Framebuffer(int width, int height);
  ~Framebuffer();

  Framebuffer(const Framebuffer &) = delete;
  Framebuffer &operator=(const Framebuffer &) = delete;

  Framebuffer(Framebuffer &&other) noexcept;
  Framebuffer &operator=(Framebuffer &&other) noexcept;

  bool create(int width, int height);
  bool resize(int width, int height);
  void destroy();

  void bind() const;
  static void bindDefault();

  unsigned int id() const { return fbo; }
  unsigned int colorTexture() const { return colorTex; }
  unsigned int depthTexture() const { return depthTex; }
  unsigned int mediumTexture() const { return mediumTex; }
  int width() const { return framebufferWidth; }
  int height() const { return framebufferHeight; }

  void clear(float r, float g, float b, float a, int mediumValue = 0) const;

private:
  bool allocateTextures(int width, int height);

  unsigned int fbo = 0;
  unsigned int colorTex = 0;
  unsigned int depthTex = 0;
  unsigned int mediumTex = 0;
  unsigned int colorTexLayer = 0;
  unsigned int depthTexLayer = 0;
  unsigned int mediumTexLayer = 0;
  int framebufferWidth = 0;
  int framebufferHeight = 0;
};
