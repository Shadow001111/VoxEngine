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
        uint32_t x0 : 9;
        uint32_t y0 : 9;
        uint32_t z0 : 9;
        uint32_t u0 : 5;

        uint32_t x1 : 9;
        uint32_t y1 : 9;
        uint32_t z1 : 9;
        uint32_t u1 : 5;

        uint32_t x2 : 9;
        uint32_t y2 : 9;
        uint32_t z2 : 9;
        uint32_t u2 : 5;

        uint32_t x3 : 9;
        uint32_t y3 : 9;
        uint32_t z3 : 9;
        uint32_t u3 : 5;

        uint32_t v0 : 5;
        uint32_t v1 : 5;
        uint32_t v2 : 5;
        uint32_t v3 : 5;
        uint32_t textureSlot : 12;
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

