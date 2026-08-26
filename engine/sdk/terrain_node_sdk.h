/*
 * Terrain Studio node SDK — the plugin boundary.
 *
 * A plugin is a shared library that exports `terrainPluginAbiVersion` and
 * `terrainRegisterPlugin`. It declares its nodes with the same descriptor
 * structures the engine's own built-in nodes use, so this ABI cannot rot:
 * the engine is its own first plugin client.
 *
 * The header is C, deliberately, and version 1 is frozen. Nothing below moves:
 * no field is reordered or removed, no signature is altered, no enum value is
 * reused. A later revision may only append — new enum values, new function
 * pointers at the end of TerrainContextApi — and it arrives as version 2,
 * which the engine will accept alongside version 1. A plugin compiled against
 * version 1 therefore keeps working without being rebuilt.
 *
 * The engine refuses any version it does not know, with a message naming both
 * numbers. It never loads a plugin it cannot vouch for and never skips one
 * silently.
 *
 * ---------------------------------------------------------------------------
 * THE DETERMINISM CONTRACT BINDS PLUGINS
 *
 * The same graph, seed and resolution must produce bit-identical output, on
 * every run and at every thread count. A node that breaks this breaks the
 * whole tool, and the engine cannot check it for you. Therefore:
 *
 *   - Randomness comes only from `randomSeed` and the inline generator below.
 *     Never `rand()`, never a thread id, never an address.
 *   - Iteration counts are fixed. Never iterate until a residual converges,
 *     and never budget work by elapsed time.
 *   - Never read a clock.
 *   - Where parallel workers scatter into shared cells, accumulate through
 *     integer atomics on a fixed-point encoding. Float atomics reorder, and
 *     float addition is not associative.
 *   - Do not carry state between evaluations. A node is a pure function of
 *     its inputs, its parameters, and its seed.
 *
 * A node that offers a GPU path takes on the same obligation for it, and it
 * is harder than it looks: the same graph must give the same bits whichever
 * path ran. Vulkan does not round 32-bit division or square root correctly,
 * denormal handling cannot be pinned on every vendor, and a device may fuse a
 * multiply-add the host computed separately. The built-in erosion nodes solve
 * all three — see ARCHITECTURE.md §9 — and are worth reading before writing a
 * fourth. Prove agreement by measurement rather than by capability bit, and
 * fall back to the CPU when a device cannot be vouched for.
 * ---------------------------------------------------------------------------
 */

#ifndef TERRAIN_NODE_SDK_H
#define TERRAIN_NODE_SDK_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TERRAIN_SDK_ABI_VERSION 1

/* ---------------------------------------------------------------- types -- */

typedef enum TerrainPortType {
    TerrainPortType_Field = 0,  /* float32 grid: heightfield or mask */
    TerrainPortType_Color = 1,  /* RGB grid */
    TerrainPortType_Points = 2  /* instance list */
} TerrainPortType;

/* A tag on a field port. Storage is identical either way; it only tells the
   viewport whether to draw a surface or drape a mask over one. */
typedef enum TerrainFieldSemantics {
    TerrainFieldSemantics_Heightfield = 0,
    TerrainFieldSemantics_Mask = 1
} TerrainFieldSemantics;

typedef enum TerrainParameterType {
    TerrainParameterType_Float = 0,
    TerrainParameterType_Int = 1,
    TerrainParameterType_Bool = 2,
    TerrainParameterType_Enum = 3,
    TerrainParameterType_Seed = 4,
    TerrainParameterType_ColorRamp = 5,
    TerrainParameterType_Path = 6,
    TerrainParameterType_Text = 7,
    /* A transfer curve, read as text: "x,y;x,y;..." with both coordinates in
       [0, 1] and the points sorted by x. See terrain/curve.h. */
    TerrainParameterType_Curve = 8
} TerrainParameterType;

typedef struct TerrainEnumOption {
    const char* name;
    const char* label;
    int32_t value;
} TerrainEnumOption;

typedef struct TerrainPortDescriptor {
    const char* name;
    const char* description;
    TerrainPortType type;
    TerrainFieldSemantics fieldSemantics;
    int32_t isOptional; /* an unconnected optional input reads as absent */
} TerrainPortDescriptor;

