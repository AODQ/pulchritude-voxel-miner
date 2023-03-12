#include <pulchritude-log/log.h>
#include <pulchritude-plugin/plugin.h>
#include <pulchritude-plugin/engine.h>
#include <pulchritude-gfx/gfx.h>

#include <vector>

namespace {
PuleEngineLayer pul;
} // namespace

// -- render -------------------------------------------------------------------
namespace {

struct {

  PuleGfxGpuBuffer bufferVoxelAttributes;
  size_t voxelCount;
  size_t voxelCapacity;

  struct {
    PuleGfxCommandList commandList;
    PuleGfxPipeline pipeline = { .id = 0, };
    PuleAssetShaderModule assetShaderModule;
    uint64_t shaderModuleId;
  } render;

  PuleCamera camera;
  PuleCameraSet cameraSet;
  PuleCameraController cameraController;

} ctx;

struct VoxelAttribute {
  PuleF32v3 origin;
  uint32_t type;
};

void initializePipeline() {
  if (ctx.render.pipeline.id != 0) {
    pul.gfxPipelineDestroy(ctx.render.pipeline);
  }
  auto descriptorSetLayout = pul.gfxPipelineDescriptorSetLayout();
  auto shaderModule = (
    pul.assetShaderModuleGfxHandle(ctx.render.assetShaderModule)
  );

  auto pipelineInfo = PuleGfxPipelineCreateInfo {
    .shaderModule = shaderModule,
    .layout = &descriptorSetLayout,
    .config = {
      .depthTestEnabled = true,
      .blendEnabled = false,
      .scissorTestEnabled = false,
      .viewportUl = PuleI32v2 { 0, 0, },
      .viewportLr = PuleI32v2 { 800, 600, },
      .scissorUl = PuleI32v2 { 0, 0, },
      .scissorLr = PuleI32v2 { 800, 600, },
    },
  };

  PuleError err = puleError();
  ctx.render.pipeline = pul.gfxPipelineCreate(&pipelineInfo, &err);
  ctx.render.shaderModuleId = shaderModule.id;
  puleLog("contex render pipeline: %d", ctx.render.pipeline);
  if (pul.errorConsume(&err) > 0) {
    return;
  }
}

void initializeContext(
  PulePluginPayload const payload,
  PulePlatform const platform
) {
  PuleError err = puleError();

  ctx.voxelCount = 0;
  ctx.voxelCapacity = 32;

  ctx.camera = puleCameraCreate();
  ctx.cameraController = puleCameraControllerFirstPerson(platform, ctx.camera);
  ctx.cameraSet = puleCameraSetCreate(puleCStr("primary"));

  puleCameraSetAdd(ctx.cameraSet, ctx.camera);

  VoxelAttribute initialVoxelAttributes[32];
  std::vector<VoxelAttribute> attrs = {
    { .origin = {.x = 0.0f, .y = 0.0f, .z = 0.0f,}, .type = 1, },
    { .origin = {.x = 0.0f, .y = 1.0f, .z = 0.0f,}, .type = 2, },
    { .origin = {.x = 0.0f, .y = 2.0f, .z = 0.0f,}, .type = 3, },
  };
  ctx.voxelCount = attrs.size();
  memcpy(initialVoxelAttributes, attrs.data(), attrs.size());

  ctx.bufferVoxelAttributes = (
    pul.gfxGpuBufferCreate(
      initialVoxelAttributes,
      sizeof(initialVoxelAttributes),
      PuleGfxGpuBufferUsage_bufferStorage,
      PuleGfxGpuBufferVisibilityFlag_hostWritable
    )
  );

  ctx.render.assetShaderModule = {
    .id = pul.pluginPayloadFetchU64(
      payload,
      puleCStr("pule-asset-shader-module-world")
    ),
  };
  ::pul = *reinterpret_cast<PuleEngineLayer *>(
    pul.pluginPayloadFetch(payload, puleCStr("pule-engine-layer"))
  );
  if (pul.errorConsume(&err)) { return; }
  initializePipeline();
}

} // namespace

