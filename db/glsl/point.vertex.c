#version 330 core

layout (location = 0) in vec4 x;
layout (location = 1) in vec4 c;

uniform mat4 matrix;
uniform float scale, size;
uniform float intensity;
out vec4 color;

void main()
{
    gl_Position = matrix * x;
    gl_PointSize = size * scale / gl_Position.w;
    color = vec4(intensity * c.rgb, c.a);
}
