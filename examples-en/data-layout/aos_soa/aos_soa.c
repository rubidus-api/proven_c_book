/* The same particles laid out three ways, and two jobs done on each.
   ★ This program measures time, so its numbers differ by machine and by run.
     Only ratios are printed --- only the conclusion is meant to last. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* about a million particles: 64 MiB as AoS, far bigger than the cache */
#define N      (1u << 20)
#define ROUNDS 5
#define PICKS  (1u << 18)

/* (1) AoS: one struct per particle, everything together --- 64 bytes, one cache line */
struct particle {
    float x, y, z, vx, vy, vz;
    float mass, charge;
    uint32_t flags, owner;
    char name[24];
};
static_assert(sizeof(struct particle) == 64);

/* (2) hot/cold split: what the update loop needs, apart from what it does not */
struct hot  { float x, y, z, vx, vy, vz; };          /* 24 bytes */
struct cold { float mass, charge; uint32_t flags, owner; char name[24]; };

/* (3) SoA: one array per field */
struct particles { float *x, *y, *z, *vx, *vy, *vz, *mass; };

static struct particle *aos;
static struct hot *hot;
static struct cold *cold;
static struct particles soa;
static uint32_t *pick;
static volatile float sink;

static double now(void) { return (double)clock() / CLOCKS_PER_SEC; }

/* job A: move every particle --- touches 6 of the 16 fields */
static double move_aos(void)
{
    double t = now();
    for (uint32_t i = 0; i < N; i++)
        { aos[i].x += aos[i].vx; aos[i].y += aos[i].vy; aos[i].z += aos[i].vz; }
    return now() - t;
}
static double move_hot(void)
{
    double t = now();
    for (uint32_t i = 0; i < N; i++)
        { hot[i].x += hot[i].vx; hot[i].y += hot[i].vy; hot[i].z += hot[i].vz; }
    return now() - t;
}
static double move_soa(void)
{
    double t = now();
    for (uint32_t i = 0; i < N; i++) soa.x[i] += soa.vx[i];
    for (uint32_t i = 0; i < N; i++) soa.y[i] += soa.vy[i];
    for (uint32_t i = 0; i < N; i++) soa.z[i] += soa.vz[i];
    return now() - t;
}

/* job B: look at scattered particles, all of their fields at once */
static double look_aos(void)
{
    double t = now(); float acc = 0;
    for (uint32_t k = 0; k < PICKS; k++) { const struct particle *p = &aos[pick[k]];
        acc += p->x + p->y + p->z + p->vx + p->vy + p->vz + p->mass; }
    sink = acc; return now() - t;
}
static double look_soa(void)
{
    double t = now(); float acc = 0;
    for (uint32_t k = 0; k < PICKS; k++) { uint32_t i = pick[k];
        acc += soa.x[i] + soa.y[i] + soa.z[i] + soa.vx[i] + soa.vy[i] + soa.vz[i]
             + soa.mass[i]; }
    sink = acc; return now() - t;
}

static int cmp(const void *a, const void *b)
{ double x = *(const double *)a, y = *(const double *)b; return (x > y) - (x < y); }
static double median(double (*job)(void))
{
    double r[ROUNDS];
    job();                                        /* warm up; thrown away */
    for (int i = 0; i < ROUNDS; i++) r[i] = job();
    qsort(r, ROUNDS, sizeof *r, cmp);
    return r[ROUNDS / 2];
}

int main(void)
{
    aos = calloc(N, sizeof *aos);
    hot = calloc(N, sizeof *hot); cold = calloc(N, sizeof *cold);
    float **f[] = { &soa.x, &soa.y, &soa.z, &soa.vx, &soa.vy, &soa.vz, &soa.mass };
    for (size_t k = 0; k < sizeof f / sizeof f[0]; k++) *f[k] = calloc(N, sizeof(float));
    pick = malloc(PICKS * sizeof *pick);
    if (!aos || !hot || !cold || !pick || !soa.x || !soa.y || !soa.z
        || !soa.vx || !soa.vy || !soa.vz || !soa.mass)
    { puts("out of memory"); return 1; }

    uint64_t rng = 0x9E3779B97F4A7C15u;
    for (uint32_t k = 0; k < PICKS; k++) {
        rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17;
        pick[k] = (uint32_t)(rng >> 32) % N;
    }
    for (uint32_t i = 0; i < N; i++) {
        float v = (float)(i % 7) * 0.25f;
        aos[i].vx = aos[i].vy = aos[i].vz = v;
        hot[i].vx = hot[i].vy = hot[i].vz = v;
        soa.vx[i] = soa.vy[i] = soa.vz[i] = v;
    }

    double a = median(move_aos), h = median(move_hot), s = median(move_soa);
    double la = median(look_aos), ls = median(look_soa);

    printf("%u particles; a struct is %zu bytes, a hot part %zu bytes\n\n",
           N, sizeof *aos, sizeof *hot);
    puts("job A: move every particle (6 of the 16 fields)");
    if (s > 0 && h > 0) {
        printf("  AoS took            %.1f times as long as SoA\n", a / s);
        printf("  hot/cold split took %.1f times as long as SoA\n", h / s);
    } else puts("  the clock is too coarse on this machine to tell them apart");
    puts("job B: read every field of particles picked at random");
    if (la > 0) printf("  SoA took            %.1f times as long as AoS\n", ls / la);
    else puts("  the clock is too coarse on this machine to tell them apart");
    puts("\n(ratios only: they change with the machine, the compiler and the run.)");

    free(aos); free(hot); free(cold); free(pick);
    for (size_t k = 0; k < sizeof f / sizeof f[0]; k++) free(*f[k]);
    return 0;
}
