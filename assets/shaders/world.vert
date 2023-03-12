#version 460 core
#extension GL_ARB_gpu_shader5 : require
struct VoxelAttr {
  vec3 origin;
  uint material;
};
// foo foo
layout(std430, binding = 0) buffer StorageVoxelAttribute {
  VoxelAttr attrs[];
} voxel;

struct Camera {
  mat4 proj;
  mat4 view;
};

layout(std140, binding = 0) uniform CameraSet {
  Camera cameras[];
} cameraSet;

out layout(location = 0) vec3 outUv;

// generate cube, stolen
// https://gist.github.com/rikusalminen/9393151
vec3 cubeOriginForVertexID(int vId) {
  int tri = (vId%36) / 3;
  int idx = (vId%36) % 3;
  int face = tri / 2;
  int top = tri % 2;

  int dir = face % 3;
  int pos = face / 3;

  int nz = dir >> 1;
  int ny = dir & 1;
  int nx = 1 ^ (ny | nz);

  vec3 d = vec3(nx, ny, nz);
  float flip = 1 - 2 * pos;

  vec3 n = flip * d;
  vec3 u = -d.yzx;
  vec3 v = flip * d.zxy;

  float mirror = -1 + 2 * top;
  return n + mirror*(1-2*(idx&1))*u + mirror*(1-2*(idx>>1))*v;
}

void main() {
  int cubeID = gl_VertexID/36;
  VoxelAttr attrib = voxel.attrs[cubeID];
  const vec3 cubeVtxOrigin = (
    cubeOriginForVertexID(gl_VertexID)*0.5f
    + vec3(attrib.origin.x, attrib.origin.y, attrib.origin.z)
  );
  const Camera cam = cameraSet.cameras[0];
  gl_Position = (cam.proj * cam.view) * vec4(cubeVtxOrigin, 1.0f);
  int colorID = int((3+(cubeID)) * 25);
  outUv = (
    vec3(
      colorID%2/20.0f,
      colorID%4/5.0f,
      mod((colorID+1), 3.3)/3.3f
    )
  );
}
