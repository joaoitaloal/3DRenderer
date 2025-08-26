#ifndef STRUTILS_H
#define STRUTILS_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <cJSON.h>
#include "vector.h"

#define SPEED 1

typedef struct OpenclContext{
    flattenedScene* fscene;
    cl_mem pixelcolors;
    cl_mem camera;
    cl_mem plane;
    cl_mem ALI;
    cl_mem lightpos;
    cl_mem lightdiffuse;
    cl_mem lightspecular;
    cl_mem objectpos;
    cl_mem objectcolor;
    cl_mem objectambient;
    cl_mem objectdiffuse;
    cl_mem objectspecular;
    cl_mem objectreflectivity;
    cl_mem objectalbedo;
    cl_mem objectradius;
    cl_kernel render_kernel;
    cl_kernel post_processing_kernel;
    cl_program render_program;
    cl_program post_processing_program;
    cl_device_id devices;
    cl_context context;
} OpenclContext;

OpenclContext* create_opencl_context(
    flattenedScene* fscene,
    cl_mem pixelcolors,
    cl_mem camera,
    cl_mem plane,
    cl_mem ALI,
    cl_mem lightpos,
    cl_mem lightdiffuse,
    cl_mem lightspecular,
    cl_mem objectpos,
    cl_mem objectcolor,
    cl_mem objectambient,
    cl_mem objectdiffuse,
    cl_mem objectspecular,
    cl_mem objectreflectivity,
    cl_mem objectalbedo,
    cl_mem objectradius,
    cl_kernel render_kernel,
    cl_kernel post_processing_kernel,
    cl_program render_program,
    cl_program post_processing_program,
    cl_device_id devices,
    cl_context context
){
    OpenclContext * oc;
    oc = (OpenclContext*)malloc(sizeof(OpenclContext));

    oc->fscene = fscene;
    oc->pixelcolors = pixelcolors;
    oc->camera = camera;
    oc->plane = plane;
    oc->ALI = ALI;
    oc->lightpos = lightpos;
    oc->lightdiffuse = lightdiffuse;
    oc->lightspecular = lightspecular;
    oc->objectpos = objectpos;
    oc->objectcolor = objectcolor;
    oc->objectambient = objectambient;
    oc->objectdiffuse = objectdiffuse;
    oc->objectspecular = objectspecular;
    oc->objectreflectivity = objectreflectivity;
    oc->objectalbedo = objectalbedo;
    oc->objectradius = objectradius;
    oc->render_kernel = render_kernel;
    oc->post_processing_kernel = post_processing_kernel;
    oc->render_program = render_program;
    oc->post_processing_program = post_processing_program;
    oc->devices = devices;
    oc->context = context;

    return oc;
}

void destroy_openclcontext(OpenclContext *opencl_context){
    destroy_flattened_scene(opencl_context->fscene);
    clReleaseMemObject(opencl_context->pixelcolors);
    clReleaseMemObject(opencl_context->camera);
    clReleaseMemObject(opencl_context->plane);
    clReleaseMemObject(opencl_context->ALI);
    clReleaseMemObject(opencl_context->lightpos);
    clReleaseMemObject(opencl_context->lightdiffuse);
    clReleaseMemObject(opencl_context->lightspecular);
    clReleaseMemObject(opencl_context->objectpos);
    clReleaseMemObject(opencl_context->objectcolor);
    clReleaseMemObject(opencl_context->objectambient);
    clReleaseMemObject(opencl_context->objectdiffuse);
    clReleaseMemObject(opencl_context->objectspecular);
    clReleaseMemObject(opencl_context->objectreflectivity);
    clReleaseMemObject(opencl_context->objectalbedo);
    clReleaseMemObject(opencl_context->objectradius);
    clReleaseKernel(opencl_context->render_kernel);
    clReleaseProgram(opencl_context->render_program);
    clReleaseKernel(opencl_context->post_processing_kernel);
    clReleaseProgram(opencl_context->post_processing_program);
    clReleaseDevice(opencl_context->devices);
    clReleaseContext(opencl_context->context);

    free(opencl_context);
}

