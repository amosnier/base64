#pragma once

#include <algorithm> // IWYU pragma: keep
/* #include <array> */
#include <cstddef>
/* #include <limits> */
#include <limits>
#include <ranges>
#include <string_view>

namespace detail {
inline constexpr auto base64_chars =
    std::string_view{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"};
}

// @brief Adaptor that converts its std::byte input range into an RFC 4648 Base64 character range view
//
// Usage example:
//
// \code{.cpp}
// // Will generate a view on "TWFu"sv
// std::array{std::byte{'M'}, std::byte{'a'}, std::byte{'n'}} | encode64
// \endcode
inline constexpr auto encode64 = [] {
	using namespace std::views;
	using std::ranges::fold_left;

	static constexpr auto shift = 24;
	static constexpr unsigned long is_present = 1 << shift;

	return transform(
		   [](std::byte byte) -> unsigned long { return is_present | static_cast<unsigned long>(byte); }) |
	       chunk(3) | transform([](auto &&triplet) {
		       // Strategy: store the number of bytes in the fourth byte and the values in the three LSBs
		       static constexpr auto present_mask = 0xFF << shift;
		       static constexpr auto value_mask = static_cast<int>(is_present - 1);
		       const auto word =
			   fold_left(triplet, 0U, [](unsigned long o1, unsigned long o2) -> unsigned long {
				   return ((o1 + o2) & present_mask) | (((o1 << 8) | o2) & value_mask);
			   });
		       const auto num_missing = static_cast<int>(3 - (word >> shift));
		       return iota(0, 4) | reverse | transform([word, num_missing](int i) {
				      return i < num_missing ? '='
							     : detail::base64_chars.at(
								   (word << (8 * num_missing) >> (6 * i)) & 0x3F);
			      });
	       }) |
	       join;
}();

// @brief Adaptor that converts its RFC 4648 Base64 character input range into a std::byte range view.
//
// This implementation ambitions to strictly implement the RFC specification. In particular:
// - Any octet triplet containing an illegal character or illegal or absent padding is rejected, i.e. it is not emitted
// to the output view.
// - Any octet triplet encoded with non-canonical encoding is rejected too. When there is one, respectively two padding
// sextets, the previous 2, respectively 4 bits will not be emitted and are therefore supposed to be zero in the encoded
// sextets. If that is not the case, the encoded sextets could still be decoded, but the encoding is considered as
// non-canonical and the input may hence be rejected by a conformant implementation. This is what this implementation
// does.
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
// // Will generate a view on std::array{std::byte{'M'}, std::byte{'a'}, std::byte{'n'}}
// "TWFu"sv | decode64
// \endcode
inline constexpr auto decode64 = [] {
	using namespace std::views;
	using std::ranges::fold_left;
	using ulong = unsigned long;

	return transform([](char encoded) -> ulong {
		       using lookup_t = std::array<unsigned char, std::numeric_limits<unsigned char>::max() + 1>;

		       // Base64 binary values are six bit-wide. We use the seventh and eighth bits for validity and
		       // padding flags.
		       static constexpr auto is_valid = 0x40;
		       static constexpr auto is_padding = 0x80;

		       static constexpr auto lookup = [] -> lookup_t {
			       lookup_t a{};
			       unsigned char i{};
			       for (const unsigned char c : detail::base64_chars) {
				       a.at(c) = is_valid | i++;
			       }
			       a.at(static_cast<unsigned char>('=')) = is_padding | is_valid;
			       return a;
		       }();

		       // Expand to an unsigned long and move the valid and padding bits to the fourth byte to ease
		       // folding. Note: the input range does not guarantee multi-pass and hence, folding will have to
		       // transfer all the necessary information in one pass, i.e. not only the value but also validity
		       // and padding information.
		       const ulong sextet = lookup.at(static_cast<unsigned char>(encoded));
		       return ((sextet & is_valid) << 18) | ((sextet & is_padding) << 21) | (sextet & 0x3F);
	       }) |
	       chunk(4) | transform([](auto &&quad) {
		       static constexpr ulong padding_mask{0xF0000000};
		       static constexpr ulong valid_mask = {padding_mask >> 4};

		       const auto word = fold_left(quad, ulong{}, [](ulong s1, ulong s2) -> ulong {
			       // If padding has been found, absence of padding on the incoming sextet is illegal.
			       if ((s1 & padding_mask) && !(s2 & padding_mask)) {
				       return 0; // force invalid state
			       }
			       return ((s1 + s2) & (padding_mask | valid_mask)) | (((s1 << 6) | s2) & 0xFFFFFF);
		       });
		       const auto num_padding = static_cast<int>((word & padding_mask) >> 28);
		       const auto num_valid = static_cast<int>((word & valid_mask) >> 24);
		       // If padding is used, the 2 or 4 least significant bits of the previous sextet are not used, so
		       // under canonical encoding, they shall be zero. Since our lookup zeroes the rest of the padding
		       // payload, we just check entire bytes, for clarity.
		       const auto is_non_canonical =
			   (num_padding == 1 && (word & 0xFF)) || (num_padding == 2 && (word & 0xFFFF));
		       return iota(0, num_valid < 4 || num_padding > 2 || is_non_canonical ? 0 : 3 - num_padding) |
			      transform(
				  [word](int i) -> std::byte { return static_cast<std::byte>(word >> ((2 - i) * 8)); });
	       }) |
	       join;
}();
