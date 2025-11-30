#version 330 core

uniform sampler2D sampler;
uniform vec4 color = vec4(1, 1, 1, 1);
in vec2 t;
out vec4 v;

void main()
{
    v = color * vec4(1, 1, 1, texture(sampler, t).r);
}
