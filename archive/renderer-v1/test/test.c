#include <stdio.h>

typedef struct {
    float x, y;
} Vector2;

// Fonction pour multiplier une matrice 3x3 par un vecteur 3D
void multiplyMatrixVector(float matrix[3][3], Vector2 vec, Vector2* result) {
    float w = matrix[0][2] * vec.x + matrix[1][2] * vec.y + matrix[2][2];
    result->x = (matrix[0][0] * vec.x + matrix[0][1] * vec.y + matrix[0][2]) / w;
    result->y = (matrix[1][0] * vec.x + matrix[1][1] * vec.y + matrix[1][2]) / w;
}

// Fonction pour résoudre le système d'équations et calculer la matrice de transformation
void computePerspectiveMatrix(Vector2 src[4], Vector2 dest[4], float matrix[3][3]) {
        float A[3][3] = {
        {src[0].x, src[1].x, src[2].x},
        {src[0].y, src[1].y, src[2].y},
        {1, 1, 1}
    };
    float B[3][3] = {
        {dest[0].x, dest[1].x, dest[2].x},
        {dest[0].y, dest[1].y, dest[2].y},
        {1, 1, 1}
    };

    float invA[3][3];
    float C[3][3];
    float D[3][3];

    // Calcul de l'inverse de A
    float det = A[0][0] * (A[1][1] * A[2][2] - A[1][2] * A[2][1]) -
                A[0][1] * (A[1][0] * A[2][2] - A[1][2] * A[2][0]) +
                A[0][2] * (A[1][0] * A[2][1] - A[1][1] * A[2][0]);
    float invdet = 1 / det;

    invA[0][0] = (A[1][1] * A[2][2] - A[1][2] * A[2][1]) * invdet;
    invA[0][1] = (A[0][2] * A[2][1] - A[0][1] * A[2][2]) * invdet;
    invA[0][2] = (A[0][1] * A[1][2] - A[0][2] * A[1][1]) * invdet;
    invA[1][0] = (A[1][2] * A[2][0] - A[1][0] * A[2][2]) * invdet;
    invA[1][1] = (A[0][0] * A[2][2] - A[0][2] * A[2][0]) * invdet;
    invA[1][2] = (A[0][2] * A[1][0] - A[0][0] * A[1][2]) * invdet;
    invA[2][0] = (A[1][0] * A[2][1] - A[1][1] * A[2][0]) * invdet;
    invA[2][1] = (A[0][1] * A[2][0] - A[0][0] * A[2][1]) * invdet;
    invA[2][2] = (A[0][0] * A[1][1] - A[0][1] * A[1][0]) * invdet;

    // Calcul de la matrice de transformation
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            C[i][j] = 0;
            for (int k = 0; k < 3; k++) {
                C[i][j] += B[i][k] * invA[k][j];
            }
        }
    }

    // Assigner la matrice de transformation
    matrix[0][0] = C[0][0];
    matrix[0][1] = C[0][1];
    matrix[0][2] = C[0][2];
    matrix[1][0] = C[1][0];
    matrix[1][1] = C[1][1];
    matrix[1][2] = C[1][2];
    matrix[2][0] = C[2][0];
    matrix[2][1] = C[2][1];
    matrix[2][2] = C[2][2];
}

// Fonction principale de transformation de perspective
Vector2 transform(Vector2 src[4], Vector2 dest[4], Vector2 pos) {
    float matrix[3][3];
    computePerspectiveMatrix(src, dest, matrix);

    Vector2 result;
    multiplyMatrixVector(matrix, pos, &result);
    return result;
}

int main() {
    Vector2 src[4] = {
        {0, 0}, {10, 0}, {10, 1}, {0, 10}
    };
    Vector2 dest[4] = {
        {100, 100}, {200, 100}, {200, 200}, {100, 200}
    };
    Vector2 pos = {5, 5};

    Vector2 transformed = transform(src, dest, pos);
    printf("Transformed Position: (%.2f, %.2f)\n", transformed.x, transformed.y);

    return 0;
}
