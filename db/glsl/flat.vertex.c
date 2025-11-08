#version 330 core

layout (location = 0) in vec4 x;
layout (location = 1) in vec4 c;

uniform mat4 matrix;
uniform float intensity;
out vec4 color;

void main()
{
   gl_Position = matrix * x;
   color = c * vec4(vec3(intensity), 1.0f);
}
