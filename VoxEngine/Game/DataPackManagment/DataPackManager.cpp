#include "DataPackManager.h"
#include "AssetRegistry.h"

#include <iostream>
#include <fstream>

namespace fs = std::filesystem;

bool fileExistsAndIsRegular(const fs::path& path)
{
    std::error_code ec;
    auto status = fs::status(path, ec);
    return !ec && fs::is_regular_file(status);
}

bool fileExistsAndIsDirectory(const fs::path& path)
{
    std::error_code ec;
    auto status = fs::status(path, ec);
    return !ec && fs::is_directory(status);
}

// TODO: Check for dependencies
// TODO: Check for duplicate id's
// TODO: Require core pack
void DataPackManager::loadAllDataPacks()
{
    // Reset
    AssetRegistry::reset();

    // Check if directory exist
	const fs::path packsDir = "res/DataPacks";

    if (!fileExistsAndIsDirectory(packsDir))
    {
        std::cerr << "[DataPackManager]: Data pack directory is not found\n";
        return;
    }

    // Load all packs
    for (const auto& entry : fs::directory_iterator(packsDir))
    {
        if (entry.is_directory())
        {
            loadDataPack(entry.path());
        }
    }

    // Link
    bool linkResult = AssetRegistry::linkAssets();
    if (!linkResult)
    {
        std::cerr << "[DataPackManager]: Failed to link assets\n";
        return;
    }

    std::cout << "[DataPackManager]: Assets are linked\n";

    std::cout << std::flush;
}

void DataPackManager::loadDataPack(const fs::path& dataPackPath)
{
    // Get data pack name
    std::string dataPackName = dataPackPath.filename().string();
    auto nameValidationResult = validateObjectName(dataPackName);
    if (nameValidationResult != ObjectNameValidationResult::Success)
    {
        printObjectNameValidationError(std::cerr, nameValidationResult, "[DataPackManager]: ", "Data pack name");
        return;
    }

    // Get metadata
    auto result = getDatapackMetadata(dataPackPath);
    if (!result.has_value())
    {
        return;
    }
    DatapackMetadata& metadata = result.value();
    metadata.name = std::move(dataPackName);

    // Load assets
    loadBlocks(dataPackPath, metadata.id);
    loadBlockModels(dataPackPath, metadata.id);
    loadItems(dataPackPath, metadata.id);
    loadItemModels(dataPackPath, metadata.id);
}

std::optional<DataPackManager::DatapackMetadata> DataPackManager::getDatapackMetadata(const std::filesystem::path& dataPackPath)
{
    // Check for file
    fs::path metadataPath = dataPackPath / "datapack.json";
    if (!fileExistsAndIsRegular(metadataPath))
    {
        std::cerr << "[DataPackManager]: Metadata is not found\n";
        return std::nullopt;
    }

    // Open json file
    std::ifstream file(metadataPath);
    if (!file)
    {
        std::cerr << "[DataPackManager]: Failed to open metadata file\n";
        return std::nullopt;
    }

    try
    {
        json j;
        file >> j;
        return parseMetadataJson(j);
    }
    catch (const json::exception& e)
    {
        std::cerr << "[DataPackManager]: JSON parsing error in file " << metadataPath << ": " << e.what() << "\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << "[DataPackManager]: Error reading file " << metadataPath << ": " << e.what() << "\n";
    }
    return std::nullopt;
}

std::optional<DataPackManager::DatapackMetadata> DataPackManager::parseMetadataJson(const json& j)
{
    DatapackMetadata metadata;

    // Datapack id
    if (!j.contains("id"))
    {
        std::cerr << "[DataPackManager]: Metadata is missing 'id' field\n";
        return std::nullopt;
    }
    const auto& jID = j.at("id");
    if (!jID.is_string())
    {
        std::cerr << "[DataPackManager]: Metadata 'id' field is not a string\n";
        return std::nullopt;
    }
    metadata.id = jID.get<std::string>();

    auto nameValidationResult = validateObjectName(metadata.id);
    if (nameValidationResult != ObjectNameValidationResult::Success)
    {
        printObjectNameValidationError(std::cerr, nameValidationResult, "[DataPackManager]: ", "Meta data id");
        return std::nullopt;
    }
    return metadata;
}

