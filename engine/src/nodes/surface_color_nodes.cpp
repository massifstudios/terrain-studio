#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "color_lookup_support.h"
#include "node_support.h"
#include "terrain/color_field.h"
#include "terrain/color_space.h"

namespace terrain::nodes {
namespace {

/* =================================================================== shared */

RgbColor readColor(const float* source, std::size_t index) {
    const std::size_t sample = index * ColorField::channelCount;
    return RgbColor{source[sample], source[sample + 1], source[sample + 2]};
}

void writeColor(float* destination, std::size_t index, const RgbColor& color) {
    const std::size_t sample = index * ColorField::channelCount;
    destination[sample] = color.red;
    destination[sample + 1] = color.green;
    destination[sample + 2] = color.blue;
}

/// How far the adjusted colour is allowed to show through, cell by cell.
float maskWeightAt(const float* mask, std::size_t index) {
    if (mask == nullptr) {
        return 1.0f;
    }
    return std::clamp(mask[index], 0.0f, 1.0f);
}

RgbColor mixColors(const RgbColor& original, const RgbColor& adjusted, float weight) {
    return RgbColor{original.red + (adjusted.red - original.red) * weight,
                    original.green + (adjusted.green - original.green) * weight,
                    original.blue + (adjusted.blue - original.blue) * weight};
}

RgbColor scaledColor(const RgbColor& color, float factor) {
    return RgbColor{std::clamp(color.red * factor, 0.0f, 1.0f),
                    std::clamp(color.green * factor, 0.0f, 1.0f),
                    std::clamp(color.blue * factor, 0.0f, 1.0f)};
}

int radiusInCells(double radiusMeters, double cellSizeMeters) {
    return std::max(1, static_cast<int>(std::lround(radiusMeters / cellSizeMeters)));
}

/* ================================================================ box means */

/// One row or one column of the grid, as a view a running-sum pass can walk
/// without caring which way it runs.
struct GridLine {
    const float* source = nullptr;
    float* destination = nullptr;
    int count = 0;
    std::ptrdiff_t step = 1;
};

/// The window a box mean carries along a line. It shrinks at the ends, so an
/// edge cell averages the samples that exist rather than repeats of itself.
class BoxWindow {
public:
    BoxWindow(const GridLine& line, int radius)
        : line_(line), radius_(radius), last_(std::min(radius, line.count - 1)) {
        for (int index = 0; index <= last_; ++index) {
            total_ += sampleAt(index);
        }
    }

    /// Slides the window onto a new centre, taking in what came within reach
    /// and dropping what fell out of it.
    void advanceTo(int centre) {
        const int last = std::min(centre + radius_, line_.count - 1);
        const int first = std::max(centre - radius_, 0);
        for (int index = last_ + 1; index <= last; ++index) {
            total_ += sampleAt(index);
        }
        for (int index = first_; index < first; ++index) {
            total_ -= sampleAt(index);
        }
        first_ = first;
        last_ = last;
    }

    float mean() const { return static_cast<float>(total_ / (last_ - first_ + 1)); }

private:
    double sampleAt(int index) const { return line_.source[index * line_.step]; }

