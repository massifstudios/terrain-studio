#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

#include "node_support.h"

namespace terrain::nodes {
namespace {

/* ================================================================ fluvial == */

/**
 * Stream-power erosion: every cell's water is routed to its lower neighbours,
 * the water gathered by a cell cuts it down in proportion to how much there is
 * and how steeply it leaves, and the hillslopes between the streams creep
 * downhill a little. Run for many passes, that carves a branching network of
 * valleys with sharp divides between them, which is the shape a mountain
 * range actually has and which no amount of rain sprinkled over a smooth
 * slope will find on its own.
 *
 * Given an uplift mask, the ground under it rises a little before every pass,
 * and the streams cut against that rise. Cutting alone only ever lowers a
 * surface, so it can carve a mountain but never make one; rising against the
 * cutting, the ground settles toward the balance where every stream cuts down
 * exactly as fast as the rock comes up, and that balance is a mountain range:
 * branching ridges, valleys widening downstream, and peaks standing where the
 * uplift is strongest and the drainage weakest. After Cordonnier et al. 2016.
 *
 * Everything is in metres and the pass count is fixed, so the same settings
 * give the same landscape at any resolution as closely as the grid allows.
 */

constexpr int neighbourCount = 8;
constexpr int neighbourRowOffset[neighbourCount] = {-1, -1, -1, 0, 0, 1, 1, 1};
constexpr int neighbourColumnOffset[neighbourCount] = {-1, 0, 1, -1, 1, -1, 0, 1};

/// The catchment the incision setting is quoted at.
constexpr double referenceCatchmentSquareMeters = 1.0e6;

struct FluvialSettings {
    int size = 0;
    double cellSizeMeters = 1.0;
    int passes = 1;
    /// Cut per pass, in metres, for a stream draining the reference catchment
    /// down a slope of one.
    double incisionMeters = 0.0;
    /// The exponent on the catchment area: how much more a big stream cuts.
    double concavity = 0.5;
    /// Hillslope diffusivity per pass, in square metres.
    double creepSquareMeters = 0.0;
    /// How far the ground under a full uplift mask rises per pass, in metres.
    double upliftMeters = 0.0;
};

/// Where each cell's water goes and in what shares, and an order that visits
/// every cell before the cells it drains into.
struct FlowRouting {
    std::vector<float> share;
    std::vector<std::uint32_t> downhillOrder;
    std::vector<double> drainageArea;

