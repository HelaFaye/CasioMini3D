#ifndef WARPPERSPECTIVE_H
#define WARPPERSPECTIVE_H

#include <stdio.h>
#include <math.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

void getPerspectiveTransform2(float src[4][2], float dst[4][2], float M[3][3]);
void getPerspectiveTransform(float src[4][2], float dst[4][2], float M[3][3]);

void printMatrix(float M[3][3]);

SDL_Surface* warpPerspective(SDL_Surface* src, float M[3][3]);

SDL_Surface* resize(SDL_Surface* src, int w, int h);

SDL_Surface* RotateAngle(SDL_Surface* src, int degre);

SDL_Surface* crop(SDL_Surface* src);
SDL_Surface* cropRect(SDL_Surface* src,int x,int y,int w,int h);
SDL_Surface* ToSquareSize(SDL_Surface* src,int w,int h);

#endif
