#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * Broadphase solver. Every method takes an immutable reference, as it is designed to be concurrent.
 */
typedef struct BroadSolver BroadSolver;

/**
 * An rectangular entity.
 */
typedef struct FazoEntity {
    uint64_t id;
    float x;
    float y;
    float width;
    float height;
    float radius;
} FazoEntity;

/**
 * An rectangular query region.
 */
typedef struct FazoQuery {
    float x;
    float y;
    float width;
    float height;
} FazoQuery;

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

    void FazoFree(const struct FazoEntity* v);

    void FazoSolverClear(struct BroadSolver* this_);

    bool FazoSolverDelete(struct BroadSolver* this_, uint64_t id);

    void FazoSolverFree(struct BroadSolver* this_);

    void FazoSolverInsert(struct BroadSolver* this_, const struct FazoEntity* entity);

    size_t FazoSolverMemoryUsage(struct BroadSolver* this_);

    void FazoSolverMutate(struct BroadSolver* this_, const struct FazoEntity* new_entity);

    struct BroadSolver* FazoSolverNew(uint32_t width, uint32_t height, uint32_t magic);

    size_t FazoSolverSolve(struct BroadSolver* this_,
        const struct FazoQuery* query,
        const struct FazoEntity** dst);

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus
