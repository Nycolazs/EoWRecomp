// Adreno regression: depth/stencil feedback copies preserve every depth texel.
// The empty-map control models the preceding no-import path; the staged-map case must preserve the payload.
#include <vulkan/vulkan.h>
#include <shaderc/shaderc.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static struct { VkInstance inst; VkPhysicalDevice phys; VkPhysicalDeviceProperties props;
                VkDevice dev; VkQueue queue; } g_vk;
#define CHECK(x) do { VkResult r = (x); if (r != VK_SUCCESS) { fprintf(stderr, "%s: %d\n", #x, r); exit(2); } } while (0)

static uint32_t memory_type(uint32_t bits, VkMemoryPropertyFlags flags) {
    VkPhysicalDeviceMemoryProperties p;
    vkGetPhysicalDeviceMemoryProperties(g_vk.phys, &p);
    for (uint32_t i = 0; i < p.memoryTypeCount; i++)
        if ((bits & (1u << i)) && (p.memoryTypes[i].propertyFlags & flags) == flags) return i;
    fprintf(stderr, "no compatible memory\n"); exit(2);
}

int main(void) {
    VkApplicationInfo app = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = VK_API_VERSION_1_3};
    VkInstanceCreateInfo ic = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &app};
    CHECK(vkCreateInstance(&ic, NULL, &g_vk.inst));
    uint32_t n = 0;
    CHECK(vkEnumeratePhysicalDevices(g_vk.inst, &n, NULL));
    if (!n) return 2;
    VkPhysicalDevice *devices = calloc(n, sizeof *devices);
    CHECK(vkEnumeratePhysicalDevices(g_vk.inst, &n, devices));
    g_vk.phys = devices[0]; free(devices);
    vkGetPhysicalDeviceProperties(g_vk.phys, &g_vk.props);
    printf("GPU: %s\n", g_vk.props.deviceName);
    float priority = 1;
    VkDeviceQueueCreateInfo qc = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueFamilyIndex = 0,
                                  .queueCount = 1, .pQueuePriorities = &priority};
    VkPhysicalDeviceFeatures features = {.shaderInt64 = VK_TRUE};
    VkDeviceCreateInfo dc = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .queueCreateInfoCount = 1, .pQueueCreateInfos = &qc,
                             .pEnabledFeatures = &features};
    CHECK(vkCreateDevice(g_vk.phys, &dc, NULL, &g_vk.dev));
    vkGetDeviceQueue(g_vk.dev, 0, 0, &g_vk.queue);

    enum {W=1564,H=880};
    VkDeviceSize bytes=(VkDeviceSize)W*H*4;
    VkBuffer buffer;
    VkBufferCreateInfo bc={.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,.size=bytes*5,.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_STORAGE_BUFFER_BIT};
    CHECK(vkCreateBuffer(g_vk.dev,&bc,NULL,&buffer));
    VkMemoryRequirements req;vkGetBufferMemoryRequirements(g_vk.dev,buffer,&req);
    VkMemoryAllocateInfo ma={.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,.allocationSize=req.size,
        .memoryTypeIndex=memory_type(req.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};
    VkDeviceMemory memory;CHECK(vkAllocateMemory(g_vk.dev,&ma,NULL,&memory));CHECK(vkBindBufferMemory(g_vk.dev,buffer,memory,0));
    float *mapped;CHECK(vkMapMemory(g_vk.dev,memory,0,VK_WHOLE_SIZE,0,(void**)&mapped));
    for(int y=0;y<H;y++)for(int x=0;x<W;x++)mapped[y*W+x]=0.25f+0.5f*(float)(x+y*W)/(float)(W*H);
    VkCommandPoolCreateInfo pc={.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    VkCommandPool pool;CHECK(vkCreateCommandPool(g_vk.dev,&pc,NULL,&pool));
    VkCommandBufferAllocateInfo ca={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,.commandPool=pool,.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,.commandBufferCount=1};
    VkCommandBuffer cb;CHECK(vkAllocateCommandBuffers(g_vk.dev,&ca,&cb));
    const char *source="#version 450\nlayout(local_size_x=64) in; layout(binding=0) uniform sampler2D depth_; layout(binding=1) uniform usampler2D stencil_; layout(std430,binding=2) buffer Output{uvec2 v[];} out_; void main(){uint i=gl_GlobalInvocationID.x; if(i>=1564u*880u)return;ivec2 p=ivec2(i%1564u,i/1564u);out_.v[i]=uvec2(floatBitsToUint(texelFetch(depth_,p,0).x),texelFetch(stencil_,p,0).x);}";
    shaderc_compiler_t compiler=shaderc_compiler_initialize();
    shaderc_compilation_result_t spv=shaderc_compile_into_spv(compiler,source,strlen(source),shaderc_compute_shader,"feedback.comp","main",NULL);
    if(shaderc_result_get_compilation_status(spv)!=shaderc_compilation_status_success){fprintf(stderr,"%s",shaderc_result_get_error_message(spv));return 2;}
    VkShaderModuleCreateInfo mc={.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,.codeSize=shaderc_result_get_length(spv),.pCode=(const uint32_t*)shaderc_result_get_bytes(spv)};
    VkShaderModule module;CHECK(vkCreateShaderModule(g_vk.dev,&mc,NULL,&module));
    VkDescriptorSetLayoutBinding bindings[3]={{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,NULL},
        {1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,NULL},{2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,NULL}};
    VkDescriptorSetLayoutCreateInfo lc={.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,.bindingCount=3,.pBindings=bindings};
    VkDescriptorSetLayout layout;CHECK(vkCreateDescriptorSetLayout(g_vk.dev,&lc,NULL,&layout));
    VkDescriptorPoolSize sizes[2]={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,2},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1}};
    VkDescriptorPoolCreateInfo dp={.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,.maxSets=1,.poolSizeCount=2,.pPoolSizes=sizes};
    VkDescriptorPool descriptors;CHECK(vkCreateDescriptorPool(g_vk.dev,&dp,NULL,&descriptors));
    VkDescriptorSetAllocateInfo sa={.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,.descriptorPool=descriptors,.descriptorSetCount=1,.pSetLayouts=&layout};
    VkDescriptorSet set;CHECK(vkAllocateDescriptorSets(g_vk.dev,&sa,&set));
    VkPipelineLayoutCreateInfo plc={.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,.setLayoutCount=1,.pSetLayouts=&layout};
    VkPipelineLayout pl;CHECK(vkCreatePipelineLayout(g_vk.dev,&plc,NULL,&pl));
    VkComputePipelineCreateInfo cp={.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,.layout=pl,
        .stage={.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_COMPUTE_BIT,.module=module,.pName="main"}};
    VkPipeline pipeline;CHECK(vkCreateComputePipelines(g_vk.dev,VK_NULL_HANDLE,1,&cp,NULL,&pipeline));
    VkSamplerCreateInfo sci={.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,.magFilter=VK_FILTER_NEAREST,.minFilter=VK_FILTER_NEAREST};
    VkSampler sampler;CHECK(vkCreateSampler(g_vk.dev,&sci,NULL,&sampler));
    bool ok=true;
    for(int phase=0;phase<4;phase++) {
        VkImage images[2];VkDeviceMemory mem[2];
        for(int i=0;i<2;i++) {
            VkImageCreateInfo ci={.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,.imageType=VK_IMAGE_TYPE_2D,.format=VK_FORMAT_D32_SFLOAT_S8_UINT,
                .extent={W,H,1},.mipLevels=1,.arrayLayers=1,.samples=VK_SAMPLE_COUNT_1_BIT,.tiling=VK_IMAGE_TILING_OPTIMAL,
                .usage=VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT};
            if(i==0 || phase>=2)ci.usage|=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            CHECK(vkCreateImage(g_vk.dev,&ci,NULL,&images[i]));
            vkGetImageMemoryRequirements(g_vk.dev,images[i],&req);
            ma.allocationSize=req.size;ma.memoryTypeIndex=memory_type(req.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            CHECK(vkAllocateMemory(g_vk.dev,&ma,NULL,&mem[i]));CHECK(vkBindImageMemory(g_vk.dev,images[i],mem[i],0));
        }
        VkImageView views[2];VkDescriptorImageInfo ii[2];VkWriteDescriptorSet writes[3];
        for(int i=0;i<2;i++) {
            VkImageViewCreateInfo vi={.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,.image=images[1],.viewType=VK_IMAGE_VIEW_TYPE_2D,.format=VK_FORMAT_D32_SFLOAT_S8_UINT,
                .subresourceRange={i?VK_IMAGE_ASPECT_STENCIL_BIT:VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,1}};
            CHECK(vkCreateImageView(g_vk.dev,&vi,NULL,&views[i]));
            ii[i]=(VkDescriptorImageInfo){sampler,views[i],VK_IMAGE_LAYOUT_GENERAL};
            writes[i]=(VkWriteDescriptorSet){.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=set,.dstBinding=(uint32_t)i,.descriptorCount=1,
                .descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,.pImageInfo=&ii[i]};
        }
        VkDescriptorBufferInfo outinfo={buffer,bytes*3,bytes*2};
        writes[2]=(VkWriteDescriptorSet){.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=set,.dstBinding=2,.descriptorCount=1,.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.pBufferInfo=&outinfo};
        vkUpdateDescriptorSets(g_vk.dev,3,writes,0,NULL);
        VkCommandBufferBeginInfo begin={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};CHECK(vkBeginCommandBuffer(cb,&begin));
        VkImageMemoryBarrier barriers[2];
        for(int i=0;i<2;i++)barriers[i]=(VkImageMemoryBarrier){.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT|VK_ACCESS_TRANSFER_READ_BIT,.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED,.newLayout=VK_IMAGE_LAYOUT_GENERAL,
            .srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.image=images[i],
            .subresourceRange={VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT,0,1,0,1}};
        vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,NULL,0,NULL,2,barriers);
        VkBufferImageCopy upload={.imageSubresource={VK_IMAGE_ASPECT_DEPTH_BIT,0,0,1},.imageExtent={W,H,1}};
        vkCmdCopyBufferToImage(cb,buffer,images[0],VK_IMAGE_LAYOUT_GENERAL,1,&upload);
        VkImageSubresourceRange range={VK_IMAGE_ASPECT_STENCIL_BIT,0,1,0,1};VkClearDepthStencilValue clear={0.0f,73};
        vkCmdClearDepthStencilImage(cb,images[0],VK_IMAGE_LAYOUT_GENERAL,&clear,1,&range);
        VkMemoryBarrier barrier={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER,.srcAccessMask=VK_ACCESS_MEMORY_WRITE_BIT,.dstAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT};
        vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,1,&barrier,0,NULL,0,NULL);
        VkImageCopy regions[2]={ {.srcSubresource={VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT,0,0,1},
            .dstSubresource={VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT,0,0,1},.extent={W,H,1}} };
        int ncopy=1;
        if(phase&1) {regions[0].srcSubresource.aspectMask=regions[0].dstSubresource.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT;
            regions[1]=regions[0];regions[1].srcSubresource.aspectMask=regions[1].dstSubresource.aspectMask=VK_IMAGE_ASPECT_STENCIL_BIT;ncopy=2;}
        vkCmdCopyImage(cb,images[0],VK_IMAGE_LAYOUT_GENERAL,images[1],VK_IMAGE_LAYOUT_GENERAL,ncopy,regions);
        vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,1,&barrier,0,NULL,0,NULL);
        vkCmdBindPipeline(cb,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline);
        vkCmdBindDescriptorSets(cb,VK_PIPELINE_BIND_POINT_COMPUTE,pl,0,1,&set,0,NULL);
        vkCmdDispatch(cb,(W*H+63)/64,1,1);
        upload.bufferOffset=bytes;
        vkCmdCopyImageToBuffer(cb,images[1],VK_IMAGE_LAYOUT_GENERAL,buffer,1,&upload);
        upload.bufferOffset=bytes*2;upload.imageSubresource.aspectMask=VK_IMAGE_ASPECT_STENCIL_BIT;
        vkCmdCopyImageToBuffer(cb,images[1],VK_IMAGE_LAYOUT_GENERAL,buffer,1,&upload);
        barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&barrier,0,NULL,0,NULL);
        CHECK(vkEndCommandBuffer(cb));
        VkSubmitInfo submit={.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,.commandBufferCount=1,.pCommandBuffers=&cb};
        CHECK(vkQueueSubmit(g_vk.queue,1,&submit,VK_NULL_HANDLE));CHECK(vkQueueWaitIdle(g_vk.queue));
        int bad=0,stencilbad=0,samplebad=0;float *out=mapped+W*H;unsigned char *stencil=(unsigned char*)(mapped+W*H*2);
        uint32_t *samples=(uint32_t*)(mapped+W*H*3);
        for(int i=0;i<W*H;i++){uint32_t bits;memcpy(&bits,mapped+i,4);bad+=mapped[i]!=out[i];stencilbad+=stencil[i]!=73;
            samplebad+=samples[i*2]!=bits || samples[i*2+1]!=73;}
        printf("%s depth/stencil copy attachment=%d split=%d: %d bad depth, %d bad stencil / %d texels\n",!bad&&!stencilbad?"PASS":"FAIL",phase>=2,phase&1,bad,stencilbad,W*H);
        printf("%s separate float-depth / uint-stencil sampling: %d mismatches\n",samplebad?"FAIL":"PASS",samplebad);
        if(bad||stencilbad||samplebad)ok=false;
        CHECK(vkResetCommandPool(g_vk.dev,pool,0));
        for(int i=0;i<2;i++){vkDestroyImageView(g_vk.dev,views[i],NULL);vkDestroyImage(g_vk.dev,images[i],NULL);vkFreeMemory(g_vk.dev,mem[i],NULL);}
    }
    vkDestroySampler(g_vk.dev,sampler,NULL);vkDestroyPipeline(g_vk.dev,pipeline,NULL);vkDestroyPipelineLayout(g_vk.dev,pl,NULL);
    vkDestroyDescriptorPool(g_vk.dev,descriptors,NULL);vkDestroyDescriptorSetLayout(g_vk.dev,layout,NULL);vkDestroyShaderModule(g_vk.dev,module,NULL);
    shaderc_result_release(spv);shaderc_compiler_release(compiler);
    vkDestroyCommandPool(g_vk.dev,pool,NULL);vkUnmapMemory(g_vk.dev,memory);vkDestroyBuffer(g_vk.dev,buffer,NULL);vkFreeMemory(g_vk.dev,memory,NULL);
    vkDestroyDevice(g_vk.dev,NULL);vkDestroyInstance(g_vk.inst,NULL);return ok?0:1;
}