typedef struct TerrainParameterDescriptor {
    const char* name;
    const char* label;
    const char* description;
    const char* unit; /* "m", "deg", "" — physical units wherever possible */
    TerrainParameterType type;
    double defaultValue;
    double minimumValue;
    double maximumValue;
    const char* defaultText; /* Path, Text and ColorRamp defaults */
    const TerrainEnumOption* enumOptions;
    int32_t enumOptionCount;
} TerrainParameterDescriptor;

/* ----------------------------------------------------------- evaluation -- */

typedef struct TerrainEvaluationContext TerrainEvaluationContext;

/*
 * One slice of a node's own work. `laneCount` is chosen by the node and is the
 * same on every machine, so a lane always covers exactly the same work; how
 * many threads the engine spends on those lanes is a speed decision only. A
 * lane must write only into storage no other lane writes, or accumulate
 * through integer atomics.
 */
typedef void (*TerrainLaneTask)(void* userData, int32_t laneIndex, int32_t laneCount);

/*
 * One scattered instance. Positions are in world meters on the graph's frame,
 * with easting growing towards the east and northing towards the north.
 *
 * The engine sorts a finished set by `id`, so ids must be unique within a set
 * and must come from the seed rather than from a running counter — a counter
 * would renumber everything the moment one point was rejected.
 */
typedef struct TerrainPoint {
    float eastingMeters;
    float northingMeters;
    float altitudeMeters;
    float rotationDegrees;
    float scale;
    uint64_t id;
} TerrainPoint;

/*
 * Everything a node may reach. No other engine internals are visible.
 * All fields in a graph share the same square grid, so width equals height.
 */
typedef struct TerrainContextApi {
    int32_t (*width)(TerrainEvaluationContext* context);
    int32_t (*height)(TerrainEvaluationContext* context);
    double (*cellSizeMeters)(TerrainEvaluationContext* context);

    /* The graph's source georeference, carried from import to export
       untouched. The coordinate system is an empty string when absent. */
    const char* (*coordinateSystem)(TerrainEvaluationContext* context);
    double (*eastingMinMeters)(TerrainEvaluationContext* context);
    double (*northingMaxMeters)(TerrainEvaluationContext* context);

    int32_t (*isInputConnected)(TerrainEvaluationContext* context, int32_t portIndex);
    /* Read-only view of an upstream result, or NULL if the port is unconnected. */
    const float* (*inputField)(TerrainEvaluationContext* context, int32_t portIndex);
    /* Writable output buffer of width * height floats, always non-NULL. */
    float* (*outputField)(TerrainEvaluationContext* context, int32_t portIndex);

    double (*parameterNumber)(TerrainEvaluationContext* context, int32_t parameterIndex);
    const char* (*parameterText)(TerrainEvaluationContext* context, int32_t parameterIndex);

    /* Derived from the graph seed and this node's stable id. */
    uint64_t (*randomSeed)(TerrainEvaluationContext* context);

    void (*reportProgress)(TerrainEvaluationContext* context, float fraction);
    int32_t (*isCancelled)(TerrainEvaluationContext* context);

    /* True only for a full-resolution build. Exporters must not write files
       during interactive preview evaluation. */
    int32_t (*isFinalBuild)(TerrainEvaluationContext* context);

    /* Aborts the evaluation with a message the UI shows on the node. */
    void (*setError)(TerrainEvaluationContext* context, const char* message);

    /* Runs `laneCount` lanes of the node's own work and returns once all of
       them have finished. The engine picks the thread count; the node picks
       the lane count, which is what the result may depend on. */
    void (*runLanes)(TerrainEvaluationContext* context, int32_t laneCount, TerrainLaneTask task,
                     void* userData);

    /* Colour ports. Three floats per cell, red green blue, row-major on the
       same grid as a field, linear and conventionally in [0, 1]. NULL from
       `inputColor` when the port is unconnected. */
    const float* (*inputColor)(TerrainEvaluationContext* context, int32_t portIndex);
    float* (*outputColor)(TerrainEvaluationContext* context, int32_t portIndex);

    /* Point ports. Reading gives the upstream set in its canonical order;
       `emitPoint` appends to an output set, which the engine sorts by id once
       the node returns. */
    int32_t (*inputPointCount)(TerrainEvaluationContext* context, int32_t portIndex);
    const TerrainPoint* (*inputPoints)(TerrainEvaluationContext* context, int32_t portIndex);
    void (*emitPoint)(TerrainEvaluationContext* context, int32_t portIndex,
                      const TerrainPoint* point);
} TerrainContextApi;

