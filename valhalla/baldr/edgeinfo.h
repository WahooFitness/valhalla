#ifndef VALHALLA_BALDR_EDGEINFO_H_
#define VALHALLA_BALDR_EDGEINFO_H_

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <valhalla/baldr/graphid.h>
#include <valhalla/baldr/json.h>
#include <valhalla/midgard/encoded.h>
#include <valhalla/midgard/pointll.h>
#include <valhalla/midgard/util.h>

namespace valhalla {
namespace baldr {

constexpr size_t kMaxNamesPerEdge = 15;
constexpr size_t kMaxEncodedShapeSize = 65535;

// Use elevation bins of 2 meters to store mean elevation. Clamp to a range
// from -500 meters to 7683 meters.
constexpr uint32_t kMaxStoredElevation = 4095; // 12 bits
constexpr float kElevationBinSize = 2.0f;
constexpr float kMinElevation = -500.0f;
constexpr float kMaxElevation = kMinElevation + (kElevationBinSize * kMaxStoredElevation);

constexpr int kElevationSampleEncodePrecision = 10;
constexpr double kElevationSampleDecodePrecision = 0.1;

// Name information. Information about names added to the names list within
// the tile. A name can have a textual representation followed by optional
// fields that provide additional information about the name.
struct NameInfo {
  uint32_t name_offset_ : 24;      // Offset to start of text string
  uint32_t additional_fields_ : 4; // Additional text fields following
                                   // the name. These can be used for
                                   // additional information like language
                                   // phonetic string, etc.
  uint32_t is_route_num_ : 1;      // Flag used to indicate if this is a route number
                                   // vs just a name.
  uint32_t tagged_ : 1;            // This indicates the text string is
                                   // specially tagged (for example uses the first char as
                                   // the tag type). To make this forward and backward
                                   // compatible, tagged text will not be read in GetNames
                                   // and GetNamesAndTags until code is ready to actually use it.
  uint32_t spare_ : 2;

  bool operator==(const NameInfo& other) const {
    return (name_offset_ == other.name_offset_);
  }

  // operator < for sorting
  bool operator<(const NameInfo& other) const {
    return (name_offset_ < other.name_offset_);
  }
};

struct linguistic_text_header_t {
  uint32_t language_ : 8; // this is just the language as we will derive locale by getting admin info
  uint32_t length_ : 8;   // pronunciation length
  uint32_t phonetic_alphabet_ : 3;
  uint32_t name_index_ : 4; // what name is this pronunciation for
  uint32_t spare_ : 1;
  uint32_t DO_NOT_USE_ : 8; // DONT EVER USE THIS WE DON'T ACTUALLY STORE IT IN THE TEXT LIST
};

/**
 * Edge information not required in shortest path algorithm and is
 * common among the 2 directions.
 */
class EdgeInfo {
public:
  EdgeInfo() = delete;
  EdgeInfo(const EdgeInfo& other) = delete;
  EdgeInfo& operator=(const EdgeInfo&) = delete;
  EdgeInfo(EdgeInfo&&) = default;
  EdgeInfo& operator=(EdgeInfo&&) = default;

  /**
   * Constructor
   * @param  ptr  Pointer to a bit of memory that has the info for this edge
   * @param  names_list  Pointer to the start of the text/names list.
   * @param  names_list_length  Length (bytes) of the text/names list.
   * @param elevation_samples Pointer to encoded elevation samples block
   * @param elevation_samples_size Length, in bytes, of encoded elevation samples block
   */
  EdgeInfo(const char* ptr, const char* names_list, const size_t names_list_length,
           const char* elevation_samples = nullptr, size_t elevation_samples_size = 0);

  /**
   * Destructor
   */
  virtual ~EdgeInfo();

  /**
   * Gets the OSM way Id.
   * @return  Returns the OSM way Id.
   */
  uint64_t wayid() const {
    return (static_cast<uint64_t>(extended_wayid3_) << 56) |
           (static_cast<uint64_t>(extended_wayid2_) << 48) |
           (static_cast<uint64_t>(ei_.extended_wayid1_) << 40) |
           (static_cast<uint64_t>(ei_.extended_wayid0_) << 32) | static_cast<uint64_t>(ei_.wayid_);
  }

  /**
   * Get the mean elevation along the edge.
   * @return  Returns mean elevation in meters relative to sea level.
   */
  float mean_elevation() const {
    return kMinElevation + (ei_.mean_elevation_ * kElevationBinSize);
  }

  /**
   * Get the bike network mask for this directed edge.
   * @return  Returns the bike network mask for this directed edge.
   */
  uint32_t bike_network() const {
    return ei_.bike_network_;
  }

  /**
   * Gets the speed limit in KPH.
   * @return  Returns the speed limit in KPH.
   */
  uint32_t speed_limit() const {
    return ei_.speed_limit_;
  }

  /**
   * Get the number of names.
   * @return Returns the name count.
   */
  uint32_t name_count() const {
    return ei_.name_count_;
  }