extern "C" {

PulePluginType pulcPluginType() {
  return PulePluginType_component;
}

void pulcComponentLoad(PulePluginPayload const payload) {
  ::pul = *reinterpret_cast<PuleEngineLayer *>(
    pulePluginPayloadFetch(payload, puleCStr("pule-engine-layer"))
  );

  auto const platform = PulePlatform {
    pulePluginPayloadFetchU64(payload, puleCStr("pule-platform"))
  };
  initializeContext(payload, platform);
}

void pulcComponentUpdate(PulePluginPayload const payload) {
  // update camera [ TODO move to application ]
  puleCameraControllerPollEvents();
  puleCameraSetRefresh(ctx.cameraSet);

  if (
    ctx.render.shaderModuleId
    != pul.assetShaderModuleGfxHandle(ctx.render.assetShaderModule).id
  ) {
    initializePipeline();
  }

  // append our task
  auto const taskGraph = PuleTaskGraph {
    .id = pul.pluginPayloadFetchU64(
      payload,
      pul.cStr("pule-render-task-graph")
    ),
  };
  PuleTaskGraphNode const renderGeometryNode = (
    pul.taskGraphNodeFetch(taskGraph, pul.cStr("render-geometry"))
  );
  auto const recorder = PuleGfxCommandListRecorder {
    pul.taskGraphNodeAttributeFetchU64(
      renderGeometryNode,
      pul.cStr("command-list-primary-recorder")
    )
  };

  pul.gfxCommandListAppendAction(
    recorder,
    PuleGfxCommand {
      .bindPipeline = {
        .action = PuleGfxAction_bindPipeline,
        .pipeline = ctx.render.pipeline,
      },
    }
  );
  pul.gfxCommandListAppendAction(
    recorder,
    PuleGfxCommand {
      .bindFramebuffer = {
        .action = PuleGfxAction_bindFramebuffer,
        .framebuffer = puleGfxFramebufferWindow(),
      },
    }
  );
  pul.gfxCommandListAppendAction(
    recorder,
    PuleGfxCommand {
      .clearFramebufferColor = {
        .action = PuleGfxAction_clearFramebufferColor,
        .framebuffer = puleGfxFramebufferWindow(),
        .color = PuleF32v4(0.2f, 0.5f, 0.2f, 1.0f),
      },
    }
  );
  pul.gfxCommandListAppendAction(
    recorder,
    PuleGfxCommand {
      .clearFramebufferDepth = {
        .action = PuleGfxAction_clearFramebufferDepth,
        .framebuffer = puleGfxFramebufferWindow(),
        .depth = 1.0f,
      },
    }
  );
  pul.gfxCommandListAppendAction(
    recorder,
    PuleGfxCommand {
      .bindBuffer = {
        .action = PuleGfxAction_bindBuffer,
        .usage = PuleGfxGpuBufferUsage_bufferStorage,
        .bindingIndex = 0,
        .buffer = ctx.bufferVoxelAttributes,
        .offset = 0, .byteLen = ctx.voxelCount * sizeof(VoxelAttribute),
      },
    }
  );
  pul.gfxCommandListAppendAction(
    recorder,
    PuleGfxCommand {
      .bindBuffer = {
        .action = PuleGfxAction_bindBuffer,
        .usage = PuleGfxGpuBufferUsage_bufferUniform,
        .bindingIndex = 0,
        .buffer = puleCameraSetGfxUniformBuffer(ctx.cameraSet),
        .offset = 0, .byteLen = sizeof(PuleF32m44)*2 + 1, // todo camera len?
      },
    }
  );

  pul.gfxCommandListAppendAction(
    recorder,
    PuleGfxCommand {
      .dispatchRender = {
        .action = PuleGfxAction_dispatchRender,
        .drawPrimitive = PuleGfxDrawPrimitive_triangle,
        .vertexOffset = 0,
        .numVertices = ctx.voxelCount * 36,
      },
    }
  );
}

} // extern C
