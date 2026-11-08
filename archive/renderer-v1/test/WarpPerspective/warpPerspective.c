#include "warpPerspective.h"
#include <math.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include "fixed.h"

SDL_Surface* crop(SDL_Surface* src){
	size_t border_size_w = src->w/10;
	size_t border_size_h = src->h/10;
	SDL_Surface* dst = SDL_CreateRGBSurface(0, src->w-2*border_size_w, src->h-2*border_size_h, 32, 0, 0, 0, 0);
	//printf("src->w = %u, dst->w = %u, border_size_w = %lu", src->w, dst->w, border_size_w);
	return dst;
	Uint32* src_pixels = src->pixels;
	Uint32* dst_pixels = dst->pixels;
	for (int i = 0; i < dst->h; i++){
		for (int j = 0; j < dst->w; j++){
			dst_pixels[i*dst->w+j] = src_pixels[(i+border_size_h)*src->w+j+border_size_w];
		}
	}
	return dst;
}

SDL_Surface* cropRect(SDL_Surface* src,int x,int y,int w,int h){
	SDL_Surface* dst = SDL_CreateRGBSurface(0, w, h, 32, 0, 0, 0, 0);
	Uint32* src_pixels = src->pixels;
	Uint32* dst_pixels = dst->pixels;
	for (int i = 0; i < dst->h; i++){
		for (int j = 0; j < dst->w; j++){
			dst_pixels[i*dst->w+j] = src_pixels[(i+y)*src->w+j+x];
		}
	}
	return dst;
}

void getPerspectiveTransform2(float src[4][2], float dst[4][2], float M[3][3])
{
	float x0 = src[0][0], x1 = src[1][0], x2 = src[2][0], x3 = src[3][0];
	float y0 = src[0][1], y1 = src[1][1], y2 = src[2][1], y3 = src[3][1];
	float u0 = dst[0][0], u1 = dst[1][0], u2 = dst[2][0], u3 = dst[3][0];
	float v0 = dst[0][1], v1 = dst[1][1], v2 = dst[2][1], v3 = dst[3][1];
	printf("x0 = %f, y0 = %f, u0 = %f, v0 = %f\n", x0, y0, u0, v0);

	float A[8][8] = {
		{x0, y0, 1, 0, 0, 0, -u0*x0, -u0*y0},
		{0, 0, 0, x0, y0, 1, -v0*x0, -v0*y0},
		{x1, y1, 1, 0, 0, 0, -u1*x1, -u1*y1},
		{0, 0, 0, x1, y1, 1, -v1*x1, -v1*y1},
		{x2, y2, 1, 0, 0, 0, -u2*x2, -u2*y2},
		{0, 0, 0, x2, y2, 1, -v2*x2, -v2*y2},
		{x3, y3, 1, 0, 0, 0, -u3*x3, -u3*y3},
		{0, 0, 0, x3, y3, 1, -v3*x3, -v3*y3}
	};
	//print A
	printf("\n A = \n");
	for (int i = 0; i < 8; i++){
		for (int j = 0; j < 8; j++){
			printf("%f ", A[i][j]);
		}
		printf("\n");
	}
	float b[8] = {u0, v0, u1, v1, u2, v2, u3, v3};
	float x[8];
	for (int i = 0; i < 8; i++){
		float max = fabs(A[i][i]);
		printf("max = A[%d][%d] = %f\n", i, i, max);
		int max_row = i;
		for (int j = i + 1; j < 8; j++){
			printf("A[%d][%d] = %f  max = %f\n", j, i, A[j][i], max);
			if (fabs(A[j][i]) > max){
				max = fabs(A[j][i]);
				max_row = j;
			}
		}
		printf("max = %f\n", max);
		for (int k = i; k < 8; k++){
			float tmp = A[i][k];
			A[i][k] = A[max_row][k];
			A[max_row][k] = tmp;
		}
		float tmp = b[i];
		b[i] = b[max_row];
		b[max_row] = tmp;
		for (int j = i + 1; j < 8; j++){
			float c = A[j][i] / A[i][i];
			printf("c = %f\n", c);
			for (int k = i; k < 8; k++){
				A[j][k] -= A[i][k] * c;
			}
			b[j] -= b[i] * c;
		}
	}
	for (int i = 7; i >= 0; i--){
		x[i] = b[i];
		for (int j = i + 1; j < 8; j++){
			x[i] -= A[i][j] * x[j];
		}
		x[i] /= A[i][i];
	}
	M[0][0] = x[0]; M[0][1] = x[1]; M[0][2] = x[2];
	M[1][0] = x[3]; M[1][1] = x[4]; M[1][2] = x[5];
	M[2][0] = x[6]; M[2][1] = x[7]; M[2][2] = 1;


}


