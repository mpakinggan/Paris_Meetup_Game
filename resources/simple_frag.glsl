#version 330 core 

out vec4 color;

uniform vec3 MatAmb;
uniform vec3 MatDif;
uniform vec3 MatSpec;
uniform float MatShine;

//interpolated normal and light vector in camera space
in vec3 fragNor;
in vec3 lightDir;
in vec3 fragPos;
//position of the vertex in camera space
in vec3 EPos;

void main()
{
	vec3 normal = normalize(fragNor);
	vec3 light = normalize(lightDir);
	vec3 viewDir = -normalize(fragPos);

	float dC = max(0, dot(normal, light));

	vec3 H = normalize(light+viewDir);
	float sC = pow(max(0, dot(normal,H)), MatShine);

	color = vec4(MatAmb + dC*MatDif + sC*MatSpec, 1.0);

}
