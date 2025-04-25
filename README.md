# 3DRenderer

A 3D renderer written in C.
The rendering method used here is raytracing, SDL3 is used to create the window.

The scene currently supports only spheres and lights, the renderer has shadows, reflections and an antialliasing method.

## Installation and Compiling observations

The main program is in main.c, image.c is a custom modification i made that only create images, if you want to use it you will need to download [stb_image_write.h](https://github.com/nothings/stb/blob/master/stb_image_write.h).

You will need to install the [SDL3 library](https://github.com/libsdl-org/SDL) to compile the code, it is possible to use SDL2 but you will need to modify the code since there are some functions that changed between versions.
Since the code actually works independently of SDL you can use any other method of placing pixels on screen.

You will also need to install the [cJSON library](https://github.com/DaveGamble/cJSON) so the json file can be used to change the scene, an alternative is to modify the code to directly create the scene there if you dont want to use the library or something.

If you want to use the OpenCL live frame rendering you will need to install the [OpenCL SDK](https://github.com/KhronosGroup/OpenCL-SDK) or at least the bindings for C i think, there are some little observations too: you need to have a gpu like device that supports OpenCL and has the drivers for it working(you can check it running clinfo on a terminal), and the minimum version i tested the program on was OpenCL 2.0.
Also the .txt files on the repository are the opencl kernel codes so they need to be on the program's directory!

Finally, if you want to use the image saving function, you will need to install the [SDL3_image library](https://github.com/libsdl-org/SDL_image), but if you do not want to use this one it is fine to just comment out the include and the save image part of the code on the main function. this is not working on windows as far as i tested.

## Usage

After you compile the program you can run it with the following arguments(in order):

1. Input-mode: Currently this can only be "file", meaning that you will load the scene from a JSON file.
2. Input: The name of the JSON file.
3. Mode: Either "image" to save the rendered scene as an JPEG image, "live" for a live frame rendering of the scene(it kinda supports moving objects but it is very slow, check the tick_physics function on main.c), or "opencl" for a live frame rendering using OpenCL.
4. Filename for image: If the third argument is image you will need to pass the desired filename to be saved as a JPEG, WARNING: it will overwrite any other jpeg with the same name.
5. Antialliasing: If you pass nothing as the fifth argument it does not use the antialliasing and if you pass anything it uses. The only reason to not use it is if you want the live mode to render faster, it will probably run 4x faster without antialliasing. The image mode and the opencl mode are always using antialliasing, you can change it in the code if you want to.

As an example you if you run

```bash
./main file scene.json image image0001
```

The program will save an image named image0001 based on the scene on scene.json file,

And

```bash
./main file scene.json opencl
```

Will run the opencl live rendering.

As a general warning this program is fairly resource intensive, it will probably not crash your device but i think it might stop responding sometimes if you run it on a old computer or something like that.

### OpenCL live rendering

The OpenCL live rendering mode supports a simple movimentation system, you can move with WASD and rotate you camera with the directional arrows, the rotation is not correct currently so you can spin around in some weird ways and the movimentation is not relative to camera position so "W" always moves you foward in just one axis.

### Editing the scene

To make the json file i recommend simply copying the "scene.json" file on the repository and editing it, since my parsing algorithm is very simple and will break if the format is not roughly the same as there.
