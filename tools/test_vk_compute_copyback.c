// GPU regression for stock-driver compute staging: LDG input, STG output, coherent CPU copy-back.
// The empty-map control models the preceding no-import path; the staged-map case must preserve the payload.
#include <vulkan/vulkan.h>
#include "../runtime/nvn/maxwell/shader.h"
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
    VkBuffer buffer;
    VkBufferCreateInfo bc = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = 4096,
                             .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT};
    CHECK(vkCreateBuffer(g_vk.dev, &bc, NULL, &buffer));
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(g_vk.dev, buffer, &req);
    VkMemoryAllocateInfo ma = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = req.size,
                               .memoryTypeIndex = memory_type(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};
    VkDeviceMemory bm;
    CHECK(vkAllocateMemory(g_vk.dev, &ma, NULL, &bm));
    CHECK(vkBindBufferMemory(g_vk.dev, buffer, bm, 0));
    uint8_t *mapped;
    CHECK(vkMapMemory(g_vk.dev, bm, 0, VK_WHOLE_SIZE, 0, (void **)&mapped));
    memset(mapped,0,4096);
    // Native heap is all zero; the relocated storage range contains the payload.
    uint32_t *map=(uint32_t *)(mapped+256);
    map[0]=16;map[1]=16;map[2]=0;map[GMAP_MAX*4]=1;
    ((uint32_t *)(mapped+1024))[0]=0x1234abcdu;
    VkDescriptorSetLayoutBinding bindings[4]={
        {17,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,NULL},
        {18,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,NULL},
        {20,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,NULL},
        {21,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,NULL}};
    VkDescriptorSetLayoutCreateInfo lc={.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,.bindingCount=4,.pBindings=bindings};
    VkDescriptorSetLayout layout;CHECK(vkCreateDescriptorSetLayout(g_vk.dev,&lc,NULL,&layout));
    VkDescriptorPoolSize sizes[2]={{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,3}};
    VkDescriptorPoolCreateInfo pc={.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,.maxSets=1,.poolSizeCount=2,.pPoolSizes=sizes};
    VkDescriptorPool pool;CHECK(vkCreateDescriptorPool(g_vk.dev,&pc,NULL,&pool));
    VkDescriptorSetAllocateInfo sa={.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,.descriptorPool=pool,.descriptorSetCount=1,.pSetLayouts=&layout};
    VkDescriptorSet set;CHECK(vkAllocateDescriptorSets(g_vk.dev,&sa,&set));
    VkDescriptorBufferInfo bi[4]={{buffer,0,64},{buffer,2048,16},{buffer,256,(GMAP_MAX+1)*16},{buffer,1024,16}};
    VkWriteDescriptorSet wr[4];
    for(int i=0;i<4;i++)wr[i]=(VkWriteDescriptorSet){.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=set,.dstBinding=bindings[i].binding,
        .descriptorCount=1,.descriptorType=bindings[i].descriptorType,.pBufferInfo=&bi[i]};
    vkUpdateDescriptorSets(g_vk.dev,4,wr,0,NULL);
    uint64_t code[16]={0};
    uint64_t *ins=code; // compute has no graphics header
    ins[1]=0x0100000000070000ull|(0x100010ull<<20); // MOV R0, address low
    ins[2]=0x0100000000070001ull; // MOV R1, address high
    ins[3]=0xeed0000000000000ull|(4ull<<48)|(1ull<<45)|(7ull<<16)|2ull; // LDG.E R2,[R0]
    ins[5]=0xeed8000000000000ull|(4ull<<48)|(1ull<<45)|(4ull<<20)|(7ull<<16)|2ull; // STG.E [R0+4],R2
    ins[6]=0xe30000000007000full;ins[7]=0xe2400fffff87000full;
    maxwell_gmem_base=0x100000;maxwell_gmem_size=0x10000;
    ShaderKey key={.glsl=1};
    ShaderInfo *si=maxwell_translate((const uint8_t *)code,sizeof code,STAGE_COMPUTE,&key);
    if(!si->msl){fprintf(stderr,"translation: %s\n",si->error);return 2;}
    // Observe the registers after the production translator's instruction sequence.
    char *source=calloc(1,strlen(si->msl)+256);
    const char *entry=strstr(si->msl,"void main()");
    const char *end=strrchr(si->msl,'}');
    memcpy(source,si->msl,entry-si->msl);
    strcat(source,"layout(std430,binding=18) buffer Result{uvec2 value;} output_;\n");
    strncat(source,entry,end-entry);
    strcat(source,"output_.value=uvec2(r2,0u);\n}\n");
    shaderc_compiler_t compiler=shaderc_compiler_initialize();
    shaderc_compile_options_t options=shaderc_compile_options_initialize();
    shaderc_compile_options_set_target_env(options,shaderc_target_env_vulkan,shaderc_env_version_vulkan_1_3);
    shaderc_compilation_result_t result=shaderc_compile_into_spv(compiler,source,strlen(source),shaderc_compute_shader,"ldc.comp","main",options);
    if(shaderc_result_get_compilation_status(result)!=shaderc_compilation_status_success){fprintf(stderr,"%s\n",shaderc_result_get_error_message(result));return 2;}
    VkShaderModuleCreateInfo mc = {.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize=shaderc_result_get_length(result),.pCode=(const uint32_t *)shaderc_result_get_bytes(result)};
    VkShaderModule module;
    CHECK(vkCreateShaderModule(g_vk.dev,&mc,NULL,&module));
    VkPipelineLayoutCreateInfo plc = {.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,.setLayoutCount=1,.pSetLayouts=&layout};
    VkPipelineLayout pipeline_layout;
    CHECK(vkCreatePipelineLayout(g_vk.dev,&plc,NULL,&pipeline_layout));
    VkComputePipelineCreateInfo cp = {.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,.layout=pipeline_layout,
        .stage={.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_COMPUTE_BIT,.module=module,.pName="main"}};
    VkPipeline pipeline;
    CHECK(vkCreateComputePipelines(g_vk.dev,VK_NULL_HANDLE,1,&cp,NULL,&pipeline));
    VkCommandPoolCreateInfo cmdp = {.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT};
    VkCommandPool command_pool;
    CHECK(vkCreateCommandPool(g_vk.dev,&cmdp,NULL,&command_pool));
    VkCommandBufferAllocateInfo ca = {.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,.commandPool=command_pool,
        .level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,.commandBufferCount=1};
    VkCommandBuffer command;
    CHECK(vkAllocateCommandBuffers(g_vk.dev,&ca,&command));
    bool ok = true;
    for (int phase = 0; phase < 2; phase++) {
    map[GMAP_MAX*4] = phase ? 1 : 0;
    uint32_t guest[2] = {0x1234abcdu, 0};
    memcpy(mapped+1024, guest, sizeof guest);
    memset(mapped, 0, 64);
    memset(mapped+2048,0,16);
    CHECK(vkResetCommandBuffer(command,0));
    VkCommandBufferBeginInfo begin={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    CHECK(vkBeginCommandBuffer(command,&begin));
    vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline);
    vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline_layout,0,1,&set,0,NULL);
    vkCmdDispatch(command,1,1,1);
    VkMemoryBarrier host={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER,.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT,.dstAccessMask=VK_ACCESS_HOST_READ_BIT};
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,NULL,0,NULL);
    CHECK(vkEndCommandBuffer(command));
    VkSubmitInfo submit={.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,.commandBufferCount=1,.pCommandBuffers=&command};
    CHECK(vkQueueSubmit(g_vk.queue,1,&submit,VK_NULL_HANDLE));CHECK(vkQueueWaitIdle(g_vk.queue));
    uint32_t *out=(uint32_t *)(mapped+2048);
    memcpy(guest, mapped+1024, sizeof guest); // retire before restoring the bound bytes
    uint32_t want = phase ? 0x1234abcdu : 0;
    bool pass=out[0]==want && guest[1]==want && guest[0]==0x1234abcdu;
    ok &= pass;
    printf("%s compute LDG/STG %s: register %08x, copy-back %08x (want %08x)\n",pass?"PASS":"FAIL",phase?"staged storage":"empty-map control",out[0],guest[1],want);
    }
    vkDeviceWaitIdle(g_vk.dev);
    vkDestroyCommandPool(g_vk.dev,command_pool,NULL);vkDestroyPipeline(g_vk.dev,pipeline,NULL);
    vkDestroyPipelineLayout(g_vk.dev,pipeline_layout,NULL);vkDestroyShaderModule(g_vk.dev,module,NULL);
    vkDestroyDescriptorPool(g_vk.dev,pool,NULL);vkDestroyDescriptorSetLayout(g_vk.dev,layout,NULL);
    vkUnmapMemory(g_vk.dev,bm);vkDestroyBuffer(g_vk.dev,buffer,NULL);vkFreeMemory(g_vk.dev,bm,NULL);
    vkDestroyDevice(g_vk.dev,NULL);vkDestroyInstance(g_vk.inst,NULL);
    shaderc_result_release(result);shaderc_compile_options_release(options);shaderc_compiler_release(compiler);
    maxwell_free(si);free(source);
    return ok?0:1;
}

