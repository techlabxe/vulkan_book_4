#version 450

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location=0) in vec3 inNormal[];
layout(location=1) in vec2 inUV0[];

in gl_PerVertex
{
  vec4 gl_Position;
} gl_in[];


layout(xfb_stride=32, xfb_offset=0, xfb_buffer=0, location=0)
out vec3 outPosition;

layout(xfb_stride=32, xfb_offset=12, xfb_buffer=0, location=1)
out vec3 outNormal;

layout(xfb_stride=32, xfb_offset=24, xfb_buffer=0, location=2)
out vec2 outUV0;

void main()
{
  for(int i=0;i<gl_in.length(); ++i) {
    outPosition = gl_in[i].gl_Position.xyz;
    outNormal = inNormal[i].xyz;
    outUV0 = inUV0[i];
    EmitVertex();
  }
  EndPrimitive();
}
