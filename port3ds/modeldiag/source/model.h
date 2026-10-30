#pragma once
#include <stdbool.h>
bool model_load(const char *path);
void model_draw(void);
void model_turn(void);
void model_free(void);
const char *model_status(void);
unsigned model_triangles(void);
