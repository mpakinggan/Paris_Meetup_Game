#version 330 core
uniform sampler2D Texture0;

in vec2 vTexCoord;
in vec3 fragNor;
in vec3 fragPos;
in vec3 lightDir;

out vec4 Outcolor;

void main() {
  	vec4 texColor0 = texture(Texture0, vTexCoord.st);

	vec4 matDif = 0.5*(texColor0);
	vec4 matSpec= 0.5*(texColor0);
	vec4 matAmb = 0.1*(texColor0);

	vec3 normal = normalize(fragNor);
	vec3 light = normalize(lightDir);
	vec3 camPos = normalize(-fragPos);

	float dC = max(0.0, dot(normal,light));

	vec3 H = normalize(light+camPos);
	float sC = pow(max(0.0, dot(normal,H)), 12);

	vec4 color = vec4(matAmb + dC*matDif + sC*matSpec);

	Outcolor = color;

}

