#include "Camera.h"
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_access.hpp>

using namespace glm;

Camera::Camera() : m_isDragged(false)
{
  m_view = glm::mat4(1.0f);
  m_proj = glm::mat4(1.0f);
}

void Camera::SetLookAt(glm::vec3 eyePos, glm::vec3 target, glm::vec3 up)
{
  m_eye = eyePos;
  m_target = target;
  m_up = up;
  m_view = glm::lookAtRH(m_eye, m_target, m_up);
}

void Camera::SetPerspective(float fovY, float aspect, float znear, float zfar)
{
  m_proj = glm::perspectiveRH(fovY, aspect, znear, zfar);
}

void Camera::OnMouseButtonDown(int buttonType)
{
  m_isDragged = true;
  m_buttonType = buttonType;
}
void Camera::OnMouseButtonUp()
{
  m_isDragged = false;
}

void Camera::CalcOrbit(float dx, float dy)
{
  auto toEye = m_eye - m_target;
  auto toEyeLength = glm::length(toEye);
  toEye = glm::normalize(toEye);

  auto phi = std::atan2f(toEye.x, toEye.z); // 方位角.
  auto theta = std::acos(toEye.y);  // 仰角.

  const auto PI = glm::pi<float>();
  const auto PI2 = glm::two_pi<float>();
  auto x = (PI + phi) / PI2;
  auto y = theta / PI;

  x += dx;
  y -= dy;
  y = std::fmax(0.02f, std::fmin(y, 0.98f));

  phi = x * PI2;
  theta = y * PI;

  auto st = std::sinf(theta);
  auto sp = std::sinf(phi);
  auto ct = std::cosf(theta);
  auto cp = std::cosf(phi);

  // 各成分より新カメラ位置への3次元ベクトルを生成.
  auto newToEye = glm::normalize(glm::vec3(-st * sp, ct, -st * cp));
  newToEye *= toEyeLength;
  m_eye = m_target + newToEye;

  m_view = glm::lookAtRH(m_eye, m_target, m_up);
}

void Camera::CalcDolly(float d)
{
  auto toTarget = m_target - m_eye;
  auto toTargetLength = glm::length(toTarget);
  if (toTargetLength < FLT_EPSILON) {
    return;
  }
  toTarget = glm::normalize(toTarget);
  auto delta = toTargetLength * d;
  m_eye += toTarget * delta;

  m_view = glm::lookAtRH(m_eye, m_target, m_up);
}

void Camera::CalcPan(float dx, float dy)
{
  auto toTarget = m_target - m_eye;
  auto toTargetLength = glm::length(toTarget);

  m_view = glm::lookAtRH(m_eye, m_target, m_up);
  auto m = glm::transpose(m_view);
  auto right = glm::vec3(m[0] * dx * toTargetLength);
  auto top = glm::vec3(m[1] * dy * toTargetLength);
  m_eye += right;
  m_target += right;
  m_eye += top;
  m_target += top;

  m_view= glm::lookAtRH(m_eye, m_target, m_up);
}

void Camera::OnMouseMove(float dx, float dy)
{
  if (!m_isDragged)
  {
    return;
  }

  if (m_buttonType == 0)
  {
    CalcOrbit(dx, dy);
  }
  if (m_buttonType == 1)
  {
    CalcDolly(dy);
  }
  if (m_buttonType == 2)
  {
    CalcPan(dx, dy);
  }
}

