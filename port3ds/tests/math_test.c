#include "port_math.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define PI 3.1415927f
#define TAU 6.2831855f

static int checks;
static void close_to(float actual, float expected)
{
    ++checks;
    if (fabsf(actual - expected) > 2.0e-5f) {
        fprintf(stderr, "Expected %.9g, got %.9g\n", expected, actual);
        assert(0);
    }
}

static float transformed(const PortMtx34 view, const PortVec3* v, int row)
{
    return view[row][0]*v->x + view[row][1]*v->y + view[row][2]*v->z + view[row][3];
}

static void camera_test(void)
{
    PortMtx34 view;
    PortVec3 eye = {4, 6, 10}, up = {0, 1, 0}, target = {1, 0, 0};
    assert(port_math_camera(view, &eye, &up, &target));
    for (int r = 0; r < 3; ++r) {
        close_to(transformed(view, &eye, r), 0);
        float length2 = 0;
        for (int c = 0; c < 3; ++c) length2 += view[r][c] * view[r][c];
        close_to(length2, 1);
        for (int other = 0; other < r; ++other) {
            float dot = 0;
            for (int c = 0; c < 3; ++c) dot += view[r][c] * view[other][c];
            close_to(dot, 0);
        }
    }
    close_to(transformed(view, &target, 0), 0);
    close_to(transformed(view, &target, 1), 0);
    assert(transformed(view, &target, 2) < 0);
    PortMtx34 previous;
    memcpy(previous, view, sizeof(view));
    assert(!port_math_camera(view, &eye, &up, &eye));
    assert(memcmp(previous, view, sizeof(view)) == 0);
    PortVec3 parallel = {3, 6, 10};
    assert(!port_math_camera(view, &eye, &parallel, &target));
    assert(!port_math_camera(view, NULL, &up, &target));
    PortVec3 invalid = {NAN, 0, 0};
    assert(!port_math_camera(view, &invalid, &up, &target));
    assert(memcmp(previous, view, sizeof(view)) == 0);
}

static void projection_test(void)
{
    PortMtx44 p;
    C_MTXPerspective(p, 90.0f, 2.0f, 1.0f, 101.0f);
    close_to(p[0][0], 0.5f);
    close_to(p[1][1], 1.0f);
    /* Original GameCube depth convention: near -1, far 0 after perspective. */
    close_to((-p[2][2] + p[2][3]), -1.0f);
    close_to((-101*p[2][2] + p[2][3]) / 101, 0.0f);
    C_MTXOrtho(p, 3, -3, -4, 4, 1, 101);
    close_to(4*p[0][0] + p[0][3], 1);
    close_to(3*p[1][1] + p[1][3], 1);
    close_to(-p[2][2] + p[2][3], -1);
    close_to(-101*p[2][2] + p[2][3], 0);
}

static void game_math_test(void)
{
    close_to(qdist2(1, 2, 4, 6), 5);
    close_to(qdist2(4, 6, 1, 2), 5);
    close_to(qdist2(1, 2, 1, 2), 0);
    PortVec3 a = {0, 20, 0}, b = {3, -20, 4};
    close_to(port_math_distance_xz(&a, &b), 5); /* Gameplay distance ignores height. */
    close_to(roundAng(-0.1f), TAU - 0.1f);
    close_to(roundAng(TAU + 0.1f), 0.1f);
    close_to(angDist(0.1f, TAU - 0.1f), 0.2f);
    close_to(angDist(TAU - 0.1f, 0.1f), -0.2f);
    close_to(port_math_turn_toward(TAU - 0.1f, 0.1f, 0.05f), TAU - 0.05f);
    close_to(port_math_turn_toward(0.1f, TAU - 0.1f, 0.05f), 0.05f);
    close_to(port_math_turn_toward(0.1f, 0.15f, 0.2f), 0.15f);
    float angle = 0;
    for (int frame = 0; frame < 100; ++frame) {
        float next = port_math_turn_toward(angle, PI, 0.04f);
        assert(fabsf(angDist(next, angle)) <= 0.04001f);
        angle = next;
    }
    close_to(angle, PI);
    close_to(port_math_turn_toward(12*TAU + 0.1f, -10*TAU + 0.15f, 0.2f), 0.15f);
}

static void concat_test(void)
{
    /* Rotate +90deg about z after translating x by 2; order must be preserved. */
    PortMtx34 rotate = {{0,-1,0,0},{1,0,0,0},{0,0,1,0}};
    PortMtx34 translate = {{1,0,0,2},{0,1,0,0},{0,0,1,0}};
    PortMtx34 combined;
    port_math_concat(combined, rotate, translate);
    PortVec3 input = {3,4,5};
    close_to(transformed(combined, &input, 0), -4);
    close_to(transformed(combined, &input, 1), 5);
    close_to(transformed(combined, &input, 2), 5);
    port_math_concat(rotate, rotate, translate);
    assert(memcmp(rotate, combined, sizeof(combined)) == 0);
    /* Right-hand alias is valid too. */
    PortMtx34 r = {{0,-1,0,0},{1,0,0,0},{0,0,1,0}};
    port_math_concat(translate, r, translate);
    assert(memcmp(translate, combined, sizeof(combined)) == 0);
}

int main(void)
{
    camera_test();
    projection_test();
    game_math_test();
    concat_test();
    printf("Original game math tests passed (%d numerical checks plus invariant assertions).\n", checks);
    return 0;
}
