#version  330 core
layout(location = 0) in vec4 vertPos;
layout(location = 1) in vec3 vertNor;
uniform mat4 P;
uniform mat4 V;
uniform mat4 M;

out vec3 fragNor;
out vec3 lightDir;
out vec3 fragPos;
out vec3 EPos;

void main()
{
	gl_Position = P * V * M * vertPos;
	fragNor = mat3(transpose(inverse(V*M))) * vertNor;
	lightDir = -vec3(V * vec4(0,-1.0,-0.3,0.0));

  	fragPos = vec3(V * M * vertPos);

	EPos = vec3(1); //PULLED for release
}