char** str_split(char* a_str, const char a_delim)
{
    //from https://stackoverflow.com/questions/9210528/split-string-with-delimiters-in-c
    //Tenho que voltar e tentar fazer isso do 0 depois, mas parece bem complexo pra algo que não vou usar tanto
    char** result    = 0;
    size_t count     = 0;
    char* tmp        = a_str;
    char* last_comma = 0;
    char delim[2];
    delim[0] = a_delim;
    delim[1] = 0;

    /* Count how many elements will be extracted. */
    while (*tmp)
    {
        if (a_delim == *tmp)
        {
            count++;
            last_comma = tmp;
        }
        tmp++;
    }

    /* Add space for trailing token. */
    count += last_comma < (a_str + strlen(a_str) - 1);

    /* Add space for terminating null string so caller
       knows where the list of returned strings ends. */
    count++;

    result = (char **)malloc(sizeof(char*) * count);

    if (result)
    {
        size_t idx  = 0;
        char* token = strtok(a_str, delim);

        while (token)
        {
            assert(idx < count);
            *(result + idx++) = strdup(token);
            token = strtok(0, delim);
        }
        assert(idx == count - 1);
        *(result + idx) = 0;
    }

    return result;
}

float* parse_string(char* str, int size){
    float* list = (float *)malloc(sizeof(float) * size);
    char arg[100];
    char** tokens = str_split(strcpy(arg, str), ',');

    for(int i = 0; i < size; i++){
        list[i] = atof(tokens[i]);
        free(tokens[i]);
    }
    free(tokens);

    return list;
}

Scene* load_scene(char* json_str){

    
    cJSON* json = cJSON_Parse(json_str);
    free(json_str);

    if(json == NULL){
        printf("Error parsing json\n");
        return NULL;
    }

    vector3D* camera;
    plane3D* plane;
    Color* ALI; //ambient light intensity
    ObjectList* objects = create_objectlist();
    LightList* lights = create_lightlist();
    int num_objects = 0;
    int num_lights = 0;

    cJSON* index = json->child;
    while(index != NULL){
        if(!strcmp(index->string, "camera")){
            float* values = parse_string(index->valuestring, 3);
            camera = create_vector3D(values[0], values[1], values[2]);
            free(values);
        }
        else if(!strcmp(index->string, "plane")){
            float* values = parse_string(index->valuestring, 2);
            plane = create_plane3D(values[0], values[1]);
            free(values);
        }
        else if(!strcmp(index->string, "ambient_light_intensity")){
            float* values = parse_string(index->valuestring, 1);
            ALI = create_color(values[0], values[0], values[0]);
            free(values);
        }
        else if(!strcmp(index->string, "objects")){
            cJSON* objindex = index->child;
            while(objindex != NULL){
                vector3D* objpos;
                float objradius;
                Color* objcolor;
                Material* objmat;

                cJSON* objprop = objindex->child;
                while(objprop != NULL){
                    if(!strcmp(objprop->string, "position")){
                        float* values = parse_string(objprop->valuestring, 3);
                        objpos = create_vector3D(values[0], values[1], values[2]);
                        free(values);
                    }
                    else if(!strcmp(objprop->string, "radius")){
                        float* values = parse_string(objprop->valuestring, 1);
                        objradius = values[0];
                        free(values);
                    }
                    else if(!strcmp(objprop->string, "color_rgb")){
                        float* values = parse_string(objprop->valuestring, 3);
                        objcolor = create_color(values[0]/255, values[1]/255, values[2]/255);
                        free(values);
                    }
                    else if(!strcmp(objprop->string, "material")){
                        float ambient;
                        float diffuse;
                        float specular;
                        float reflectivity;
                        float albedo;
                        
                        cJSON* matprop = objprop->child;
                        while(matprop != NULL){
                            if(!strcmp(matprop->string, "ambient")){
                                float* values = parse_string(matprop->valuestring, 1);
                                ambient = values[0];
                                free(values);
                            }
                            else if(!strcmp(matprop->string, "diffuse")){
                                float* values = parse_string(matprop->valuestring, 1);
                                diffuse = values[0];
                                free(values);
                            }
                            else if(!strcmp(matprop->string, "specular")){
                                float* values = parse_string(matprop->valuestring, 1);
                                specular = values[0];
                                free(values);
                            }
                            else if(!strcmp(matprop->string, "reflectivity")){
                                float* values = parse_string(matprop->valuestring, 1);
                                reflectivity = values[0];
                                free(values);
                            }
                            else if(!strcmp(matprop->string, "albedo")){
                                float* values = parse_string(matprop->valuestring, 1);
                                albedo = values[0];
                                free(values);
                            }
                            matprop = matprop->next;
                        }
                        objmat = create_material(ambient, diffuse, specular, reflectivity, albedo);
                    }

                    objprop = objprop->next;
                }
                Sphere* object = create_sphere2(objpos, objradius, objcolor, objmat);
                objects = add_to_objectlist(objects, object);
                num_objects++;

                objindex = objindex->next;
            }
        }
        else if(!strcmp(index->string, "lights")){
            cJSON* lightsindex = index->child;
            while(lightsindex != NULL){
                vector3D* lightpos;
                Color* lightdiff;
                Color* lightspec;

                cJSON* lightprops = lightsindex->child;
                while(lightprops != NULL){
                    if(!strcmp(lightprops->string, "position")){
                        float* values = parse_string(lightprops->valuestring, 3);
                        lightpos = create_vector3D(values[0], values[1], values[2]);
                        free(values);
                    }
                    if(!strcmp(lightprops->string, "diffuse")){
                        float* values = parse_string(lightprops->valuestring, 1);
                        lightdiff = create_color(values[0], values[0], values[0]);
                        free(values);
                    }
                    if(!strcmp(lightprops->string, "specular")){
                        float* values = parse_string(lightprops->valuestring, 1);
                        lightspec = create_color(values[0], values[0], values[0]);
                        free(values);
                    }

                    lightprops = lightprops->next;
                }
                Light* light = create_light2(lightpos, lightdiff, lightspec);
                lights = add_to_lightlist(lights, light);
                num_lights++;

                lightsindex = lightsindex->next;
            } 
        }

        index = index->next;
    }


    cJSON_Delete(json);

    Scene* scene = create_scene(camera, plane, ALI, lights, objects, num_lights, num_objects);

    return scene;
}

