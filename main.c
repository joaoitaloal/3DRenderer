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
    vector3D* sub = subtractVectors(collision->colObject->center, &collision->colPoint); //tá certo isso?
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

void tick_physics(Scene* scene){

    addVectors2(scene->objects->sphere->center, 0, 0.3, -1);

    return;
}
/*xy: float[2]*
    x:1080, y: 720
    x = floor(num/1080); y = num mod 1080;
    0,0: 0+1080*0
    1,0: 1+1080*0
    2,0: 2+1080*0
    3,0: 3+1080*0
    ...
    1079,0: 1079+1080*0
    0,1: 0+1080*1
    ...
    3,5: 3+1080*5
    ...
    1079,719: 1079+1080*719
*/
int main(int argc, char* argv[]){
    // ./main {inputmode} {input} {runmode} ?{filename/antialliasing}

    if(argc <= 1 || argc >= 7){
        printf("Unexpected number of arguments\n"
        "Check the readme to see the usage\n");
        return 3;
    }

    if(!SDL_Init(SDL_INIT_VIDEO)){
        printf("Error starting SDL\n");
        return 2;
    }

    SDL_Window * window = SDL_CreateWindow("render", WIDTH, HEIGHT, 0);
    SDL_Renderer * renderer = SDL_CreateRenderer(window, NULL);

    if(!window || !renderer){
        printf("Error starting SDL\n");
        return 2;
    }
    Scene* scene;

    if(!strcmp(argv[1], "file")){
        FILE *file = fopen(argv[2], "r");
        if (file == NULL){
            printf("Could not open file\n");
            return 1;
        }

        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        char *json_str = malloc(file_size + 1);
        fread(json_str, 1, file_size, file);
        json_str[file_size] = '\0';
        scene = load_scene(json_str);

        fclose(file);
    }
    else if(!strcmp(argv[1], "string")){//how do i make this work from the command line?
        char* string = "";
        strcpy(string, argv[2]);
        string[strlen(string) - 1] = 0;
        string = string+1;

        scene = load_scene(string);
    }else{
        printf("Options:\n"
        "'file' then provide a valid json file name or\n");
        return 3;
    }
    if(scene == NULL){
        printf("Could not load scene\n");
        return 1;
    }

    SDL_Surface* surface = NULL;
    if(!strcmp(argv[3], "live")){
        int antialliasing = 0;
        if(argv[4] != NULL) antialliasing = 1;
        int running = 1;
        while (running) {
            SDL_Event e;
            while(SDL_PollEvent(&e)){
                if (e.type == SDL_EVENT_QUIT){
                    running = 0;
                }
            }
            
            tick_physics(scene);
            renderScene(renderer, scene, antialliasing);

            SDL_RenderPresent(renderer);
        }
    }
    else if(!strcmp(argv[3], "image")){
        if(argv[4] == NULL){
            printf("Provide the desired filename as the second argument\n");
            return 1;
        }

        renderScene(renderer, scene, 1);
        SDL_RenderPresent(renderer);

        surface = SDL_RenderReadPixels(renderer, NULL);
        if(IMG_SaveJPG(surface, strcat(argv[4], ".jpeg"), 100)) printf("Image saved\n");
        else printf("Error while saving the image\n");

        SDL_Delay(1000); //delay só dar pra ver rapidinho a imagem antes de fechar
    }
    else if(!strcmp(argv[3], "opencl")){
        //iniciando opencl
        cl_int err;
        cl_platform_id plataforms;
        cl_uint num_plataforms;
        err = clGetPlatformIDs(1, &plataforms, &num_plataforms);
        if(err != CL_SUCCESS){
            printf("an error ocurred while finding available opencl plataforms");
            exit(1);
        }
        cl_device_id devices;
        cl_uint num_devices;
        err = clGetDeviceIDs(plataforms, CL_DEVICE_TYPE_GPU, 1, &devices, &num_devices);
        if(err != CL_SUCCESS){
            printf("an error ocurred while finding available opencl devices");
            exit(1);
        }
        printf("%d %d\n", num_plataforms, num_devices);
        cl_context context = clCreateContext(NULL, 1, &devices, NULL, NULL, &err);
        if(!context || err != CL_SUCCESS){
            printf("An error ocurred while creating the context\n");
            exit(1);
        }

        const char* kernel_source = load_strfile("render.txt");
        /*const char* kernel_source = "__kernel void render(__global float pixelcolors[], const unsigned int screensize) {"
            "const float WIDTH = 1080;"
            "const float HEIGHT = 720;"
            "int i = get_global_id(0);"
            "float y = floor((float)(i/WIDTH));"
            "float x = fmod(i, WIDTH);"
            "if (i < screensize) {"
            "    pixelcolors[i*3] = x/WIDTH;"
            "    pixelcolors[(i*3)+1] = y/HEIGHT;"
            "    pixelcolors[(i*3)+2] = 0.5;"
            "}"
        "}";*/
        cl_program program = clCreateProgramWithSource(context, 1, &kernel_source, NULL, &err);
        if(err != CL_SUCCESS){
            printf("an error ocurred while creating the opencl program: %d\n", err);
            exit(1);
        }
        err = clBuildProgram(program, 1, &devices, NULL, NULL, NULL);
        if (err == CL_BUILD_PROGRAM_FAILURE) {
            // Determine the size of the log
            size_t log_size;
            clGetProgramBuildInfo(program, devices, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
        
            // Allocate memory for the log
            char *log = (char *) malloc(log_size);
        
            // Get the log
            clGetProgramBuildInfo(program, devices, CL_PROGRAM_BUILD_LOG, log_size, log, NULL);
        
            // Print the log
            printf("%s\n", log);
            exit(1);
        }

        cl_command_queue queue = clCreateCommandQueueWithProperties(context, devices, NULL, NULL);
        
        const long screensize = WIDTH*HEIGHT;
        const size_t screensizebytes = screensize*sizeof(float)*3;
        cl_mem pixelcolors = clCreateBuffer(context, CL_MEM_READ_WRITE, screensizebytes, NULL, NULL);
    
        cl_mem camera = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(float)*3, NULL, NULL);
        cl_mem plane = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(float)*12, NULL, NULL);
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

        cl_kernel kernel = clCreateKernel(program, "render", NULL);

        err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &pixelcolors);
        if (err != CL_SUCCESS) {
            printf("Error setting kernel arg: %d\n", err);
            exit(1);
        }
        err = clSetKernelArg(kernel, 1, sizeof(long), &screensize);
        if (err != CL_SUCCESS) {
            printf("Error setting kernel arg: %d\n", err);
            exit(1);
        }
        float cur_frame = 0;
        err = clSetKernelArg(kernel, 2, sizeof(long), &cur_frame);
        if (err != CL_SUCCESS) {
            printf("Error setting kernel arg: %d\n", err);
            exit(1);
        }
        err = clSetKernelArg(kernel, 3, sizeof(cl_mem), &camera);
        if (err != CL_SUCCESS) {
            printf("Error setting kernel arg: %d\n", err);
            exit(1);
        }
        err = clSetKernelArg(kernel, 4, sizeof(cl_mem), &plane);
        if (err != CL_SUCCESS) {
            printf("Error setting kernel arg: %d\n", err);
            exit(1);
        }
        err = clSetKernelArg(kernel, 5, sizeof(cl_mem), &ALI);
        if (err != CL_SUCCESS) {
            printf("Error setting kernel arg: %d\n", err);
            exit(1);
        }
        err = clSetKernelArg(kernel, 6, sizeof(cl_mem), &lightpos);
        if (err != CL_SUCCESS) {
            printf("Error setting kernel arg: %d\n", err);
            exit(1);
        }
        err = clSetKernelArg(kernel, 7, sizeof(cl_mem), &lightdiffuse);
        if (err != CL_SUCCESS) {
            printf("Error setting kernel arg: %d\n", err);
            exit(1);
        }
        err = clSetKernelArg(kernel, 8, sizeof(cl_mem), &lightspecular);
        if (err != CL_SUCCESS) {
            printf("Error setting kernel arg: %d\n", err);
            exit(1);
        }
        err = clSetKernelArg(kernel, 9, sizeof(cl_mem), &objectpos);
        if (err != CL_SUCCESS) {
            printf("Error setting kernel arg: %d\n", err);
            exit(1);
        }
        err = clSetKernelArg(kernel, 10, sizeof(cl_mem), &objectcolor);
        if (err != CL_SUCCESS) {
            printf("Error setting kernel arg: %d\n", err);
            exit(1);
        }
        err = clSetKernelArg(kernel, 11, sizeof(cl_mem), &objectambient);
        if (err != CL_SUCCESS) {
            printf("Error setting kernel arg: %d\n", err);
            exit(1);
        }
        err = clSetKernelArg(kernel, 12, sizeof(cl_mem), &objectdiffuse);
        if (err != CL_SUCCESS) {
            printf("Error setting kernel arg: %d\n", err);
            exit(1);
        }
        err = clSetKernelArg(kernel, 13, sizeof(cl_mem), &objectspecular);
        if (err != CL_SUCCESS) {
            printf("Error setting kernel arg: %d\n", err);
            exit(1);
        }
        err = clSetKernelArg(kernel, 14, sizeof(cl_mem), &objectreflectivity);
        if (err != CL_SUCCESS) {
            printf("Error setting kernel arg: %d\n", err);
            exit(1);
        }
        err = clSetKernelArg(kernel, 15, sizeof(cl_mem), &objectalbedo);
        if (err != CL_SUCCESS) {
            printf("Error setting kernel arg: %d\n", err);
            exit(1);
        }
        err = clSetKernelArg(kernel, 16, sizeof(cl_mem), &objectradius);
        if (err != CL_SUCCESS) {
            printf("Error setting kernel arg: %d\n", err);
            exit(1);
        }
        err = clSetKernelArg(kernel, 17, sizeof(int), &fscene->num_lights);
        if (err != CL_SUCCESS) {
            printf("Error setting kernel arg: %d\n", err);
            exit(1);
        }

        err = clSetKernelArg(kernel, 18, sizeof(int), &fscene->num_objects);
        if (err != CL_SUCCESS) {
            printf("Error setting kernel arg: %d\n", err);
            exit(1);
        }


        size_t globalsize = screensize;
        size_t localsize = 128;
        err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &globalsize, &localsize, 0, NULL, NULL);
        if (err != CL_SUCCESS) {
            printf("Error executing queued command: %d\n", err);
            exit(1);
        }
        clFinish(queue);

        float* pixels = (float*)malloc(screensizebytes);
        err = clEnqueueReadBuffer(queue, pixelcolors, CL_TRUE, 0, screensizebytes, pixels, 0, NULL, NULL);
        if (err != CL_SUCCESS) {
            printf("Error reading queued buffer: %d\n", err);
            exit(1);
        }

        int running = 1;
        while (running) {
            SDL_Event e;
            while(SDL_PollEvent(&e)){
                if (e.type == SDL_EVENT_QUIT){
                    running = 0;
                }
            }
            for(int i = 0; i < screensize; i++){
                int y = floor(i/WIDTH);
                int x = fmod(i, 1080);
                SDL_SetRenderDrawColor(renderer, pixels[i*3]*255, pixels[(i*3)+1]*255, pixels[(i*3)+2]*255, 255);
                SDL_RenderPoint(renderer, x, HEIGHT-y);
            }

            SDL_RenderPresent(renderer);
            
            cur_frame+=0.01;
            err = clSetKernelArg(kernel, 2, sizeof(float), &cur_frame);
            if (err != CL_SUCCESS) {
                printf("Error setting kernel arg: %d\n", err);
                exit(1);
            }

            err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &globalsize, &localsize, 0, NULL, NULL);
            if (err != CL_SUCCESS) {
                printf("Error executing queued command: %d\n", err);
                exit(1);
            }
            clFinish(queue);
    
            err = clEnqueueReadBuffer(queue, pixelcolors, CL_TRUE, 0, screensizebytes, pixels, 0, NULL, NULL);
            if (err != CL_SUCCESS) {
                printf("Error reading queued buffer: %d\n", err);
                exit(1);
            }
    
        }

        destroy_flattened_scene(fscene);
        clReleaseMemObject(pixelcolors);
        clReleaseMemObject(camera);
        clReleaseMemObject(plane);
        clReleaseMemObject(ALI);
        clReleaseMemObject(lightpos);
        clReleaseMemObject(lightdiffuse);
        clReleaseMemObject(lightspecular);
        clReleaseMemObject(objectpos);
        clReleaseMemObject(objectcolor);
        clReleaseMemObject(objectambient);
        clReleaseMemObject(objectdiffuse);
        clReleaseMemObject(objectspecular);
        clReleaseMemObject(objectreflectivity);
        clReleaseMemObject(objectalbedo);
        clReleaseMemObject(objectradius);
        clReleaseKernel(kernel);
        clReleaseProgram(program);
        clReleaseCommandQueue(queue);
        clReleaseContext(context);
    }

    destroy_scene(scene);
    SDL_DestroySurface(surface);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
