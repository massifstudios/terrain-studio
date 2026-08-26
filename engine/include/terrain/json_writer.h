#pragma once

#include <string>
#include <string_view>

namespace terrain {

/**
 * Minimal JSON writer. Keys come out in the order they are written, which is
 * what keeps generated documents diffable.
 */
class JsonWriter {
public:
    void beginObject();
    void endObject();
    void beginArray();
    void endArray();

    void key(std::string_view name);
    void value(std::string_view text);
    // Without this a `const char*` would bind to the bool overload.
    void value(const char* text);
    void value(double number);
    void value(long long number);
    void value(bool flag);

    void member(std::string_view name, std::string_view text);
    void member(std::string_view name, const char* text);
    void member(std::string_view name, double number);
    void member(std::string_view name, long long number);
    void member(std::string_view name, bool flag);

    const std::string& text() const { return text_; }

private:
    void separate();

    std::string text_;
    bool expectingValue_ = false;
    bool atContainerStart_ = true;
};

std::string escapeJsonString(std::string_view text);

}  // namespace terrain
