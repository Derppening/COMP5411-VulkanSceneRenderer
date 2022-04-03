#include "tessellation.h"

bool tessellation::supported() const {
  return app().enabledFeatures.features.tessellationShader;
}

void tessellation::populate_ci(vk::GraphicsPipelineCreateInfo& create_info,
                               std::vector<vk::PipelineShaderStageCreateInfo>& shader_stages) const {
  if (!enabled()) {
    return;
  }

  const_cast<vk::PipelineInputAssemblyStateCreateInfo*>(create_info.pInputAssemblyState)->topology =
      vk::PrimitiveTopology::ePatchList;

  shader_stages.resize(shader_stages.size() + 2);
  create_info.stageCount = static_cast<std::uint32_t>(shader_stages.size());
  create_info.pStages = shader_stages.data();

  auto& tesc = shader_stages[shader_stages.size() - 2];
  tesc.stage = vk::ShaderStageFlagBits::eTessellationControl;
  tesc.pName = "main";
  auto& tese = shader_stages[shader_stages.size() - 1];
  tese.stage = vk::ShaderStageFlagBits::eTessellationEvaluation;
  tese.pName = "main";
  if (_mode_ == 1) {
    tesc.module = _shader_modules_._passthrough_ctrl_;
    tese.module = _shader_modules_._passthrough_eval_;
  } else if (_mode_ == 2) {
    tesc.module = _shader_modules_._pn_ctrl_;
    tese.module = _shader_modules_._pn_eval_;
  } else {
    throw std::runtime_error("Unknown mode enumeration");
  }
}

void tessellation::setup(VulkanExampleBase& app) {
  if (supported()) {
    _shader_modules_._passthrough_ctrl_ = app.loadShader(app.getShadersPath() + "pntriangles/passthrough.tesc.spv",
                                                         vk::ShaderStageFlagBits::eTessellationControl).module;
    _shader_modules_._passthrough_eval_ = app.loadShader(app.getShadersPath() + "pntriangles/passthrough.tese.spv",
                                                         vk::ShaderStageFlagBits::eTessellationEvaluation).module;
    _shader_modules_._pn_ctrl_ = app.loadShader(app.getShadersPath() + "pntriangles/pntriangles.tesc.spv",
                                                vk::ShaderStageFlagBits::eTessellationControl).module;
    _shader_modules_._pn_eval_ = app.loadShader(app.getShadersPath() + "pntriangles/pntriangles.tese.spv",
                                                vk::ShaderStageFlagBits::eTessellationEvaluation).module;
  }
}

void tessellation::destroy() {
  // no-op
}