    explicit FlowRouting(std::size_t count)
        : share(count * neighbourCount, 0.0f), downhillOrder(), drainageArea(count, 0.0) {
        downhillOrder.reserve(count);
    }
};

bool isInside(int row, int column, int size) {
    return row >= 0 && row < size && column >= 0 && column < size;
}

double neighbourDistance(int direction, double cellSizeMeters) {
    if (neighbourRowOffset[direction] != 0 && neighbourColumnOffset[direction] != 0) {
        return cellSizeMeters * std::numbers::sqrt2;
    }
    return cellSizeMeters;
}

std::size_t neighbourIndex(std::size_t index, int direction, int size) {
    const int row = static_cast<int>(index / size) + neighbourRowOffset[direction];
    const int column = static_cast<int>(index % size) + neighbourColumnOffset[direction];
    return static_cast<std::size_t>(row) * size + column;
}

/// Water leaves a cell towards every lower neighbour, each taking a share in
/// proportion to the slope towards it. Sending it all one way locks every
/// stream onto one of eight compass headings.
void routeCell(const float* surface, const FluvialSettings& settings, std::size_t index,
               float* shares) {
    const int row = static_cast<int>(index / settings.size);
    const int column = static_cast<int>(index % settings.size);
    const float here = surface[index];
    float total = 0.0f;

    for (int direction = 0; direction < neighbourCount; ++direction) {
        shares[direction] = 0.0f;
        if (!isInside(row + neighbourRowOffset[direction],
                      column + neighbourColumnOffset[direction], settings.size)) {
            continue;
        }
        const float drop = here - surface[neighbourIndex(index, direction, settings.size)];
        if (drop <= 0.0f) {
            continue;
        }
        shares[direction] =
            drop / static_cast<float>(neighbourDistance(direction, settings.cellSizeMeters));
        total += shares[direction];
    }
    if (total <= 0.0f) {
        return;
    }
    for (int direction = 0; direction < neighbourCount; ++direction) {
        shares[direction] /= total;
    }
}

void routeFlow(const TerrainContextApi* api, TerrainEvaluationContext* context,
               const float* surface, const FluvialSettings& settings, FlowRouting& routing) {
    forEachRowRange(api, context, settings.size, [&](int firstRow, int lastRow) {
        const std::size_t first = static_cast<std::size_t>(firstRow) * settings.size;
        const std::size_t last = static_cast<std::size_t>(lastRow) * settings.size;
        for (std::size_t index = first; index < last; ++index) {
            routeCell(surface, settings, index, routing.share.data() + index * neighbourCount);
        }
    });
}

/**
 * Cells in an order that puts every cell before the cells it drains into.
 * Peaks go first, then whatever has nothing left draining into it, and so on
 * down. The queue is seeded in index order and drained first in, first out,
 * so the order is a function of the routing alone.
 */
void orderDownhill(const FluvialSettings& settings, FlowRouting& routing) {
    const std::size_t count = routing.drainageArea.size();
    std::vector<std::uint32_t> donorsLeft(count, 0);
    for (std::size_t index = 0; index < count; ++index) {
        const float* shares = routing.share.data() + index * neighbourCount;
        for (int direction = 0; direction < neighbourCount; ++direction) {
            if (shares[direction] > 0.0f) {
                ++donorsLeft[neighbourIndex(index, direction, settings.size)];
            }
        }
    }

    std::vector<std::uint32_t>& order = routing.downhillOrder;
    order.clear();
    for (std::size_t index = 0; index < count; ++index) {
        if (donorsLeft[index] == 0) {
            order.push_back(static_cast<std::uint32_t>(index));
        }
    }
    for (std::size_t visited = 0; visited < order.size(); ++visited) {
        const std::size_t index = order[visited];
        const float* shares = routing.share.data() + index * neighbourCount;
        for (int direction = 0; direction < neighbourCount; ++direction) {
            if (shares[direction] <= 0.0f) {
                continue;
            }
            const std::size_t receiver = neighbourIndex(index, direction, settings.size);
            if (--donorsLeft[receiver] == 0) {
                order.push_back(static_cast<std::uint32_t>(receiver));
            }
        }
    }
}

void accumulateDrainage(const FluvialSettings& settings, FlowRouting& routing) {
    const double cellArea = settings.cellSizeMeters * settings.cellSizeMeters;
    std::fill(routing.drainageArea.begin(), routing.drainageArea.end(), cellArea);
    for (const std::uint32_t index : routing.downhillOrder) {
        const float* shares = routing.share.data() + static_cast<std::size_t>(index) * neighbourCount;
        const double gathered = routing.drainageArea[index];
        for (int direction = 0; direction < neighbourCount; ++direction) {
            if (shares[direction] <= 0.0f) {
                continue;
            }
            routing.drainageArea[neighbourIndex(index, direction, settings.size)] +=
                gathered * shares[direction];
        }
    }
}

/**
 * One pass of incision, solved implicitly from the outlets upward. Each cell
 * settles towards the new height of the cells it drains into, pulled harder
 * the more water it gathers and the steeper it leaves, and can never drop
 * below them — so however hard the setting, a pass cannot dig a hole.
 */
void incise(const FluvialSettings& settings, const FlowRouting& routing, float* surface) {
    const double erodibility =
        settings.incisionMeters / std::pow(referenceCatchmentSquareMeters, settings.concavity);
    const std::vector<std::uint32_t>& order = routing.downhillOrder;

    for (auto visit = order.rbegin(); visit != order.rend(); ++visit) {
        const std::size_t index = *visit;
        const float* shares = routing.share.data() + index * neighbourCount;
        const double power = erodibility * std::pow(routing.drainageArea[index], settings.concavity);
        double pull = 0.0;
        double pulledTowards = 0.0;
        for (int direction = 0; direction < neighbourCount; ++direction) {
            if (shares[direction] <= 0.0f) {
                continue;
            }
            const double factor =
                power * shares[direction] / neighbourDistance(direction, settings.cellSizeMeters);
            pull += factor;
            pulledTowards += factor * surface[neighbourIndex(index, direction, settings.size)];
        }
        if (pull <= 0.0) {
            continue;
        }
        surface[index] = static_cast<float>((surface[index] + pulledTowards) / (1.0 + pull));
    }
}

/// The Laplacian over the neighbours a cell has, so the edges neither leak
/// nor pile up.
float laplacianAt(const float* surface, int size, int row, int column) {
    const std::size_t index = static_cast<std::size_t>(row) * size + column;
    float total = 0.0f;
    int counted = 0;
    const int rowOffsets[4] = {-1, 1, 0, 0};
    const int columnOffsets[4] = {0, 0, -1, 1};
    for (int side = 0; side < 4; ++side) {
        const int neighbourRow = row + rowOffsets[side];
        const int neighbourColumn = column + columnOffsets[side];
        if (!isInside(neighbourRow, neighbourColumn, size)) {
            continue;
        }
        total += surface[static_cast<std::size_t>(neighbourRow) * size + neighbourColumn];
        ++counted;
    }
    return total - counted * surface[index];
}

/**
 * Hillslope creep as diffusion, in as many sub-steps as the explicit scheme
 * needs to stay stable on this cell size. Without it the divides between
 * streams sharpen into knife edges; with it they round off to ridges.
 */
void creep(const TerrainContextApi* api, TerrainEvaluationContext* context,
           const FluvialSettings& settings, float* surface, std::vector<float>& scratch) {
    if (settings.creepSquareMeters <= 0.0) {
        return;
    }
    const double cellArea = settings.cellSizeMeters * settings.cellSizeMeters;
    const int subSteps =
        std::max(1, static_cast<int>(std::ceil(4.0 * settings.creepSquareMeters / cellArea)));
    const float rate = static_cast<float>(settings.creepSquareMeters / subSteps / cellArea);

    for (int step = 0; step < subSteps; ++step) {
        forEachRowRange(api, context, settings.size, [&](int firstRow, int lastRow) {
            for (int row = firstRow; row < lastRow; ++row) {
                for (int column = 0; column < settings.size; ++column) {
                    const std::size_t index = static_cast<std::size_t>(row) * settings.size + column;
                    scratch[index] =
                        surface[index] + rate * laplacianAt(surface, settings.size, row, column);
                }
            }
        });
        std::copy(scratch.begin(), scratch.end(), surface);
    }
}

/**
 * The rise of one pass. The border of the grid is where the water leaves, so
 * it stays where it is: raised with the rest, an edge cell with no lower
 * neighbour inside the grid would have nowhere to drain and would rise
 * without end.
 */
void uplift(const float* upliftMask, const FluvialSettings& settings, float* surface) {
    if (upliftMask == nullptr || settings.upliftMeters <= 0.0) {
        return;
    }
    for (int row = 1; row + 1 < settings.size; ++row) {
        for (int column = 1; column + 1 < settings.size; ++column) {
            const std::size_t index = static_cast<std::size_t>(row) * settings.size + column;
            surface[index] += static_cast<float>(std::clamp(upliftMask[index], 0.0f, 1.0f)
                                                 * settings.upliftMeters);
        }
    }
}

/// The catchment as a mask: log-scaled, 0 at a single cell, 1 at the whole grid.
void writeDrainageMask(const FlowRouting& routing, const FluvialSettings& settings,
                       float* destination) {
    const double cellArea = settings.cellSizeMeters * settings.cellSizeMeters;
    const double fullScale = std::log(static_cast<double>(routing.drainageArea.size()));
    for (std::size_t index = 0; index < routing.drainageArea.size(); ++index) {
        const double cells = std::max(routing.drainageArea[index] / cellArea, 1.0);
        destination[index] = static_cast<float>(std::clamp(std::log(cells) / fullScale, 0.0, 1.0));
    }
}

const TerrainPortDescriptor fluvialInputs[] = {
    {"In", "The surface to erode", TerrainPortType_Field, TerrainFieldSemantics_Heightfield, 0},
    {"Mask", "Scales the effect per cell", TerrainPortType_Field, TerrainFieldSemantics_Mask, 1},
    {"Uplift", "Where the ground rises against the streams, and how strongly",
     TerrainPortType_Field, TerrainFieldSemantics_Mask, 1}};

const TerrainPortDescriptor fluvialOutputs[] = {
    {"Out", "The eroded surface", TerrainPortType_Field, TerrainFieldSemantics_Heightfield, 0},
    {"Flow", "How much ground drains through each cell, log-scaled", TerrainPortType_Field,
     TerrainFieldSemantics_Mask, 0}};

const TerrainParameterDescriptor fluvialParameters[] = {
    {"passes", "Passes", "How many rounds of stream cutting and hillslope creep are run", "",
     TerrainParameterType_Int, 80.0, 1.0, 1000.0, "", nullptr, 0},
    {"incision", "Incision",
     "How far a stream draining one square kilometre cuts down per pass on a 45 degree slope",
     "m", TerrainParameterType_Float, 10.0, 0.0, 500.0, "", nullptr, 0},
    {"concavity", "Concavity",
     "How much harder a big stream cuts than a small one; higher leaves wider valleys "
     "and finer headwaters",
     "", TerrainParameterType_Float, 0.5, 0.2, 0.8, "", nullptr, 0},
    {"creep", "Hillslope creep",
     "How far loose ground creeps down the hillslopes each pass, which rounds the divides "
     "between streams",
     "m", TerrainParameterType_Float, 10.0, 0.0, 500.0, "", nullptr, 0},
    {"uplift", "Uplift",
     "How far the ground under a full uplift mask rises each pass; only used with an uplift "
     "mask connected",
     "m", TerrainParameterType_Float, 5.0, 0.0, 500.0, "", nullptr, 0}};

FluvialSettings fluvialSettingsFor(const TerrainContextApi* api,
                                   TerrainEvaluationContext* context) {
    FluvialSettings settings;
    settings.size = api->width(context);
    settings.cellSizeMeters = api->cellSizeMeters(context);
    settings.passes = static_cast<int>(api->parameterNumber(context, 0));
    settings.incisionMeters = api->parameterNumber(context, 1);
    settings.concavity = api->parameterNumber(context, 2);
    // A creep distance is the reach of a diffusion, and a diffusion's reach
    // grows with the square root of its strength.
    const double creepMeters = api->parameterNumber(context, 3);
    settings.creepSquareMeters = creepMeters * creepMeters * 0.25;
    settings.upliftMeters = api->parameterNumber(context, 4);
    return settings;
}

void evaluateFluvialErosion(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const std::size_t count = cellCount(api, context);
    const float* input = api->inputField(context, 0);
    const FluvialSettings settings = fluvialSettingsFor(api, context);
    if (settings.size < 3) {
        api->setError(context, "Fluvial erosion needs a grid of at least three cells");
        return;
    }

    std::vector<float> surface(input, input + count);
    std::vector<float> scratch(count);
    FlowRouting routing(count);
    const float* upliftMask = api->inputField(context, 2);

    for (int pass = 0; pass < settings.passes; ++pass) {
        if (api->isCancelled(context) != 0) {
            return;
        }
        uplift(upliftMask, settings, surface.data());
        routeFlow(api, context, surface.data(), settings, routing);
        orderDownhill(settings, routing);
        accumulateDrainage(settings, routing);
        incise(settings, routing, surface.data());
        creep(api, context, settings, surface.data(), scratch);
        api->reportProgress(context, static_cast<float>(pass + 1) / settings.passes);
    }

    blendThroughMask(input, surface.data(), api->inputField(context, 1),
                     api->outputField(context, 0), count);
    writeDrainageMask(routing, settings, api->outputField(context, 1));
}

const TerrainNodeDescriptor fluvialDescriptor = {
    "FluvialErosion",
    "Erode",
    "Cuts a branching network of stream valleys with sharp divides between them",
    1,
    fluvialInputs,
    3,
    fluvialOutputs,
    2,
    fluvialParameters,
    5,
    &evaluateFluvialErosion};

}  // namespace

void registerFluvialNodes(const TerrainRegistryApi& api, TerrainRegistry* registry) {
    api.registerNode(registry, &fluvialDescriptor);
}

}  // namespace terrain::nodes