void DataPackManager::loadBlocks(const std::filesystem::path& dataPackPath, const std::string& dataPackStringId)
{
    fs::path blocksDir = dataPackPath / "Blocks";
    if (!fileExistsAndIsDirectory(blocksDir))
    {
        return;
    }

    //
    BlockAsset asset;

    // Iterate through all JSON files in directory
    for (const auto& entry : fs::directory_iterator(blocksDir))
    {
        // Check if entry is file and its extension is json
        if (!entry.is_regular_file() || entry.path().extension() != ".json")
        {
            continue;
        }

        // Get block name
        fs::path blockFilepath = entry.path();
        std::string blockName = blockFilepath.stem().string();

        // Check block name
        auto nameValidationResult = validateObjectName(blockName);
        if (nameValidationResult != ObjectNameValidationResult::Success)
        {
            printObjectNameValidationError(std::cerr, nameValidationResult, "[DataPackManager]: ", "Block name");
            continue;
        }

        // Open json file
        std::ifstream file(entry.path());
        if (!file)
        {
            std::cerr << "[DataPackManager]: Failed to open block file: " << blockFilepath << "\n";
            return;
        }

        // Parse
        BlockAsset asset;
        asset.stringId = dataPackStringId + ":" + blockName;
        std::cout << "[DataPackManager]: Loading block " << asset.stringId << "\n";
        try
        {
            json j;
            file >> j;
            if (!parseBlockJson(j, asset))
            {
                continue;
            }
        }
        catch (const json::exception& e)
        {
            std::cerr << "[DataPackManager]: JSON parsing error in file " << blockFilepath << ": " << e.what() << "\n";
            continue;
        }
        catch (const std::exception& e)
        {
            std::cerr << "[DataPackManager]: Error reading file " << blockFilepath << ": " << e.what() << "\n";
            continue;
        }

        // Register
        AssetRegistry::registerBlock(asset);
    }
}

bool DataPackManager::parseBlockJson(const json& j, BlockAsset& outAsset)
{
    return
        parseBlockPropertiesJson(j, outAsset) &&
        parseBlockVisualsJson(j, outAsset) &&
        parseBlockSoundsJson(j, outAsset);
}

bool DataPackManager::parseBlockPropertiesJson(const json& j, BlockAsset& outAsset)
{
    if (j.contains("absorbs_light"))
    {
        if (j.at("absorbs_light").is_boolean())
        {
            outAsset.absorbsLight = j.at("absorbs_light").get<bool>();
        }
        else
        {
            std::cerr << "[DataPackManager]: Block 'absorbs_light' field is not a boolean\n";
        }
    }

    if (j.contains("light_emission"))
    {
        if (j.at("light_emission").is_number())
        {
            outAsset.lightEmission = j.at("light_emission").get<uint8_t>();
        }
        else
        {
            std::cerr << "[DataPackManager]: Block 'absorbs_light' field is not a number\n";
        }
    }

    if (j.contains("raycastable"))
    {
        if (j.at("raycastable").is_boolean())
        {
            outAsset.raycastable = j.at("raycastable").get<bool>();
        }
        else
        {
            std::cerr << "[DataPackManager]: Block 'raycastable' field is not a boolean\n";
        }
    }

    return true;
}

