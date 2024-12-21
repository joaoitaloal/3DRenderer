#include <stdio.h>
#include <math.h>
#include <SDL2/SDL.h>
#include "vector.h"

#define WIDTH 1080
#define HEIGHT 720

SDL_Renderer * init_sdl(){
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    SDL_Window * window = SDL_CreateWindow("render", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, 0);
    SDL_Renderer * renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    return renderer;
}

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
    int hit = 0;
    Collision* collision = NULL;
    
    while(scene != NULL){
        vector3D * oc = subtractVectors(scene->sphere->center, origin);
        float a = dotProduct(dir, dir);
        float b = 2*dotProduct(oc, dir);
        float c = dotProduct(oc, oc)-scene->sphere->radius*scene->sphere->radius;
        free(oc);

        float temp = quadraticFormula(a, b, c);
        if(temp >= 0 && temp < t){
            t = temp;
            collision = create_collision(addVectors(origin, scaleVector(dir, t)), scene->sphere);
        }
        
        scene = scene->next;
    }

    return collision;
}

int main(){
    vector3D* camera = create_vector3D(0, 0, -1);
    plane3D* plane = create_plane3D(1, 0.66);
    Color* ALI = create_color(0.5, 0.5, 0.5); //ambient light intensity
    ObjectList* scene = create_objectlist();
    LightList* lights = create_lightlist();

    Light* light1 = create_light(400, 200, 20, create_color(0.1, 0.1, 0.1), create_color(0.1, 0.1, 0.1));
    Light* light2 = create_light(-150, 20, 40, create_color(0.1, 0.1, 0.1), create_color(0.1, 0.1, 0.1));
    lights = add_to_lightlist(lights, light1);
    lights = add_to_lightlist(lights, light2);

    Sphere* sphere1 = create_sphere(0, 0, 30, 10, 1, 0, 1, create_material(0.1,0.1,0.1, 1));
    Sphere* sphere2 = create_sphere(-20, 5, 40, 8, 0, 0.5, 1, create_material(0.1, 0.1, 0.1, 1));
    Sphere* sphere3 = create_sphere(20, 0, 50, 3, 0.3, 0.5, 0.3, create_material(0.1, 0.1, 0.1, 1));
    scene = add_to_objectlist(scene, sphere1);
    scene = add_to_objectlist(scene, sphere2);
    scene = add_to_objectlist(scene, sphere3);

    SDL_Renderer * renderer = init_sdl();

    for(int x = 0; x < WIDTH; x++){
        for(int y = 0; y < HEIGHT; y++){
            float alpha = (float)x/WIDTH;
            float beta = (float)y/HEIGHT;

            //(1-alpha)*x1 + alpha*x2
            vector3D * t = addVectors(scaleVector(plane->x1, 1.0-alpha), scaleVector(plane->x2, alpha));
            vector3D * b = addVectors(scaleVector(plane->x3, 1.0-alpha), scaleVector(plane->x4, alpha));
            vector3D * origin = addVectors(scaleVector(t, 1.0-beta), scaleVector(b, beta));
            free(t);
            free(b);
            
            vector3D * direction = subtractVectors(camera, origin);

            //255-(x+2)*(255/4)) = 255 - (63,75*x+127,5)  //convertendo posição pra cor
            //255-(y+3*(255/6)) = 255 - (42,5*y+127,5)
            //SDL_SetRenderDrawColor(renderer, 255-(63.75*direction->x+127.5), 255-(42.5*direction->y+127.5), 125, 255);

            Collision* collision = checkRayCollisions(direction, origin, scene);
            if(collision == NULL){
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            }else{
                Color* drawn_color = productColors(collision->colObject->color, ALI);
                vector3D* normalized = normalizeVector(subtractVectors(collision->colObject->center, collision->colPoint));

                LightList* index = lights; //creio que não é pra dar free nesse index
                while(index != NULL){
                    vector3D* L = normalizeVector(subtractVectors(collision->colPoint, index->light->position));
                    float dot = dotProduct(L, normalized);
                    if(dot >= 0){
                        //O addColors cria e retorna uma cor nova, eu devia dar free no espaço que tava alocado antes?
                        //que horror
                        drawn_color = addColors(drawn_color, scaleColor(productColors(index->light->diffuse, collision->colObject->material->diffuse), dot));

                        vector3D* reflectance = scaleVector(subtractVectors(L, normalized), 2*dot);
                        vector3D* view = subtractVectors(collision->colPoint , normalizeVector(camera));
                        dot = pow(dotProduct(reflectance, view), collision->colObject->material->albedo);
                        drawn_color = addColors(drawn_color, scaleColor(productColors(index->light->specular, collision->colObject->material->specular), dot));
                    }

                    index = index->next;
                }
                drawn_color = clampColor(drawn_color, 0, 1);
                SDL_SetRenderDrawColor(renderer, (int)(drawn_color->red*255), (int)(drawn_color->green*255), (int)(drawn_color->blue*255), 255);
            }

            SDL_RenderDrawPoint(renderer, x, HEIGHT-y);

            //printf("x: %f, y: %f, z: %f    screenx: %d, screeny: %d    alpha: %f, beta: %f\n", direction->x, direction->y, direction->z, x, y, alpha, beta);
            free(origin);
            free(direction);
        }
    }
    
    SDL_RenderPresent(renderer);
    SDL_Delay(5000);

    return 0;
}