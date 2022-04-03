#include "query_pool.h"

#include <fmt/format.h>

void query_pool::setup(VulkanExampleBase& app) {
  if (!app.enabledFeatures.features.pipelineStatisticsQuery) {
    return;
  }

  _pipeline_stat_names_ = {
      "Input assembly vertex count        ",
      "Input assembly primitives count    ",
      "Vertex shader invocations          ",
      "Clipping stage primitives processed",
      "Clipping stage primitives output   ",
      "Fragment shader invocations        "
  };
  if (app.enabledFeatures.features.geometryShader) {
    _pipeline_stat_names_.insert(_pipeline_stat_names_.begin() + 3, {
        "Geometry shader invocations",
        "Geometry shader primitives count"
    });
  }
  if (app.enabledFeatures.features.tessellationShader) {
    _pipeline_stat_names_.insert(_pipeline_stat_names_.end(), {
        "Tessellation control shader patches",
        "Tessellation eval. shader invocations"
    });
  }

  vk::QueryPoolCreateInfo query_pool_info = {};
  query_pool_info.queryType = vk::QueryType::ePipelineStatistics;
  query_pool_info.pipelineStatistics =
      vk::QueryPipelineStatisticFlagBits::eInputAssemblyVertices |
          vk::QueryPipelineStatisticFlagBits::eInputAssemblyPrimitives |
          vk::QueryPipelineStatisticFlagBits::eVertexShaderInvocations |
          vk::QueryPipelineStatisticFlagBits::eClippingInvocations |
          vk::QueryPipelineStatisticFlagBits::eClippingPrimitives |
          vk::QueryPipelineStatisticFlagBits::eFragmentShaderInvocations;
  if (app.enabledFeatures.features.geometryShader) {
    query_pool_info.pipelineStatistics = query_pool_info.pipelineStatistics |
        vk::QueryPipelineStatisticFlagBits::eGeometryShaderInvocations |
        vk::QueryPipelineStatisticFlagBits::eGeometryShaderPrimitives;
  }
  if (app.enabledFeatures.features.tessellationShader) {
    query_pool_info.pipelineStatistics = query_pool_info.pipelineStatistics |
        vk::QueryPipelineStatisticFlagBits::eTessellationControlShaderPatches |
        vk::QueryPipelineStatisticFlagBits::eTessellationEvaluationShaderInvocations;
  }
  query_pool_info.queryCount = static_cast<std::uint32_t>(_pipeline_stat_names_.size());
  _query_pool_ = app.device.createQueryPoolUnique(query_pool_info);
}

void query_pool::destroy() {
  _query_pool_.reset();
}

void query_pool::begin(vk::CommandBuffer command_buffer) const {
  if (!enabled()) {
    return;
  }

  command_buffer.beginQuery(*_query_pool_, 0, {});
}

void query_pool::end(vk::CommandBuffer command_buffer) const {
  if (!enabled()) {
    return;
  }

  command_buffer.endQuery(*_query_pool_, 0);
}

void query_pool::reset(vk::CommandBuffer command_buffer) const {
  if (!enabled()) {
    return;
  }

  command_buffer.resetQueryPool(*_query_pool_, 0, static_cast<std::uint32_t>(_pipeline_stat_names_.size()));
}

void query_pool::update_query_results() {
  if (!enabled()) {
    return;
  }

  const auto count = static_cast<std::uint32_t>(_pipeline_stat_names_.size());
  auto result = app().device.getQueryPoolResults<std::uint64_t>(*_query_pool_,
                                                                0,
                                                                1,
                                                                count * sizeof(std::uint64_t),
                                                                sizeof(std::uint64_t),
                                                                vk::QueryResultFlagBits::e64);
  if (result.result == vk::Result::eSuccess) {
    _query_results_ = std::move(result.value);
  }
}

const std::vector<std::uint64_t>& query_pool::query_results() const {
  return _query_results_;
}

const std::vector<std::string>& query_pool::pipeline_stat_names() const {
  return _pipeline_stat_names_;
}
