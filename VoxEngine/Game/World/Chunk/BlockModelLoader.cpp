#include "BlockModelLoader.h"
#include <iostream>
#include <fstream>

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

    if (!faceJson.contains("normal") || !faceJson.at("normal").is_number_unsigned())
    {
        std::cerr << "[BlockModelLoader]: Aligned face missing or invalid normal" << std::endl;
        return std::nullopt;
    }
    else if (faceJson.at("normal") > 5)
    {
        std::cerr << "[BlockModelLoader]: Aligned face normal is out of range" << std::endl;
        return std::nullopt;
    }
    face.normal = faceJson.at("normal");

    if (!faceJson.contains("texture_slot") || !faceJson.at("texture_slot").is_number_unsigned())
    {
        std::cerr << "[BlockModelLoader]: Aligned face missing or invalid texture_slot" << std::endl;
        return std::nullopt;
    }
    face.textureSlot = faceJson.at("texture_slot");

    return face;
}

std::optional<BlockModelLoader::ModelNonAlignedFace> BlockModelLoader::parseNonAlignedFace(const json& faceJson)
{
    ModelNonAlignedFace face;

    // Parse vertices
    if (!faceJson.contains("vertices") || !faceJson.at("vertices").is_array() || faceJson.at("vertices").size() != 4)
    {
        std::cerr << "[BlockModelLoader]: Non-aligned face missing or invalid vertices array (must have 4 vertices)" << std::endl;
        return std::nullopt;
    }

    int vertices[4][3];
    for (int i = 0; i < 4; i++)
    {
        const auto& vertexJson = faceJson["vertices"][i];
        if (!vertexJson.contains("x") || !vertexJson.at("x").is_number() ||
            !vertexJson.contains("y") || !vertexJson.at("y").is_number() ||
            !vertexJson.contains("z") || !vertexJson.at("z").is_number()) {
            std::cerr << "[BlockModelLoader]: Non-aligned face vertex " << i << " missing or invalid coordinates" << std::endl;
            return std::nullopt;
        }

        float x, y, z;
        vertexJson.at("x").get_to(x);
        vertexJson.at("y").get_to(y);
        vertexJson.at("z").get_to(z);

        vertices[i][0] = 16.0f * fminf(1.0f, fmaxf(0.0f, x));
        vertices[i][1] = 16.0f * fminf(1.0f, fmaxf(0.0f, y));
        vertices[i][2] = 16.0f * fminf(1.0f, fmaxf(0.0f, z));
    }

    // Pack vertices
    face.x0 = vertices[0][0];
    face.y0 = vertices[0][1];
    face.z0 = vertices[0][2];

    face.x1 = vertices[1][0];
    face.y1 = vertices[1][1];
    face.z1 = vertices[1][2];

    face.x2 = vertices[2][0];
    face.y2 = vertices[2][1];
    face.z2 = vertices[2][2];

    face.x3 = vertices[3][0];
    face.y3 = vertices[3][1];
    face.z3 = vertices[3][2];

    // Parse UV coordinates
    if (!faceJson.contains("uv") || !faceJson.at("uv").is_array() || faceJson.at("uv").size() != 4)
    {
        std::cerr << "[BlockModelLoader]: Non-aligned face missing or invalid uv array (must have 4 UV coordinates)" << std::endl;
        return std::nullopt;
    }

    int uvs[4][2];
    for (int i = 0; i < 4; i++)
    {
        const auto& uvJson = faceJson["uv"][i];
        if (!uvJson.contains("u") || !uvJson.at("u").is_number() ||
            !uvJson.contains("v") || !uvJson.at("v").is_number()) {
            std::cerr << "[BlockModelLoader]: Non-aligned face UV " << i << " missing or invalid coordinates" << std::endl;
            return std::nullopt;
        }

        float u, v;
        uvJson.at("u").get_to(u);
        uvJson.at("v").get_to(v);

        uvs[i][0] = 16.0f * fminf(1.0f, fmaxf(0.0f, u));
        uvs[i][1] = 16.0f * fminf(1.0f, fmaxf(0.0f, v));
    }

    // Pack UV coordinates
    face.u0 = uvs[0][0];
    face.v0 = uvs[0][1];

    face.u1 = uvs[1][0];
    face.v1 = uvs[1][1];

    face.u2 = uvs[2][0];
    face.v2 = uvs[2][1];

    face.u3 = uvs[3][0];
    face.v3 = uvs[3][1];

    // Parse texture slot
    if (!faceJson.contains("texture_slot") || !faceJson.at("texture_slot").is_number_unsigned())
    {
        std::cerr << "[BlockModelLoader]: Non-aligned face missing or invalid texture_slot" << std::endl;
        return std::nullopt;
    }
    face.textureSlot = faceJson.at("texture_slot");

    return face;
}

std::optional<BlockModelLoader::BlockModel> BlockModelLoader::loadModelByName(const std::string& blockModelName)
{
    return loadFromFile("res/BlockModels/" + blockModelName + ".json");
}
