#version 330 core

in vec3 Normal;
in vec3 FragPos;  

out vec4 FragColor;

uniform vec3 objectColor;
uniform vec3 lightColor;
uniform vec3 lightPos;
uniform vec3 viewPos;


void main() {
    float ambientStrength = 0.1;
    float specularStrength = 0.5;
    float shininess = 64; 

    // there can also be ambient material and ambient light
    // here we take the same lightColor as diffuse
    vec3 ambient = ambientStrength * lightColor;

    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    // without max, diffuse can be negative due to dot product (i.e. the angle between them is bigger than 90deg) which will "eat up" the ambient light
    vec3 diffuse = max(dot(norm, lightDir), 0.0) * lightColor;

    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    vec3 specular = specularStrength * spec * lightColor;  



    vec3 result = (ambient + diffuse + specular) * objectColor;

    FragColor = vec4(result, 1.0);
}