#version 330 core

in vec4 color;
out vec4 v;

void main()
{
    vec2 r = (gl_PointCoord - vec2(0.5));
    float rr = dot(r, r);
    if (rr > 0.25) {
        discard;
    }

    v = vec4(color.rgb * (rr < 0.08 ? 0.9 : 0.5), color.a);
}
