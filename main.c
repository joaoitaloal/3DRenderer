#include <stdio.h>
#include <math.h>
#include <SDL2/SDL.h>
#include "vector.h"

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

//retorna cor da colisão ou NULL;
Collision* checkRayCollisions(vector3D * dir, vector3D * origin, ObjectList * scene){
    float t = INFINITY;
    Collision* collision = NULL;
    
    ObjectList* index = scene;
    while(index != NULL){
        vector3D * oc = subtractVectors(index->sphere->center, origin);
        float a = dotProduct(dir, dir);
        float b = 2*dotProduct(oc, dir);
        float c = dotProduct(oc, oc)-index->sphere->radius*index->sphere->radius;
        free(oc);

        float temp = quadraticFormula(a, b, c);
        if(temp >= 1 && temp < t){
            t = temp;
            vector3D* scale = scaleVector(dir, t);
            vector3D* sumn = addVectors(origin, scale);
            collision = create_collision(sumn, index->sphere);
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

int isInShadow(Collision* col, Light* light, ObjectList* scene){
    ObjectList* index = scene;
    while(index != NULL){
        if(index->sphere == col->colObject){
            index = index->next;
            continue;
        }
        vector3D* sub = subtractVectors(col->colPoint, light->position);
        float t = checkSingleObjectCollisionDistance(sub, col->colPoint, index->sphere);
        if(0 < t && t < 1){
            return 1;
        }

        free(sub);
        index = index->next;
    }
    return 0;
}

Color* checkCollisionColor(Collision* col, Color* ALI, vector3D* camera, LightList* lights, ObjectList* scene){
    Color * drawn_color = create_color(0, 0, 0);

    vector3D* sub = subtractVectors(col->colObject->center, col->colPoint);
    vector3D* normalized = normalizeVector(sub);
    free(sub);

    vector3D* normCam = normalizeVector(camera);
    vector3D* view = subtractVectors(col->colPoint , normCam);
    free(normCam);

    LightList* index = lights; //creio que não é pra dar free nesse index
    while(index != NULL){
        if(isInShadow(col, index->light, scene)){
            index = index->next;
            continue;
        }
        vector3D* sub = subtractVectors(col->colPoint, index->light->position);
        vector3D* L = normalizeVector(sub);
        free(sub);

        float dot = dotProduct(L, normalized);

        vector3D* scaled = scaleVector(normalized, 2*dot);
        vector3D* reflectance = subtractVectors(L, scaled);
        free(scaled);

        float dot2 = dotProduct(reflectance, view);
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

        free(L);
        free(reflectance);
        
        index = index->next;
    }

    Color* ambProd = productColors(col->colObject->material->ambient, ALI);
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

void renderScene(SDL_Renderer* renderer, Scene* scene){
    for(int x = 0; x < WIDTH; x++){
        for(int y = 0; y < HEIGHT; y++){
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
            vector3D * origin = addVectors(scalet, scaleb);
            free(scalet);
            free(scaleb);

            free(t);
            free(b);

            vector3D * direction = subtractVectors(scene->camera, origin);

            //255-(x+2)*(255/4)) = 255 - (63,75*x+127,5)  //convertendo posição pra cor
            //255-(y+3*(255/6)) = 255 - (42,5*y+127,5)
            //SDL_SetRenderDrawColor(renderer, 255-(63.75*direction->x+127.5), 255-(42.5*direction->y+127.5), 125, 255);

            Collision* collision = checkRayCollisions(direction, origin, scene->objects);
            //if(collision != NULL && collision->colPoint->y == 0)
                //printf("colpoint: x: %f y: %f z: %f    esfera: raio: %f\n", collision->colPoint->x, collision->colPoint->y, collision->colPoint->z, collision->colObject->radius);
            if(collision == NULL){
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            }else{
                Color* renderedColor = checkCollisionColor(collision, scene->ALI, scene->camera, scene->lights, scene->objects);
                SDL_SetRenderDrawColor(renderer, (int)(renderedColor->red*255), (int)(renderedColor->green*255), (int)(renderedColor->blue*255), 255);
                free(renderedColor);
            }
            //destroy_collision(collision);

            SDL_RenderDrawPoint(renderer, x, HEIGHT-y);

            free(origin);
            free(direction);
            //free(collision->colPoint); como eu destruo isso? eu devo destruir isso?
            free(collision);
        }
    }
}

int main(){
    vector3D* camera = create_vector3D(0, 0, -1);
    plane3D* plane = create_plane3D(1, 0.66);
    Color* ALI = create_color(0.5, 0.5, 0.5); //ambient light intensity
    ObjectList* objects = create_objectlist(); //esse ponteiro e o de baixo tão sendo perdidos
    LightList* lights = create_lightlist();

    Color* light1diff = create_color(0.5, 0.5, 0.5);
    Color* light1spec = create_color(0.1, 0.1, 0.1);
    Light* light1 = create_light(-10, 0, -20, light1diff, light1spec);

    Color* light2diff = create_color(0.2, 0.2, 0.2);
    Color* light2spec = create_color(0.1, 0.1, 0.1);
    Light* light2 = create_light(-20, 50, 10, light2diff, light2spec);

    lights = add_to_lightlist(lights, light1);
    lights = add_to_lightlist(lights, light2);

    Sphere* sphere1 = create_sphere(0, 0, 30, 10, 1, 0, 1, create_material(0.4, 0.4, 0.1, 1));
    Sphere* sphere2 = create_sphere(-5, 0, 5, 1, 0, 0.5, 1, create_material(0.4, 0.4, 0.1, 1));
    Sphere* sphere3 = create_sphere(20, -20, 50, 3, 0.3, 0, 0.3, create_material(0.4, 0.4, 0.1, 1));
    Sphere* sphere4 = create_sphere(15, 0, 40, 3, 1, 0, 0, create_material(0.4, 0.4, 0.1, 1));
    objects = add_to_objectlist(objects, sphere1);
    objects = add_to_objectlist(objects, sphere2);
    objects = add_to_objectlist(objects, sphere3);
    objects = add_to_objectlist(objects, sphere4);

    Scene* scene = create_scene(camera, plane, ALI, lights, objects);

    SDL_Init(SDL_INIT_VIDEO);
    atexit(SDL_Quit);

    SDL_Window * window = SDL_CreateWindow("render", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, 0);
    SDL_Renderer * renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if(!window || !renderer){
        printf("erro inicializando");
        return 1;
    }

    renderScene(renderer, scene); // meu deus que chatice isso aqui
    destroy_scene(scene);
    //destroy_light(light1);
    //destroy_light(light2);
    free(light1diff);
    free(light2diff);
    free(light1spec);
    free(light2spec);
    /*destroy_sphere(sphere1);
    destroy_sphere(sphere2);
    destroy_sphere(sphere3);
    destroy_sphere(sphere4);*/
    SDL_RenderPresent(renderer);

    /*while (1) { isso tá crashando cuidado / this crashes be careful
        SDL_Event e;
        if (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                break;
            }
        }

        renderScene(renderer, scene);
        SDL_RenderPresent(renderer);
    }*/
    SDL_Delay(5000);

    printf("exiting\n");
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
