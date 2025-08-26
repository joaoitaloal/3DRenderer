#define CL_TARGET_OPENCL_VERSION 200
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <CL/cl.h>
#include "vector.h"
#include "utils.h"
//#include <time.h>

#define WIDTH 1080
#define HEIGHT 720

//implementation with some adjusts for the collision checking function
float quadraticFormula(float a, float b, float c){
    float delta = b*b - 4*a*c;
    if(delta < 0) return delta;
    delta = sqrtf(delta);

    float t1 = (-b+delta)/(2*a);
    float t2 = (-b-delta)/(2*a);

    if(fmin(t1, t2) < 0) return fmax(t1, t2);
    else return fmin(t1,t2);
}

//Se não houver colisão retorna uma estrutura com colObject vazio
Collision* checkRayCollisions(vector3D * dir, vector3D * origin, ObjectList * objects){
    float t = INFINITY;
    Collision* collision = create_collision();
    
    ObjectList* index = objects;
    while(index->sphere != NULL){
        vector3D * oc = subtractVectors(index->sphere->center, origin);
        float a = dotProduct(dir, dir);
        float b = 2*dotProduct(oc, dir);
        float c = dotProduct(oc, oc)-index->sphere->radius*index->sphere->radius;
        free(oc);

        float temp = quadraticFormula(a, b, c);
        /*
            there is a bug in the reflexion that ocurs when two objects are pretty close to each other,
            it is originated from this line bellow, it ignores collisions too close to the origin, something
            that makes sense for rays originated from the camera but not for rays originated from other objects.
            The problem is that changing this to temp > 0 causes a lot of dots and artifacts to appear
            dont really understand why but need to take a look at this. 
            There is the chance that it is a perspective thing too, i need to test it.
        */
        if(temp >= 1 && temp < t){ 
            t = temp;
            vector3D* scale = scaleVector(dir, t);
            vector3D* sumn = addVectors(origin, scale);
            update_collision(collision, *sumn, index->sphere);
            free(scale);
            free(sumn);
        }
        
        index = index->next;
    }

    return collision;
}

float checkSingleObjectCollisionDistance(vector3D * dir, vector3D * origin, Sphere* sphere){
        vector3D * oc = subtractVectors(sphere->center, origin);
        float a = dotProduct(dir, dir);
        float b = 2*dotProduct(oc, dir);
        float c = dotProduct(oc, oc)-sphere->radius*sphere->radius;
        free(oc);

        return quadraticFormula(a, b, c);
}

int isInShadow(Collision* col, Light* light, ObjectList* objects){
    ObjectList* index = objects;
    while(index->sphere != NULL){
        if(index->sphere == col->colObject){
            index = index->next;
            continue;
        }

        vector3D* sub = subtractVectors(&col->colPoint, light->position);
        float t = checkSingleObjectCollisionDistance(sub, &col->colPoint, index->sphere);
        free(sub);

        if(0 < t && t < 1){
            return 1;
        }

        index = index->next;
    }
    return 0;
}

Color* checkCollisionColor(Collision* col, Scene* scene){
    Color * drawn_color = create_color(0, 0, 0);

    vector3D* sub = subtractVectors(col->colObject->center, &col->colPoint);
    vector3D* normalized = normalizeVector(sub);
    free(sub);

    vector3D* normCam = normalizeVector(scene->camera);
    vector3D* view = subtractVectors(&col->colPoint , normCam);
    free(normCam);

    LightList* index = scene->lights;
    while(index->light != NULL){
        if(isInShadow(col, index->light, scene->objects)){
            index = index->next;
            continue;
        }
        vector3D* sub = subtractVectors(&col->colPoint, index->light->position);
        vector3D* L = normalizeVector(sub);
        free(sub);

        float dot = dotProduct(L, normalized);

        vector3D* scaled = scaleVector(normalized, 2*dot);
        vector3D* reflectance = subtractVectors(L, scaled);
        free(scaled);

        float dot2 = dotProduct(reflectance, view);
        free(L);
        free(reflectance);

        if(dot < 0){
            index = index->next;
            continue;
        };

        Color* diffuseProd = productColors(index->light->diffuse, col->colObject->material->diffuse);
        Color* diffuseScale = scaleColor(diffuseProd, dot);
        Color* diffuseResult = clampColor(diffuseScale, 0, 1);
        addColors(drawn_color, diffuseResult);
        free(diffuseProd);
        free(diffuseScale);
        free(diffuseResult);

        dot2 = powf(dot2, col->colObject->material->albedo);
        
        Color* specProd = productColors(index->light->specular, col->colObject->material->specular);
        Color* specScale = scaleColor(specProd, dot2);
        Color* specResult = clampColor(specScale, 0, 1);
        addColors(drawn_color, specResult);
        free(specProd);
        free(specScale);
        free(specResult);

        index = index->next;
    }

    Color* ambProd = productColors(col->colObject->material->ambient, scene->ALI);
    addColors(drawn_color, ambProd);
    free(ambProd);

    Color* hueScale = scaleColor(col->colObject->color, 0.2);
    addColors(drawn_color, hueScale);
    free(hueScale);

    free(normalized);
    free(view);

    Color* result = clampColor(drawn_color, 0, 1);
    free(drawn_color);
    return result;
}

