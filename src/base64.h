#pragma once

#include <algorithm>
#include <array>
#include <bits/ranges_algo.h>
#include <concepts>
#include <cstddef>
#include <expected>
#include <functional>
#include <iostream>
#include <iterator>
#include <limits>
#include <ranges>
#include <string_view>
#include <vector>

enum class decode64_error : signed char {
	illegal_character,
	missing_character,
	illegal_padding,
	non_canonical,
};

template <typename I, typename O>
struct decode64_error_result {
	std::ranges::in_out_result<I, O> in_out_result;
	decode64_error error;
};

namespace detail {

inline constexpr std::string_view base64_chars{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"};

namespace encode64 {

struct encode : std::ranges::range_adaptor_closure<encode> {
	template <std::ranges::viewable_range R>
		requires std::convertible_to<std::ranges::range_reference_t<R>, std::byte>
	constexpr auto operator()(R &&r) const
	{
		using namespace std::views;
		using std::ranges::fold_left;

		using ulong = unsigned long;

		static constexpr auto shift = 24;

		return cartesian_product(
			   std::forward<R>(r) |
			       transform([](std::byte byte) -> ulong { return static_cast<ulong>(byte); }) | chunk(3) |
			       transform([](auto triplet) -> ulong {
				       static constexpr ulong one_missing_byte{1 << shift};

				       // Note: we store the number of missing bytes (zero, one, or two)
				       // to the otherwise unused fourth byte, to handle padding.
				       return fold_left(triplet, 3 * one_missing_byte, [](ulong o1, ulong o2) -> ulong {
					       static constexpr ulong mask{one_missing_byte - 1};
					       return ((o1 - one_missing_byte) & ~mask) | (((o1 << 8) | o2) & mask);
				       });
			       }),
			   iota(0, 4) | reverse) |
		       transform([](auto indexed_word) -> char {
			       const auto [word, i] = indexed_word;
			       const auto num_missing = static_cast<int>(word >> shift);

			       std::cout << i << ", " << num_missing << '\n';

			       return i < num_missing
					  ? '='
					  : detail::base64_chars.at((word << (8 * num_missing) >> (6 * i)) & 0x3F);
		       });
	}
};

} // namespace encode64

namespace decode64 {

constexpr auto max_size(size_t input_size) -> std::expected<size_t, decode64_error>
{
	if ((input_size % 4) != 0) {
		return std::unexpected{decode64_error::missing_character};
	}

	return 3 * input_size / 4;
}

struct try_decode {
	template <std::input_iterator I, std::sentinel_for<I> S, std::weakly_incrementable O,
		  typename Proj = std::identity>
		requires(std::convertible_to<std::indirect_result_t<Proj, I>, char> and
			 std::indirectly_writable<O, std::byte>)
	constexpr auto operator()(I first1, S last1, O result, Proj proj = {}) const
	    -> std::expected<std::ranges::in_out_result<I, O>, decode64_error_result<I, O>>
	{
		using namespace std::views;

		using lookup_t = std::array<unsigned char, std::numeric_limits<unsigned char>::max() + 1>;

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

		while (first1 != last1) {
			static constexpr auto chunk_size = 4;
			using ulong = unsigned long;
			ulong word{};
			auto num_padding = 0;
			auto i = 0;
			for (i = 0; i < chunk_size and first1 != last1; ++i, ++first1) {
				const auto sextet = lookup.at(static_cast<unsigned char>(std::invoke(proj, *first1)));
				if (not(sextet & is_valid)) {
					return std::unexpected{decode64_error_result<I, O>{
					    .in_out_result = {std::move(first1), std::move(result)},
					    .error = decode64_error::illegal_character,
					}};
				}
				const auto sextet_is_padding = static_cast<int>(sextet >> 7);
				if ((i < 2 and sextet_is_padding) or (num_padding and not sextet_is_padding)) {
					return std::unexpected{decode64_error_result<I, O>{
					    .in_out_result = {std::move(first1), std::move(result)},
					    .error = decode64_error::illegal_padding,
					}};
				}
				num_padding += sextet_is_padding;
				word = (word << 6) | (sextet & 0x3F);
			}
			if (i != chunk_size) {
				return std::unexpected{decode64_error_result<I, O>{
				    .in_out_result = {std::move(first1), std::move(result)},
				    .error = decode64_error::missing_character,
				}};
			}
			if ((num_padding == 1 and (word & 0xC0)) or (num_padding == 2 and (word & 0xF000))) {
				return std::unexpected{decode64_error_result<I, O>{
				    .in_out_result = {std::move(first1), std::move(result)},
				    .error = decode64_error::non_canonical,
				}};
			}
			for (i = 0; i < 3 - num_padding; ++i) {
				*result++ = static_cast<std::byte>(word >> ((2 - i) * 8));
			}
		}

		return std::ranges::in_out_result<I, O>{std::move(first1), std::move(result)};
	}

	template <std::ranges::input_range R, std::weakly_incrementable O, typename Proj = std::identity>
		requires(std::convertible_to<std::indirect_result_t<Proj, std::ranges::iterator_t<R>>, char> and
			 std::indirectly_writable<O, std::byte>)
	// NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
	constexpr auto operator()(R &&r, O result, Proj proj = {}) const
	    -> std::expected<std::ranges::in_out_result<std::ranges::borrowed_iterator_t<R>, O>,
			     decode64_error_result<std::ranges::borrowed_iterator_t<R>, O>>
	{
		return (*this)(std::ranges::begin(r), std::ranges::end(r), std::move(result), std::move(proj));
	}
};

} // namespace decode64
} // namespace detail

inline constexpr detail::encode64::encode encode64{};
inline constexpr detail::decode64::try_decode try_decode64{};

template <typename R>
	requires(std::ranges::sized_range<R> or std::ranges::forward_range<R>)
// NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
constexpr auto decode64_max_size(R &&r) -> std::expected<size_t, decode64_error>
{
	if constexpr (std::ranges::sized_range<R>) {
		return detail::decode64::max_size(std::ranges::size(r));
	} else {
		return detail::decode64::max_size(std::ranges::distance(r));
	}
}

namespace detail::decode64 {

template <template <typename> typename C>
struct try_decode_to {
	template <std::ranges::input_range R, typename Proj = std::identity>
		requires std::convertible_to<std::indirect_result_t<Proj, std::ranges::iterator_t<R>>, char>
	constexpr auto operator()(R &&r, Proj proj = {}) const -> std::expected<C<std::byte>, decode64_error>;
};

template <>
struct try_decode_to<std::vector> {
	template <std::ranges::input_range R, typename Proj = std::identity>
		requires std::convertible_to<std::indirect_result_t<Proj, std::ranges::iterator_t<R>>, char>
	constexpr auto operator()(R &&r, Proj proj = {}) const -> std::expected<std::vector<std::byte>, decode64_error>
	{
		std::vector<std::byte> v{};

		return try_decode64(std::forward<R>(r), std::back_insert_iterator{v}, std::move(proj))
		    .transform([v = std::move(v)](auto &&) mutable { return std::move(v); })
		    .transform_error([](const auto &error) { return error.error; });
	}

