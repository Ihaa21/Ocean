
#include "ocean_demo.h"

inline void DemoAllocGlobals(linear_arena* Arena)
{
    // IMPORTANT: These are always the top of the program memory
    DemoState = PushStruct(Arena, demo_state);
    RenderState = PushStruct(Arena, render_state);
}

DEMO_INIT(Init)
{
    // NOTE: Init Memory
    {
        linear_arena Arena = LinearArenaCreate(ProgramMemory, ProgramMemorySize);
        DemoAllocGlobals(&Arena);
        *DemoState = {};
        *RenderState = {};
        DemoState->Arena = Arena;
        DemoState->TempArena = LinearSubArena(&DemoState->Arena, MegaBytes(10));
    }

    // NOTE: Init Vulkan
    {
        VkInit(VulkanLib, hInstance, WindowHandle, &DemoState->Arena, &DemoState->TempArena, WindowWidth, WindowHeight, MegaBytes(100));
        
        // NOTE: Init descriptor pool
        {
            VkDescriptorPoolSize Pools[5] = {};
            Pools[0].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            Pools[0].descriptorCount = 1000;
            Pools[1].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            Pools[1].descriptorCount = 1000;
            Pools[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            Pools[2].descriptorCount = 1000;
            Pools[3].type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
            Pools[3].descriptorCount = 1000;
            Pools[4].type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
            Pools[4].descriptorCount = 1000;
            
            VkDescriptorPoolCreateInfo CreateInfo = {};
            CreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            CreateInfo.maxSets = 1000;
            CreateInfo.poolSizeCount = ArrayCount(Pools);
            CreateInfo.pPoolSizes = Pools;
            VkCheckResult(vkCreateDescriptorPool(RenderState->Device, &CreateInfo, 0, &RenderState->DescriptorPool));
        }
    }
    
    // NOTE: Create samplers
    {
        VkSamplerCreateInfo CreateInfo = {};
        CreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        CreateInfo.magFilter = VK_FILTER_NEAREST;
        CreateInfo.minFilter = VK_FILTER_NEAREST;
        CreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        CreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        CreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        CreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        CreateInfo.mipLodBias = 0.0f;
        CreateInfo.anisotropyEnable = VK_FALSE;
        CreateInfo.compareEnable = VK_FALSE;
        CreateInfo.minLod = 0.0f;
        CreateInfo.maxLod = 0.0f;
        CreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        CreateInfo.unnormalizedCoordinates = VK_FALSE;
        VkCheckResult(vkCreateSampler(RenderState->Device, &CreateInfo, 0, &DemoState->PointSampler));

        CreateInfo.magFilter = VK_FILTER_LINEAR;
        CreateInfo.minFilter = VK_FILTER_LINEAR;
        CreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        VkCheckResult(vkCreateSampler(RenderState->Device, &CreateInfo, 0, &DemoState->LinearSampler));
    }
    
    DemoState->Camera = CameraFpsCreate(V3(0, 0, 0), V3(0, 0, 1), f32(RenderState->WindowWidth / RenderState->WindowHeight),
                                       0.001f, 1000.0f, 90.0f, 1.0f, 0.01f);
        
    // NOTE: Init render target entries
    DemoState->SwapChainEntry = RenderTargetSwapChainEntryCreate(RenderState->WindowWidth, RenderState->WindowHeight,
                                                               RenderState->SwapChainFormat);
    DemoState->DepthEntry = RenderTargetEntryCreate(&RenderState->GpuArena, RenderState->WindowWidth, RenderState->WindowHeight,
                                                  VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                                  VK_IMAGE_ASPECT_DEPTH_BIT);

    // NOTE: Geometry RT
    {
        render_target_builder Builder = RenderTargetBuilderBegin(&DemoState->Arena, &DemoState->TempArena, RenderState->WindowWidth,
                                                                 RenderState->WindowHeight);
        RenderTargetAddTarget(&Builder, &DemoState->SwapChainEntry, VkClearColorCreate(0, 0, 0, 1));
        RenderTargetAddTarget(&Builder, &DemoState->DepthEntry, VkClearDepthStencilCreate(0, 0));
                            
        vk_render_pass_builder RpBuilder = VkRenderPassBuilderBegin(&DemoState->TempArena);

        u32 ColorId = VkRenderPassAttachmentAdd(&RpBuilder, DemoState->SwapChainEntry.Format, VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED,
                                                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        u32 DepthId = VkRenderPassAttachmentAdd(&RpBuilder, DemoState->DepthEntry.Format, VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED,
                                                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

        VkRenderPassSubPassBegin(&RpBuilder, VK_PIPELINE_BIND_POINT_GRAPHICS);
        VkRenderPassColorRefAdd(&RpBuilder, ColorId, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderPassDepthRefAdd(&RpBuilder, DepthId, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        VkRenderPassSubPassEnd(&RpBuilder);

        DemoState->GeometryRenderTarget = RenderTargetBuilderEnd(&Builder, VkRenderPassBuilderEnd(&RpBuilder, RenderState->Device));
    }
        
    // NOTE: Create ocean data
    {
        // NOTE: Create descriptor set layout
        {
            vk_descriptor_layout_builder Builder = VkDescriptorLayoutBegin(&DemoState->OceanDescLayout);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
            VkDescriptorLayoutEnd(RenderState->Device, &Builder);
        }

        // NOTE: Create Pipeline
        {
            vk_pipeline_builder Builder = VkPipelineBuilderBegin(&DemoState->TempArena);

            // NOTE: Shaders
            VkPipelineVertexShaderAdd(&Builder, "ocean_vert.spv", "main");
            VkPipelineFragmentShaderAdd(&Builder, "ocean_frag.spv", "main");
                
            // NOTE: Specify input vertex data format
            VkPipelineVertexBindingBegin(&Builder);
            VkPipelineVertexAttributeAdd(&Builder, VK_FORMAT_R32G32B32_SFLOAT, sizeof(v3));
            VkPipelineVertexBindingEnd(&Builder);

            VkPipelineDepthStateAdd(&Builder, VK_TRUE, VK_TRUE, VK_COMPARE_OP_GREATER);
                
            // NOTE: Set the blending state
            VkPipelineColorAttachmentAdd(&Builder, VK_FALSE, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO,
                                         VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO);

            DemoState->OceanPipeline = VkPipelineBuilderEnd(&Builder, RenderState->Device, &RenderState->PipelineManager,
                                                             DemoState->GeometryRenderTarget.RenderPass, 0,
                                                             &DemoState->OceanDescLayout, 1);
        }

        // NOTE: Create resources
        DemoState->OceanInputBuffer = VkBufferCreate(RenderState->Device, &RenderState->HostArena,
                                                      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                      sizeof(ocean_input));

        // NOTE: Create descriptor set
        {
            DemoState->OceanDescriptor = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, DemoState->OceanDescLayout);
            VkDescriptorBufferWrite(&RenderState->DescriptorManager, DemoState->OceanDescriptor, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                    DemoState->OceanInputBuffer);
            VkDescriptorManagerFlush(RenderState->Device, &RenderState->DescriptorManager);
        }
    }

    // NOTE: Upload assets
    VkCommandsBegin(RenderState->Device, RenderState->Commands);
    {
        // NOTE: Push ocean vertices/indices
        {
            u32 Width = 100;
            u32 Height = 100;
            DemoState->OceanVertices = VkBufferCreate(RenderState->Device, &RenderState->GpuArena,
                                                       VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                       Width*Height*sizeof(v3));
            v3* Vertices = VkTransferPushBufferWriteArray(&RenderState->TransferManager, DemoState->OceanVertices, v3, Width*Height, 1,
                                                          BarrierMask(VkAccessFlagBits(0), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT),
                                                          BarrierMask(VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT));
            for (u32 Y = 0; Y < Height; ++Y)
            {
                for (u32 X = 0; X < Width; ++X)
                {
                    Vertices[Y * Width + X] = V3(f32(X), f32(Y), 0.0f);
                }
            }

            DemoState->OceanNumIndices = 6*(Width-1)*(Height-1);
            DemoState->OceanIndices = VkBufferCreate(RenderState->Device, &RenderState->GpuArena,
                                                      VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                      DemoState->OceanNumIndices*sizeof(u32));
            u32* Indices = VkTransferPushBufferWriteArray(&RenderState->TransferManager, DemoState->OceanIndices, u32,
                                                          DemoState->OceanNumIndices, 1,
                                                          BarrierMask(VkAccessFlagBits(0), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT),
                                                          BarrierMask(VK_ACCESS_INDEX_READ_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT));
            u32* CurrIndex = Indices;
            for (u32 Y = 0; Y < Height - 1; ++Y)
            {
                for (u32 X = 0; X < Width - 1; ++X)
                {
                    *CurrIndex++ = Y*Width + X;
                    *CurrIndex++ = Y*Width + (X + 1);
                    *CurrIndex++ = (Y + 1)*Width + (X + 1);
                    *CurrIndex++ = (Y + 1)*Width + (X + 1);
                    *CurrIndex++ = (Y + 1)*Width + X;
                    *CurrIndex++ = Y*Width + X;
                }
            }
        }
        
        VkTransferManagerFlush(&RenderState->TransferManager, RenderState->Device, RenderState->Commands.Buffer, &RenderState->BarrierManager);
    }
    VkCommandsSubmit(RenderState->GraphicsQueue, RenderState->Commands);
}

DEMO_CODE_RELOAD(CodeReload)
{
    linear_arena Arena = LinearArenaCreate(ProgramMemory, ProgramMemorySize);
    // IMPORTANT: We are relying on the memory being the same here since we have the same base ptr with the VirtualAlloc so we just need
    // to patch our global pointers here
    DemoAllocGlobals(&Arena);

    VkGetGlobalFunctionPointers(VulkanLib);
    VkGetInstanceFunctionPointers();
    VkGetDeviceFunctionPointers();
}

DEMO_MAIN_LOOP(MainLoop)
{
    u32 ImageIndex;
    VkCheckResult(vkAcquireNextImageKHR(RenderState->Device, RenderState->SwapChain,
                                        UINT64_MAX, RenderState->ImageAvailableSemaphore,
                                        VK_NULL_HANDLE, &ImageIndex));
    DemoState->SwapChainEntry.View = RenderState->SwapChainViews[ImageIndex];

    CameraUpdate(&DemoState->Camera, CurrInput, PrevInput);

    vk_commands Commands = RenderState->Commands;
    VkCommandsBegin(RenderState->Device, Commands);

    // NOTE: Update pipelines
    VkPipelineUpdateShaders(RenderState->Device, &RenderState->CpuArena, &RenderState->PipelineManager);

    RenderTargetUpdateEntries(&DemoState->Arena, &DemoState->GeometryRenderTarget);

    // NOTE: Upload uniforms
    {
        // NOTE: Upload camera
        {
            camera_input* Data = VkTransferPushBufferWriteStruct(&RenderState->TransferManager, DemoState->Camera.GpuBuffer, camera_input, 1,
                                                                 BarrierMask(VkAccessFlagBits(0), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT),
                                                                 BarrierMask(VK_ACCESS_UNIFORM_READ_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT));
            *Data = {};
            Data->CameraPos = DemoState->Camera.Pos;
        }

        {
            ocean_input* Data = VkTransferPushBufferWriteStruct(&RenderState->TransferManager, DemoState->OceanInputBuffer, ocean_input, 1,
                                                                BarrierMask(VkAccessFlagBits(0), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT),
                                                                BarrierMask(VK_ACCESS_UNIFORM_READ_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT));
            *Data = {};
            Data->VP = CameraGetVP(&DemoState->Camera);
            Data->Time = DemoState->TotalProgramTime;
        }
            
        VkTransferManagerFlush(&RenderState->TransferManager, RenderState->Device, RenderState->Commands.Buffer, &RenderState->BarrierManager);
    }

    RenderTargetRenderPassBegin(&DemoState->GeometryRenderTarget, Commands, RenderTargetRenderPass_SetViewPort | RenderTargetRenderPass_SetScissor);
        
    // NOTE: Draw Ocean
    {
        vkCmdBindPipeline(Commands.Buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, DemoState->OceanPipeline->Handle);
        vkCmdBindDescriptorSets(Commands.Buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, DemoState->OceanPipeline->Layout, 0,
                                1, &DemoState->OceanDescriptor, 0, 0);

        VkDeviceSize Offset = 0;
        vkCmdBindVertexBuffers(Commands.Buffer, 0, 1, &DemoState->OceanVertices, &Offset);
        vkCmdBindIndexBuffer(Commands.Buffer, DemoState->OceanIndices, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(Commands.Buffer, DemoState->OceanNumIndices, 1, 0, 0, 0);
    }
        
    RenderTargetRenderPassEnd(Commands);        
    VkCheckResult(vkEndCommandBuffer(Commands.Buffer));
                    
    // NOTE: Render to our window surface
    // NOTE: Tell queue where we render to surface to wait
    VkPipelineStageFlags WaitDstMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo SubmitInfo = {};
    SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    SubmitInfo.waitSemaphoreCount = 1;
    SubmitInfo.pWaitSemaphores = &RenderState->ImageAvailableSemaphore;
    SubmitInfo.pWaitDstStageMask = &WaitDstMask;
    SubmitInfo.commandBufferCount = 1;
    SubmitInfo.pCommandBuffers = &Commands.Buffer;
    SubmitInfo.signalSemaphoreCount = 1;
    SubmitInfo.pSignalSemaphores = &RenderState->FinishedRenderingSemaphore;
    VkCheckResult(vkQueueSubmit(RenderState->GraphicsQueue, 1, &SubmitInfo, Commands.Fence));
            
    VkPresentInfoKHR PresentInfo = {};
    PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    PresentInfo.waitSemaphoreCount = 1;
    PresentInfo.pWaitSemaphores = &RenderState->FinishedRenderingSemaphore;
    PresentInfo.swapchainCount = 1;
    PresentInfo.pSwapchains = &RenderState->SwapChain;
    PresentInfo.pImageIndices = &ImageIndex;
    VkResult Result = vkQueuePresentKHR(RenderState->PresentQueue, &PresentInfo);

    switch (Result)
    {
        case VK_SUCCESS:
        {
        } break;

        case VK_ERROR_OUT_OF_DATE_KHR:
        case VK_SUBOPTIMAL_KHR:
        {
            // NOTE: Window size changed
            InvalidCodePath;
        } break;

        default:
        {
            InvalidCodePath;
        } break;
    }

    DemoState->TotalProgramTime += FrameTime;
}
