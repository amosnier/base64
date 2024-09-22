#pragma once

#include <algorithm> // IWYU pragma: keep
#include <ranges>
#include <string_view>

namespace base64::detail {
inline constexpr auto base64_chars =
    std::string_view{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"};
}

// @brief Adaptor that converts its std::byte input range into an RFC 4648 Base64 character range view
//
// Usage example:
//
// \code{.cpp}
// std::array{std::byte{'M'}, std::byte{'a'}, std::byte{'n'}} | encode64 // will generate view on "TWFu"sv
// \endcode
inline constexpr auto encode64 = [] {
	using namespace std::views;
	using std::ranges::fold_left;

	return transform([](std::byte byte) -> uint32_t { return static_cast<uint32_t>(byte); }) | chunk(3) |
	       transform([](auto &&triplet) {
		       static constexpr auto shift = 24;
		       static constexpr uint32_t one{1 << shift};

		       // Note: we store the number of missing bytes (zero, one, or two) to the otherwise unused MSB, to
		       // handle padding.
		       const auto word = fold_left(triplet, 3 * one, [](uint32_t o1, uint32_t o2) -> uint32_t {
			       static constexpr uint32_t mask{~(one - 1)};
			       return ((o1 - one) & mask) | (o1 << 8) | o2;
		       });

		       const auto num_missing = static_cast<int>(word >> shift);

		       return iota(0, 4) | reverse | transform([word, num_missing](int i) {
				      return i < num_missing ? '='
							     : base64::detail::base64_chars.at(
								   (word << (8 * num_missing) >> (6 * i)) & 0x3F);
			      });
	       }) |
	       join;
}();

// @brief Adaptor that converts its RFC 4648 Base64 character input range into a std::byte range view.
//
// This implementation ambitions to strictly implement the RFC specification. In particular:
// - Any octet triplet containing an illegal character is rejected, i.e. it is not emitted to the output view.
// - In case of illegal padding, the implementation produces a result, but it is basically undefined (the RFC does not
// mandate any other behaviour).
//
// The RFC mandates: "Implementations MUST reject the encoded data if it contains characters outside the base alphabet
// when interpreting base-encoded data". This implementation does that, with an important caveat. Since it is
// range-based, it has no way of rejecting more than one octet triplet at a time. Only higher level code has the ability
// to reject larger chunks of data. Since this implementation does not emit illegal octets triplets, higher level
// validation could for instance be achieved by comparing the size of the output produced by this implementation with
// the expected output size.
//
// Usage example:
//
// \code{.cpp}
// "TWFu"sv | decode16 // will generate view on std::array{std::byte{'M'}, std::byte{'a'}, std::byte{'n'}}
// \endcode
inline constexpr auto decode64 = [] {
	using namespace std::views;

	using std::ranges::fold_left;

	return transform([](char encoded) -> uint32_t {
		       static constexpr auto u = std::bit_cast<char, uint8_t>;
		       static constexpr auto illegal = 0x80;
		       static constexpr auto flag = 0x40;

		       static constexpr auto lookup = [] -> std::array<uint8_t, 0xFF> {
			       std::array<uint8_t, 0xFF> a{};
			       a.fill(illegal);
			       uint8_t i{};
			       for (const auto c : base64::detail::base64_chars) {
				       a.at(u(c)) = i++;
			       }
			       // Use the otherwise unused next to most significant bit as a padding flag.
			       a.at(u('=')) = flag;
			       return a;
		       }();

		       // Store sextet to a uint32_t, and move the padding flag to the MSB. This is analogous to what
		       // we do during encoding. Move the illegal flag to the most significant nibble.
		       const uint32_t n{lookup.at(u(encoded))};
		       return ((n & flag) << 18) | ((n & illegal) << 21) | (n & ~(flag | illegal));
	       }) |
	       chunk(4) | transform([](auto &&quad) {
		       static constexpr auto shift = 24;
		       static constexpr uint32_t one{1 << shift};
		       static constexpr uint32_t mask{one - 1};
		       static constexpr auto max_num_octets = 3;

		       const auto word = fold_left(quad, 0, [](uint32_t s1, uint32_t s2) {
			       return ((s1 & ~mask) + (s2 & ~mask)) | ((s1 & mask) << 6) | (s2 & mask);
		       });

		       const auto is_illegal = (word >> (shift + 4)) > 0;
		       const auto num_missing =
			   is_illegal ? max_num_octets : static_cast<int>(word >> shift) & max_num_octets;

		       return iota(0, max_num_octets - num_missing) | transform([word](int i) -> std::byte {
				      return static_cast<std::byte>(word >> ((2 - i) * 8));
			      });
	       }) |
	       join;
}();
