#pragma once

#include "framework_vulkan\framework_vulkan.h"

struct ocean_input
{
    m4 VP;
    f32 Time;
};

struct demo_state
{
    linear_arena Arena;
    linear_arena TempArena;

    f32 TotalProgramTime;

    // NOTE: Samplers
    VkSampler PointSampler;
    VkSampler LinearSampler;
    
    camera Camera;

    // NOTE: Render Target Entries
    render_target_entry SwapChainEntry;
    render_target_entry DepthEntry;
    render_target GeometryRenderTarget;
    
    // NOTE: Ocean data
    u32 OceanNumIndices;
    VkBuffer OceanVertices;
    VkBuffer OceanIndices;

    VkBuffer OceanInputBuffer;
    VkDescriptorSet OceanDescriptor;
    VkDescriptorSetLayout OceanDescLayout;
    vk_pipeline* OceanPipeline;
};

global demo_state* DemoState;
