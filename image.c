#define CL_TARGET_OPENCL_VERSION 200
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <CL/cl.h>
#include "vector.h"
#include "utils.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <time.h>

#define WIDTH 1080
#define HEIGHT 720

OpenclContext* init_opencl(Scene* scene, char* postproc_file){
    //iniciando opencl
    cl_int err;
    cl_platform_id plataforms;
    err = clGetPlatformIDs(1, &plataforms, NULL);
    if(err != CL_SUCCESS){
        printf("an error ocurred while finding available opencl plataforms");
        exit(1);
    }
    cl_device_id devices;
    err = clGetDeviceIDs(plataforms, CL_DEVICE_TYPE_GPU, 1, &devices, NULL);
    if(err != CL_SUCCESS){
        printf("an error ocurred while finding available opencl devices");
        exit(1);
    }
    cl_context context = clCreateContext(NULL, 1, &devices, NULL, NULL, &err);
    if(!context || err != CL_SUCCESS){
        printf("An error ocurred while creating the context\n");
        exit(1);
    }

    const char* render_kernel_source = load_strfile("render.txt");
    cl_program render_program = clCreateProgramWithSource(context, 1, &render_kernel_source, NULL, &err);
    if(err != CL_SUCCESS){
        printf("an error ocurred while creating the opencl program: %d\n", err);
        exit(1);
    }
    err = clBuildProgram(render_program, 1, &devices, NULL, NULL, NULL);
    if (err == CL_BUILD_PROGRAM_FAILURE) {
        //this block came from stackoveflow, will search for the link to credit when i can
        size_t log_size;
        clGetProgramBuildInfo(render_program, devices, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
    
        char *log = (char *) malloc(log_size);
    
        clGetProgramBuildInfo(render_program, devices, CL_PROGRAM_BUILD_LOG, log_size, log, NULL);
    
        printf("%s\n", log);
        exit(1);
    }else if(err != CL_SUCCESS){
        printf("an error ocurred while building the opencl program: %d\n", err);
        exit(1);
    }
    const char* post_processing_source = load_strfile(postproc_file);
    cl_program post_processing_program = clCreateProgramWithSource(context, 1, &post_processing_source, NULL, &err);
    if(err != CL_SUCCESS){
        printf("an error ocurred while creating the opencl program: %d\n", err);
        exit(1);
    }
    err = clBuildProgram(post_processing_program, 1, &devices, NULL, NULL, NULL);
    if (err == CL_BUILD_PROGRAM_FAILURE) {
        size_t log_size;
        clGetProgramBuildInfo(post_processing_program, devices, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
    
        char *log = (char *) malloc(log_size);
    
        clGetProgramBuildInfo(post_processing_program, devices, CL_PROGRAM_BUILD_LOG, log_size, log, NULL);
    
        printf("%s\n", log);
        exit(5);
    }else if(err != CL_SUCCESS){
        printf("an error ocurred while building the opencl program: %d\n", err);
        exit(1);
    }

    cl_command_queue queue = clCreateCommandQueueWithProperties(context, devices, NULL, NULL);
    
    const long screensize = WIDTH*HEIGHT;
    const size_t screensizebytes = screensize*sizeof(float)*3;
    cl_mem pixelcolors = clCreateBuffer(context, CL_MEM_READ_WRITE, screensizebytes*4, NULL, NULL);

    cl_mem camera = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(float)*3, NULL, NULL);
    cl_mem  plane = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(float)*12, NULL, NULL);
    cl_mem ALI = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(float)*3, NULL, NULL);
    cl_mem lightpos = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(float)*scene->num_lights*3, NULL, NULL);
    cl_mem lightdiffuse = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(float)*scene->num_lights*3, NULL, NULL);
    cl_mem lightspecular = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(float)*scene->num_lights*3, NULL, NULL);
    cl_mem objectpos = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(float)*scene->num_objects*3, NULL, NULL);
    cl_mem objectcolor = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(float)*scene->num_objects*3, NULL, NULL);
    cl_mem objectambient = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(float)*scene->num_objects*3, NULL, NULL);
    cl_mem objectdiffuse = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(float)*scene->num_objects*3, NULL, NULL);
    cl_mem objectspecular = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(float)*scene->num_objects*3, NULL, NULL);
    cl_mem objectreflectivity = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(float)*scene->num_objects*3, NULL, NULL);
    cl_mem objectalbedo = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(float)*scene->num_objects, NULL, NULL);
    cl_mem objectradius = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(float)*scene->num_objects, NULL, NULL);

    flattenedScene* fscene = flattenScene(scene);
    clEnqueueWriteBuffer(queue, camera, CL_TRUE, 0, sizeof(float)*3, fscene->camera, 0, NULL, NULL);
    clEnqueueWriteBuffer(queue, plane, CL_TRUE, 0, sizeof(float)*12, fscene->plane, 0, NULL, NULL);
    clEnqueueWriteBuffer(queue, ALI, CL_TRUE, 0, sizeof(float)*3, fscene->ALI, 0, NULL, NULL);
    clEnqueueWriteBuffer(queue, lightpos, CL_TRUE, 0, sizeof(float)*scene->num_lights*3, fscene->lightpos, 0, NULL, NULL);
    clEnqueueWriteBuffer(queue, lightdiffuse, CL_TRUE, 0, sizeof(float)*scene->num_lights*3, fscene->lightdiffuse, 0, NULL, NULL);
    clEnqueueWriteBuffer(queue, lightspecular, CL_TRUE, 0, sizeof(float)*scene->num_lights*3, fscene->lightspecular, 0, NULL, NULL);
    clEnqueueWriteBuffer(queue, objectpos, CL_TRUE, 0, sizeof(float)*scene->num_objects*3, fscene->objectpos, 0, NULL, NULL);
    clEnqueueWriteBuffer(queue, objectcolor, CL_TRUE, 0, sizeof(float)*scene->num_objects*3, fscene->objectcolor, 0, NULL, NULL);
    clEnqueueWriteBuffer(queue, objectambient, CL_TRUE, 0, sizeof(float)*scene->num_objects*3, fscene->objectambient, 0, NULL, NULL);
    clEnqueueWriteBuffer(queue, objectdiffuse, CL_TRUE, 0, sizeof(float)*scene->num_objects*3, fscene->objectdiffuse, 0, NULL, NULL);
    clEnqueueWriteBuffer(queue, objectspecular, CL_TRUE, 0, sizeof(float)*scene->num_objects*3, fscene->objectspecular, 0, NULL, NULL);
    clEnqueueWriteBuffer(queue, objectreflectivity, CL_TRUE, 0, sizeof(float)*scene->num_objects*3, fscene->objectreflectivity, 0, NULL, NULL);
    clEnqueueWriteBuffer(queue, objectalbedo, CL_TRUE, 0, sizeof(float)*scene->num_objects, fscene->objectalbedo, 0, NULL, NULL);
    clEnqueueWriteBuffer(queue, objectradius, CL_TRUE, 0, sizeof(float)*scene->num_objects, fscene->objectradius, 0, NULL, NULL);

    cl_kernel render_kernel = clCreateKernel(render_program, "render", NULL);
    cl_kernel post_processing_kernel = clCreateKernel(post_processing_program, "postprocess", NULL);

    err = clSetKernelArg(render_kernel, 0, sizeof(cl_mem), &pixelcolors);
    if (err != CL_SUCCESS) {
        printf("Error setting kernel arg: %d\n", err);
        exit(1);
    }
    err = clSetKernelArg(render_kernel, 1, sizeof(long), &screensize);
    if (err != CL_SUCCESS) {
        printf("Error setting kernel arg: %d\n", err);
        exit(1);
    }
    err = clSetKernelArg(render_kernel, 2, sizeof(cl_mem), &camera);
    if (err != CL_SUCCESS) {
        printf("Error setting kernel arg: %d\n", err);
        exit(1);
    }
    err = clSetKernelArg(render_kernel, 3, sizeof(cl_mem), &plane);
    if (err != CL_SUCCESS) {
        printf("Error setting kernel arg: %d\n", err);
        exit(1);
    }
    err = clSetKernelArg(render_kernel, 4, sizeof(cl_mem), &ALI);
    if (err != CL_SUCCESS) {
        printf("Error setting kernel arg: %d\n", err);
        exit(1);
    }
    err = clSetKernelArg(render_kernel, 5, sizeof(cl_mem), &lightpos);
    if (err != CL_SUCCESS) {
        printf("Error setting kernel arg: %d\n", err);
        exit(1);
    }
    err = clSetKernelArg(render_kernel, 6, sizeof(cl_mem), &lightdiffuse);
    if (err != CL_SUCCESS) {
        printf("Error setting kernel arg: %d\n", err);
        exit(1);
    }
    err = clSetKernelArg(render_kernel, 7, sizeof(cl_mem), &lightspecular);
    if (err != CL_SUCCESS) {
        printf("Error setting kernel arg: %d\n", err);
        exit(1);
    }
    err = clSetKernelArg(render_kernel, 8, sizeof(cl_mem), &objectpos);
    if (err != CL_SUCCESS) {
        printf("Error setting kernel arg: %d\n", err);
        exit(1);
    }
    err = clSetKernelArg(render_kernel, 9, sizeof(cl_mem), &objectcolor);
    if (err != CL_SUCCESS) {
        printf("Error setting kernel arg: %d\n", err);
        exit(1);
    }
    err = clSetKernelArg(render_kernel, 10, sizeof(cl_mem), &objectambient);
    if (err != CL_SUCCESS) {
        printf("Error setting kernel arg: %d\n", err);
        exit(1);
    }
    err = clSetKernelArg(render_kernel, 11, sizeof(cl_mem), &objectdiffuse);
    if (err != CL_SUCCESS) {
        printf("Error setting kernel arg: %d\n", err);
        exit(1);
    }
    err = clSetKernelArg(render_kernel, 12, sizeof(cl_mem), &objectspecular);
    if (err != CL_SUCCESS) {
        printf("Error setting kernel arg: %d\n", err);
        exit(1);
    }
    err = clSetKernelArg(render_kernel, 13, sizeof(cl_mem), &objectreflectivity);
    if (err != CL_SUCCESS) {
        printf("Error setting kernel arg: %d\n", err);
        exit(1);
    }
    err = clSetKernelArg(render_kernel, 14, sizeof(cl_mem), &objectalbedo);
    if (err != CL_SUCCESS) {
        printf("Error setting kernel arg: %d\n", err);
        exit(1);
    }
    err = clSetKernelArg(render_kernel, 15, sizeof(cl_mem), &objectradius);
    if (err != CL_SUCCESS) {
        printf("Error setting kernel arg: %d\n", err);
        exit(1);
    }
    err = clSetKernelArg(render_kernel, 16, sizeof(int), &fscene->num_lights);
    if (err != CL_SUCCESS) {
        printf("Error setting kernel arg: %d\n", err);
        exit(1);
    }
    err = clSetKernelArg(render_kernel, 17, sizeof(int), &fscene->num_objects);
    if (err != CL_SUCCESS) {
        printf("Error setting kernel arg: %d\n", err);
        exit(1);
    }

    err = clSetKernelArg(post_processing_kernel, 0, sizeof(cl_mem), &pixelcolors);
    if (err != CL_SUCCESS) { 
        printf("Error setting kernel arg: %d\n", err);
        exit(1);
    }

    clReleaseCommandQueue(queue);

    return create_opencl_context(
        fscene,
        pixelcolors,
        camera,
        plane,
        ALI,
        lightpos,
        lightdiffuse,
        lightspecular,
        objectpos,
        objectcolor,
        objectambient,
        objectdiffuse,
        objectspecular,
        objectreflectivity,
        objectalbedo,
        objectradius,
        render_kernel,
        post_processing_kernel,
        render_program,
        post_processing_program,
        devices,
        context
    );
}

int main(int argc, char* argv[]){
    Scene* scene;

    FILE *file = fopen(argv[1], "r");
    if (file == NULL){
        printf("Could not open file\n");
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *json_str = malloc(file_size + 1);
    fread(json_str, 1, file_size, file);
    json_str[file_size] = '\0';
    scene = load_scene(json_str);

    fclose(file);

    if(scene == NULL){
        printf("Error: Could not load the scene\n");
        exit(1);
    }

    scene->plane->x1->x += scene->camera->x; //Manually doing this so that the users can
    scene->plane->x2->x += scene->camera->x; //change the camera position without distorting
    scene->plane->x3->x += scene->camera->x; //the view
    scene->plane->x4->x += scene->camera->x;
    scene->plane->x1->y += scene->camera->y;
    scene->plane->x2->y += scene->camera->y;
    scene->plane->x3->y += scene->camera->y;
    scene->plane->x4->y += scene->camera->y;
    scene->plane->x1->z += scene->camera->z+1;
    scene->plane->x2->z += scene->camera->z+1;
    scene->plane->x3->z += scene->camera->z+1;
    scene->plane->x4->z += scene->camera->z+1;

    cl_int err;

    OpenclContext *opencl_context = init_opencl(scene, argv[3]);
    cl_command_queue queue = clCreateCommandQueueWithProperties(opencl_context->context, opencl_context->devices, NULL, NULL);

    const long screensize = WIDTH*HEIGHT;
    const size_t screensizebytes = screensize*sizeof(float)*3;

    size_t globalsize = screensize*4;//times the amount of extra rays for the antialliasing
    size_t localsize = 128;
    float* pixels = (float*)malloc(screensizebytes*4);

    err = clEnqueueNDRangeKernel(queue, opencl_context->render_kernel, 1, NULL, &globalsize, &localsize, 0, NULL, NULL);
    if (err != CL_SUCCESS) {
        printf("Error executing queued command: %d\n", err);
        exit(1);
    }
    clFinish(queue);

    err = clEnqueueNDRangeKernel(queue, opencl_context->post_processing_kernel, 1, NULL, &globalsize, &localsize, 0, NULL, NULL);
    if (err != CL_SUCCESS) {
        printf("Error executing queued command: %d\n", err);
        exit(1);
    }
    clFinish(queue);

    err = clEnqueueReadBuffer(queue, opencl_context->pixelcolors, CL_TRUE, 0, screensizebytes*4, pixels, 0, NULL, NULL);
    if (err != CL_SUCCESS) {
        printf("Error reading queued buffer: %d\n", err);
        exit(1);
    }
    clFinish(queue);

    unsigned char *img = (unsigned char*)malloc(WIDTH * HEIGHT * 3);  // 3 for RGB channels
    
    for(int i = 0; i < screensize; i++){
        int y = (int)floor((float)i/1080) % 720;
        int x = i % 1080;

        int image_index = (y * WIDTH + x)*3;
        
        for(int z = 0; z < 4; z++){
            int pos = (i*3)+777600*z*3;
            img[image_index] += (uint8_t)(pixels[pos]*255);
            img[image_index+1] += (uint8_t)(pixels[pos+1]*255);
            img[image_index+2] += (uint8_t)(pixels[pos+2]*255);
        }
    }

    int success = stbi_write_jpg(strcat(argv[2], ".jpg"), WIDTH, HEIGHT, 3, img, 100);

    if (!success) {
        printf("Failed to write image\n");
        free(img);
        exit(1);
    }
    
    printf("Image created and saved as %s\n", argv[2]);

    free(img);
    destroy_openclcontext(opencl_context);
    clReleaseCommandQueue(queue);

    return 0;
}
