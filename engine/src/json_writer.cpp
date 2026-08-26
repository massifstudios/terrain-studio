#include "terrain/json_writer.h"

#include <charconv>
#include <cmath>
#include <cstdio>

namespace terrain {
namespace {

std::string formatNumber(double number) {
    if (!std::isfinite(number)) {
        return "0";
    }
    char buffer[32];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), number);
    return std::string(buffer, result.ptr);
}

}  // namespace

std::string escapeJsonString(std::string_view text) {
    std::string escaped;
    escaped.reserve(text.size() + 2);
    for (const char character : text) {
        if (character == '"' || character == '\\') {
            escaped.push_back('\\');
            escaped.push_back(character);
        } else if (character == '\n') {
            escaped += "\\n";
        } else if (character == '\t') {
            escaped += "\\t";
        } else if (static_cast<unsigned char>(character) < 0x20) {
            char buffer[8];
            std::snprintf(buffer, sizeof(buffer), "\\u%04x", character);
            escaped += buffer;
        } else {
            escaped.push_back(character);
        }
    }
    return escaped;
}

void JsonWriter::separate() {
    if (expectingValue_) {
        expectingValue_ = false;
        return;
    }
    if (!atContainerStart_) {
        text_.push_back(',');
    }
    atContainerStart_ = false;
}

void JsonWriter::beginObject() {
    separate();
    text_.push_back('{');
    atContainerStart_ = true;
}

void JsonWriter::endObject() {
    text_.push_back('}');
    atContainerStart_ = false;
}

void JsonWriter::beginArray() {
    separate();
    text_.push_back('[');
    atContainerStart_ = true;
}

void JsonWriter::endArray() {
    text_.push_back(']');
    atContainerStart_ = false;
}

void JsonWriter::key(std::string_view name) {
    separate();
    text_.push_back('"');
    text_ += escapeJsonString(name);
    text_ += "\":";
    expectingValue_ = true;
}

void JsonWriter::value(std::string_view text) {
    separate();
    text_.push_back('"');
    text_ += escapeJsonString(text);
    text_.push_back('"');
}

void JsonWriter::value(const char* text) {
    if (text == nullptr) {
        value(std::string_view{});
        return;
    }
    value(std::string_view{text});
}

void JsonWriter::value(double number) {
    separate();
    text_ += formatNumber(number);
}

void JsonWriter::value(long long number) {
    separate();
    text_ += std::to_string(number);
}

void JsonWriter::value(bool flag) {
    separate();
    if (flag) {
        text_ += "true";
        return;
    }
    text_ += "false";
}

void JsonWriter::member(std::string_view name, std::string_view text) {
    key(name);
    value(text);
}

void JsonWriter::member(std::string_view name, const char* text) {
    key(name);
    value(text);
}

void JsonWriter::member(std::string_view name, double number) {
    key(name);
    value(number);
}

void JsonWriter::member(std::string_view name, long long number) {
    key(name);
    value(number);
}

void JsonWriter::member(std::string_view name, bool flag) {
    key(name);
    value(flag);
}

}  // namespace terrain
