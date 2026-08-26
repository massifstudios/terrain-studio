#include "terrain/formats/elevation_import.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>

#include "terrain/formats/heightfield_png.h"
#include "terrain/formats/massifdem.h"
#include "terrain/formats/raw_float32.h"

namespace terrain::formats {
namespace {

std::string lowercaseExtension(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char character) { return std::tolower(character); });
    return extension;
}

}  // namespace

Heightfield readElevationModel(const std::filesystem::path& path,
                               const ElevationImportOptions& options) {
    const std::string extension = lowercaseExtension(path);

    if (extension == ".massifdem") {
        return readMassifDem(path).field;
    }
    if (extension == ".png") {
        return readHeightfieldPng(
            path, HeightPngBand{1.0, options.minimumAltitudeMeters, options.maximumAltitudeMeters});
    }
    if (extension == ".r32" || extension == ".raw" || extension == ".f32") {
        return readRawFloat32(path, RawFloat32Layout{options.rawWidth, options.rawHeight, 1.0});
    }
    throw std::runtime_error("Unsupported elevation format: " + extension);
}

}  // namespace terrain::formats
