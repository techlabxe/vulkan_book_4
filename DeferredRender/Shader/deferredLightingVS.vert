#version 450

layout(location=0) out vec2 outUV0;

void main()
{
  outUV0.x = (gl_VertexIndex << 1) & 2;
  outUV0.y = (gl_VertexIndex & 2);

  gl_Position = vec4(outUV0 * 2.0 - 1.0, 0, 1);
}
