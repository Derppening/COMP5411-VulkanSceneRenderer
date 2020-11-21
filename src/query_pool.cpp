#include "query_pool.h"

#include <fmt/format.h>

void query_pool::setup(vk::Device device, vk::PhysicalDeviceFeatures enabled_features) {
  _logical_device_ = device;
  _enabled_features_ = enabled_features;

  if (!_enabled_features_.pipelineStatisticsQuery) {
    fmt::print("Pipeline statistics query not enabled/supported!\n");
  }

  _pipeline_stat_names_ = {
      "Input assembly vertex count        ",
      "Input assembly primitives count    ",
      "Vertex shader invocations          ",
      "Clipping stage primitives processed",
      "Clipping stage primitives output    ",
      "Fragment shader invocations        "
  };

  vk::QueryPoolCreateInfo query_pool_info = {};
  query_pool_info.queryType = vk::QueryType::ePipelineStatistics;
  query_pool_info.pipelineStatistics =
      vk::QueryPipelineStatisticFlagBits::eInputAssemblyVertices |
          vk::QueryPipelineStatisticFlagBits::eInputAssemblyPrimitives |
          vk::QueryPipelineStatisticFlagBits::eVertexShaderInvocations |
          vk::QueryPipelineStatisticFlagBits::eClippingInvocations |
          vk::QueryPipelineStatisticFlagBits::eClippingPrimitives |
          vk::QueryPipelineStatisticFlagBits::eFragmentShaderInvocations;
  query_pool_info.queryCount = 6;
  _query_pool_ = device.createQueryPoolUnique(query_pool_info);
}

void query_pool::destroy() {
  _query_pool_.reset();
}

void query_pool::begin(vk::CommandBuffer command_buffer) const {
  command_buffer.beginQuery(*_query_pool_, 0, {});
}

void query_pool::end(vk::CommandBuffer command_buffer) const {
  command_buffer.endQuery(*_query_pool_, 0);
}

void query_pool::reset(vk::CommandBuffer command_buffer) const {
  command_buffer.resetQueryPool(*_query_pool_, 0, static_cast<std::uint32_t>(_pipeline_stat_names_.size()));
}

void query_pool::update_query_results() {
  const auto count = static_cast<std::uint32_t>(_pipeline_stat_names_.size());
  auto result = _logical_device_.getQueryPoolResults<std::uint64_t>(*_query_pool_,
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

bool query_pool::is_supported(vk::PhysicalDeviceFeatures features) {
  return features.pipelineStatisticsQuery;
}