Color* colorFromRecursiveRayCast(vector3D * dir, vector3D * origin, Scene* scene, int depth){
    Color* drawn_color = create_color(0, 0, 0);
    if(depth <= 0) return drawn_color;

    Collision* collision = checkRayCollisions(dir, origin, scene->objects);
    if(collision->colObject == NULL){
        destroy_collision(collision);

        return drawn_color;
    }
    Color* colColor = checkCollisionColor(collision, scene);
    Color* reflecScaled = scaleColor(collision->colObject->material->reflectivity, depth/2); //3 de depth hard coded basicamente, dá pra melhorar depois
    Color* reflecColor = productColors(colColor, reflecScaled);
    free(colColor);
    free(reflecScaled);

    addColors(drawn_color, reflecColor);
    free(reflecColor);

    vector3D* inverse = scaleVector(dir, -1);
    vector3D* V = normalizeVector(inverse);
    vector3D* sub = subtractVectors(collision->colObject->center, &collision->colPoint);
    vector3D* N = normalizeVector(sub);
    float dot = dotProduct(V, N);
    vector3D* normalScaled = scaleVector(N, 2*dot);
    vector3D* reflectance = subtractVectors(V, normalScaled);
    free(inverse);
    free(V);
    free(sub);
    free(N);
    free(normalScaled);

    Color* reflected = colorFromRecursiveRayCast(reflectance, &collision->colPoint, scene, depth-1);
    addColors(drawn_color, reflected);
    destroy_collision(collision);
    free(reflected);
    free(reflectance);

    Color* final_color = clampColor(drawn_color, 0, 1);
    free(drawn_color);

    return final_color;
}

Color* antialliased(Scene* scene, int x, int y){
    Color* base_color = create_color(0, 0, 0);
    int index = 0;
    while(index < 4){
        float alpha;
        float beta;
        switch (index)
        {
        case 0:
            alpha = (float)x/WIDTH;
            beta = (float)y/HEIGHT;
            break;
        case 1:
            alpha = ((float)x + 0.5)/WIDTH;
            beta = (float)y/HEIGHT;
            break;
        case 2:
            alpha = (float)x/WIDTH;
            beta = ((float)y + 0.5)/HEIGHT;
            break;
        default:
            alpha = ((float)x + 0.5)/WIDTH;
            beta = ((float)y + 0.5)/HEIGHT;
            break;
        }

        vector3D* scalex1 = scaleVector(scene->plane->x1, 1.0-alpha);
        vector3D* scalex2 = scaleVector(scene->plane->x2, alpha);
        vector3D * t = addVectors(scalex1, scalex2);
        free(scalex1);
        free(scalex2);

        vector3D* scalex3 = scaleVector(scene->plane->x3, 1.0-alpha);
        vector3D* scalex4 = scaleVector(scene->plane->x4, alpha);
        vector3D * b = addVectors(scalex3, scalex4);
        free(scalex3);
        free(scalex4);

        vector3D* scalet = scaleVector(t, 1.0-beta);
        vector3D* scaleb = scaleVector(b, beta);
        vector3D * origin = addVectors(scalet, scaleb);
        free(scalet);
        free(scaleb);

        free(t);
        free(b);

        vector3D * direction = subtractVectors(scene->camera, origin);

        Color* renderedColor = colorFromRecursiveRayCast(direction, origin, scene, 3);
        addColors(base_color, renderedColor);
        free(origin);
        free(direction);
        free(renderedColor);

        index+=1;
    }

    Color* averageColor = scaleColor(base_color, (float)1/4);
    Color* finalColor = clampColor(averageColor, 0, 1);
    free(base_color);
    free(averageColor);

    return finalColor;
}

