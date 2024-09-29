#include "base64.h"

#include <cassert>
#include <sstream>

namespace {

constexpr auto to_bytes = std::views::transform([](unsigned num) -> std::byte { return static_cast<std::byte>(num); });

template <size_t size>
void test_decoding_non_forward_range(const std::string &input, const std::array<int, size> &output)
{
	std::istringstream ss{input};
	static_assert(not std::ranges::forward_range<decltype(std::views::istream<char>(ss))>);
	assert(std::ranges::equal(std::views::istream<char>(ss) | decode64, output | to_bytes));
}

void test_encoding_non_forward_range(const std::string &input, const std::string_view &output)
{
	std::istringstream ss{input};
	static_assert(not std::ranges::forward_range<decltype(std::views::istream<char>(ss))>);
	assert(std::ranges::equal(std::views::istream<char>(ss) | to_bytes | encode64, output));
}

} // namespace

auto main() -> int
{
	using namespace std::literals;
	using namespace std::views;
	using std::ranges::equal;

	// Test some examples from Wikipedia, but switch from implicit ASCII to binary, to avoid being dependent on text
	// encoding.

	// "Man" in ASCII
	static_assert(equal(std::array{0x4D, 0x61, 0x6E} | to_bytes | encode64, "TWFu"sv));
	static_assert(equal(std::array{0x4D, 0x61} | to_bytes | encode64, "TWE="sv));
	static_assert(equal(std::array{0x4D} | to_bytes | encode64, "TQ=="sv));
	static_assert(equal("TWFu"sv | decode64, std::array{0x4D, 0x61, 0x6E} | to_bytes));
	static_assert(equal("TWE="sv | decode64, std::array{0x4D, 0x61} | to_bytes));
	static_assert(equal("TQ=="sv | decode64, std::array{0x4D} | to_bytes));

	// "light work." in ASCII
	static_assert(equal(std::array{108, 105, 103, 104, 116, 32, 119, 111, 114, 107, 46} | to_bytes | encode64,
			    "bGlnaHQgd29yay4="sv));
	static_assert(equal(std::array{108, 105, 103, 104, 116, 32, 119, 111, 114, 107} | to_bytes | encode64,
			    "bGlnaHQgd29yaw=="sv));
	static_assert(
	    equal(std::array{108, 105, 103, 104, 116, 32, 119, 111, 114} | to_bytes | encode64, "bGlnaHQgd29y"sv));
	static_assert(equal(std::array{108, 105, 103, 104, 116, 32, 119, 111} | to_bytes | encode64, "bGlnaHQgd28="sv));
	static_assert(equal(std::array{108, 105, 103, 104, 116, 32, 119} | to_bytes | encode64, "bGlnaHQgdw=="sv));
	static_assert(equal("bGlnaHQgd29yay4="sv | decode64,
			    std::array{108, 105, 103, 104, 116, 32, 119, 111, 114, 107, 46} | to_bytes));
	static_assert(equal("bGlnaHQgd29yaw=="sv | decode64,
			    std::array{108, 105, 103, 104, 116, 32, 119, 111, 114, 107} | to_bytes));
	static_assert(
	    equal("bGlnaHQgd29y"sv | decode64, std::array{108, 105, 103, 104, 116, 32, 119, 111, 114} | to_bytes));
	static_assert(equal("bGlnaHQgd28="sv | decode64, std::array{108, 105, 103, 104, 116, 32, 119, 111} | to_bytes));
	static_assert(equal("bGlnaHQgdw=="sv | decode64, std::array{108, 105, 103, 104, 116, 32, 119} | to_bytes));

	// "Many hands make light work." in ASCII
	static_assert(equal(std::array{77,  97, 110, 121, 32,  104, 97,	 110, 100, 115, 32,  109, 97, 107,
				       101, 32, 108, 105, 103, 104, 116, 32,  119, 111, 114, 107, 46} |
				to_bytes | encode64,
			    "TWFueSBoYW5kcyBtYWtlIGxpZ2h0IHdvcmsu"sv));
	static_assert(equal("TWFueSBoYW5kcyBtYWtlIGxpZ2h0IHdvcmsu"sv | decode64,
			    std::array{77,  97, 110, 121, 32,  104, 97,	 110, 100, 115, 32,  109, 97, 107,
				       101, 32, 108, 105, 103, 104, 116, 32,  119, 111, 114, 107, 46} |
				to_bytes));

	// Check that by composing encoding and decoding, we obtain the identity function (ASCII does not really matter
	// here).

	static_assert(equal("Man"sv | to_bytes | encode64 | decode64, "Man"sv | to_bytes));
	static_assert(equal("Ma"sv | to_bytes | encode64 | decode64, "Ma"sv | to_bytes));
	static_assert(equal("M"sv | to_bytes | encode64 | decode64, "M"sv | to_bytes));

	static_assert(equal("light work."sv | to_bytes | encode64 | decode64, "light work."sv | to_bytes));
	static_assert(equal("light work"sv | to_bytes | encode64 | decode64, "light work"sv | to_bytes));
	static_assert(equal("light wor"sv | to_bytes | encode64 | decode64, "light wor"sv | to_bytes));
	static_assert(equal("light wo"sv | to_bytes | encode64 | decode64, "light wo"sv | to_bytes));
	static_assert(equal("light w"sv | to_bytes | encode64 | decode64, "light w"sv | to_bytes));

	static_assert(equal("Many hands make light work."sv | to_bytes | encode64 | decode64,
			    "Many hands make light work."sv | to_bytes));

	static_assert(equal("TWFu"sv | decode64 | encode64, "TWFu"sv));
	static_assert(equal("TWE="sv | decode64 | encode64, "TWE="sv));
	static_assert(equal("TQ=="sv | decode64 | encode64, "TQ=="sv));

	static_assert(equal("bGlnaHQgd29yay4="sv | decode64 | encode64, "bGlnaHQgd29yay4="sv));
	static_assert(equal("bGlnaHQgd29yaw=="sv | decode64 | encode64, "bGlnaHQgd29yaw=="sv));
	static_assert(equal("bGlnaHQgd29y"sv | decode64 | encode64, "bGlnaHQgd29y"sv));
	static_assert(equal("bGlnaHQgd28="sv | decode64 | encode64, "bGlnaHQgd28="sv));
	static_assert(equal("bGlnaHQgdw=="sv | decode64 | encode64, "bGlnaHQgdw=="sv));

	static_assert(equal("TWFueSBoYW5kcyBtYWtlIGxpZ2h0IHdvcmsu"sv | decode64 | encode64,
			    "TWFueSBoYW5kcyBtYWtlIGxpZ2h0IHdvcmsu"sv));

	// Test that illegal padding is rejected

	static_assert(equal("="sv | decode64, ""sv | to_bytes));
	static_assert(equal("=="sv | decode64, ""sv | to_bytes));
	static_assert(equal("==="sv | decode64, ""sv | to_bytes));
	static_assert(equal("===="sv | decode64, ""sv | to_bytes));
	static_assert(equal("b"sv | decode64, ""sv | to_bytes));
	static_assert(equal("b="sv | decode64, ""sv | to_bytes));
	static_assert(equal("b=="sv | decode64, ""sv | to_bytes));
	static_assert(equal("TQ=A"sv | decode64, ""sv | to_bytes));
	static_assert(equal("=a=bTWE=x=y="sv | decode64, std::array{0x4D, 0x61} | to_bytes));

	// Test that an illegal character leads to the rejection of an octet triplet

	static_assert(equal("TW\000uTWFu"sv | decode64, std::array{0x4D, 0x61, 0x6E} | to_bytes));
	static_assert(equal("TW?uTWFu"sv | decode64, std::array{0x4D, 0x61, 0x6E} | to_bytes));
	static_assert(equal("bGln?\012Qgd2\015yay4="sv | decode64, std::array{108, 105, 103, 107, 46} | to_bytes));

	// Test that non-canonical encoding is rejected

	static_assert(equal("TWF="sv | decode64, ""sv | to_bytes));
	static_assert(equal("TWG="sv | decode64, ""sv | to_bytes));
	static_assert(equal("Tf=="sv | decode64, ""sv | to_bytes));

	// Encoding/decoding of the RFC 4648 test vectors

	static_assert(equal(""sv | to_bytes | encode64, ""sv));
	static_assert(equal(std::array{0x66} | to_bytes | encode64, "Zg=="sv));
	static_assert(equal(std::array{0x66, 0x6F} | to_bytes | encode64, "Zm8="sv));
	static_assert(equal(std::array{0x66, 0x6F, 0x6F} | to_bytes | encode64, "Zm9v"sv));
	static_assert(equal(std::array{0x66, 0x6F, 0x6F, 0x62} | to_bytes | encode64, "Zm9vYg=="sv));
	static_assert(equal(std::array{0x66, 0x6F, 0x6F, 0x62, 0x61} | to_bytes | encode64, "Zm9vYmE="sv));
	static_assert(equal(std::array{0x66, 0x6F, 0x6F, 0x62, 0x61, 0x72} | to_bytes | encode64, "Zm9vYmFy"sv));

	static_assert(equal(""sv | decode64, ""sv | to_bytes));
	static_assert(equal("Zg=="sv | decode64, std::array{0x66} | to_bytes));
	static_assert(equal("Zm8="sv | decode64, std::array{0x66, 0x6F} | to_bytes));
	static_assert(equal("Zm9v"sv | decode64, std::array{0x66, 0x6F, 0x6F} | to_bytes));
	static_assert(equal("Zm9vYg=="sv | decode64, std::array{0x66, 0x6F, 0x6F, 0x62} | to_bytes));
	static_assert(equal("Zm9vYmE="sv | decode64, std::array{0x66, 0x6F, 0x6F, 0x62, 0x61} | to_bytes));
	static_assert(equal("Zm9vYmFy"sv | decode64, std::array{0x66, 0x6F, 0x6F, 0x62, 0x61, 0x72} | to_bytes));

	// Test that the implementation works on non-forward streams. This is hard to achieve at compile time, so we
	// fall back to run time. But we do assert at compile time that the picked range is, indeed, not a forward
	// range.

	// Regular encoding
	test_encoding_non_forward_range("\x{4D}\x{61}\x{6E}"s, "TWFu"sv);

	// Regular decoding
	test_decoding_non_forward_range("01C0FFEE"s, std::array{1, 192, 255, 238});

	return 0;
}
