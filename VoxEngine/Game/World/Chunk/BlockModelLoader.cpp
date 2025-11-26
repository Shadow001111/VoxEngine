#include "BlockModelLoader.h"
#include <iostream>
#include <fstream>

std::unordered_map<uint32_t, BlockModelLoader::BlockModel> BlockModelLoader::blockModelStorage;

std::optional<BlockModelLoader::BlockModel> BlockModelLoader::loadFromFile(const std::filesystem::path& filepath)
{
    if (!std::filesystem::exists(filepath))
    {
        std::cerr << "[BlockModelLoader]: File not found: " << filepath << std::endl;
        return std::nullopt;
    }

    std::ifstream file(filepath);
    if (!file) return std::nullopt;

    try
    {
        json j;
        file >> j;
        return parseJson(j);
    }
    catch (const json::exception& e)
    {
        std::cerr << "[BlockModelLoader]: JSON parsing error in file " << filepath << ": " << e.what() << std::endl;
        return std::nullopt;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[BlockModelLoader]: Error reading file " << filepath << ": " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::optional<BlockModelLoader::BlockModel> BlockModelLoader::parseJson(const json& j)
{
    BlockModel model;

    if (!j.contains("faces") || !j["faces"].is_array())
    {
        std::cerr << "[BlockModelLoader]: Missing or invalid faces array" << std::endl;
        return std::nullopt;
    }

    for (const auto& faceJson : j["faces"])
    {
        if (!faceJson.contains("type") || !faceJson["type"].is_string())
        {
            std::cerr << "[BlockModelLoader]: Face missing type field" << std::endl;
            continue;
        }

        std::string type = faceJson["type"];

        if (type == "aligned")
        {
            auto result = parseAlignedFace(faceJson);
            if (result.has_value())
            {
                const auto& face = result.value();
                model.alignedFaces.push_back(face);
            }
        }
        else if (type == "non_aligned")
        {
            auto result = parseNonAlignedFace(faceJson);
            if (result.has_value())
            {
                const auto& face = result.value();
                model.nonAlignedFaces.push_back(face);
            }
        }
        else
        {
            std::cerr << "[BlockModelLoader]: Unknown face type: " << type << std::endl;
        }
    }

    return model;
}

std::optional<BlockModelLoader::ModelAlignedFace> BlockModelLoader::parseAlignedFace(const json& faceJson)
{
    ModelAlignedFace face;

    if (!faceJson.contains("normal") || !faceJson["normal"].is_number_unsigned())
    {
        std::cerr << "[BlockModelLoader]: Aligned face missing or invalid normal" << std::endl;
        return std::nullopt;
    }
    else if (faceJson["normal"] > 5)
    {
        std::cerr << "[BlockModelLoader]: Aligned face normal is out of range" << std::endl;
        return std::nullopt;
    }
    face.normal = faceJson["normal"];

    if (!faceJson.contains("texture_slot") || !faceJson["texture_slot"].is_number_unsigned())
    {
        std::cerr << "[BlockModelLoader]: Aligned face missing or invalid texture_slot" << std::endl;
        return std::nullopt;
    }
    face.textureSlot = faceJson["texture_slot"];

    return face;
}

std::optional<BlockModelLoader::ModelNonAlignedFace> BlockModelLoader::parseNonAlignedFace(const json& faceJson)
{
    ModelNonAlignedFace face;

    // Parse vertices
    if (!faceJson.contains("vertices") || !faceJson["vertices"].is_array() || faceJson["vertices"].size() != 4)
    {
        std::cerr << "[BlockModelLoader]: Non-aligned face missing or invalid vertices array (must have 4 vertices)" << std::endl;
        return std::nullopt;
    }

    for (size_t i = 0; i < 4; i++)
    {
        const auto& vertexJson = faceJson["vertices"][i];
        if (!vertexJson.contains("x") || !vertexJson["x"].is_number() ||
            !vertexJson.contains("y") || !vertexJson["y"].is_number() ||
            !vertexJson.contains("z") || !vertexJson["z"].is_number()) {
            std::cerr << "[BlockModelLoader]: Non-aligned face vertex " << i << " missing or invalid coordinates" << std::endl;
            return std::nullopt;
        }

        face.vertices[i] = {
            vertexJson["x"],
            vertexJson["y"],
            vertexJson["z"]
        };
    }

    // Parse UV coordinates
    if (!faceJson.contains("uv") || !faceJson["uv"].is_array() || faceJson["uv"].size() != 4)
    {
        std::cerr << "[BlockModelLoader]: Non-aligned face missing or invalid uv array (must have 4 UV coordinates)" << std::endl;
        return std::nullopt;
    }

    for (size_t i = 0; i < 4; i++)
    {
        const auto& uvJson = faceJson["uv"][i];
        if (!uvJson.contains("u") || !uvJson["u"].is_number() ||
            !uvJson.contains("v") || !uvJson["v"].is_number()) {
            std::cerr << "[BlockModelLoader]: Non-aligned face UV " << i << " missing or invalid coordinates" << std::endl;
            return std::nullopt;
        }

        face.uv[i] = {
            uvJson["u"],
            uvJson["v"]
        };
    }

    // Parse texture slot
    if (!faceJson.contains("texture_slot") || !faceJson["texture_slot"].is_number_unsigned())
    {
        std::cerr << "[BlockModelLoader]: Non-aligned face missing or invalid texture_slot" << std::endl;
        return std::nullopt;
    }
    face.textureSlot = faceJson["texture_slot"];

    return face;
}

void BlockModelLoader::loadModels(const std::vector<std::string>& blockModelNames)
{
    uint32_t modelID = 0;
    for (const auto& modelName : blockModelNames)
    {
        auto result = loadFromFile("res/BlockModels/" + modelName + ".json");
        if (result.has_value())
        {
            blockModelStorage.emplace(modelID, result.value());
        }
        else
        {
            // TODO: Add fallback model
        }
        modelID++;
    }
}

const BlockModelLoader::BlockModel& BlockModelLoader::getBlockModelById(uint32_t id)
{
    auto it = blockModelStorage.find(id);
    if (it != blockModelStorage.end())
    {
        return it->second;
    }
    if (blockModelStorage.empty())
    {
        static BlockModel emptyModel;
        return emptyModel;
    }
    else
    {
        return blockModelStorage[0];
    }
}
