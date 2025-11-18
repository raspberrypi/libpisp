#pragma once

#include <cstdint>
#include <type_traits>

namespace libpisp
{

template <typename T>
class Bitfield
{
	static_assert(std::is_enum<T>::value, "Bitfield<T>: T must be an enum type");
	using Type = typename std::underlying_type<T>::type;

public:
	Type value;

	constexpr Bitfield() : value(0) {}
	constexpr Bitfield(T e) : value(static_cast<Type>(e)) {}
	explicit constexpr Bitfield(Type v) : value(v) {}
	constexpr Bitfield(Bitfield const &b) : value(b.value) {}
	constexpr Bitfield(Bitfield &&b) noexcept : value(b.value) {}

	constexpr operator Type() const { return value; }
	constexpr operator T() const { return static_cast<T>(value); }

	// Assignment
	Bitfield &operator=(Bitfield const &) = default;
	Bitfield &operator=(Bitfield &&) = default;

	Bitfield &operator=(T e)
	{
		value = static_cast<Type>(e);
		return *this;
	}

	// Prevent assignment from raw integers
	Bitfield &operator=(Type v) = delete;

	Bitfield &operator|=(T e)
	{
		value |= static_cast<Type>(e);
		return *this;
	}
	Bitfield &operator&=(T e)
	{
		value &= static_cast<Type>(e);
		return *this;
	}
	Bitfield &operator^=(T e)
	{
		value ^= static_cast<Type>(e);
		return *this;
	}

	Bitfield &operator|=(Bitfield other)
	{
		value |= other.value;
		return *this;
	}
	Bitfield &operator&=(Bitfield other)
	{
		value &= other.value;
		return *this;
	}
	Bitfield &operator^=(Bitfield other)
	{
		value ^= other.value;
		return *this;
	}

	friend constexpr Bitfield operator|(Bitfield a, Bitfield b) { return Bitfield(a.value | b.value); }
	friend constexpr Bitfield operator|(Bitfield a, T b) { return Bitfield(a.value | static_cast<Type>(b)); }
	friend constexpr Bitfield operator|(T a, Bitfield b) { return Bitfield(static_cast<Type>(a) | b.value); }

	friend constexpr Bitfield operator&(Bitfield a, Bitfield b) { return Bitfield(a.value & b.value); }
	friend constexpr Bitfield operator&(Bitfield a, T b) { return Bitfield(a.value & static_cast<Type>(b)); }
	friend constexpr Bitfield operator&(T a, Bitfield b) { return Bitfield(static_cast<Type>(a) & b.value); }

	friend constexpr Bitfield operator^(Bitfield a, Bitfield b) { return Bitfield(a.value ^ b.value); }
	friend constexpr Bitfield operator^(Bitfield a, T b) { return Bitfield(a.value ^ static_cast<Type>(b)); }
	friend constexpr Bitfield operator^(T a, Bitfield b) { return Bitfield(static_cast<Type>(a) ^ b.value); }

	constexpr Bitfield operator~() const { return Bitfield(~value); }

	constexpr bool any() const { return value != 0; }
	constexpr bool none() const { return value == 0; }
	explicit constexpr operator bool() const { return any(); }

	constexpr bool contains(T e) const { return (value & static_cast<Type>(e)) != 0; }
};

template <typename T>
inline constexpr typename std::enable_if<std::is_enum<T>::value, T>::type operator|(T a, T b)
{
	using U = typename std::underlying_type<T>::type;
	return static_cast<T>(static_cast<U>(a) | static_cast<U>(b));
}

template <typename T>
inline constexpr typename std::enable_if<std::is_enum<T>::value, T>::type operator&(T a, T b)
{
	using U = typename std::underlying_type<T>::type;
	return static_cast<T>(static_cast<U>(a) & static_cast<U>(b));
}

template <typename T>
inline constexpr typename std::enable_if<std::is_enum<T>::value, T>::type operator^(T a, T b)
{
	using U = typename std::underlying_type<T>::type;
	return static_cast<T>(static_cast<U>(a) ^ static_cast<U>(b));
}

template <typename T>
inline typename std::enable_if<std::is_enum<T>::value, T &>::type operator|=(T &a, T b)
{
	a = a | b;
	return a;
}

template <typename T>
inline typename std::enable_if<std::is_enum<T>::value, T &>::type operator&=(T &a, T b)
{
	a = a & b;
	return a;
}

template <typename T>
inline typename std::enable_if<std::is_enum<T>::value, T &>::type operator^=(T &a, T b)
{
	a = a ^ b;
	return a;
}

template <typename T>
inline constexpr typename std::enable_if<std::is_enum<T>::value, T>::type operator~(T a)
{
	using U = typename std::underlying_type<T>::type;
	return static_cast<T>(~static_cast<U>(a));
}

#define CREATE_BITFIELD(ClassName, EnumType) \
class ClassName : public Bitfield<EnumType> \
{ \
public: \
	using Bitfield::Bitfield; /* inherit constructors */ \
	/* Allow construction from base class (needed for operator returns) */ \
	ClassName(Bitfield<EnumType> const &b) : Bitfield<EnumType>(b) {} \
};

} // namespace libpisp
