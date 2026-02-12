#version 330 core

layout (location = 0) in vec4 x;
uniform mat4 matrix;

void main()
{
    gl_Position = matrix * x;
}