bool DataPackManager::parseBlockVisualsJson(const json& j, BlockAsset& outAsset)
{
    // Model name
    if (!j.contains("model"))
    {
        outAsset.modelName = "None";
        return true;
    }
    else if (!j.at("model").is_string())
    {
        std::cerr << "[DataPackManager]: Block 'model' field is not a string\n";
        outAsset.modelName = "None";
        return true;
    }
    else
    {
        outAsset.modelName = j.at("model").get<std::string>();
        if (outAsset.modelName == "None")
        {
            return true;
        }
    }

    if (outAsset.modelName.empty())
    {
        outAsset.modelName = "None";
        return true;
    }

    auto nameValidationResult = validateObjectStringId(outAsset.modelName);
    if (nameValidationResult != ObjectNameValidationResult::Success)
    {
        printObjectNameValidationError(std::cerr, nameValidationResult, "[DataPackManager]: ", "Block model id");
        return false;
    }

    // Textures
    if (!j.contains("textures"))
    {
        return true;
    }
    else if (!j.at("textures").is_array())
    {
        std::cerr << "[DataPackManager]: Block 'textures' field is not an array\n";
        return true;
    }
    else if (j.at("textures").size() >= MAX_TEXTURE_SLOT_COUNT)
    {
        std::cerr << "[DataPackManager]: Block 'textures' array is bigger than limit of: " << MAX_TEXTURE_SLOT_COUNT << "\n";
        return true;
    }
    
    BlockAsset::TextureInfo textureInfo;

    for (const auto& textureJson : j.at("textures"))
    {
        // Texture name
        textureInfo.textureName.clear();

        if (!textureJson.contains("name"))
        {
            std::cerr << "[DataPackManager]: Block texture is missing 'name' field\n";
            return false;
        }
        else if (!textureJson.at("name").is_string())
        {
            std::cerr << "[DataPackManager]: Block texture 'name' field is not a string\n";
            return false;
        }

        textureInfo.textureName = textureJson.at("name").get<std::string>();

        // Texture transformation
        textureInfo.transformation = BlockAsset::TextureInfo::TextureTransformation::None;
        if (textureJson.contains("transformation"))
        {
            if (textureJson.at("transformation").is_string())
            {
                std::string transformationStr = textureJson.at("transformation").get<std::string>();
                if (transformationStr == "None")
                {
                    textureInfo.transformation = BlockAsset::TextureInfo::TextureTransformation::None;
                }
                else if (transformationStr == "Flip")
                {
                    textureInfo.transformation = BlockAsset::TextureInfo::TextureTransformation::Flip;
                }
                else if (transformationStr == "RotateAndFlip")
                {
                    textureInfo.transformation = BlockAsset::TextureInfo::TextureTransformation::RotateAndFlip;
                }
                else
                {
                    std::cerr << "[DataPackManager]: Block texture 'transformation' has invalid value\n";
                }
            }
            else
            {
                std::cerr << "[DataPackManager]: Block texture 'transformation' field is not a string\n";
            }
        }

        // Is texture translucent
        textureInfo.isTranslucent = false;
        if (textureJson.contains("translucent"))
        {
            if (textureJson.at("translucent").is_boolean())
            {
                textureInfo.isTranslucent = textureJson.at("translucent").get<bool>();
            }
            else
            {
                std::cerr << "[DataPackManager]: Block texture 'translucent' field is not a boolean\n";
            }
        }

        outAsset.textureInfo.push_back(textureInfo);
    }
    return true;
}

bool DataPackManager::parseBlockSoundsJson(const json& j, BlockAsset& outAsset)
{
    // Break sounds
    if (j.contains("break_sounds"))
    {
        if (j.at("break_sounds").is_array())
        {
            for (const auto& soundJson : j.at("break_sounds"))
            {
                if (soundJson.is_string())
                {
                    outAsset.breakSounds.push_back(soundJson.get<std::string>());
                }
            }
        }
        else
        {
            std::cerr << "[DataPackManager]: Block 'break_sounds' field is not an array\n";
        }
    }

    // Place sounds
    if (j.contains("place_sounds"))
    {
        if (j.at("place_sounds").is_array())
        {
            for (const auto& soundJson : j.at("place_sounds"))
            {
                if (soundJson.is_string())
                {
                    outAsset.placeSounds.push_back(soundJson.get<std::string>());
                }
            }
        }
        else
        {
            std::cerr << "[DataPackManager]: Block 'place_sounds' field is not an array\n";
        }
    }

    // Step sounds
    if (j.contains("step_sounds"))
    {
        if (j.at("step_sounds").is_array())
        {
            for (const auto& soundJson : j.at("step_sounds"))
            {
                if (soundJson.is_string())
                {
                    outAsset.stepSounds.push_back(soundJson.get<std::string>());
                }
            }
        }
        else
        {
            std::cerr << "[DataPackManager]: Block 'step_sounds' field is not an array\n";
        }
    }

    return true;
}

