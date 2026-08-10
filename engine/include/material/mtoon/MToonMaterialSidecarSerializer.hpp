#pragma once

#include <material/mtoon/MToonMaterialSidecar.hpp>

#include <filesystem>
#include <string>
#include <string_view>

namespace stylized::material
{

struct MToonSidecarError
{
    std::string material;
    std::string field;
    std::string message;

    void clear() noexcept
    {
        material.clear();
        field.clear();
        message.clear();
    }
};

[[nodiscard]] bool serializeMToonSidecar(
    const MToonMaterialSidecar& sidecar,
    std::string& destination,
    MToonSidecarError& error);

[[nodiscard]] bool deserializeMToonSidecar(
    std::string_view source,
    MToonMaterialSidecar& destination,
    MToonSidecarError& error);

[[nodiscard]] bool loadMToonSidecarFile(
    const std::filesystem::path& path,
    MToonMaterialSidecar& destination,
    MToonSidecarError& error);

[[nodiscard]] bool saveMToonSidecarFile(
    const std::filesystem::path& path,
    const MToonMaterialSidecar& sidecar,
    MToonSidecarError& error);

} // namespace stylized::material
