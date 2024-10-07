#include "base64.h"

#include <cassert>
#include <iostream>
#include <sstream>

namespace {

constexpr auto to_bytes = std::views::transform([](auto num) -> std::byte { return static_cast<std::byte>(num); });

constexpr auto encoding_is_successful(std::string_view binary, std::string_view encoded) -> bool
{
	/* Check than encoding preserves range features. */
	static_assert(std::ranges::random_access_range<decltype(binary | to_bytes | encode64)>);
	static_assert(std::ranges::sized_range<decltype(binary | to_bytes | encode64)>);

	return std::ranges::equal(binary | to_bytes | encode64, encoded);
}

template <std::ranges::view V, typename Proj = std::identity>
	requires std::convertible_to<std::indirect_result_t<Proj, std::ranges::iterator_t<V>>, char>
constexpr auto decoding_works_as_expected(V encoded, std::string_view binary, Proj proj = {}) -> bool
{
	return try_decode64_to_vector(encoded, std::move(proj))
	    .transform([&binary](const auto &v) -> bool { return std::ranges::equal(v, binary | to_bytes); })
	    .value_or(false);
}

template <std::ranges::view V, typename Proj = std::identity>
	requires std::convertible_to<std::indirect_result_t<Proj, std::ranges::iterator_t<V>>, char>
constexpr auto decoding_works_as_expected(V encoded, decode64_error expected_error, Proj proj = {}) -> bool
{
	return try_decode64_to_vector(encoded, std::move(proj))
	    .transform_error([expected_error](decode64_error error) -> bool { return error == expected_error; })
	    .error_or(false);
}

template <typename Proj = std::identity>
constexpr auto test_is_successful(std::string_view binary, std::string_view encoded, Proj proj = {}) -> bool
{
	return encoding_is_successful(binary, encoded) &&
	       decoding_works_as_expected(encoded, binary, std::move(proj)) and
	       decoding_works_as_expected(binary | to_bytes | encode64, binary, std::move(proj)) and
	       std::ranges::equal(try_decode64_to_vector(encoded).value_or(std::vector<std::byte>{}) | encode64,
				  encoded);
}

void test_encoding_non_forward_range(std::string_view binary, std::string_view encoded)
{
	std::istringstream ss{std::string{binary}};
	using view = decltype(std::views::istream<char>(ss));
	static_assert(not std::ranges::forward_range<view>);
	for (const auto c : view(ss) | to_bytes | encode64) {
	}
	/* assert(std::ranges::equal(view(ss) | to_bytes | encode64, encoded)); */
}

void test_decoding_non_forward_range(std::string_view encoded, auto expected_result)
{
	std::istringstream ss{std::string{encoded}};
	using view = decltype(std::views::istream<char>(ss));
	static_assert(not std::ranges::forward_range<view>);
	assert(decoding_works_as_expected(view(ss), expected_result));
}

void test_non_forward_range(std::string_view binary, std::string_view encoded)
{
	test_encoding_non_forward_range(binary, encoded);
	test_decoding_non_forward_range(encoded, binary);
}

} // namespace