flattenedScene* flattenScene(Scene* scene){
    flattenedScene* flattenned = (flattenedScene*)malloc(sizeof(flattenedScene));

    //deeply regret all my past actions that led me to this moment
    flattenned->camera[0] = scene->camera->x;
    flattenned->camera[1] = scene->camera->y;
    flattenned->camera[2] = scene->camera->z;
    flattenned->ALI[0] = scene->ALI->red;
    flattenned->ALI[1] = scene->ALI->green;
    flattenned->ALI[2] = scene->ALI->blue;
    flattenned->plane[0] = scene->plane->x1->x;
    flattenned->plane[1] = scene->plane->x1->y;
    flattenned->plane[2] = scene->plane->x1->z;
    flattenned->plane[3] = scene->plane->x2->x;
    flattenned->plane[4] = scene->plane->x2->y;
    flattenned->plane[5] = scene->plane->x2->z;
    flattenned->plane[6] = scene->plane->x3->x;
    flattenned->plane[7] = scene->plane->x3->y;
    flattenned->plane[8] = scene->plane->x3->z;
    flattenned->plane[9] = scene->plane->x4->x;
    flattenned->plane[10] = scene->plane->x4->y;
    flattenned->plane[11] = scene->plane->x4->z;
    flattenned->num_lights = scene->num_lights;
    flattenned->num_objects = scene->num_objects;

    flattenned->lightpos = (float *)malloc(sizeof(float)*scene->num_lights*3);
    flattenned->lightdiffuse = (float *)malloc(sizeof(float)*scene->num_lights*3);
    flattenned->lightspecular = (float *)malloc(sizeof(float)*scene->num_lights*3);
    flattenned->objectpos = (float *)malloc(sizeof(float)*scene->num_objects*3);
    flattenned->objectcolor = (float *)malloc(sizeof(float)*scene->num_objects*3);
    flattenned->objectambient = (float *)malloc(sizeof(float)*scene->num_objects*3);
    flattenned->objectdiffuse = (float *)malloc(sizeof(float)*scene->num_objects*3);
    flattenned->objectspecular = (float *)malloc(sizeof(float)*scene->num_objects*3);
    flattenned->objectreflectivity = (float *)malloc(sizeof(float)*scene->num_objects*3);
    flattenned->objectradius = (float *)malloc(sizeof(float)*scene->num_objects);
    flattenned->objectalbedo = (float *)malloc(sizeof(float)*scene->num_objects);

    LightList* lindex = scene->lights;
    int i = 0;
    while(lindex->light){
        flattenned->lightpos[i*3] = lindex->light->position->x;
        flattenned->lightpos[i*3+1] = lindex->light->position->y;
        flattenned->lightpos[i*3+2] = lindex->light->position->z;

        flattenned->lightdiffuse[i*3] = lindex->light->diffuse->red;
        flattenned->lightdiffuse[i*3+1] = lindex->light->diffuse->green;
        flattenned->lightdiffuse[i*3+2] = lindex->light->diffuse->blue;

        flattenned->lightspecular[i*3] = lindex->light->specular->red;
        flattenned->lightspecular[i*3+1] = lindex->light->specular->green;
        flattenned->lightspecular[i*3+2] = lindex->light->specular->blue;

        i++;
        lindex = lindex->next;
    }
    
    ObjectList* oindex = scene->objects;
    i = 0;
    while(oindex->sphere){
        flattenned->objectpos[i*3] = oindex->sphere->center->x;
        flattenned->objectpos[i*3+1] = oindex->sphere->center->y;
        flattenned->objectpos[i*3+2] = oindex->sphere->center->z;
        flattenned->objectcolor[i*3] = oindex->sphere->color->red;
        flattenned->objectcolor[i*3+1] = oindex->sphere->color->green;
        flattenned->objectcolor[i*3+2] = oindex->sphere->color->blue;
        flattenned->objectambient[i*3] = oindex->sphere->material->ambient->red;
        flattenned->objectambient[i*3+1] = oindex->sphere->material->ambient->green;
        flattenned->objectambient[i*3+2] = oindex->sphere->material->ambient->blue;
        flattenned->objectdiffuse[i*3] = oindex->sphere->material->diffuse->red;
        flattenned->objectdiffuse[i*3+1] = oindex->sphere->material->diffuse->green;
        flattenned->objectdiffuse[i*3+2] = oindex->sphere->material->diffuse->blue;
        flattenned->objectspecular[i*3] = oindex->sphere->material->specular->red;
        flattenned->objectspecular[i*3+1] = oindex->sphere->material->specular->green;
        flattenned->objectspecular[i*3+2] = oindex->sphere->material->specular->blue;
        flattenned->objectreflectivity[i*3] = oindex->sphere->material->reflectivity->red;
        flattenned->objectreflectivity[i*3+1] = oindex->sphere->material->reflectivity->green;
        flattenned->objectreflectivity[i*3+2] = oindex->sphere->material->reflectivity->blue;
        flattenned->objectradius[i] = oindex->sphere->radius;
        flattenned->objectalbedo[i] = oindex->sphere->material->albedo;
        
        i++;
        oindex = oindex->next;
    }

    return flattenned;
}

