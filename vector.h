#ifndef VECTOR_H
#define VECTOR_H

#include <stdlib.h>

typedef struct vector3D{
    float x;
    float y;
    float z;
} vector3D;

typedef struct plane3D{
    vector3D* x1;//armazenar os ponteiros aqui faz mais sentido do que armazenar os vetores em si?
    vector3D* x2;
    vector3D* x3;
    vector3D* x4;
} plane3D;

typedef struct Color{
    float red;
    float green;
    float blue;
} Color;

typedef struct Light{
    vector3D* position;
    Color* diffuse;
    Color* specular;
} Light;

typedef struct Material{
    Color* ambient;
    Color* diffuse;
    Color* specular;
    float albedo;
} Material;

typedef struct Sphere{
    vector3D* center;
    float radius;
    Color* color;
    Material* material;
} Sphere;

typedef struct Collision{
    vector3D* colPoint;
    Sphere* colObject;
} Collision;

typedef struct ObjectList{
    Sphere* sphere;
    struct ObjectList* next;
} ObjectList;

typedef struct LightList{
    Light* light;
    struct LightList* next;
} LightList;

/*typedef struct Scene{//posso colocar a camera e o plano aqui mas por enquanto não

} Scene;*/

vector3D * create_vector3D(float x, float y, float z){
    vector3D * vector;
    vector = (vector3D *)malloc(sizeof(vector3D));

    vector->x = x;
    vector->y = y;
    vector->z = z;

    return vector;
}

plane3D * create_plane3D(float w, float h){
    plane3D * plane;
    plane = (plane3D *)malloc(sizeof(plane3D));

    plane->x1 = create_vector3D(w, h, 0);
    plane->x2 = create_vector3D(-w, h, 0);
    plane->x3 = create_vector3D(w, -h, 0);
    plane->x4 = create_vector3D(-w, -h, 0);

    return plane;
}

Color* create_color(float red, float green, float blue){
    Color* color;
    color = (Color *)malloc(sizeof(Color));
    
    color->red = red;
    color->blue = blue;
    color->green = green;

    return color;
}

Light * create_light(float x, float y, float z, Color* diffuse, Color* specular){
    Light* light;
    light = (Light *)malloc(sizeof(Light));

    light->position = create_vector3D(x, y, z);
    light->diffuse = diffuse;
    light->specular = specular;

    return light;
}

Material * create_material(float ambient, float diffuse, float specular, float albedo){
    Material* material;
    material = (Material *)malloc(sizeof(Material));

    material->ambient = create_color(ambient, ambient, ambient);
    material->diffuse = create_color(diffuse, diffuse, diffuse);
    material->specular = create_color(specular, specular, specular);
    material->albedo = albedo;

    return material;
}

Collision * create_collision(vector3D* colPoint, Sphere* Object){
    Collision* collision;
    collision = (Collision *)malloc(sizeof(Collision));

    collision->colPoint = colPoint;
    collision->colObject = Object;

    return collision;
}

//colors: [0,1]
Sphere * create_sphere(float x, float y, float z, float radius, float red, float green, float blue, Material* material){
    Sphere* sphere;
    sphere = (Sphere *)malloc(sizeof(Sphere));

    sphere->center = create_vector3D(x, y, z);
    sphere->radius = radius;
    sphere->color = create_color(red, green, blue);
    sphere->material = material;

    return sphere;
}

ObjectList* create_objectlist(){
    ObjectList* list;
    list = (ObjectList *)malloc(sizeof(ObjectList));

    list = NULL;

    return list;
}
ObjectList* add_to_objectlist(ObjectList * list, Sphere * sphere){
    ObjectList * new;
    new = (ObjectList *)malloc(sizeof(ObjectList));

    new->next = list;
    new->sphere = sphere;

    return new;
}

LightList* create_lightlist(){
    LightList* list;
    list = (LightList *)malloc(sizeof(LightList));

    list = NULL;

    return list;
}
LightList* add_to_lightlist(LightList * list, Light * light){
    LightList * new;
    new = (LightList *)malloc(sizeof(LightList));

    new->next = list;
    new->light = light;

    return new;
}

float clamp(float num, float min, float max){//https://stackoverflow.com/questions/427477/fastest-way-to-clamp-a-real-fixed-floating-point-value
    const float t = num < min ? min : num;
    return t > max ? max : t;
}

//v1 + v2
vector3D * addVectors(vector3D * v1, vector3D * v2){
    vector3D * vector = create_vector3D(v1->x + v2->x, v1->y + v2->y, v1->z + v2->z);
    return vector;
}
//v2 - v1
vector3D * subtractVectors(vector3D * v1, vector3D * v2){
    vector3D * vector = create_vector3D(v2->x - v1->x, v2->y - v1->y, v2->z - v1->z);
    return vector;
}
//v*scalar
vector3D * scaleVector(vector3D * v, float scalar){
    vector3D * vector = create_vector3D(v->x * scalar, v->y * scalar, v->z * scalar);
    return vector;
}
float dotProduct(vector3D* v1, vector3D* v2){
    return v1->x*v2->x + v1->y*v2->y + v1->z*v2->z;
}
float getMagnitude(vector3D * v){
    return sqrt(v->x*v->x + v->y*v->y + v->z*v->z);
}
vector3D* normalizeVector(vector3D * v){
    vector3D * vector = scaleVector(v, 1/getMagnitude(v));
}

Color* addColors(Color* c1, Color* c2){
    return create_color(c1->red+c2->red, c1->green+c2->green, c1->blue+c2->blue);
}
Color* productColors(Color* c1, Color* c2){
    return create_color(c1->red*c2->red, c1->green*c2->green, c1->blue*c2->blue);
}
Color* scaleColor(Color* c, float scalar){
    return create_color(c->red*scalar, c->green*scalar, c->blue*scalar);
}
Color* clampColor(Color* c, float min, float max){
    return create_color(clamp(c->red, min, max), clamp(c->green, min, max), clamp(c->blue, min, max));
}

#endif