    GridLine line_;
    int radius_ = 1;
    int last_ = 0;
    int first_ = 0;
    double total_ = 0.0;
};

void boxMeanAlongLine(const GridLine& line, int radius) {
    BoxWindow window(line, radius);
    for (int index = 0; index < line.count; ++index) {
        window.advanceTo(index);
        line.destination[index * line.step] = window.mean();
    }
}

/// The mean of the field over a square window, as two running-sum passes, so
/// the cost is the same whatever radius the scale in meters works out to.
void localMean(const TerrainContextApi* api, TerrainEvaluationContext* context, const float* field,
               int radius, float* destination) {
    const int size = api->width(context);
    std::vector<float> alongRows(cellCount(api, context));

    forEachRowRange(api, context, size, [&](int firstRow, int lastRow) {
        for (int row = firstRow; row < lastRow; ++row) {
            const std::size_t start = static_cast<std::size_t>(row) * size;
            boxMeanAlongLine(GridLine{field + start, alongRows.data() + start, size, 1}, radius);
        }
    });
    forEachRowRange(api, context, size, [&](int firstColumn, int lastColumn) {
        for (int column = firstColumn; column < lastColumn; ++column) {
            boxMeanAlongLine(
                GridLine{alongRows.data() + column, destination + column, size, size}, radius);
        }
    });
}

/* ============================================================== Weathering */

const TerrainPortDescriptor weatheringInputs[] = {
    {"Color", "The colour map to weather", TerrainPortType_Color,
     TerrainFieldSemantics_Heightfield, 0},
    {"Terrain", "The ground the colour map lies on", TerrainPortType_Field,
     TerrainFieldSemantics_Heightfield, 0},
    {"Mask", "Scales the effect per cell", TerrainPortType_Field, TerrainFieldSemantics_Mask, 1}};

const TerrainPortDescriptor weatheringOutputs[] = {
    {"Out", "The weathered colour map", TerrainPortType_Color, TerrainFieldSemantics_Heightfield,
     0}};

const TerrainParameterDescriptor weatheringParameters[] = {
    {"scale", "Scale", "Ground distance the surrounding height is read over; small picks up "
                       "cracks, large picks up ridges",
     "m", TerrainParameterType_Float, 100.0, 1.0, 20000.0, "", nullptr, 0},
    {"creep", "Creep", "How far the weathering spreads off the form it came from", "",
     TerrainParameterType_Float, 0.3, 0.0, 1.0, "", nullptr, 0},
    {"amount", "Amount", "How much of the weathered colour is laid over the original", "",
     TerrainParameterType_Float, 0.3, 0.0, 1.0, "", nullptr, 0},
    {"washedOut", "Washed out", "Fades and drains the weathered colour, as long exposure does",
     "", TerrainParameterType_Bool, 0.0, 0.0, 1.0, "", nullptr, 0},
    {"inverse", "Inverse", "Wears the sheltered ground instead of the exposed", "",
     TerrainParameterType_Bool, 0.0, 0.0, 1.0, "", nullptr, 0},
    {"dirt", "Dirt", "How much grime gathers in the recesses", "", TerrainParameterType_Float,
     0.3, 0.0, 1.0, "", nullptr, 0},
    {"darker", "Darker", "Lets the dirt bite harder", "", TerrainParameterType_Bool, 0.0, 0.0,
     1.0, "", nullptr, 0},
    {"hueShift", "Hue shift", "Turns the weathered colour around the hue wheel", "deg",
     TerrainParameterType_Float, 0.0, -180.0, 180.0, "", nullptr, 0},
    {"saturation", "Saturation", "Drains the weathered colour towards grey or pushes it further",
     "", TerrainParameterType_Float, 0.0, -1.0, 1.0, "", nullptr, 0},
    {"lightness", "Lightness", "Takes the weathered colour towards black or towards white", "",
     TerrainParameterType_Float, 0.0, -1.0, 1.0, "", nullptr, 0}};

/// How far the highlight on a fully exposed edge lifts the colour, and how far
/// the shading in a fully sheltered recess drops it.
constexpr float highlightRange = 0.5f;
constexpr float gentleDirtRange = 0.5f;
constexpr float harshDirtRange = 0.85f;

/// Everything the weathered colour of one cell is built from, other than the
/// colour itself and the form reading under it.
struct WeatheringLook {
    float dirtRange = 0.0f;
    bool washedOut = false;
    ColorGrade grade;
};

/**
 * How far each cell stands above or below the ground around it, as [-1, 1]
 * against the strongest reading in the field. Normalising against the field's
 * own relief is what keeps one amount setting meaningful on a dune field and
 * on an alpine wall alike.
 */
std::vector<float> formSignal(const TerrainContextApi* api, TerrainEvaluationContext* context,
                              double scaleMeters, float creep, bool inverse) {
    const std::size_t count = cellCount(api, context);
    const double cellSize = api->cellSizeMeters(context);
    const float* terrain = api->inputField(context, 1);

    std::vector<float> mean(count);
    localMean(api, context, terrain, radiusInCells(scaleMeters, cellSize), mean.data());
    std::vector<float> signal(count);
    for (std::size_t index = 0; index < count; ++index) {
        signal[index] = terrain[index] - mean[index];
    }
    if (creep > 0.0f) {
        localMean(api, context, signal.data(), radiusInCells(scaleMeters * creep, cellSize),
                  mean.data());
        signal.swap(mean);
    }

    const FieldRange range = fieldRange(signal.data(), count);
    float scale = 1.0f / std::max(std::max(std::abs(range.minimum), std::abs(range.maximum)),
                                  1.0e-12f);
    if (inverse) {
        scale = -scale;
    }
    for (float& value : signal) {
        value = std::clamp(value * scale, -1.0f, 1.0f);
    }
    return signal;
}

/// Prolonged exposure bleaches: the colour drains towards its own brightness
/// and lifts a little, rather than towards any particular grey.
RgbColor washedOutColor(const RgbColor& color) {
    constexpr float drain = 0.6f;
    constexpr float lift = 0.12f;
    const float grey = luminance(color);
    const RgbColor drained = mixColors(color, RgbColor{grey, grey, grey}, drain);
    return mixColors(drained, RgbColor{1.0f, 1.0f, 1.0f}, lift);
}

/// The colour a cell would take if it were fully weathered: brighter where it
/// protrudes, darker and grimier where it is sheltered.
RgbColor weatheredColor(const RgbColor& original, float signal, const WeatheringLook& look) {
    const float highlight = 1.0f + highlightRange * signal;
    const float grime = 1.0f - look.dirtRange * std::max(-signal, 0.0f);
    RgbColor worn = scaledColor(original, highlight * grime);
    if (look.washedOut) {
        worn = washedOutColor(worn);
    }
    return gradeColor(worn, look.grade);
}

WeatheringLook weatheringLookFor(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const float dirt = static_cast<float>(api->parameterNumber(context, 5));
    const bool darker = api->parameterNumber(context, 6) != 0.0;

    WeatheringLook look;
    look.dirtRange = dirt * gentleDirtRange;
    if (darker) {
        look.dirtRange = dirt * harshDirtRange;
    }
    look.washedOut = api->parameterNumber(context, 3) != 0.0;
    look.grade.hueShiftDegrees = static_cast<float>(api->parameterNumber(context, 7));
    look.grade.saturationShift = static_cast<float>(api->parameterNumber(context, 8));
    look.grade.lightnessShift = static_cast<float>(api->parameterNumber(context, 9));
    return look;
}

/**
 * An ageing pass over a finished colour map. The terrain's form at the chosen
 * scale — how far a cell stands above the ground around it — decides where the
 * light catches and where grime gathers, and the result is laid over the
 * original at the strength the amount dial asks for. At an amount of zero the
 * input comes back untouched.
 */
void evaluateWeathering(const TerrainContextApi* api, TerrainEvaluationContext* context) {
    const std::size_t count = cellCount(api, context);
    const float* input = api->inputColor(context, 0);
    const float* mask = api->inputField(context, 2);
    float* output = api->outputColor(context, 0);

    const float amount = static_cast<float>(api->parameterNumber(context, 2));
    const std::vector<float> signal =
        formSignal(api, context, api->parameterNumber(context, 0),
                   static_cast<float>(api->parameterNumber(context, 1)),
                   api->parameterNumber(context, 4) != 0.0);
    const WeatheringLook look = weatheringLookFor(api, context);

    for (std::size_t index = 0; index < count; ++index) {
        const RgbColor original = readColor(input, index);
        const RgbColor worn = weatheredColor(original, signal[index], look);
        writeColor(output, index,
                   mixColors(original, worn, amount * maskWeightAt(mask, index)));
    }
}

const TerrainNodeDescriptor weatheringDescriptor = {
    "Weathering",
    "Texture",
    "Ages a colour map: light on what protrudes, grime in what is sheltered",
    1,
    weatheringInputs,
    3,
    weatheringOutputs,
    1,
    weatheringParameters,
    10,
    &evaluateWeathering};

}  // namespace

void registerSurfaceColorNodes(const TerrainRegistryApi& api, TerrainRegistry* registry) {
    api.registerNode(registry, &weatheringDescriptor);
}

}  // namespace terrain::nodes