void renderScene(SDL_Renderer* renderer, Scene* scene, int antialliasing){
    for(int x = 0; x < WIDTH; x++){
        for(int y = 0; y < HEIGHT; y++){
            Color* renderedColor;

            if(!antialliasing){
                float alpha = (float)x/WIDTH;
                float beta = (float)y/HEIGHT;

                //(1-alpha)*x1 + alpha*x2
                vector3D* scalex1 = scaleVector(scene->plane->x1, 1.0-alpha);
                vector3D* scalex2 = scaleVector(scene->plane->x2, alpha);
                vector3D * t = addVectors(scalex1, scalex2);
                free(scalex1);
                free(scalex2);

                vector3D* scalex3 = scaleVector(scene->plane->x3, 1.0-alpha);
                vector3D* scalex4 = scaleVector(scene->plane->x4, alpha);
                vector3D * b = addVectors(scalex3, scalex4);
                free(scalex3);
                free(scalex4);

                vector3D* scalet = scaleVector(t, 1.0-beta);
                vector3D* scaleb = scaleVector(b, beta);
                vector3D* origin = addVectors(scalet, scaleb);
                free(scalet);
                free(scaleb);

                free(t);
                free(b);

                vector3D* direction = subtractVectors(scene->camera, origin);

                renderedColor = colorFromRecursiveRayCast(direction, origin, scene, 3);

                free(direction);
                free(origin);
            }
            else renderedColor = antialliased(scene, x, y);

            SDL_SetRenderDrawColor(renderer, (int)(renderedColor->red*255), (int)(renderedColor->green*255), (int)(renderedColor->blue*255), 255);

            free(renderedColor);

            SDL_RenderPoint(renderer, x, HEIGHT-y);
        }
    }
}