auto main() -> int
{
	using namespace std::literals;
	using namespace std::views;
	using std::ranges::equal;

	/*
	static_assert(equal("light work."sv | to_bytes | encode64, "bGlnaHQgd29yay4="sv));
	static_assert(equal("light work"sv | to_bytes | encode64, "bGlnaHQgd29yaw=="sv));
	static_assert(equal("light wor"sv | to_bytes | encode64, "bGlnaHQgd29y"sv));
	static_assert(equal("light wo"sv | to_bytes | encode64, "bGlnaHQgd28="sv));
	static_assert(equal("light w"sv | to_bytes | encode64, "bGlnaHQgdw=="sv));

	static_assert(
	    equal("Many hands make light work."sv | to_bytes | encode64, "TWFueSBoYW5kcyBtYWtlIGxpZ2h0IHdvcmsu"sv));

	// Test some arbitrary encoding/decoding

	static_assert(test_is_successful("\x{4D}\x{61}\x{6E}"sv, "TWFu"sv));
	static_assert(test_is_successful("\x{4D}\x{61}"sv, "TWE="sv));
	static_assert(test_is_successful("\x{4D}"sv, "TQ=="sv));
	*/

	test_non_forward_range("\x{4D}\x{61}\x{6E}"sv, "TWFu"sv);

	/*
	test_non_forward_range("\x{4D}\x{61}"sv, "TWE="sv);
	test_non_forward_range("\x{4D}"sv, "TQ=="sv);

	test_non_forward_range("\x{01}\x{C0}\x{FF}\x{EE}"sv, "01C0FFEE"sv);

	// Test that all the hexadecimal alphabet works

	static_assert(test_is_successful("\x{01}\x{23}\x{45}\x{67}\x{89}\x{AB}\x{CD}\x{EF}"sv, "0123456789ABCDEF"sv));

	test_non_forward_range("\x{01}\x{23}\x{45}\x{67}\x{89}\x{AB}\x{CD}\x{EF}"sv, "0123456789ABCDEF"sv);

	// Test decoding rejection in case of illegal characters

	static_assert(decoding_works_as_expected("01C0FZFF\x{00}AEE"sv, decode64_error::illegal_character));
	static_assert(decoding_works_as_expected("01C0FZFF\x{FF}AEE"sv, decode64_error::illegal_character));
	static_assert(decoding_works_as_expected("01C0FF\x{00}AEE"sv, decode64_error::illegal_character));
	static_assert(decoding_works_as_expected("01C0FF\x{FF}AEE"sv, decode64_error::illegal_character));

	test_decoding_non_forward_range("01C0FZFF\x{00}AEE"sv, decode64_error::illegal_character);
	test_decoding_non_forward_range("01C0FZFF\x{FF}AEE"sv, decode64_error::illegal_character);
	test_decoding_non_forward_range("01C0FF\x{00}AEE"sv, decode64_error::illegal_character);
	test_decoding_non_forward_range("01C0FF\x{FF}AEE"sv, decode64_error::illegal_character);

	// Test decoding an odd number of nibbles

	static_assert(decoding_works_as_expected("01C0FFEE5"sv, decode64_error::missing_character));

	test_decoding_non_forward_range("01C0FFEE5"sv, decode64_error::missing_character);

	// Test projection

	static_assert(decoding_works_as_expected("01C0FFEE"sv | transform([](char c) { return std::pair{c, char{}}; }),
						 "\x{01}\x{C0}\x{FF}\x{EE}"sv, &std::pair<char, char>::first));
	static_assert(decoding_works_as_expected("01C0FFEE5"sv | transform([](char c) { return std::pair{c, char{}}; }),
						 decode64_error::missing_character, &std::pair<char, char>::first));

	// Test RFC 4648 test vectors, but in binary, in order to avoid making any assumptions on encoding. Hexadecimal
	// 666F6F626172, in ASCII, is "foobar".

	static_assert(test_is_successful(""sv, ""sv));
	static_assert(test_is_successful("\x{66}"sv, "66"sv));
	static_assert(test_is_successful("\x{66}\x{6F}"sv, "666F"sv));
	static_assert(test_is_successful("\x{66}\x{6F}\x{6F}"sv, "666F6F"sv));
	static_assert(test_is_successful("\x{66}\x{6F}\x{6F}\x{62}"sv, "666F6F62"sv));
	static_assert(test_is_successful("\x{66}\x{6F}\x{6F}\x{62}\x{61}"sv, "666F6F6261"sv));
	static_assert(test_is_successful("\x{66}\x{6F}\x{6F}\x{62}\x{61}\x{72}"sv, "666F6F626172"sv));

	test_non_forward_range(""sv, ""sv);
	test_non_forward_range("\x{66}"sv, "66"sv);
	test_non_forward_range("\x{66}\x{6F}"sv, "666F"sv);
	test_non_forward_range("\x{66}\x{6F}\x{6F}"sv, "666F6F"sv);
	test_non_forward_range("\x{66}\x{6F}\x{6F}\x{62}"sv, "666F6F62"sv);
	test_non_forward_range("\x{66}\x{6F}\x{6F}\x{62}\x{61}"sv, "666F6F6261"sv);
	test_non_forward_range("\x{66}\x{6F}\x{6F}\x{62}\x{61}\x{72}"sv, "666F6F626172"sv);
	*/

	return 0;
}