/*void getPerspectiveTransform(float src[4][2], float dst[4][2], float M[3][3])
{
	fixed_t x0 = FLOAT_TO_FIXED(src[0][0]), x1 = FLOAT_TO_FIXED(src[1][0]), x2 = FLOAT_TO_FIXED(src[2][0]), x3 = FLOAT_TO_FIXED(src[3][0]);
	fixed_t y0 = FLOAT_TO_FIXED(src[0][1]), y1 = FLOAT_TO_FIXED(src[1][1]), y2 = FLOAT_TO_FIXED(src[2][1]), y3 = FLOAT_TO_FIXED(src[3][1]);
	fixed_t u0 = FLOAT_TO_FIXED(dst[0][0]), u1 = FLOAT_TO_FIXED(dst[1][0]), u2 = FLOAT_TO_FIXED(dst[2][0]), u3 = FLOAT_TO_FIXED(dst[3][0]);
	fixed_t v0 = FLOAT_TO_FIXED(dst[0][1]), v1 = FLOAT_TO_FIXED(dst[1][1]), v2 = FLOAT_TO_FIXED(dst[2][1]), v3 = FLOAT_TO_FIXED(dst[3][1]);
	printf("x0 = %f, y0 = %f, u0 = %f, v0 = %f\n", TO_FLOAT(x0), TO_FLOAT(y0), TO_FLOAT(u0), TO_FLOAT(v0));
	fixed_t A[8][8] = {
		{x0, y0, 1, 0, 0, 0, -fmul(u0,x0), -fmul(u0,y0)},
		{0, 0, 0, x0, y0, 1, -fmul(v0,x0), -fmul(v0,y0)},
		{x1, y1, 1, 0, 0, 0, -fmul(u1,x1), -fmul(u1,y1)},
		{0, 0, 0, x1, y1, 1, -fmul(v1,x1), -fmul(v1,y1)},
		{x2, y2, 1, 0, 0, 0, -fmul(u2,x2), -fmul(u2,y2)},
		{0, 0, 0, x2, y2, 1, -fmul(v2,x2), -fmul(v2,y2)},
		{x3, y3, 1, 0, 0, 0, -fmul(u3,x3), -fmul(u3,y3)},
		{0, 0, 0, x3, y3, 1, -fmul(v3,x3), -fmul(v3,y3)}
	};
	//print A
	printf("\n A = \n");
	for (int i = 0; i < 8; i++){
		for (int j = 0; j < 8; j++){
			printf("%f ", TO_FLOAT(A[i][j]));
		}
		printf("\n");
	}
	fixed_t b[8] = {u0, v0, u1, v1, u2, v2, u3, v3};
	fixed_t x[8];
	for (int i = 0; i < 8; i++){
		fixed_t max = fixed_abs(A[i][i]);
		printf("max = A[%d][%d] = %f\n", i, i, TO_FLOAT(max));
		int max_row = i;
		for (int j = i + 1; j < 8; j++){
			printf("A[%d][%d] = %f  max = %f\n", j, i, TO_FLOAT(A[j][i]), TO_FLOAT(max));
			if (fixed_abs(A[j][i]) > max){
				max = fixed_abs(A[j][i]);
				max_row = j;
			}
		}
		printf("max = %f\n", TO_FLOAT(max));
		for (int k = i; k < 8; k++){
			fixed_t tmp = A[i][k];
			A[i][k] = A[max_row][k];
			A[max_row][k] = tmp;
		}
		fixed_t tmp = b[i];
		b[i] = b[max_row];
		b[max_row] = tmp;
		for (int j = i + 1; j < 8; j++){
			fixed_t c = fdiv(A[j][i], A[i][i]);
			printf("c = %f\n", TO_FLOAT(c));
			for (int k = i; k < 8; k++){
				A[j][k] -= fmul(A[i][k], c);
			}
			b[j] -= fmul(b[i], c);
		}
	}
	for (int i = 7; i >= 0; i--){
		x[i] = b[i];
		for (int j = i + 1; j < 8; j++){
			x[i] -= fmul(A[i][j], x[j]);
		}
		x[i] = fdiv(x[i], A[i][i]);
	}
	M[0][0] = TO_FLOAT(x[0]); M[0][1] = TO_FLOAT(x[1]); M[0][2] = TO_FLOAT(x[2]);
	M[1][0] = TO_FLOAT(x[3]); M[1][1] = TO_FLOAT(x[4]); M[1][2] = TO_FLOAT(x[5]);
	M[2][0] = TO_FLOAT(x[6]); M[2][1] = TO_FLOAT(x[7]); M[2][2] = 1;


}*/

