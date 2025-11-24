#version 330 core

layout(location = 0) in vec3 vPos;

uniform vec3 position;    // 2D -> 3D (z값 추가)
uniform vec3 scale;       // 2D -> 3D (z값 추가)
uniform mat4 view;        // 뷰 행렬
uniform mat4 projection;  // 투영 행렬

out vec3 FragPos;

void main()
{
    vec3 pos = vPos * scale + position;
    FragPos = pos;
    gl_Position = projection * view * vec4(pos, 1.0);
}