#pragma once
#include <algorithm>
#include <cmath>
namespace Desktop {
enum class WindowMode { Windowed, Borderless, Fullscreen };
enum class Framing { Square, Cropped };
struct Picture {
  float x = 0, y = 0, width = 0, height = 0, u = 0, v = 0, du = 1, dv = 1;
};
inline Picture FitPicture(float sourceW, float sourceH, float targetW,
                          float targetH, Framing framing) {
  Picture p;
  if (sourceW <= 0 || sourceH <= 0 || targetW <= 0 || targetH <= 0)
    return p;
  const float aspect = framing == Framing::Square ? 1.f : targetW / targetH;
  float w = sourceW, h = sourceH;
  if (w / h > aspect)
    w = h * aspect;
  else
    h = w / aspect;
  p.du = w / sourceW;
  p.dv = h / sourceH;
  p.u = (1 - p.du) * .5f;
  p.v = (1 - p.dv) * .5f;
  const float scale = std::min(1.f, std::min(targetW / w, targetH / h));
  p.width = std::max(1.f, std::floor(w * scale));
  p.height = std::max(1.f, std::floor(h * scale));
  p.x = std::floor((targetW - p.width) * .5f);
  p.y = std::floor((targetH - p.height) * .5f);
  return p;
}
inline float Deadzone(short value, int deadzone) {
  const float magnitude = std::abs(int(value));
  return magnitude <= deadzone
             ? 0.f
             : std::copysign(
                   std::min(1.f, (magnitude - deadzone) / (32767 - deadzone)),
                   float(value));
}
inline Picture FitTheater(float sourceW, float sourceH, float targetW,
                          float targetH, float aspect, Framing framing) {
  Picture p;
  if (sourceW <= 0 || sourceH <= 0 || targetW <= 0 || targetH <= 0 ||
      aspect <= 0)
    return p;
  const float width = std::min(sourceW, sourceH * aspect),
              height = width / aspect;
  const float availableW =
      framing == Framing::Square ? std::min(targetW, targetH) : targetW;
  const float availableH =
      framing == Framing::Square ? std::min(targetW, targetH) : targetH;
  const float scale =
      std::min(1.f, std::min(availableW / width, availableH / height));
  p.width = std::max(1.f, std::floor(width * scale));
  p.height = std::max(1.f, std::floor(height * scale));
  p.x = std::floor((targetW - p.width) * .5f);
  p.y = std::floor((targetH - p.height) * .5f);
  return p;
}
} // namespace Desktop
