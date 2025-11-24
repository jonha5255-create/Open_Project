#version 330 core

out vec4 FragColor;
in vec3 FragPos;

uniform vec3 color;
uniform vec3 lightPos;
uniform vec3 viewPos;

void main()
{
    // 간단한 조명 효과
    vec3 norm = normalize(cross(dFdx(FragPos), dFdy(FragPos)));
    vec3 lightDir = normalize(lightPos - FragPos);
    
    // Ambient
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * color;
    
    // Diffuse
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * color;
    
    vec3 result = ambient + diffuse;
    FragColor = vec4(result, 1.0);
}