void getPerspectiveTransform3(float src[4][2], float dst[4][2], float M[3][3]) {
    float A[8][8] = {0}; // Matrice pour résoudre les équations
    float B[8] = {0};    // Matrice des résultats

    // Construction de la matrice A et du vecteur B
    for (int i = 0; i < 4; ++i) {
        float x = src[i][0];
        float y = src[i][1];
        float x_ = dst[i][0];
        float y_ = dst[i][1];

        A[2*i][0] = x;
        A[2*i][1] = y;
        A[2*i][2] = 1;
        A[2*i][6] = -x * x_;
        A[2*i][7] = -y * x_;
        A[2*i+1][3] = x;
        A[2*i+1][4] = y;
        A[2*i+1][5] = 1;
        A[2*i+1][6] = -x * y_;
        A[2*i+1][7] = -y * y_;

        B[2*i] = x_;
        B[2*i+1] = y_;
    }

    // Simplification pour résoudre le système d'équations
    for (int i = 0; i < 8; ++i) {
        // Trouver le pivot non nul
        int max_row = i;
        for (int k = i + 1; k < 8; ++k) {
            if (fabs(A[k][i]) > fabs(A[max_row][i])) {
                max_row = k;
            }
        }

        // Échanger les lignes si nécessaire
        if (i != max_row) {
            for (int j = 0; j < 8; ++j) {
                float temp = A[i][j];
                A[i][j] = A[max_row][j];
                A[max_row][j] = temp;
            }
            float temp = B[i];
            B[i] = B[max_row];
            B[max_row] = temp;
        }

        // Normaliser la ligne du pivot
        float pivot = A[i][i];
        if (fabs(pivot) < 1e-6) {
            printf("Erreur : pivot trop proche de zéro\n");
            return;
        }
        for (int j = i; j < 8; ++j) {
            A[i][j] /= pivot;
        }
        B[i] /= pivot;

        // Élimination des autres lignes
        for (int k = 0; k < 8; ++k) {
            if (k != i) {
                float factor = A[k][i];
                for (int j = i; j < 8; ++j) {
                    A[k][j] -= factor * A[i][j];
                }
                B[k] -= factor * B[i];
            }
        }
    }

    // Remplir la matrice de perspective
    M[0][0] = B[0];
    M[0][1] = B[1];
    M[0][2] = B[2];
    M[1][0] = B[3];
    M[1][1] = B[4];
    M[1][2] = B[5];
    M[2][0] = B[6];
    M[2][1] = B[7];
    M[2][2] = 1.0f;
}