void DataPackManager::loadBlockModels(const std::filesystem::path& dataPackPath, const std::string& dataPackStringId)
{
    fs::path modelsDir = dataPackPath / "BlockModels";
    if (!fileExistsAndIsDirectory(modelsDir))
    {
        return;
    }

    // Iterate through all JSON files in directory
    for (const auto& entry : fs::directory_iterator(modelsDir))
    {
        // Check if entry is file and its extension is json
        if (!entry.is_regular_file() || entry.path().extension() != ".json")
        {
            continue;
        }

        // Get model name
        fs::path modelFilepath = entry.path();
        std::string modelName = modelFilepath.stem().string();

        // Check block name
        auto nameValidationResult = validateObjectName(modelName);
        if (nameValidationResult != ObjectNameValidationResult::Success)
        {
            printObjectNameValidationError(std::cerr, nameValidationResult, "[DataPackManager]: ", "Block model name");
            continue;
        }

        // Open json file
        std::ifstream file(entry.path());
        if (!file)
        {
            std::cerr << "[DataPackManager]: Failed to open block model file: " << modelFilepath << "\n";
            return;
        }

        // Parse
        BlockModelData asset;
        const std::string stringId = dataPackStringId + ":" + modelName;
        std::cout << "[DataPackManager]: Loading block model " << stringId << "\n";
        try
        {
            json j;
            file >> j;
            if (!parseBlockModelJson(j, asset))
            {
                continue;
            }
        }
        catch (const json::exception& e)
        {
            std::cerr << "[DataPackManager]: JSON parsing error in file " << modelFilepath << ": " << e.what() << "\n";
            continue;
        }
        catch (const std::exception& e)
        {
            std::cerr << "[DataPackManager]: Error reading file " << modelFilepath << ": " << e.what() << "\n";
            continue;
        }

        // Register
        AssetRegistry::registerBlockModel(asset, stringId);
    }
}

bool DataPackManager::parseBlockModelJson(const json& j, BlockModelData& outAsset)
{
    // Check faces field
    if (!j.contains("faces"))
    {
        std::cerr << "[DataPackManager]: Block model is missing 'faces' field\n";
        return false;
    }
    else if (!j.at("faces").is_array())
    {
        std::cerr << "[DataPackManager]: Block model 'faces' field is not an array\n";
        return false;
    }

    // Parse faces
    for (const auto& faceJson : j.at("faces"))
    {
        // Check type field
        if (!faceJson.contains("type"))
        {
            std::cerr << "[DataPackManager]: Block model face is missing 'type' field\n";
            continue;
        }
        else if (!faceJson.at("type").is_string())
        {
            std::cerr << "[DataPackManager]: Block model face 'type' field is not a string\n";
            continue;
        }

        std::string type = faceJson.at("type").get<std::string>();

        if (type == "aligned")
        {
            auto result = parseBlockModelAlignedFaceJson(faceJson);
            if (result.has_value())
            {
                const auto& face = result.value();
                outAsset.alignedFaces.push_back(face);
            }
        }
        else if (type == "non_aligned")
        {
            auto result = parseBlockModelUnalignedFaceJson(faceJson);
            if (result.has_value())
            {
                const auto& face = result.value();
                outAsset.unalignedFaces.push_back(face);
            }
        }
        else
        {
            std::cerr << "[DataPackManager]: Unknown block model face type: " << type << "\n";
        }
    }

    return true;
}

std::optional<BlockModelData::AlignedFace> DataPackManager::parseBlockModelAlignedFaceJson(const json& j)
{
    // Check
    if (!j.contains("normal"))
    {
        std::cerr << "[DataPackManager]: Block model aligned face is missing 'normal' field\n";
        return std::nullopt;
    }
    else if (!j.at("normal").is_number_unsigned())
    {
        std::cerr << "[DataPackManager]: Block model aligned face 'normal' field is not an unsigned integer\n";
        return std::nullopt;
    }

    if (!j.contains("texture_slot"))
    {
        std::cerr << "[DataPackManager]: Block model aligned face is missing 'texture_slot' field\n";
        return std::nullopt;
    }
    else if (!j.at("texture_slot").is_number_unsigned())
    {
        std::cerr << "[DataPackManager]: Block model aligned face 'texture_slot' field is not an unsigned integer\n";
        return std::nullopt;
    }

    // Get
    BlockModelData::AlignedFace face;
    face.normal = j.at("normal");
    face.textureSlot = j.at("texture_slot");

    return face;
}

