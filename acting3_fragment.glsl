#version 330 core

in vec3 FragPos;
in vec3 Normal;
in vec3 Color;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;

out vec4 FragColor;

void main()
{
    // Ambient
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * Color;
    
    // Diffuse
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor * Color;
    
    // Specular
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = specularStrength * spec * lightColor;
    
    vec3 result = ambient + diffuse + specular;
    FragColor = vec4(result, 1.0);
}