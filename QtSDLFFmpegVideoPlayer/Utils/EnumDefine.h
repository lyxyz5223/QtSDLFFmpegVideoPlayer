#pragma once
#include <string>
#include <type_traits>

template <typename T, typename UnderLyingType=std::underlying_type_t<T>>
class EnumType {
private:
    T v{ static_cast<T>(0) };
public:
    EnumType(T t) : v(t) {}
    EnumType(const EnumType& t) : v(t.v) {}
    EnumType(const EnumType&& t) noexcept : v(t.v) {}
    ~EnumType() = default;
    T value() const { return v; }
    std::string name() const;
    static std::string getName(T value);
    bool isEqual(const EnumType& other) const {
        return v == other.v;
    }
    operator T() {
        return v;
    }
    operator UnderLyingType() {
        return static_cast<int>(v);
    }
    bool operator==(const EnumType& other) const {
        return isEqual(other);
    }
    bool operator!=(const EnumType& other) const {
        return !isEqual(other);
    }
    EnumType& operator=(const EnumType& other) {
        if (this != &other)
            v = other.v;
        return *this;
    }
};
