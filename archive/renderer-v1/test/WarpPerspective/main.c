#include <err.h>
#include <stdio.h>
#include <math.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "warpPerspective.h"

SDL_Surface* load_image(const char* path)
{
	SDL_Surface *temp = IMG_Load(path);
	if (temp == NULL){
		// errx(EXIT_FAILURE, "%s", SDL_GetError());
		printf("ERROR : %s", SDL_GetError());
		exit(1);
	}
	SDL_Surface *o = SDL_ConvertSurfaceFormat(temp, SDL_PIXELFORMAT_RGB888, 0);
	SDL_FreeSurface(temp);
	return o;
}

int main(int argc, char** argv)
{
	// Checks the number of arguments.
	char* path;
	if (argc != 2){
		//errx(EXIT_FAILURE, "Usage: image-file");
		printf("Usage: image-file\n");
		path = "card.jpg";
		//exit(1);
	}
	else{
		path = argv[1];
	}
	/*// - Initialize the SDL.
	if (SDL_Init(SDL_INIT_VIDEO) != 0){
		errx(EXIT_FAILURE, "%s", SDL_GetError());
		printf("ERROR : %s", SDL_GetError());
		exit(1);
	}
	// - Create a window.
	SDL_Window* window = SDL_CreateWindow("Image", 0, 0, 10, 10, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
	if (window == NULL){
		errx(EXIT_FAILURE, "%s", SDL_GetError());
		printf("ERROR : %s", SDL_GetError());
		exit(1);
	}
	// - Create a renderer.
	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	if (renderer == NULL){
		errx(EXIT_FAILURE, "%s", SDL_GetError());
		printf("ERROR : %s", SDL_GetError());
		exit(1);
	}*/
	// - Create a surface from the colored image.
	SDL_Surface *surface = load_image(path);
	// - Create a texture from the colored surface.
	//SDL_Texture *colored_texture = SDL_CreateTextureFromSurface(renderer, surface);
	//WarpTransform
	float src[4][2] = { { 0, 0 }, { surface->w, 0 }, { surface->w, surface->h }, { 0, surface->h } };
	//x1,y1 is the top left corner of the image
	//x2,y2 is the top right corner of the image
	//x3,y3 is the bottom right corner of the image
	//x4,y4 is the bottom left corner of the image
	float dst[4][2] = { { 61,71 }, { 161,30 }, { 270,142 }, { 151,218 } };
	float M[3][3];

	getPerspectiveTransform(src, dst, M);
	SDL_Surface* new_surface = warpPerspective(surface, M);
	printf("\n\n\n\n\n\n\n");
	//getPerspectiveTransform2(dst, src, M);
	//printMatrix(M);
	printf("\n\n");
	getPerspectiveTransform(dst, src, M);
	printMatrix(M);
	SDL_Surface* new_surface2 = warpPerspective(new_surface, M);
	SDL_Surface* resized = resize(new_surface, 400*0.6f, 400*0.7f);
	// save surface as png
	resized = crop(resized);
	IMG_SavePNG(new_surface2, "out.png");

	// - Free the surface.
	SDL_FreeSurface(surface);
	SDL_FreeSurface(new_surface);
	SDL_FreeSurface(resized);
	// - Destroy the objects.
	//SDL_DestroyTexture(colored_texture);
	//SDL_DestroyTexture(grayscale_texture);
	//SDL_DestroyRenderer(renderer);
	//SDL_DestroyWindow(window);
	
	SDL_Quit();

	return EXIT_SUCCESS;
}
