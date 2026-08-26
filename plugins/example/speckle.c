/*
 * Terrain Studio example plugin — the template for writing your own.
 *
 * `Speckle` roughens a surface by nudging every cell up or down by a seeded
 * amount. It is deliberately the smallest node that still shows everything an
 * author has to get right: a typed input and output, an optional mask, a seed
 * parameter, parallel work through the engine's lanes, progress, cancellation,
 * and the determinism contract the SDK header sets out.
 *
 * Build it against the SDK header alone — it links nothing:
 *
 *     cmake -S plugins/example -B plugins/example/build
 *     cmake --build plugins/example/build
 *
 * and drop the result into a plugin directory. See PLUGINS.md.
 */

#include <string.h>

#include "terrain_node_sdk.h"

#if defined(_WIN32)
#define TERRAIN_PLUGIN_EXPORT __declspec(dllexport)
#else
#define TERRAIN_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

enum { surfaceInput = 0, maskInput = 1 };
enum { surfaceOutput = 0 };
enum { amplitudeParameter = 0, seedParameter = 1 };

/*
 * A fixed lane count, chosen here and not by the engine. This is the whole of
 * the parallel determinism rule: the engine decides how many threads to spend
 * on these lanes, but a lane always covers the same cells, so the answer
 * cannot depend on how much machine it was given.
 */
enum { speckleLaneCount = 64 };

typedef struct SpeckleWork {
    const TerrainContextApi* api;
    TerrainEvaluationContext* context;
    const float* input;
    const float* mask;
    float* output;
    int32_t width;
    int32_t height;
    uint64_t seed;
    float amplitudeMeters;
} SpeckleWork;

static float maskAt(const SpeckleWork* work, int32_t index) {
    if (work->mask == NULL) {
        return 1.0f;
    }
    return work->mask[index];
}

/* The seed is mixed with the cell index rather than the lane index, so the
   value a cell gets is the same however the rows were divided up. */
static void speckleRow(const SpeckleWork* work, int32_t row) {
    int32_t column;
    for (column = 0; column < work->width; ++column) {
        const int32_t index = row * work->width + column;
        uint64_t state = terrainSeedForLane(work->seed, (uint64_t)index);
        const float offset = terrainRandomUnitFloat(&state) * 2.0f - 1.0f;
        work->output[index] =
            work->input[index] + offset * work->amplitudeMeters * maskAt(work, index);
    }
}

static void speckleLane(void* userData, int32_t laneIndex, int32_t laneCount) {
    const SpeckleWork* work = (const SpeckleWork*)userData;
    int32_t row;
    for (row = laneIndex; row < work->height; row += laneCount) {
        if (work->api->isCancelled(work->context)) {
            return;
        }
        speckleRow(work, row);
    }
}

static void fillWork(SpeckleWork* work, const TerrainContextApi* api,
                     TerrainEvaluationContext* context) {
    work->api = api;
    work->context = context;
    work->input = api->inputField(context, surfaceInput);
    work->mask = api->inputField(context, maskInput);
    work->output = api->outputField(context, surfaceOutput);
    work->width = api->width(context);
    work->height = api->height(context);
    work->amplitudeMeters = (float)api->parameterNumber(context, amplitudeParameter);

    /* The node's own seed on top of the one the engine derived from the graph
       seed and this node's id, so rolling it varies this node alone. */
    work->seed = api->randomSeed(context) ^
                 (uint64_t)(int64_t)api->parameterNumber(context, seedParameter);
}

static void evaluateSpeckle(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    SpeckleWork work;
    fillWork(&work, api, context);
    if (work.input == NULL) {
        api->setError(context, "Speckle needs a surface on its In port");
        return;
    }

    api->runLanes(context, speckleLaneCount, speckleLane, &work);
    api->reportProgress(context, 1.0f);
}

static const TerrainPortDescriptor speckleInputs[] = {
    {"In", "The surface to roughen", TerrainPortType_Field, TerrainFieldSemantics_Heightfield, 0},
    {"Mask", "Scales the effect per cell", TerrainPortType_Field, TerrainFieldSemantics_Mask, 1}};

static const TerrainPortDescriptor speckleOutputs[] = {
    {"Out", "The roughened surface", TerrainPortType_Field, TerrainFieldSemantics_Heightfield, 0}};

static const TerrainParameterDescriptor speckleParameters[] = {
    {"amplitude", "Amplitude", "How far a cell may move, up or down", "m",
     TerrainParameterType_Float, 2.0, 0.0, 500.0, "", NULL, 0},
    {"seed", "Seed", "Varies this node without touching the rest of the graph", "",
     TerrainParameterType_Seed, 0.0, 0.0, 4294967295.0, "", NULL, 0}};

TERRAIN_PLUGIN_EXPORT uint32_t terrainPluginAbiVersion(void) {
    return TERRAIN_SDK_ABI_VERSION;
}

TERRAIN_PLUGIN_EXPORT int32_t terrainRegisterPlugin(const TerrainRegistryApi* api,
                                                    TerrainRegistry* registry) {
    TerrainNodeDescriptor descriptor;
    memset(&descriptor, 0, sizeof descriptor);

    descriptor.typeName = "Speckle";
    descriptor.category = "Example";
    descriptor.description = "Roughens a surface with seeded per-cell noise";
    descriptor.version = 1;
    descriptor.inputs = speckleInputs;
    descriptor.inputCount = 2;
    descriptor.outputs = speckleOutputs;
    descriptor.outputCount = 1;
    descriptor.parameters = speckleParameters;
    descriptor.parameterCount = 2;
    descriptor.evaluate = evaluateSpeckle;

    return api->registerNode(registry, &descriptor);
}
