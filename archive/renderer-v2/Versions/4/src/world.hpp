#ifndef WORLD_H
#define WORLD_H
#include "types.hpp"
#include "fixed.hpp"
#include "display.hpp"
#include "custom_math.hpp"
#include <cstring>
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>

#define BLOCK_SIZE 32
#define MAX_QUADS 1024

class Camera
{
private:
    RenderQuad quads[MAX_QUADS];
    size_t quad_count = 0;
    void SortQuads();
    fVector3 fposition;
    fVector2 fangle;
public:
    static Camera *current;
    Vector3<int> position;
    int angle;
    Camera(Vector3<int> position, int angle);
    void UpdateCoordinates();
    void CalculateProjection(Vertex* vertice, fVector3 offset);
    void AddQuad(Vertex *points[4]);
    void Render();
};

class Block
{
public:
    virtual void Render() = 0;
    virtual void CalculateProjection(fVector3 offset) = 0;
};

class Cube : public Block
{
public:
    fVector2 uv_start, uv_end;
    Vertex vertices[8];
    Cube();
    void Render() override;
    void CalculateProjection(fVector3 offset) override;
};

class World
{
private:
    uint8_t *ids;
    int width;
    int height;
    int depth;
public:
    Block **blocks;
    Vector3<int> offset;
    World(int width, int height, int depth, Block *blocks[]);
    ~World();

    Block *get_block(int x, int y, int z);
    Block *get_block_unsafe(int x, int y, int z);
    void set_block(int x, int y, int z, Block *block);
    void set_block_unsafe(int x, int y, int z, Block *block);

    int get_id_unsafe(int x, int y, int z);
    void set_id_unsafe(int x, int y, int z, int id);

    bool DetectInRange(fVector3 position);
    
    void Render();
};


#endif