std::optional<BlockModelData::UnalignedFace> DataPackManager::parseBlockModelUnalignedFaceJson(const json& j)
{
    // Check fields
    if (!j.contains("vertices"))
    {
        std::cerr << "[DataPackManager]: Block model non-aligned face is missing 'vertices' field\n";
        return std::nullopt;
    }
    else if (!j.at("vertices").is_array() || j.at("vertices").size() != 4)
    {
        std::cerr << "[DataPackManager]: Block model non-aligned face 'vertices' field is not an array of 4 elements\n";
        return std::nullopt;
    }

    if (!j.contains("uv"))
    {
        std::cerr << "[DataPackManager]: Block model non-aligned face is missing 'uv' field\n";
        return std::nullopt;
    }
    else if (!j.at("uv").is_array() || j.at("uv").size() != 4)
    {
        std::cerr << "[DataPackManager]: Block model non-aligned face 'uv' field is not an array of 4 elements\n";
        return std::nullopt;
    }

    if (!j.contains("texture_slot"))
    {
        std::cerr << "[DataPackManager]: Block model non-aligned face is missing 'texture_slot' field\n";
        return std::nullopt;
    }
    else if (!j.at("texture_slot").is_number_unsigned())
    {
        std::cerr << "[DataPackManager]: Block model non-aligned face 'texture_slot' field is not an unsigned integer\n";
        return std::nullopt;
    }

    // Parse vertices
    int vertices[4][3];
    const auto& verticesJson = j.at("vertices");
    for (int i = 0; i < 4; i++)
    {
        const auto& vertexJson = verticesJson[i];
        if (!vertexJson.contains("x") || !vertexJson.at("x").is_number() ||
            !vertexJson.contains("y") || !vertexJson.at("y").is_number() ||
            !vertexJson.contains("z") || !vertexJson.at("z").is_number())
        {
            std::cerr << "[DataPackManager]: Block model non-aligned face vertex " << i << " is missing or has invalid coordinates\n";
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

    // Parse UV coordinates
    int uvs[4][2];
    const auto& uvsJson = j.at("uv");
    for (int i = 0; i < 4; i++)
    {
        const auto& uvJson = uvsJson[i];
        if (!uvJson.contains("u") || !uvJson.at("u").is_number() ||
            !uvJson.contains("v") || !uvJson.at("v").is_number())
        {
            std::cerr << "[DataPackManager]: Block model non-aligned face UV " << i << " is missing or has invalid coordinates\n";
            return std::nullopt;
        }

        float u, v;
        uvJson.at("u").get_to(u);
        uvJson.at("v").get_to(v);

        uvs[i][0] = 16.0f * fminf(1.0f, fmaxf(0.0f, u));
        uvs[i][1] = 16.0f * fminf(1.0f, fmaxf(0.0f, v));
    }

    // Pack vertices
    BlockModelData::UnalignedFace face;

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
    face.textureSlot = j.at("texture_slot");

    return face;
}

void DataPackManager::loadItems(const std::filesystem::path& dataPackPath, const std::string& dataPackStringId)
{
    fs::path itemsDir = dataPackPath / "Items";
    if (!fileExistsAndIsDirectory(itemsDir))
    {
        return;
    }

    // Iterate through all JSON files in directory
    for (const auto& entry : fs::directory_iterator(itemsDir))
    {
        // Check if entry is file and its extension is json
        if (!entry.is_regular_file() || entry.path().extension() != ".json")
        {
            continue;
        }

        // Get item name
        fs::path itemFilepath = entry.path();
        std::string itemName = itemFilepath.stem().string();

        // Check block name
        auto nameValidationResult = validateObjectName(itemName);
        if (nameValidationResult != ObjectNameValidationResult::Success)
        {
            printObjectNameValidationError(std::cerr, nameValidationResult, "[DataPackManager]: ", "Block model name");
            continue;
        }

        // Open json file
        std::ifstream file(entry.path());
        if (!file)
        {
            std::cerr << "[DataPackManager]: Failed to open iem file: " << itemFilepath << "\n";
            return;
        }

        // Parse
        ItemAsset asset;
        asset.stringId = dataPackStringId + ":" + itemName;
        std::cout << "[DataPackManager]: Loading item " << asset.stringId << "\n";
        try
        {
            json j;
            file >> j;
            if (!parseItemJson(j, asset))
            {
                continue;
            }
        }
        catch (const json::exception& e)
        {
            std::cerr << "[DataPackManager]: JSON parsing error in file " << itemFilepath << ": " << e.what() << "\n";
            continue;
        }
        catch (const std::exception& e)
        {
            std::cerr << "[DataPackManager]: Error reading file " << itemFilepath << ": " << e.what() << "\n";
            continue;
        }

        // Register
        AssetRegistry::registerItem(asset);
    }
}

bool DataPackManager::parseItemJson(const json& j, ItemAsset& outAsset)
{
    // Stack size
    if (!j.contains("stack_size"))
    {
        std::cerr << "[DataPackManager]: Item is missing 'stack_size' field\n";
        outAsset.stackSize = 64;
        return true;
    }
    else if (!j.at("stack_size").is_number_unsigned())
    {
        std::cerr << "[DataPackManager]: Item 'stack_size' field is not an unsigned number\n";
        outAsset.stackSize = 64;
        return true;
    }
    uint32_t stackSize = j.at("stack_size").get<uint32_t>();
    if (stackSize > 255)
    {
        std::cerr << "[DataPackManager]: Item 'stack_size' field value is larger than limit of 255\n";
        stackSize = 255;
    }
    outAsset.stackSize = static_cast<uint8_t>(stackSize);

    // Block placeable
    if (j.contains("block_placeable"))
    {
        if (!j.at("block_placeable").is_string())
        {
            std::cerr << "[DataPackManager]: Item 'block_placeable' field is not a string\n";
        }
        else
        {
            outAsset.blockPlaceableStringId = j.at("block_placeable").get<std::string>();
            auto nameValidationResult = validateObjectStringId(outAsset.blockPlaceableStringId);
            if (nameValidationResult != ObjectNameValidationResult::Success)
            {
                printObjectNameValidationError(std::cerr, nameValidationResult, "[DataPackManager]: ", "Item string id");
                outAsset.blockPlaceableStringId.clear();
            }
            else
            {
                outAsset.hasBlockPlaceable = true;
            }
        }
    }

    // Texture
    if (!j.contains("ui_texture"))
    {
        return true;
    }
    else if (!j.at("ui_texture").is_string())
    {
        std::cerr << "[DataPackManager]: Item 'ui_texture' field is not a string\n";
        return true;
    }
    outAsset.uiTextureName = j.at("ui_texture").get<std::string>();

    return true;
}

void DataPackManager::loadItemModels(const std::filesystem::path& dataPackPath, const std::string& dataPackStringId)
{
    fs::path modelsDir = dataPackPath / "ItemModels";
    if (!!fileExistsAndIsDirectory(modelsDir))
    {
        return;
    }

    // Iterate through all JSON files in directory
    for (const auto& entry : fs::directory_iterator(modelsDir))
    {
        // Check if entry is file and its extension is json
        if (!entry.is_regular_file() || entry.path().extension() != ".json")
        {
            continue;
        }

        // Get model name
        fs::path modelFilepath = entry.path();
        std::string modelName = modelFilepath.stem().string();

        // Check block name
        auto nameValidationResult = validateObjectName(modelName);
        if (nameValidationResult != ObjectNameValidationResult::Success)
        {
            printObjectNameValidationError(std::cerr, nameValidationResult, "[DataPackManager]: ", "Item model name");
            continue;
        }

        // Open json file
        std::ifstream file(entry.path());
        if (!file)
        {
            std::cerr << "[DataPackManager]: Failed to open item model file: " << modelFilepath << "\n";
            return;
        }

        // Parse
        ItemModelData asset;
        const std::string stringId = dataPackStringId + ":" + modelName;
        std::cout << "[DataPackManager]: Loading item model " << stringId << "\n";
        try
        {
            json j;
            file >> j;
            if (!parseItemModelJson(j, asset))
            {
                continue;
            }
        }
        catch (const json::exception& e)
        {
            std::cerr << "[DataPackManager]: JSON parsing error in file " << modelFilepath << ": " << e.what() << "\n";
            continue;
        }
        catch (const std::exception& e)
        {
            std::cerr << "[DataPackManager]: Error reading file " << modelFilepath << ": " << e.what() << "\n";
            continue;
        }

        // Register
        AssetRegistry::registerItemModel(asset, stringId);
    }
}

bool DataPackManager::parseItemModelJson(const json& j, ItemModelData& outAsset)
{
    // Check faces field
    if (!j.contains("faces"))
    {
        std::cerr << "[DataPackManager]: Item model is missing 'faces' field\n";
        return false;
    }
    else if (!j.at("faces").is_array())
    {
        std::cerr << "[DataPackManager]: Item model 'faces' field is not an array\n";
        return false;
    }

    // Parse faces
    for (const auto& faceJson : j.at("faces"))
    {
        auto result = parseItemModelFacesJson(faceJson);
        if (result.has_value())
        {
            const auto& face = result.value();
            outAsset.faces.push_back(face);
        }
    }

    return true;
}

std::optional<ItemModelData::Face> DataPackManager::parseItemModelFacesJson(const json& j)
{
    // Check fields
    if (!j.contains("vertices"))
    {
        std::cerr << "[DataPackManager]: Item model face is missing 'vertices' field\n";
        return std::nullopt;
    }
    else if (!j.at("vertices").is_array() || j.at("vertices").size() != 4)
    {
        std::cerr << "[DataPackManager]: Item model face 'vertices' field is not an array of 4 elements\n";
        return std::nullopt;
    }

    if (!j.contains("uv"))
    {
        std::cerr << "[DataPackManager]: Item model face is missing 'uv' field\n";
        return std::nullopt;
    }
    else if (!j.at("uv").is_array() || j.at("uv").size() != 4)
    {
        std::cerr << "[DataPackManager]: Item model face 'uv' field is not an array of 4 elements\n";
        return std::nullopt;
    }

    if (!j.contains("texture_slot"))
    {
        std::cerr << "[DataPackManager]: Item model face is missing 'texture_slot' field\n";
        return std::nullopt;
    }
    else if (!j.at("texture_slot").is_number_unsigned())
    {
        std::cerr << "[DataPackManager]: Item model face 'texture_slot' field is not an unsigned integer\n";
        return std::nullopt;
    }

    // Parse vertices
    int vertices[4][3];
    const auto& verticesJson = j.at("vertices");
    for (int i = 0; i < 4; i++)
    {
        const auto& vertexJson = verticesJson[i];
        if (!vertexJson.contains("x") || !vertexJson.at("x").is_number() ||
            !vertexJson.contains("y") || !vertexJson.at("y").is_number() ||
            !vertexJson.contains("z") || !vertexJson.at("z").is_number())
        {
            std::cerr << "[DataPackManager]: Item model face vertex " << i << " is missing or has invalid coordinates\n";
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

    // Parse UV coordinates
    int uvs[4][2];
    const auto& uvsJson = j.at("uv");
    for (int i = 0; i < 4; i++)
    {
        const auto& uvJson = uvsJson[i];
        if (!uvJson.contains("u") || !uvJson.at("u").is_number() ||
            !uvJson.contains("v") || !uvJson.at("v").is_number())
        {
            std::cerr << "[DataPackManager]: Item model face UV " << i << " is missing or has invalid coordinates\n";
            return std::nullopt;
        }

        float u, v;
        uvJson.at("u").get_to(u);
        uvJson.at("v").get_to(v);

        uvs[i][0] = 16.0f * fminf(1.0f, fmaxf(0.0f, u));
        uvs[i][1] = 16.0f * fminf(1.0f, fmaxf(0.0f, v));
    }

    // Pack vertices
    ItemModelData::Face face;

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
    face.textureSlot = j.at("texture_slot");

    return face;
}
