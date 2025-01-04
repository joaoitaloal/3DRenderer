//gcc -o main main.c   -I/usr/local/include/SDL3 -I/usr/local/include/SDL3_image   -L/usr/local/lib -lSDL3 -lSDL3_image -lm -lcjson
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <cjson/cJSON.h>
#include "vector.h"
#include "strutils.h"

#define WIDTH 1080
#define HEIGHT 720

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

    LightList* index = scene->lights; //creio que não é pra dar free nesse index
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

float* parse_string(char* str, int size){
    float* list = malloc(sizeof(float) * size);
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

    cJSON* index = json->child;
    while(index != NULL){
        if(!strcmp(index->string, "camera")){
            float* values = parse_string(index->valuestring, 3);
            camera = create_vector3D(values[0], values[1], values[2]);
            free(values); // preciso fazer isso?
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

                lightsindex = lightsindex->next;
            } 
        }

        index = index->next;
    }


    cJSON_Delete(json);

    Scene* scene = create_scene(camera, plane, ALI, lights, objects);

    return scene;
}

void tick_physics(Scene* scene){

    addVectors2(scene->objects->sphere->center, 0, 0.3, -1);

    return;
}

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
            fprintf(stderr, "Could not open file\n");
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
        char* string;
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
        fprintf(stderr, "Could not load scene\n");
        return 1;
    }

    SDL_Surface* surface = NULL;
    if(!strcmp(argv[3], "live")){
        int antialliasing = 0;
        if(argv[4] != NULL) antialliasing = 1;
        int running = 1;
        while (running) {
            //O programa pega instantaneamente 100% de um núcleo da CPU, não sei se isso é normal ou não
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

    destroy_scene(scene);
    SDL_DestroySurface(surface);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
