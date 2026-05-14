#pragma once

typedef enum direction
{
    DIRECTION_NORTH,
    DIRECTION_SOUTH,
    DIRECTION_EAST,
    DIRECTION_WEST,
    DIRECTION_UP,
    DIRECTION_DOWN,
    DIRECTION_COUNT,
}
direction_t;

static const int DIRECTIONS[DIRECTION_COUNT][3] =
{
    { 0, 0, 1 },
    { 0, 0,-1 },
    { 1, 0, 0 },
    {-1, 0, 0 },
    { 0, 1, 0 },
    { 0,-1, 0 },
};
