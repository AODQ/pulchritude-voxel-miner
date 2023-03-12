#version 460 core
in layout(location = 0) vec3 inUv;
out layout(location = 0) vec4 outColor;

void main() {
  outColor = vec4(inUv, 1.0f);
}