void getPerspectiveTransform(float src[4][2], float dst[4][2], float M[3][3]) {
    fixed_t A[8][8] = {0}; // Matrice pour résoudre les équations
    fixed_t B[8] = {0};    // Vecteur des résultats

    // Construction de la matrice A et du vecteur B
    for (int i = 0; i < 4; ++i) {
        fixed_t x = FLOAT_TO_FIXED(src[i][0]);
        fixed_t y = FLOAT_TO_FIXED(src[i][1]);
        fixed_t x_ = FLOAT_TO_FIXED(dst[i][0]);
        fixed_t y_ = FLOAT_TO_FIXED(dst[i][1]);

        A[2*i][0] = x;
        A[2*i][1] = y;
        A[2*i][2] = FIXED_ONE;
        A[2*i][6] = -fmul(x, x_);
        A[2*i][7] = -fmul(y, x_);
        A[2*i+1][3] = x;
        A[2*i+1][4] = y;
        A[2*i+1][5] = FIXED_ONE;
        A[2*i+1][6] = -fmul(x, y_);
        A[2*i+1][7] = -fmul(y, y_);

        B[2*i] = x_;
        B[2*i+1] = y_;
    }

    // Résoudre le système d'équations A * H = B
    for (int i = 0; i < 8; ++i) {
        // Trouver le pivot non nul
        int max_row = i;
        for (int k = i + 1; k < 8; ++k) {
            if (fixed_abs(A[k][i]) > fixed_abs(A[max_row][i])) {
                max_row = k;
            }
        }

        // Échanger les lignes si nécessaire
        if (i != max_row) {
            for (int j = 0; j < 8; ++j) {
                fixed_t temp = A[i][j];
                A[i][j] = A[max_row][j];
                A[max_row][j] = temp;
            }
            fixed_t temp = B[i];
            B[i] = B[max_row];
            B[max_row] = temp;
        }

        // Normaliser la ligne du pivot
        fixed_t pivot = A[i][i];
        if (pivot == 0)
			continue;
        for (int j = i; j < 8; ++j) {
            A[i][j] = fdiv(A[i][j], pivot);
        }
        B[i] = fdiv(B[i], pivot);

        // Élimination des autres lignes
        for (int k = 0; k < 8; ++k) {
            if (k != i) {
                fixed_t factor = A[k][i];
                for (int j = i; j < 8; ++j) {
                    A[k][j] -= fmul(factor, A[i][j]);
                }
                B[k] -= fmul(factor, B[i]);
            }
        }
    }

    // Remplir la matrice de perspective
    M[0][0] = TO_FLOAT(B[0]);
    M[0][1] = TO_FLOAT(B[1]);
    M[0][2] = TO_FLOAT(B[2]);
    M[1][0] = TO_FLOAT(B[3]);
    M[1][1] = TO_FLOAT(B[4]);
    M[1][2] = TO_FLOAT(B[5]);
    M[2][0] = TO_FLOAT(B[6]);
    M[2][1] = TO_FLOAT(B[7]);
    M[2][2] = 1;
}

void printMatrix(float M[3][3]){
	printf("\n");
	for (int i = 0; i < 3; i++){
		for (int j = 0; j < 3; j++){
			printf("%f ", M[i][j]);
		}
		printf("\n");
	}
}
//getPerspectiveTransform est une fonction qui permet de calculer la matrice de transformation
//pour une transformation perspective. Elle prend en parametre les coordonnees des points de la
//source et de la destination. Elle retourne la matrice de transformation M.
//la matrice de transformation sert a transformer les coordonnees des points de la source
//en coordonnees des points de la destination.
//la matrice de transformation represente une transformation affine


SDL_Surface* warpPerspective(SDL_Surface* src, float M[3][3])
{
	SDL_Surface* dst = SDL_CreateRGBSurface(0, src->w, src->h, 32, 0, 0, 0, 0);
	SDL_LockSurface(src);
	SDL_LockSurface(dst);
	Uint32* src_pixels = src->pixels;
	Uint32* dst_pixels = dst->pixels;
	for (int y = 0; y < src->h; y++){
		for (int x = 0; x < src->w; x++){
			float u = M[0][0] * x + M[0][1] * y + M[0][2];
			float v = M[1][0] * x + M[1][1] * y + M[1][2];
			float w = M[2][0] * x + M[2][1] * y + M[2][2];
			u /= w;
			v /= w;
			if (u < 0 || u >= src->w || v < 0 || v >= src->h){
				dst_pixels[y * src->w + x] = 0;
			}
			else{
				dst_pixels[y * src->w + x] = src_pixels[(int)v * src->w + (int)u];
			}
		}
	}
	SDL_UnlockSurface(src);
	SDL_UnlockSurface(dst);
	return dst;
}
//warpPerspective est une fonction qui permet de transformer une image source en une image destination
//en appliquant une transformation perspective. Elle prend en parametre l'image source et
//la matrice de transformation M. Elle retourne l'image destination.
//pour chaque pixel de l'image destination, on calcule les coordonnees du pixel correspondant
//dans l'image source. Si les coordonnees sont dans l'image source, on recupere la couleur du pixel
//dans l'image source et on la met dans l'image destination. Sinon, on met la couleur noire dans
//l'image destination.


