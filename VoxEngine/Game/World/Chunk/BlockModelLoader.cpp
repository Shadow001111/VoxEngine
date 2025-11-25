#include "BlockModelLoader.h"
#include <iostream>
#include <fstream>

// TODO: Fall to default cube model if not found, with broken texture
std::optional<BlockModel> BlockModelLoader::loadFromFile(const std::filesystem::path& filepath)
{
    if (!std::filesystem::exists(filepath))
    {
        std::cerr << "[BlockModelLoader]: File not found: " << filepath << std::endl;
        return std::nullopt;
    }

    try
    {
        std::ifstream file(filepath);
        json j;
        file >> j;
        return parseJson(j);
    }
    catch (const std::exception& e)
    {
        std::cerr << "[BlockModelLoader]: Error loading file " << filepath << ": " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::optional<BlockModel> BlockModelLoader::parseJson(const json& j)
{
    BlockModel model;

    // Check if texture_slots exists
    if (!j.contains("texture_slots") || !j["texture_slots"].is_number_unsigned())
    {
        std::cerr << "[BlockModelLoader]: Missing or invalid texture_slots" << std::endl;
        return std::nullopt;
    }
    model.texture_slots = j["texture_slots"];

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
            if (auto alignedFace = parseAlignedFace(faceJson))
            {
                model.alignedFaces.push_back(*alignedFace);
            }
        }
        else if (type == "non_aligned")
        {
            if (auto nonAlignedFace = parseNonAlignedFace(faceJson))
            {
                model.nonAlignedFaces.push_back(*nonAlignedFace);
            }
        }
        else
        {
            std::cerr << "[BlockModelLoader]: Unknown face type: " << type << std::endl;
        }
    }

    std::cout << "[BlockModelLoader]: Model loaded successfully with " << j["faces"].size() << " faces" << std::endl;
    std::cout << "[BlockModelLoader]: Texture slots: " << j["texture_slots"] << std::endl;

    return model;
}

std::optional<AlignedFace> BlockModelLoader::parseAlignedFace(const json& faceJson)
{
    AlignedFace face;

    if (!faceJson.contains("normal") || !faceJson["normal"].is_number_unsigned())
    {
        std::cerr << "[BlockModelLoader]: Aligned face missing or invalid normal" << std::endl;
        return std::nullopt;
    }
    face.normal = faceJson["normal"];

    if (!faceJson.contains("texture_slot") || !faceJson["texture_slot"].is_number_unsigned())
    {
        std::cerr << "[BlockModelLoader]: Aligned face missing or invalid texture_slot" << std::endl;
        return std::nullopt;
    }
    face.texture_slot = faceJson["texture_slot"];

    return face;
}

std::optional<NonAlignedFace> BlockModelLoader::parseNonAlignedFace(const json& faceJson)
{
    NonAlignedFace face;

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
    face.texture_slot = faceJson["texture_slot"];

    return face;
}
