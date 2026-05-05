#include "framebuffer.hpp"

#include <glad/glad.h>

#include <iostream>
#include <utility>

Framebuffer::Framebuffer(int width, int height) { create(width, height); }

Framebuffer::~Framebuffer() { destroy(); }

Framebuffer::Framebuffer(Framebuffer &&other) noexcept {
  *this = std::move(other);
}

Framebuffer &Framebuffer::operator=(Framebuffer &&other) noexcept {
  if (this == &other) {
    return *this;
  }

  destroy();

  fbo = other.fbo;
  colorTex = other.colorTex;
  depthTex = other.depthTex;
  mediumTex = other.mediumTex;
  framebufferWidth = other.framebufferWidth;
  framebufferHeight = other.framebufferHeight;

  other.fbo = 0;
  other.colorTex = 0;
  other.depthTex = 0;
  other.mediumTex = 0;
  other.framebufferWidth = 0;
  other.framebufferHeight = 0;

  return *this;
}

bool Framebuffer::create(int width, int height) {
  destroy();

  framebufferWidth = width;
  framebufferHeight = height;

  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);

  glGenTextures(1, &colorTex);
  glGenTextures(1, &depthTex);
  glGenTextures(1, &mediumTex);

  if (!allocateTextures(width, height)) {
    destroy();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return false;
  }

  const bool complete =
      glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
  if (!complete) {
    std::cerr << "Framebuffer is not complete!" << std::endl;
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return complete;
}

bool Framebuffer::resize(int width, int height) {
  if (width == framebufferWidth && height == framebufferHeight) {
    return true;
  }

  if (fbo == 0) {
    return create(width, height);
  }

  framebufferWidth = width;
  framebufferHeight = height;

  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  const bool allocated = allocateTextures(width, height);
  const bool complete =
      glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  if (!complete) {
    std::cerr << "Framebuffer is not complete after resize!" << std::endl;
  }

  return allocated && complete;
}

void Framebuffer::destroy() {
  if (depthTex != 0) {
    glDeleteTextures(1, &depthTex);
    depthTex = 0;
  }

  if (mediumTex != 0) {
    glDeleteTextures(1, &mediumTex);
    mediumTex = 0;
  }

  if (colorTex != 0) {
    glDeleteTextures(1, &colorTex);
    colorTex = 0;
  }

  if (fbo != 0) {
    glDeleteFramebuffers(1, &fbo);
    fbo = 0;
  }

  framebufferWidth = 0;
  framebufferHeight = 0;
}

void Framebuffer::bind() const { glBindFramebuffer(GL_FRAMEBUFFER, fbo); }

void Framebuffer::bindDefault() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }

void Framebuffer::clear(float r, float g, float b, float a,
                        int mediumValue) const {
  const float color[] = {r, g, b, a};
  const int medium[] = {mediumValue, 0, 0, 0};
  glClearBufferfv(GL_COLOR, 0, color);
  glClearBufferiv(GL_COLOR, 1, medium);
  glClear(GL_DEPTH_BUFFER_BIT);
}

bool Framebuffer::allocateTextures(int width, int height) {
  if (width <= 0 || height <= 0) {
    return false;
  }

  glBindTexture(GL_TEXTURE_2D, colorTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         colorTex, 0);

  glBindTexture(GL_TEXTURE_2D, mediumTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R32I, width, height, 0, GL_RED_INTEGER,
               GL_INT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D,
                         mediumTex, 0);

  glBindTexture(GL_TEXTURE_2D, depthTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, width, height, 0,
               GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                         depthTex, 0);

  const unsigned int drawBuffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
  glDrawBuffers(2, drawBuffers);

  glBindTexture(GL_TEXTURE_2D, 0);
  return true;
}