  /**
   * Get the size of the encoded shape (number of bytes).
   * @return  Returns the shape size.
   */
  uint32_t encoded_shape_size() const {
    return ei_.encoded_shape_size_;
  }

  /**
   * Get the name info for the specified name index.
   * @param  index  Index into the name list.
   * @return  Returns the name info.
   */
  NameInfo GetNameInfo(uint8_t index) const;

  /**
   * Convenience method to get the names for an edge
   * @return   Returns a list (vector) of names.
   */
  std::vector<std::string> GetNames() const;

  /**
   * Convenience method to get the names for an edge
   * @param  only_pronunciations  Bool indicating whether or not to return only the pronunciations
   *
   * @return   Returns a list (vector) of tagged names.
   */
  std::vector<std::string> GetTaggedValues(bool only_pronunciations = false) const;

  /**
   * Convenience method to get the names and route number flags for an edge.
   * @param  include_tagged_values  Bool indicating whether or not to return the tagged values too
   *
   * @return   Returns a list (vector) of name/route number pairs.
   */
  std::vector<std::pair<std::string, bool>> GetNamesAndTypes(std::vector<uint8_t>& types,
                                                             bool include_tagged_names = false) const;

  /**
   * Convenience method to get tags of the edge.
   * Key incidates tag, value may contain arbitrary blob of data.
   * @return   Returns a map of tags
   */
  const std::multimap<TaggedValue, std::string>& GetTags() const;

  /**
   * Convenience method to get a pronunciation map for an edge.
   * @return   Returns a unordered_map of type/name pairs with a key that references the name
   * index from GetNamesAndTypes
   */
  std::unordered_map<uint8_t, std::pair<uint8_t, std::string>> GetPronunciationsMap() const;

  /**
   * Convenience method to get the types for the names.
   * @return   Returns types - If a bit is set, it is a route number.
   */
  uint16_t GetTypes() const;

  /**
   * Get the shape of the edge.
   * @return  Returns the the list of lat,lng points describing the
   *          shape of the edge.
   */
  const std::vector<midgard::PointLL>& shape() const;

  midgard::Shape7Decoder<midgard::PointLL> lazy_shape() const {
    return midgard::Shape7Decoder<midgard::PointLL>(encoded_shape_, ei_.encoded_shape_size_);
  }

  /**
   * Returns the encoded shape string.
   * @return  Returns the encoded shape string.
   */
  std::string encoded_shape() const;

  /**
   * Returns a block of encoded elevation samples
   * @return Returns the encoded elevation sample string
   */
  std::string_view encoded_elevation_samples() const;

  /**
   * Gets elevation samples corresponding to the edge shape
   * @return List of elevation values corresponding to shape points
   */
  std::vector<double> elevation_samples() const;

  /**
   * Get layer index of the edge relatively to other edges(Z-level). Can be negative.
   * @see https://wiki.openstreetmap.org/wiki/Key:layer
   * @return layer index of the edge
   */
  int8_t layer() const;

  /**
   * Returns json representing this object
   * @return json object
   */
  json::MapPtr json() const;

  // Operator EqualTo based on nodea and nodeb.
  bool operator==(const EdgeInfo& rhs) const;

  // Fixed size data within EdgeInfo
  struct EdgeInfoInner {
    uint32_t wayid_ : 32; // OSM way Id

    uint32_t mean_elevation_ : 12; // Mean elevation with 2 meter precision
    uint32_t bike_network_ : 4;    // Mask of bicycle network types (see graphconstants.h)
    uint32_t speed_limit_ : 8;     // Speed limit (kph)
    uint32_t extended_wayid0_ : 8; // Next byte of the way id

    uint32_t name_count_ : 4;          // How many name infos we expect
    uint32_t encoded_shape_size_ : 16; // How many bytes long the encoded shape is
    uint32_t extended_wayid1_ : 8;     // Next next byte of the way id
    uint32_t extended_wayid_size_ : 2; // How many more bytes the way id is stored in
    uint32_t spare0_ : 2;              // not used
  };

protected:
  // Fixed size information
  EdgeInfoInner ei_;

  // List of name information (offsets, etc.)
  const NameInfo* name_info_list_;

  // The encoded shape of the edge
  const char* encoded_shape_;

  // Where we optionally keep the last 2 bytes of a 64bit wayid
  uint8_t extended_wayid2_;
  uint8_t extended_wayid3_;

  // Lng, lat shape of the edge
  mutable std::vector<midgard::PointLL> shape_;

  // The list of names within the tile
  const char* names_list_;

  // The size of the names list
  size_t names_list_length_;

  // encoded elevation data within the tile
  const char* elevation_samples_;

  // length of encoded elevation data
  size_t elevation_samples_size_;
  // for fast access to tag values stored in names list
  mutable std::multimap<TaggedValue, std::string> tag_cache_;
  mutable bool tag_cache_ready_ = false;
};

} // namespace baldr
} // namespace valhalla

#endif // VALHALLA_BALDR_EDGEINFO_H_
