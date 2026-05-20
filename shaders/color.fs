#version 330 core

struct Light {
    vec3 position;
  
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

in vec3 Normal;
in vec3 FragPos;
in vec2 TexCoords;

out vec4 FragColor;

uniform Light light;  

uniform vec3 viewPos;

uniform sampler2D texture_diffuse1;
uniform sampler2D texture_specular1;


void main() {

    // there can also be ambient material and ambient light
    // here we take the same lightColor as diffuse
    vec3 ambient = texture(texture_diffuse1, TexCoords).rgb * light.ambient;

    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(light.position - FragPos);

    // without max, diffuse can be negative due to dot product (i.e. the angle between them is bigger than 90deg) which will "eat up" the ambient light
    vec3 diffuse = max(dot(norm, lightDir), 0.0) * light.diffuse * texture(texture_diffuse1, TexCoords).rgb;

    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);

    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = texture(texture_specular1, TexCoords).rgb * spec * light.specular;  

    vec3 result = ambient + diffuse + specular;

    FragColor = vec4(result, 1.0);
}