SDL_Surface* resize(SDL_Surface* src, int w, int h)
{
	SDL_Surface* dst = SDL_CreateRGBSurface(0, w, h, 32, 0, 0, 0, 0);
	SDL_LockSurface(src);
	SDL_LockSurface(dst);
	Uint32* src_pixels = src->pixels;
	Uint32* dst_pixels = dst->pixels;
	for (int y = 0; y < h; y++){
		for (int x = 0; x < w; x++){
			float u = (float)x / w * src->w;
			float v = (float)y / h * src->h;
			dst_pixels[y * w + x] = src_pixels[(int)v * src->w + (int)u];
		}
	}
	SDL_UnlockSurface(src);
	SDL_UnlockSurface(dst);
	return dst;
}
//resize est une fonction qui permet de redimensionner une image. Elle prend en parametre
//l'image source et les dimensions de l'image destination. Elle retourne l'image destination.
//pour chaque pixel de l'image destination, on calcule les coordonnees du pixel correspondant
//dans l'image source. On recupere la couleur du pixel dans l'image source et on la met dans
//l'image destination.

//example:
//float src[4][2] = { { 0, 0 }, { surface->w, 0 }, { surface->w, surface->h }, { 0, surface->h } };
//x1,y1 is the top left corner of the image
//x2,y2 is the top right corner of the image
//x3,y3 is the bottom right corner of the image
//x4,y4 is the bottom left corner of the image
//float dst[4][2] = { { 61,71 }, { 161,30 }, { 270,142 }, { 151,218 } };
//float M[3][3];
//getPerspectiveTransform(src, dst, M);
//SDL_Surface* warped = warpPerspective(surface, M);
//SDL_Surface* resized = resize(warped, 400*0.6f, 400*0.7f);

//degree : degree%360 (true modulo)
int Mod(int x, int m) {
    return (x%m + m)%m;
}
SDL_Surface* RotateAngle(SDL_Surface* src, int degre)
{
	//in rotation, the size of the image is same as the size of the source image
	//rotation from the center of the image
	SDL_Surface* dst = SDL_CreateRGBSurface(0, src->w, src->h, 32, 0, 0, 0, 0);
	SDL_LockSurface(src);
	SDL_LockSurface(dst);
	degre = Mod(degre, 360);
	Uint32* src_pixels = src->pixels;
	Uint32* dst_pixels = dst->pixels;
	float angle = (float)degre * 3.14159265358979323846f / 180.0f;
	float cos_angle = cos(angle);
	float sin_angle = sin(angle);
	for (int y = 0; y < src->h; y++){
		for (int x = 0; x < src->w; x++){
			float u = (x - src->w / 2) * cos_angle - (y - src->h / 2) * sin_angle + src->w / 2;
			float v = (x - src->w / 2) * sin_angle + (y - src->h / 2) * cos_angle + src->h / 2;
			if (u < 0 || u >= src->w || v < 0 || v >= src->h){
				dst_pixels[y * src->w + x] = 0;
			}
			else{
				dst_pixels[y * src->w + x] = src_pixels[(int)v * src->w + (int)u];
			}
		}
	}
	SDL_UnlockSurface(src);
	SDL_UnlockSurface(dst);
	return dst;
}

SDL_Surface* ToSquareSize(SDL_Surface* src,int w,int h){
	SDL_Surface* resized;
	if (src->w > src->h){
		resized = resize(src, w, w * src->h / src->w);
	}
	else{
		resized = resize(src, h * src->w / src->h, h);
	}
	return resized;
}

/*SDL_Surface* ToSquareSize(SDL_Surface* src,int w,int h)
{
	//prend la taille max de l'image et la met dans une image carrée (les pixels en trop sont mis en noir)
	//resize aussi avec la taille max de l'image
	SDL_Surface* dst = SDL_CreateRGBSurface(0, w, h, 32, 0, 0, 0, 0);
	SDL_LockSurface(dst);
	Uint32* dst_pixels = dst->pixels;
	SDL_Surface* resized;
	if (src->w > src->h){
		resized = resize(src, w, w * src->h / src->w);
	}
	else{
		resized = resize(src, h * src->w / src->h, h);
	}
	SDL_LockSurface(resized);
	Uint32* src_pixels = resized->pixels;
	for (int y = 0; y < h; y++){
		for (int x = 0; x < w; x++){
			if (x < resized->w && y < resized->h){
				dst_pixels[y * w + x] = src_pixels[y * resized->w + x];
			}
			else{
				dst_pixels[y * w + x] = SDL_MapRGB(resized->format,255,255,255);
			}
		}
	}
	SDL_UnlockSurface(dst);
	SDL_UnlockSurface(resized);
	SDL_FreeSurface(resized);
	return dst;
}*/