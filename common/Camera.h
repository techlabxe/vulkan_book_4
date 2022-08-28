#pragma once
#ifndef GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#endif
#include <glm/glm.hpp>

class Camera
{
public:
  Camera();
  void SetLookAt(glm::vec3 eyePos, glm::vec3 target, glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f));
  void SetPerspective(
    float fovY, float aspect, float znear, float zfar);

  void OnMouseMove(float dx, float dy);
  void OnMouseButtonDown(int buttonType);
  void OnMouseButtonUp();

  glm::mat4 GetViewMatrix()const { return m_view; }
  glm::mat4 GetProjectionMatrix() const { return m_proj; }

  glm::vec3 GetPosition() const { return m_eye; }
  glm::vec3 GetTarget() const { return m_target; }

private:
  void CalcOrbit(float dx, float dy);
  void CalcDolly(float d);
  void CalcPan(float dx, float dy);
private:
  glm::vec3 m_eye;
  glm::vec3 m_target;
  glm::vec3 m_up;

  glm::mat4 m_view;
  glm::mat4 m_proj;

  bool m_isDragged;
  int m_buttonType;
};