typedef void (*TerrainNodeEvaluate)(const TerrainContextApi* api,
                                    TerrainEvaluationContext* context);

typedef struct TerrainNodeDescriptor {
    const char* typeName;
    const char* category;
    const char* description;
    /* Bump when the node's output changes for the same inputs. It is part of
       the cache key, so a stale cached result cannot survive the change. */
    uint32_t version;

    const TerrainPortDescriptor* inputs;
    int32_t inputCount;
    const TerrainPortDescriptor* outputs;
    int32_t outputCount;
    const TerrainParameterDescriptor* parameters;
    int32_t parameterCount;

    TerrainNodeEvaluate evaluate;
} TerrainNodeDescriptor;

/* --------------------------------------------------------- registration -- */

typedef struct TerrainRegistry TerrainRegistry;

/* Returns 1 on success, 0 if the type name is already taken. The registry
   copies everything the descriptor points at, so a plugin may free its own
   storage as soon as registration returns. */
typedef int32_t (*TerrainRegisterNodeFn)(TerrainRegistry* registry,
                                         const TerrainNodeDescriptor* descriptor);

typedef struct TerrainRegistryApi {
    uint32_t abiVersion;
    TerrainRegisterNodeFn registerNode;
} TerrainRegistryApi;

/*
 * Every plugin exports exactly these two symbols, with C linkage and default
 * visibility.
 *
 * `terrainPluginAbiVersion` returns TERRAIN_SDK_ABI_VERSION as the plugin saw
 * it at compile time. The engine reads it before anything else and refuses the
 * plugin outright if it does not know that version.
 *
 * `terrainRegisterPlugin` declares the plugin's nodes through
 * `api->registerNode` and returns 1 when it is willing to be loaded, 0 to
 * refuse itself. Refusing is not a failure — a plugin that finds its own
 * prerequisites missing should say so here rather than register a node that
 * cannot evaluate. A node whose type name is already taken is rejected by
 * `registerNode`, which returns 0 for that node alone.
 */
uint32_t terrainPluginAbiVersion(void);
int32_t terrainRegisterPlugin(const TerrainRegistryApi* api, TerrainRegistry* registry);

typedef uint32_t (*TerrainPluginAbiVersionFn)(void);
typedef int32_t (*TerrainRegisterPluginFn)(const TerrainRegistryApi* api,
                                           TerrainRegistry* registry);

/* ----------------------------------------------------------- randomness -- */

/*
 * The engine's own generator, inline so a plugin and a built-in draw the same
 * sequence from the same seed. splitmix64: one multiply-xor chain, no state
 * beyond the counter, and no dependence on library or platform.
 */
static inline uint64_t terrainRandomNext(uint64_t* state) {
    uint64_t value = (*state += 0x9E3779B97F4A7C15ull);
    value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ull;
    value = (value ^ (value >> 27)) * 0x94D049BB133111EBull;
    return value ^ (value >> 31);
}

/* Uniform in [0, 1). 24 bits of mantissa, which is all a float carries. */
static inline float terrainRandomUnitFloat(uint64_t* state) {
    return (float)(terrainRandomNext(state) >> 40) * (1.0f / 16777216.0f);
}

/* Mixes a seed with a lane index so parallel workers stay reproducible
   without depending on how the work was divided. */
static inline uint64_t terrainSeedForLane(uint64_t seed, uint64_t lane) {
    uint64_t state = seed ^ (lane * 0x9E3779B97F4A7C15ull);
    return terrainRandomNext(&state);
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif /* TERRAIN_NODE_SDK_H */
