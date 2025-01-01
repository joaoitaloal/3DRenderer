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

            //Versão não recursiva, sem reflexos
            /*Collision* collision = checkRayCollisions(direction, origin, scene->objects); 

            if(collision->colObject == NULL){
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            }else{
                Color* renderedColor = checkCollisionColor(collision, scene);
                SDL_SetRenderDrawColor(renderer, (int)(renderedColor->red*255), (int)(renderedColor->green*255), (int)(renderedColor->blue*255), 255);
                free(renderedColor);
            }
            destroy_collision(collision);*/

            Color* renderedColor = colorFromRecursiveRayCast(direction, origin, scene, 3);
            SDL_SetRenderDrawColor(renderer, (int)(renderedColor->red*255), (int)(renderedColor->green*255), (int)(renderedColor->blue*255), 255);
            free(renderedColor);

            SDL_RenderDrawPoint(renderer, x, HEIGHT-y);

            free(origin);
            free(direction);
        }
    }
}

void tick_physics(Scene* scene){

    addVectors2(scene->objects->sphere->center, -0.5, 0, -1);

    return;
}

int main(){
    vector3D* camera = create_vector3D(0, 0, -1);
    plane3D* plane = create_plane3D(1, 0.66);
    Color* ALI = create_color(0.5, 0.5, 0.5); //ambient light intensity
    ObjectList* objects = create_objectlist();
    LightList* lights = create_lightlist();

    Color* light1diff = create_color(0.5, 0.5, 0.5);
    Color* light1spec = create_color(0.1, 0.1, 0.1);
    Light* light1 = create_light(-10, 0, -20, light1diff, light1spec);

    Color* light2diff = create_color(0.2, 0.2, 0.2);
    Color* light2spec = create_color(0.1, 0.1, 0.1);
    Light* light2 = create_light(-20, 50, 10, light2diff, light2spec);

    lights = add_to_lightlist(lights, light1);
    lights = add_to_lightlist(lights, light2);

    Sphere* sphere1 = create_sphere(0, 0, 30, 10, 1, 0, 1, create_material(0.4, 0.4, 0.1, 1, 1));
    Sphere* sphere2 = create_sphere(-5, 0, 5, 1, 0, 0.5, 1, create_material(0.4, 0.4, 0.1, 1, 1));
    Sphere* sphere3 = create_sphere(20, -20, 50, 3, 0.3, 0, 0.3, create_material(0.4, 0.4, 0.1, 1, 1));
    Sphere* sphere4 = create_sphere(15, 0, 40, 3, 1, 0, 0, create_material(0.4, 0.4, 0.1, 1, 1));
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

    //renderScene(renderer, scene);
    //destroy_scene(scene);

    int running = 1;
    while (running) {
        //O programa pega instantaneamente 100% de um núcleo da CPU, não sei se isso é normal ou não
        SDL_Event e;
        while(SDL_PollEvent(&e)){
            if (e.type == SDL_QUIT){
                running = 0;
            }
        }
        
        tick_physics(scene);
        renderScene(renderer, scene);
        SDL_RenderPresent(renderer);
    }

    printf("exiting\n");
    destroy_scene(scene);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
