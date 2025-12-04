#pragma once

#include <cstdint>
#include <type_traits>

namespace libpisp
{

// Concept to constrain types to enums only
template <typename T>
concept EnumType = std::is_enum_v<T>;

template <EnumType T>
class Bitfield
{
private:
	using Type = std::underlying_type_t<T>;
	Type value;

	// Only accessible internally or via friend operators
	explicit constexpr Bitfield(Type v) noexcept : value(v) {}

public:
	// Constructors
	constexpr Bitfield() noexcept : value(0) {}
	constexpr Bitfield(T e) noexcept : value(static_cast<Type>(e)) {}
	constexpr Bitfield(Bitfield const &b) noexcept : value(b.value) {}
	constexpr Bitfield(Bitfield &&b) noexcept : value(b.value) {}

	// Conversions
	constexpr operator T() const noexcept { return static_cast<T>(value); }
	constexpr operator Type() const noexcept { return value; }

	// Assignment
	Bitfield &operator=(Bitfield const &) noexcept = default;
	Bitfield &operator=(Bitfield &&) noexcept = default;
	Bitfield &operator=(T e) noexcept
	{
		value = static_cast<Type>(e);
		return *this;
	}

	// Compound bitwise operators
	Bitfield &operator|=(Bitfield other) noexcept
	{
		value |= other.value;
		return *this;
	}
	Bitfield &operator&=(Bitfield other) noexcept
	{
		value &= other.value;
		return *this;
	}
	Bitfield &operator^=(Bitfield other) noexcept
	{
		value ^= other.value;
		return *this;
	}

	// Bitwise operators
	friend constexpr Bitfield operator|(Bitfield a, Bitfield b) noexcept { return Bitfield(a.value | b.value); }
	friend constexpr Bitfield operator&(Bitfield a, Bitfield b) noexcept { return Bitfield(a.value & b.value); }
	friend constexpr Bitfield operator^(Bitfield a, Bitfield b) noexcept { return Bitfield(a.value ^ b.value); }
	constexpr Bitfield operator~() const noexcept { return Bitfield(~value); }

	// Prevent mixing with integers or other enum types
	template <typename U>
	requires (std::is_integral_v<U> || (EnumType<U> && !std::is_same_v<U, T>))
	friend constexpr Bitfield operator|(Bitfield, U) = delete;

	template <typename U>
	requires (std::is_integral_v<U> || (EnumType<U> && !std::is_same_v<U, T>))
	friend constexpr Bitfield operator&(Bitfield, U) = delete;

	template <typename U>
	requires (std::is_integral_v<U> || (EnumType<U> && !std::is_same_v<U, T>))
	friend constexpr Bitfield operator^(Bitfield, U) = delete;

	template <typename U>
	requires (std::is_integral_v<U> || (EnumType<U> && !std::is_same_v<U, T>))
	friend constexpr Bitfield operator|(U, Bitfield) = delete;

	template <typename U>
	requires (std::is_integral_v<U> || (EnumType<U> && !std::is_same_v<U, T>))
	friend constexpr Bitfield operator&(U, Bitfield) = delete;

	template <typename U>
	requires (std::is_integral_v<U> || (EnumType<U> && !std::is_same_v<U, T>))
	friend constexpr Bitfield operator^(U, Bitfield) = delete;
	
	// Comparison operators
	friend constexpr bool operator==(Bitfield a, Bitfield b) noexcept { return a.value == b.value; }
	friend constexpr bool operator!=(Bitfield a, Bitfield b) noexcept { return a.value != b.value; }
	friend constexpr bool operator==(Bitfield a, T b) noexcept { return a.value == static_cast<Type>(b); }
	friend constexpr bool operator!=(Bitfield a, T b) noexcept { return a.value != static_cast<Type>(b); }
	friend constexpr bool operator==(T a, Bitfield b) noexcept { return static_cast<Type>(a) == b.value; }
	friend constexpr bool operator!=(T a, Bitfield b) noexcept { return static_cast<Type>(a) != b.value; }

	// Query methods
	constexpr bool any() const noexcept { return value != 0; }
	constexpr bool none() const noexcept { return value == 0; }
	explicit constexpr operator bool() const noexcept { return any(); }

	constexpr bool contains(T e) const noexcept { return (value & static_cast<Type>(e)) != 0; }
};

// Bitwise operators for enum types
template <EnumType T>
inline constexpr T operator|(T a, T b) noexcept
{
	using U = std::underlying_type_t<T>;
	return static_cast<T>(static_cast<U>(a) | static_cast<U>(b));
}

template <EnumType T>
inline constexpr T operator&(T a, T b) noexcept
{
	using U = std::underlying_type_t<T>;
	return static_cast<T>(static_cast<U>(a) & static_cast<U>(b));
}

template <EnumType T>
inline constexpr T operator^(T a, T b) noexcept
{
	using U = std::underlying_type_t<T>;
	return static_cast<T>(static_cast<U>(a) ^ static_cast<U>(b));
}

// Compound assignment operators for enum types
template <EnumType T>
inline T &operator|=(T &a, T b) noexcept
{
	a = a | b;
	return a;
}

template <EnumType T>
inline T &operator&=(T &a, T b) noexcept
{
	a = a & b;
	return a;
}

template <EnumType T>
inline T &operator^=(T &a, T b) noexcept
{
	a = a ^ b;
	return a;
}

// Bitwise NOT operator for enum types
template <EnumType T>
inline constexpr T operator~(T a) noexcept
{
	using U = std::underlying_type_t<T>;
	return static_cast<T>(~static_cast<U>(a));
}

} // namespace libpisp
