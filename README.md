# 3DRenderer
A 3D renderer written in C.
The rendering method used here is raytracing, SDL3 is used to create the window.

The scene currently supports only spheres and lights, the renderer has shadows, reflections and an antialliasing method.

## Compiling observations
You will need to install the [SDL3 library](https://wiki.libsdl.org/SDL3/Installation) to compile the code, it is possible to use SDL2 but you will need to modify the code since there are some functions that changed between versions.
Since the code actually works independently of SDL you can use any other method of placing pixels on screen.

You will also need to install the [cJSON library](https://github.com/DaveGamble/cJSON) so the json file can be used to change the scene, an alternative is to modify the code to directly create the scene there if you dont want to use the library or something.

Finally, you will need to install the [SDL3_image library](https://github.com/libsdl-org/SDL_image) to use the image saving function, but if you do not want to use this one it is fine to just comment out the include and the save image part of the code on the main function.

## Usage
After you compile the program you can run it with the following arguments(in order):

1. Input-mode: Currently this can only be "file", meaning that you will load the scene from a JSON file.
2. Input: The name of the JSON file.
3. Mode: Either "image" to save the rendered scene as an JPEG image or "live" for a live frame rendering of the scene(it kinda supports moving objects but it is very slow, check the tick_physics function on main.c).
4. Filename for image: If the third argument is image you will need to pass the desired filename to be saved as a JPEG, WARNING: it will overwrite any other jpeg with the same name.
5. Antialliasing: If you pass nothing as the fifth argument it does not use the antialliasing and if you pass anything it uses. The only reason to not use it is if you want the live mode to render faster, it will probably run 4x faster without antialliasing. The image mode is always using antialliasing, you can change it in the code if you want to.

As an example you if you run
```bash
./main file scene.json image image0001
```
The program will save an image named image0001 based on the scene on scene.json file.
### Editing the scene
To make the json file i recommend simply copying the "scene.json" file on the repository and editing it, since my parsing algorithm is very terrible and will break if the format is not the same on there.
