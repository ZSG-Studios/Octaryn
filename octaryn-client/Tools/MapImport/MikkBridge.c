#include "mikktspace.h"
#include <stdint.h>
#include <string.h>

typedef struct Mesh {
    const float *positions, *normals, *uvs;
    const uint32_t *indices;
    float *tangents;
    int faces;
} Mesh;
static int faces(const SMikkTSpaceContext *c) { return ((Mesh *)c->m_pUserData)->faces; }
static int corners(const SMikkTSpaceContext *c, int f) { (void)c; (void)f; return 3; }
static void position(const SMikkTSpaceContext *c, float out[], int f, int v) {
    const Mesh *m = c->m_pUserData;
    memcpy(out, m->positions + 3 * m->indices[3 * f + v], 3 * sizeof(float));
}
static void normal(const SMikkTSpaceContext *c, float out[], int f, int v) {
    const Mesh *m = c->m_pUserData;
    memcpy(out, m->normals + 3 * m->indices[3 * f + v], 3 * sizeof(float));
}
static void uv(const SMikkTSpaceContext *c, float out[], int f, int v) {
    const Mesh *m = c->m_pUserData;
    memcpy(out, m->uvs + 2 * m->indices[3 * f + v], 2 * sizeof(float));
}
static void tangent(const SMikkTSpaceContext *c, const float t[], float sign, int f, int v) {
    Mesh *m = c->m_pUserData;
    float *out = m->tangents + 4 * (3 * f + v);
    memcpy(out, t, 3 * sizeof(float)); out[3] = sign;
}
#ifdef _WIN32
__declspec(dllexport)
#endif
int map_mikk(const float *positions, const float *normals, const float *uvs,
             const uint32_t *indices, unsigned index_count, float *tangents) {
    if (!index_count || index_count % 3 || index_count > 0x7fffffff) return 0;
    Mesh mesh = {positions, normals, uvs, indices, tangents, (int)(index_count / 3)};
    SMikkTSpaceInterface api = {faces, corners, position, normal, uv, tangent, 0};
    SMikkTSpaceContext context = {&api, &mesh};
    return genTangSpaceDefault(&context);
}