OpenclContext* init_opencl(SDL_Renderer* renderer, Scene* scene){
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
    const char* cam_kernel_source = load_strfile("cam_dir.txt");
    cl_program cam_program = clCreateProgramWithSource(context, 1, &cam_kernel_source, NULL, &err);
    if(err != CL_SUCCESS){
        printf("an error ocurred while creating the opencl program: %d\n", err);
        exit(1);
    }
    err = clBuildProgram(cam_program, 1, &devices, NULL, NULL, NULL);
    if (err == CL_BUILD_PROGRAM_FAILURE) {
        size_t log_size;
        clGetProgramBuildInfo(cam_program, devices, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
    
        char *log = (char *) malloc(log_size);
    
        clGetProgramBuildInfo(cam_program, devices, CL_PROGRAM_BUILD_LOG, log_size, log, NULL);
    
        printf("%s\n", log);
        exit(1);
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
    cl_kernel cam_kernel = clCreateKernel(cam_program, "cam_dir", NULL);

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

    err = clSetKernelArg(cam_kernel, 0, sizeof(cl_mem), &camera);
    if (err != CL_SUCCESS) {
        printf("Error setting kernel arg: %d\n", err);
        exit(1);
    }
    err = clSetKernelArg(cam_kernel, 1, sizeof(cl_mem), &plane);
    if (err != CL_SUCCESS) {
        printf("Error setting kernel arg: %d\n", err);
        exit(1);
    }
    int cam_xmov = 0; int cam_ymov = 0;
    err = clSetKernelArg(cam_kernel, 2, sizeof(int), &cam_xmov);
    if (err != CL_SUCCESS) {
        printf("Error setting kernel arg: %d\n", err);
        exit(1);
    }
    err = clSetKernelArg(cam_kernel, 3, sizeof(int), &cam_ymov);
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
        cam_kernel,
        render_program,
        cam_program,
        devices,
        context
    );
}

/*xyz: float[3]*
    x:1080, y: 720, z:4 <--- z seria o indice de raios extra pro antialliasing
    x = num mod 1080; y = floor(num/1080) mod 720; z = floor(num/777600);
    0,0,0: 0+1080*0+777600*0
    1,0,0: 1+1080*0+777600*0
    2,0,0: 2+1080*0+777600*0
    ...
    1079,0,0: 1079+1080*0+777600*0
    0,1,0: 0+1080*1+777600
    ...
    0,0,1: 0+1080*0+777600*1
    ...
    693,59,2: 693+1080*59+777600*2
    ...
    1079,719,3: 1079+1080*719+777600*3
*/
int main(int argc, char* argv[]){

    if(argc <= 1 || argc >= 7){
        printf("Unexpected number of arguments\n"
        "Check the readme to see the usage\n");
        exit(2);
    }

    if(!SDL_Init(SDL_INIT_VIDEO)){
        printf("Error starting SDL\n");
        exit(1);
    }

    SDL_Window * window = SDL_CreateWindow("render", WIDTH, HEIGHT, 0);
    SDL_Renderer * renderer = SDL_CreateRenderer(window, NULL);

    if(!window || !renderer){
        printf("Error starting SDL\n");
        exit(1);
    }

    Scene* scene;
    if(!strcmp(argv[1], "file")){
        FILE *file = fopen(argv[2], "r");
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
    }else{
        printf("Options:\n"
        "'file' then provide a valid json file name\n");
        exit(2);
    }
    if(scene == NULL){
        printf("Error: Could not load the scene\n");
        exit(1);
    }

    if(!strcmp(argv[3], "live")){
        int antialliasing = 0;
        if(argc > 4) antialliasing = 1;
        int running = 1;
        while (running) {
            SDL_Event e;
            while(SDL_PollEvent(&e)){
                if (e.type == SDL_EVENT_QUIT){
                    running = 0;
                }
            }
            
            renderScene(renderer, scene, antialliasing);

            SDL_RenderPresent(renderer);
        }
    }
    else if(!strcmp(argv[3], "image")){
        if(argv[4] == NULL){
            printf("Provide the desired filename as the second argument\n");
            exit(2);
        }
        SDL_Surface* surface = NULL;

        renderScene(renderer, scene, 1);
        surface = SDL_RenderReadPixels(renderer, NULL);
        SDL_RenderPresent(renderer);

        if(IMG_SaveJPG(surface, strcat(argv[4], ".jpeg"), 100)) printf("Image saved\n");
        else printf("Error while saving the image\n");

        SDL_Delay(1000); //delay só dar pra ver rapidinho a imagem antes de fechar
        SDL_DestroySurface(surface);
    }
    else if(!strcmp(argv[3], "image_opencl")){
        if(argv[4] == NULL){
            printf("Provide the desired filename as the second argument\n");
            exit(2);
        }
        cl_int err;
        uint8_t* texture_pixels;
        int pitch;
        SDL_Surface* surface = NULL;

        OpenclContext *opencl_context = init_opencl(renderer, scene);
        cl_command_queue queue = clCreateCommandQueueWithProperties(opencl_context->context, opencl_context->devices, NULL, NULL);
        SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);

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

        err = clEnqueueReadBuffer(queue, opencl_context->pixelcolors, CL_TRUE, 0, screensizebytes*4, pixels, 0, NULL, NULL);
        if (err != CL_SUCCESS) {
            printf("Error reading queued buffer: %d\n", err);
            exit(1);
        }
        clFinish(queue);

        SDL_LockTexture(texture, NULL, (void**)&texture_pixels, &pitch);
        
        for(int i = 0; i < screensize; i++){
            int y = (int)floor((float)i/1080) % 720;
            int x = i % 1080;

            uint32_t* texture_row = (uint32_t*)(texture_pixels + y * pitch);
            
            float red = 0; float green = 0; float blue = 0;
            for(int z = 0; z < 4; z++){
                int pos = (i*3)+777600*z*3;
                red+=pixels[pos];
                green+=pixels[pos+1];
                blue+=pixels[pos+2];
            }

            texture_row[x] = (255 << 24) | ((uint8_t)(red*255) << 16) | ((uint8_t)(green*255) << 8) | (uint8_t)(blue*255); // ARGB8888
        }
        SDL_UnlockTexture(texture);
        SDL_RenderTexture(renderer, texture, NULL, NULL);

        surface = SDL_RenderReadPixels(renderer, NULL);
        SDL_RenderPresent(renderer);

        if(IMG_SaveJPG(surface, strcat(argv[4], ".jpeg"), 100)) printf("Image saved\n");
        else printf("Error while saving the image\n");

        destroy_openclcontext(opencl_context);
        clReleaseCommandQueue(queue);
        SDL_DestroySurface(surface);
    }
    else if(!strcmp(argv[3], "opencl")){
        cl_int err;    
        uint8_t* texture_pixels;
        int pitch;

        OpenclContext *opencl_context = init_opencl(renderer, scene);
        flattenedScene *fscene = opencl_context->fscene;
        cl_command_queue queue = clCreateCommandQueueWithProperties(opencl_context->context, opencl_context->devices, NULL, NULL);
        SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);

        const long screensize = WIDTH*HEIGHT;
        const size_t screensizebytes = screensize*sizeof(float)*3;

        int cam_xmov = 0; int cam_ymov = 0; int cam_moved = 0;

        size_t globalsize = screensize*4;//times the amount of extra rays for the antialliasing
        size_t localsize = 128;
        float* pixels = (float*)malloc(screensizebytes*4);
        int running = 1;
        while (running) {
            SDL_Event e;
            while(SDL_PollEvent(&e)){
                if (e.type == SDL_EVENT_QUIT){
                    running = 0;
                }else if(e.type == SDL_EVENT_KEY_DOWN){
                    const char* key_pressed = SDL_GetKeyName(e.key.key);
                    if(!strcmp(key_pressed, "Escape")) running = 0;
                    else if(!strcmp(key_pressed, "Up")){
                        cam_ymov = 1; cam_moved = 1;
                    }else if(!strcmp(key_pressed, "Down")){
                        cam_ymov = -1; cam_moved = 1;
                    }else if(!strcmp(key_pressed, "Left")){
                        cam_xmov = 1; cam_moved = 1;
                    }else if(!strcmp(key_pressed, "Right")){
                        cam_xmov = -1; cam_moved = 1;
                    }else{
                        handle_keyboard_input(key_pressed, fscene);
                    }
                    if(cam_moved){
                        err = clSetKernelArg(opencl_context->post_processing_kernel, 2, sizeof(int), &cam_xmov);
                        if (err != CL_SUCCESS) {
                            printf("Error setting kernel arg: %d\n", err);
                            exit(1);
                        }
                        err = clSetKernelArg(opencl_context->post_processing_kernel, 3, sizeof(int), &cam_ymov);
                        if (err != CL_SUCCESS) {
                            printf("Error setting kernel arg: %d\n", err);
                            exit(1);
                        }
                    }
                }
            }

            err = clEnqueueWriteBuffer(queue, opencl_context->camera, CL_TRUE, 0, sizeof(float)*3, fscene->camera, 0, NULL, NULL);
            if (err != CL_SUCCESS) {
                printf("Error executing queued command: %d\n", err);
                exit(1);
            }
            err = clEnqueueWriteBuffer(queue, opencl_context->plane, CL_TRUE, 0, sizeof(float)*12, fscene->plane, 0, NULL, NULL);
            if (err != CL_SUCCESS) {
                printf("Error executing queued command: %d\n", err);
                exit(1);
            }
            
            if(cam_moved){
                const size_t four = 4;//ill probably look at this later and wonder what was i doing
                err = clEnqueueNDRangeKernel(queue, opencl_context->post_processing_kernel, 1, NULL, &four, &four, 0, NULL, NULL);
                if (err != CL_SUCCESS) {
                    printf("Error executing queued command: %d\n", err);
                    exit(1);
                }
                cam_xmov = 0; cam_ymov = 0; cam_moved = 0;
            }
            
            err = clEnqueueNDRangeKernel(queue, opencl_context->render_kernel, 1, NULL, &globalsize, &localsize, 0, NULL, NULL);
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
            err = clEnqueueReadBuffer(queue, opencl_context->camera, CL_TRUE, 0, sizeof(float)*3, fscene->camera, 0, NULL, NULL);
            if (err != CL_SUCCESS) {
                printf("Error reading queued buffer: %d\n", err);
                exit(1);
            }
            err = clEnqueueReadBuffer(queue, opencl_context->plane, CL_TRUE, 0, sizeof(float)*12, fscene->plane, 0, NULL, NULL);
            if (err != CL_SUCCESS) {
                printf("Error reading queued buffer: %d\n", err);
                exit(1);
            }

            clFinish(queue);

            SDL_LockTexture(texture, NULL, (void**)&texture_pixels, &pitch);
            
            SDL_RenderClear(renderer);
            for(int i = 0; i < screensize; i++){
                int y = (int)floor((float)i/1080) % 720;
                int x = i % 1080;

                uint32_t* texture_row = (uint32_t*)(texture_pixels + y * pitch);
                
                float red = 0; float green = 0; float blue = 0;
                for(int z = 0; z < 4; z++){
                    int pos = (i*3)+777600*z*3;
                    red+=pixels[pos];
                    green+=pixels[pos+1];
                    blue+=pixels[pos+2];
                }

                texture_row[x] = (255 << 24) | ((uint8_t)(red*255) << 16) | ((uint8_t)(green*255) << 8) | (uint8_t)(blue*255); // ARGB8888
            }
            SDL_UnlockTexture(texture);
            SDL_RenderTexture(renderer, texture, NULL, NULL);
            SDL_RenderPresent(renderer);
        }
        
        destroy_openclcontext(opencl_context);
        SDL_DestroyTexture(texture);
        clReleaseCommandQueue(queue);
    }

    destroy_scene(scene);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
