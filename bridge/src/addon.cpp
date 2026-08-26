#include <napi.h>

#include <string>

#include "terrain/version.h"

namespace {

Napi::Value EngineVersion(const Napi::CallbackInfo& info) {
    return Napi::String::New(info.Env(), std::string(terrain::engineVersion()));
}

Napi::Object Initialize(Napi::Env env, Napi::Object exports) {
    exports.Set("engineVersion", Napi::Function::New(env, EngineVersion));
    return exports;
}

}  // namespace

NODE_API_MODULE(engine, Initialize)