	template <std::ranges::input_range R, typename Proj = std::identity>
		requires(std::convertible_to<std::indirect_result_t<Proj, std::ranges::iterator_t<R>>, char> and
			 (std::ranges::sized_range<R> or std::ranges::forward_range<R>))
	constexpr auto operator()(R &&r, Proj proj = {}) const -> std::expected<std::vector<std::byte>, decode64_error>
	{
		const auto size = decode64_max_size(std::forward<R>(r));

		if (not size.has_value()) {
			return std::unexpected{size.error()};
		}

		std::vector<std::byte> v(*size);

		const auto result = try_decode64(std::forward<R>(r), v.begin(), std::move(proj));

		if (result.has_value()) {
			v.resize(result->out - v.begin());
		}

		return result.transform([v = std::move(v)](auto &&) mutable { return std::move(v); })
		    .transform_error([](const auto &error) { return error.error; });
	}
};

template <template <typename> typename C>
	requires std::default_initializable<C<std::byte>>
struct decode_to {
	template <std::ranges::input_range R, typename Proj = std::identity>
		requires std::convertible_to<std::indirect_result_t<Proj, std::ranges::iterator_t<R>>, char>
	constexpr auto operator()(R &&r, Proj proj = {}) const -> C<std::byte>
	{
		return try_decode_to<C>{}(std::forward<R>(r), std::move(proj)).value_or(C<std::byte>{});
	}
};

} // namespace detail::decode64

template <template <typename> typename C>
inline constexpr detail::decode64::try_decode_to<C> try_decode64_to{};

template <template <typename> typename C>
inline constexpr detail::decode64::decode_to<C> decode64_to{};

inline constexpr auto try_decode64_to_vector = try_decode64_to<std::vector>;
inline constexpr auto decode64_to_vector = decode64_to<std::vector>;