//compiling errors were coming from here but they should be fixed now
const char* load_strfile(char* file_path){
    FILE* file = fopen(file_path, "r");

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    char* str = (char*)malloc((file_size + 1)*sizeof(char));

    long ending = fread(str, 1, file_size, file);
    str[ending] = '\0';

    fclose(file);
    return str;
}

void handle_keyboard_input(const char* key_pressed, flattenedScene* fscene){
    if(!strcmp(key_pressed, "W")){
        fscene->camera[2] += SPEED;
        fscene->plane[2] += SPEED;
        fscene->plane[5] += SPEED;
        fscene->plane[8] += SPEED;
        fscene->plane[11] += SPEED;
    }else if(!strcmp(key_pressed, "S")){
        fscene->camera[2] -= SPEED;
        fscene->plane[2] -= SPEED;
        fscene->plane[5] -= SPEED;
        fscene->plane[8] -= SPEED;
        fscene->plane[11] -= SPEED;
    }
    if(!strcmp(key_pressed, "A")){
        fscene->camera[0] += SPEED;
        fscene->plane[0] += SPEED;
        fscene->plane[3] += SPEED;
        fscene->plane[6] += SPEED;
        fscene->plane[9] += SPEED;
    }else if(!strcmp(key_pressed, "D")){
        fscene->camera[0] -= SPEED;
        fscene->plane[0] -= SPEED;
        fscene->plane[3] -= SPEED;
        fscene->plane[6] -= SPEED;
        fscene->plane[9] -= SPEED;
    }
    if(!strcmp(key_pressed, "Space")){
        fscene->camera[1] -= SPEED;
        fscene->plane[1] -= SPEED;
        fscene->plane[4] -= SPEED;
        fscene->plane[7] -= SPEED;
        fscene->plane[10] -= SPEED;
    }else if(!strcmp(key_pressed, "Left Shift")){
        fscene->camera[1] += SPEED;
        fscene->plane[1] += SPEED;
        fscene->plane[4] += SPEED;
        fscene->plane[7] += SPEED;
        fscene->plane[10] += SPEED;
    }

    return;
}

#endif