#pragma once

#include <array>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <unordered_map>

#include "typedefs.hpp"
#include "maths/Heightmap.hpp"
#include "StructurePlacement.hpp"

class Content;
class VoxelFragment;
struct GeneratorDef;

struct VoxelStructureMeta {
    std::string name;
};

struct BlocksLayer {
    /// @brief Layer block
    std::string block;
    
    /// @brief Layer height. -1 is resizeable layer
    int height;

    /// @brief Layer can present under the sea level (default: true) else will
    /// extend the next layer
    bool belowSeaLevel;

    struct {
        /// @brief Layer block index
        blockid_t id;
    } rt;
};

/// @brief Set of blocks layers with additional info
struct BlocksLayers {
    std::vector<BlocksLayer> layers;

    /// @brief Total height of all layers after the resizeable one
    uint lastLayersHeight;
};

struct BiomeParameter {
    /// @brief Central parameter value for the biome
    float value;
    /// @brief Parameter score multiplier 
    /// (the higher the weight, the greater the chance of biome choosing)
    float weight;
};

struct WeightedEntry {
    std::string name;
    float weight;

    struct {
        size_t id;
    } rt;

    bool operator>(const WeightedEntry& other) const {
        return weight > other.weight;
    }
};

struct BiomeElementList {
    static inline float MIN_CHANCE = 1e-6f;

    /// @brief Entries sorted by weight descending.
    std::vector<WeightedEntry> entries;
    /// @brief Sum of weight values
    float weightsSum = 0.0f;
    /// @brief Value generation chance
    float chance;

    BiomeElementList() {}

    BiomeElementList(std::vector<WeightedEntry> entries, float chance)
     : entries(entries), chance(chance) {
        for (const auto& entry : entries) {
            weightsSum += entry.weight;
        }
    }

    /// @brief Choose value based on weight
    /// @param rand some random value in range [0, 1)
    /// @return *.index of chosen value
    inline size_t choose(float rand, size_t def=0) const {
        if (entries.empty() || rand > chance || chance < MIN_CHANCE) {
            return def;
        }
        rand = rand / chance;
        rand *= weightsSum;
        for (const auto& entry : entries) {
            rand -= entry.weight;
            if (rand <= 0.0f) {
                return entry.rt.id;
            }
        }
        return entries[entries.size()-1].rt.id;
    }
};

struct Biome {
    /// @brief Biome name
    std::string name;

    std::vector<BiomeParameter> parameters;

    /// @brief Plant is a single-block structure randomly generating in world
    BiomeElementList plants;

    BiomeElementList structures;

    BlocksLayers groundLayers;

    BlocksLayers seaLayers;
};

/// @brief Generator behaviour and settings interface
class GeneratorScript {
public:
    virtual ~GeneratorScript() = default;

    /// @brief Generate a heightmap with values in range 0..1
    /// @param offset position of the heightmap in the world
    /// @param size size of the heightmap
    /// @param seed world seed
    /// @param bpd blocks per dot
    /// @return generated heightmap (can't be nullptr)
    virtual std::shared_ptr<Heightmap> generateHeightmap(
        const glm::ivec2& offset,
        const glm::ivec2& size,
        uint64_t seed,
        uint bpd
    ) = 0;

    /// @brief Generate a biomes parameters maps
    /// @param offset position of maps in the world
    /// @param size maps size
    /// @param seed world seed
    /// @param bpd blocks per dot
    /// @return generated maps (can't be nullptr)
    virtual std::vector<std::shared_ptr<Heightmap>> generateParameterMaps(
        const glm::ivec2& offset,
        const glm::ivec2& size,
        uint64_t seed,
        uint bpd
    ) = 0;

    /// @brief Generate a list of structures placements. Structures may be
    /// placed to nearest N chunks also (position of out area), where N is 
    /// wide-structs-chunks-radius
    /// @param offset position of the area
    /// @param size size of the area (blocks)
    /// @param seed world seed
    /// @param chunkHeight chunk height to use as heights multiplier
    virtual std::vector<Placement> placeStructuresWide(
        const glm::ivec2& offset, 
        const glm::ivec2& size, 
        uint64_t seed,
        uint chunkHeight
    ) = 0;

    /// @brief Generate a list of structures placements. Structures may be
    /// placed to nearest chunks also (position of out area).
    /// @param offset position of the area
    /// @param size size of the area (blocks)
    /// @param seed world seed
    /// @param heightmap area heightmap
    /// @param chunkHeight chunk height to use as heights multiplier
    /// @return structure & line placements
    virtual std::vector<Placement> placeStructures(
        const glm::ivec2& offset, const glm::ivec2& size, uint64_t seed,
        const std::shared_ptr<Heightmap>& heightmap, uint chunkHeight) = 0;
};

/// @brief Structure voxel fragments and metadata
struct VoxelStructure {
    VoxelStructureMeta meta;
    /// @brief voxel fragment and 3 pre-calculated rotated versions
    std::array<std::unique_ptr<VoxelFragment>, 4> fragments;

    VoxelStructure(
        VoxelStructureMeta meta,
        std::unique_ptr<VoxelFragment> structure
    );
};

/// @brief Generator information
struct GeneratorDef {
    /// @brief Generator full name - packid:name
    std::string name;
    /// @brief Generator display name
    std::string caption;

    std::unique_ptr<GeneratorScript> script;

    /// @brief Sea level (top of seaLayers)
    uint seaLevel = 0;

    /// @brief Number of biome parameters, that biome choosing depending on
    uint biomeParameters = 0;

    /// @brief Biome parameter map blocks per dot
    uint biomesBPD = 8;

    /// @brief Heightmap blocks per dot
    uint heightsBPD = 4;

    /// @brief Number of chunks must be generated before and after wide
    /// structures placement triggered
    uint wideStructsChunksRadius = 3;

    std::unordered_map<std::string, size_t> structuresIndices;
    std::vector<std::unique_ptr<VoxelStructure>> structures;
    std::vector<Biome> biomes;

    GeneratorDef(std::string name);
    GeneratorDef(const GeneratorDef&) = delete;

    void prepare(const Content* content);
};