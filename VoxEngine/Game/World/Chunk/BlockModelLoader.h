#pragma once
#include <vector>
#include <optional>
#include <filesystem>
#include <unordered_map>
#include <json.hpp>

using json = nlohmann::json;

class BlockModelLoader
{
public:
    struct ModelAlignedFace
    {
        uint8_t normal : 3 = 0;
        uint8_t textureSlot = 0;
    };

    struct ModelNonAlignedFace
    {
        struct Vertex
        {
            float x, y, z;
        };

        struct UV
        {
            float u, v;
        };

        Vertex vertices[4];
        UV uv[4];
        uint8_t textureSlot;
    };

    struct BlockModel
    {
        std::vector<ModelAlignedFace> alignedFaces;
        std::vector<ModelNonAlignedFace> nonAlignedFaces;
    };
private:
    static std::unordered_map<uint32_t, BlockModel> blockModelStorage;

    static std::optional<BlockModel> loadFromFile(const std::filesystem::path& filepath);
    static std::optional<BlockModel> parseJson(const json& j);
    static std::optional<ModelAlignedFace> parseAlignedFace(const json& faceJson);
    static std::optional<ModelNonAlignedFace> parseNonAlignedFace(const json& faceJson);
public:
    static void loadModels(const std::vector<std::string>& blockModelNames);
    static const BlockModel& getBlockModelById(uint32_t id);
};

