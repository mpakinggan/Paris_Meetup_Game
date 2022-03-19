#version  330 core
layout(location = 0) in vec3 vertPos;
layout(location = 1) in vec3 vertNor;
layout(location = 2) in vec2 vertTex;
uniform mat4 P;
uniform mat4 M;
uniform mat4 V;

out vec2 vTexCoord;
out vec3 fragNor;
out vec3 fragPos;
out vec3 lightDir;

void main() {

  vec4 vPosition;

  /* First model transforms */
  gl_Position = P * V * M * vec4(vertPos.xyz, 1.0);
  fragNor = mat3(transpose(inverse(V * M))) * vertNor;
  
  fragPos = vec3(V * M * vec4(vertPos, 1.0));

  lightDir = -vec3(V * vec4(0,-1.0,-0.3,0.0));

  /* pass through the texture coordinates to be interpolated */
  vTexCoord = vertTex;
}
