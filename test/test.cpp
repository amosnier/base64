#include "base64.h"

auto main() -> int
{
	using namespace std::literals;
	using namespace std::views;
	using std::ranges::equal;

	constexpr auto to_bytes = transform([](char c) -> std::byte { return std::bit_cast<std::byte>(c); });
	constexpr auto to_chars = transform([](std::byte byte) -> char { return std::bit_cast<char>(byte); });

	// Test some examples from Wikipedia

	static_assert(equal("Man"sv | to_bytes | encode64, "TWFu"sv));
	static_assert(equal("Ma"sv | to_bytes | encode64, "TWE="sv));
	static_assert(equal("M"sv | to_bytes | encode64, "TQ=="sv));

	static_assert(equal("light work."sv | to_bytes | encode64, "bGlnaHQgd29yay4="sv));
	static_assert(equal("light work"sv | to_bytes | encode64, "bGlnaHQgd29yaw=="sv));
	static_assert(equal("light wor"sv | to_bytes | encode64, "bGlnaHQgd29y"sv));
	static_assert(equal("light wo"sv | to_bytes | encode64, "bGlnaHQgd28="sv));
	static_assert(equal("light w"sv | to_bytes | encode64, "bGlnaHQgdw=="sv));

	static_assert(
	    equal("Many hands make light work."sv | to_bytes | encode64, "TWFueSBoYW5kcyBtYWtlIGxpZ2h0IHdvcmsu"sv));

	static_assert(equal("TWFu"sv | decode64 | to_chars, "Man"sv));
	static_assert(equal("TWE="sv | decode64 | to_chars, "Ma"sv));
	static_assert(equal("TQ=="sv | decode64 | to_chars, "M"sv));

	static_assert(equal("bGlnaHQgd29yay4="sv | decode64 | to_chars, "light work."sv));
	static_assert(equal("bGlnaHQgd29yaw=="sv | decode64 | to_chars, "light work"sv));
	static_assert(equal("bGlnaHQgd29y"sv | decode64 | to_chars, "light wor"sv));
	static_assert(equal("bGlnaHQgd28="sv | decode64 | to_chars, "light wo"sv));
	static_assert(equal("bGlnaHQgdw=="sv | decode64 | to_chars, "light w"sv));

	static_assert(
	    equal("TWFueSBoYW5kcyBtYWtlIGxpZ2h0IHdvcmsu"sv | decode64 | to_chars, "Many hands make light work."sv));

	static_assert(equal("Man"sv | to_bytes | encode64 | decode64 | to_chars, "Man"sv));
	static_assert(equal("Ma"sv | to_bytes | encode64 | decode64 | to_chars, "Ma"sv));
	static_assert(equal("M"sv | to_bytes | encode64 | decode64 | to_chars, "M"sv));

	static_assert(equal("light work."sv | to_bytes | encode64 | decode64 | to_chars, "light work."sv));
	static_assert(equal("light work"sv | to_bytes | encode64 | decode64 | to_chars, "light work"sv));
	static_assert(equal("light wor"sv | to_bytes | encode64 | decode64 | to_chars, "light wor"sv));
	static_assert(equal("light wo"sv | to_bytes | encode64 | decode64 | to_chars, "light wo"sv));
	static_assert(equal("light w"sv | to_bytes | encode64 | decode64 | to_chars, "light w"sv));

	static_assert(equal("Many hands make light work."sv | to_bytes | encode64 | decode64 | to_chars,
			    "Many hands make light work."sv));

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

	// Test that nothing bad happens during decoding if invalid padding is used

	static_assert(equal("="sv | decode64, std::array{0, 0} | to_bytes));
	static_assert(equal("=="sv | decode64, std::array{0} | to_bytes));
	static_assert(equal("==="sv | decode64, ""sv | to_bytes));
	static_assert(equal("===="sv | decode64, std::array{0, 0, 0} | to_bytes));
	static_assert(equal("b"sv | decode64, std::array{0, 0, 27} | to_bytes));
	static_assert(equal("b="sv | decode64, std::array{0, 6} | to_bytes));
	static_assert(equal("b=="sv | decode64, std::array{1} | to_bytes));
	static_assert(equal("Z=="sv | decode64, std::array{1} | to_bytes));

	// Test that an illegal character leads to the rejection of an octet triplet

	static_assert(equal("TW\000uTWFu"sv | decode64 | to_chars, "Man"sv));
	static_assert(equal("TW?uTWFu"sv | decode64 | to_chars, "Man"sv));
	static_assert(equal("bGln?\012Qgd2\015yay4="sv | decode64 | to_chars, "ligk."sv));

	// Encoding/decoding of the RFC 4648 test vectors

	static_assert(equal(""sv | to_bytes | encode64, ""sv));
	static_assert(equal("f"sv | to_bytes | encode64, "Zg=="sv));
	static_assert(equal("fo"sv | to_bytes | encode64, "Zm8="sv));
	static_assert(equal("foo"sv | to_bytes | encode64, "Zm9v"sv));
	static_assert(equal("foob"sv | to_bytes | encode64, "Zm9vYg=="sv));
	static_assert(equal("fooba"sv | to_bytes | encode64, "Zm9vYmE="sv));
	static_assert(equal("foobar"sv | to_bytes | encode64, "Zm9vYmFy"sv));

	static_assert(equal(""sv | decode64, ""sv | to_bytes));
	static_assert(equal("Zg=="sv | decode64, "f"sv | to_bytes));
	static_assert(equal("Zm8="sv | decode64, "fo"sv | to_bytes));
	static_assert(equal("Zm9v"sv | decode64, "foo"sv | to_bytes));
	static_assert(equal("Zm9vYg=="sv | decode64, "foob"sv | to_bytes));
	static_assert(equal("Zm9vYmE="sv | decode64, "fooba"sv | to_bytes));
	static_assert(equal("Zm9vYmFy"sv | decode64, "foobar"sv | to_bytes));

	return 0;
}
