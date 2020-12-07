#include "midgard/encoded.h"

namespace valhalla {
namespace midgard {

void encode7Sample(int number, std::string& output) {
  // get the sign bit down on the least significant end to
  // make the most significant bits mostly zeros
  number = number < 0 ? ~(static_cast<unsigned int>(number) << 1) : number << 1;
  // we take 7 bits of this at a time
  while (number > 0x7f) {
    // marking the most significant bit means there are more pieces to come
    int nextValue = (0x80 | (number & 0x7f));
    output.push_back(static_cast<char>(nextValue));
    number >>= 7;
  }
  // write the last chunk
  output.push_back(static_cast<char>(number & 0x7f));
}

int32_t decode7Sample(const char** begin, const char* end, int32_t previous) noexcept(false) {
  int32_t byte, shift = 0, result = 0;
  do {
    if (*begin == end) {
      throw std::runtime_error("Bad encoded polyline");
    }
    // take the least significant 7 bits shifted into place
    byte = *(*begin)++;
    result |= (byte & 0x7f) << shift;
    shift += 7;
    // if the most significant bit is set there is more to this number
  } while (byte & 0x80);
  // handle the bit flipping and add to previous since its an offset
  return previous + ((result & 1 ? ~result : result) >> 1);
}

std::string encode7Samples(const std::vector<double>& values, int precision) {
  auto output = std::string{};

  auto lastValue = 0;
  for (const auto value : values) {
    const auto scaledValue = static_cast<int>(std::round(value * precision));
    encode7Sample(scaledValue - lastValue, output);
    lastValue = scaledValue;
  }

  return output;
}

std::vector<double> decode7Samples(const std::string& encodedString,
                                   double precision) noexcept(false) {
  auto decoded = std::vector<double>{};

  auto begin = encodedString.data();
  auto end = begin + encodedString.size();

  auto previous = 0;
  while (begin != end) {
    previous = decode7Sample(&begin, end, previous);
    decoded.push_back(static_cast<double>(previous) * precision);
  }

  return decoded;
}

} // namespace midgard
} // namespace valhalla
