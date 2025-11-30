#version 330 core

layout (location = 0) in vec4 x;
out vec2 t;

uniform mat4 matrix;

void main()
{
    t = (vec2(x) + 1.0) / 2.0;
    gl_Position = matrix * x;
}
