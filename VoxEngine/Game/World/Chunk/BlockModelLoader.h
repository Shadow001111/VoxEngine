#pragma once
#include <vector>
#include <optional>
#include <filesystem>
#include <json.hpp>

using json = nlohmann::json;

struct AlignedFace
{
    uint32_t x : 4;
    uint32_t y : 4;
    uint32_t z : 4;
    uint32_t normal : 3;
    uint32_t texture_slot;
};

struct NonAlignedFace
{
    struct Vertex
    {
        float x, y, z;
    };

    struct UV
    {
        float u, v;
    };

    std::array<Vertex, 4> vertices;
    std::array<UV, 4> uv;
    uint32_t texture_slot;
};

struct BlockModel
{
    uint32_t texture_slots;
    std::vector<AlignedFace> alignedFaces;
    std::vector<NonAlignedFace> nonAlignedFaces;
};

class BlockModelLoader
{
public:
    static std::optional<BlockModel> loadFromFile(const std::filesystem::path& filepath);
private:
    static std::optional<BlockModel> parseJson(const json& j);
    static std::optional<AlignedFace> parseAlignedFace(const json& faceJson);
    static std::optional<NonAlignedFace> parseNonAlignedFace(const json& faceJson);
};

