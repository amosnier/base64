#pragma once

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <expected>
#include <functional>
#include <iterator>
#include <limits>
#include <ranges>
#include <string_view>
#include <vector>

enum class decode16_error : signed char {
	illegal_character,
	missing_character,
};

template <typename I, typename O>
struct decode16_error_result {
	std::ranges::in_out_result<I, O> in_out_result;
	decode16_error error;
};

namespace detail {

inline constexpr std::string_view base16_chars{"0123456789ABCDEF"};

namespace encode16 {

struct encode : std::ranges::range_adaptor_closure<encode> {
	template <std::ranges::viewable_range R>
		requires std::convertible_to<std::ranges::range_reference_t<R>, std::byte>
	constexpr auto operator()(R &&r) const
	{
		using namespace std::views;
		return cartesian_product(std::forward<R>(r), iota(0, 2) | reverse) |
		       transform([](std::tuple<std::byte, int> indexed_byte) -> char {
			       const auto [byte, i] = indexed_byte;
			       return detail::base16_chars.at(static_cast<unsigned>((byte >> (4 * i))) & 0xF);
		       });
	}
};

} // namespace encode16

namespace decode16 {

constexpr auto max_size(size_t input_size) -> std::expected<size_t, decode16_error>
{
	if ((input_size % 2) != 0) {
		return std::unexpected{decode16_error::missing_character};
	}

	return input_size / 2;
}

struct try_decode {
	template <std::input_iterator I, std::sentinel_for<I> S, std::weakly_incrementable O,
		  typename Proj = std::identity>
		requires(std::convertible_to<std::indirect_result_t<Proj, I>, char> and
			 std::indirectly_writable<O, std::byte>)
	constexpr auto operator()(I first1, S last1, O result, Proj proj = {}) const
	    -> std::expected<std::ranges::in_out_result<I, O>, decode16_error_result<I, O>>
	{
		using namespace std::views;

		using lookup_t = std::array<unsigned char, std::numeric_limits<unsigned char>::max() + 1>;

		static constexpr auto is_valid = 0x10;

		static constexpr auto lookup = [] -> lookup_t {
			lookup_t a{};
			unsigned char i{};
			for (const unsigned char c : detail::base16_chars) {
				a.at(c) = is_valid | i++;
			}
			return a;
		}();

		for (; first1 != last1; (void)++result) {
			static constexpr auto chunk_size = 2;
			unsigned char byte{};
			auto i = 0;
			for (i = 0; i < chunk_size and first1 != last1; ++i, ++first1) {
				const auto nibble = lookup.at(static_cast<unsigned char>(std::invoke(proj, *first1)));
				if (not(nibble & is_valid)) {
					return std::unexpected{decode16_error_result<I, O>{
					    .in_out_result = {std::move(first1), std::move(result)},
					    .error = decode16_error::illegal_character,
					}};
				}
				byte = (byte << 4) | (nibble & 0xF);
			}
			if (i != chunk_size) {
				return std::unexpected{decode16_error_result<I, O>{
				    .in_out_result = {std::move(first1), std::move(result)},
				    .error = decode16_error::missing_character,
				}};
			}
			*result = static_cast<std::byte>(byte);
		}

		return std::ranges::in_out_result<I, O>{std::move(first1), std::move(result)};
	}

	template <std::ranges::input_range R, std::weakly_incrementable O, typename Proj = std::identity>
		requires(std::convertible_to<std::indirect_result_t<Proj, std::ranges::iterator_t<R>>, char> and
			 std::indirectly_writable<O, std::byte>)
	// NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
	constexpr auto operator()(R &&r, O result, Proj proj = {}) const
	    -> std::expected<std::ranges::in_out_result<std::ranges::borrowed_iterator_t<R>, O>,
			     decode16_error_result<std::ranges::borrowed_iterator_t<R>, O>>
	{
		return (*this)(std::ranges::begin(r), std::ranges::end(r), std::move(result), std::move(proj));
	}
};

} // namespace decode16
} // namespace detail

inline constexpr detail::encode16::encode encode16{};
inline constexpr detail::decode16::try_decode try_decode16{};

template <typename R>
	requires(std::ranges::sized_range<R> or std::ranges::forward_range<R>)
// NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
constexpr auto decode16_max_size(R &&r) -> std::expected<size_t, decode16_error>
{
	if constexpr (std::ranges::sized_range<R>) {
		return detail::decode16::max_size(std::ranges::size(r));
	} else {
		return detail::decode16::max_size(std::ranges::distance(r));
	}
}

namespace detail::decode16 {

template <template <typename> typename C>
struct try_decode_to {
	template <std::ranges::input_range R, typename Proj = std::identity>
		requires std::convertible_to<std::indirect_result_t<Proj, std::ranges::iterator_t<R>>, char>
	constexpr auto operator()(R &&r, Proj proj = {}) const -> std::expected<C<std::byte>, decode16_error>;
};

template <>
struct try_decode_to<std::vector> {
	template <std::ranges::input_range R, typename Proj = std::identity>
		requires std::convertible_to<std::indirect_result_t<Proj, std::ranges::iterator_t<R>>, char>
	constexpr auto operator()(R &&r, Proj proj = {}) const -> std::expected<std::vector<std::byte>, decode16_error>
	{
		std::vector<std::byte> v{};

		return try_decode16(std::forward<R>(r), std::back_insert_iterator{v}, std::move(proj))
		    .transform([v = std::move(v)](auto &&) mutable { return std::move(v); })
		    .transform_error([](const auto &error) { return error.error; });
	}

	template <std::ranges::input_range R, typename Proj = std::identity>
		requires(std::convertible_to<std::indirect_result_t<Proj, std::ranges::iterator_t<R>>, char> and
			 (std::ranges::sized_range<R> or std::ranges::forward_range<R>))
	constexpr auto operator()(R &&r, Proj proj = {}) const -> std::expected<std::vector<std::byte>, decode16_error>
	{
		const auto size = decode16_max_size(std::forward<R>(r));

		if (not size.has_value()) {
			return std::unexpected{size.error()};
		}

		std::vector<std::byte> v(*size);

		return try_decode16(std::forward<R>(r), v.begin(), std::move(proj))
		    .transform([v = std::move(v)](auto &&) mutable { return std::move(v); })
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

} // namespace detail::decode16

template <template <typename> typename C>
inline constexpr detail::decode16::try_decode_to<C> try_decode16_to{};

template <template <typename> typename C>
inline constexpr detail::decode16::decode_to<C> decode16_to{};

inline constexpr auto try_decode16_to_vector = try_decode16_to<std::vector>;
inline constexpr auto decode16_to_vector = decode16_to<std::vector>;
