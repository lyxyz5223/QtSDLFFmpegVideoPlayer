#pragma once
#include <string>
#include <type_traits>

template <typename T, typename UnderLyingType=std::underlying_type_t<T>>
class EnumType {
private:
    T value{ static_cast<T>(0) };
public:
    EnumType(T t) : value(t) {}
    EnumType(const EnumType& t) : value(t.value) {}
    EnumType(const EnumType&& t) noexcept : value(t.value) {}
    ~EnumType() = default;
    T type() const { return value; }
    std::string name() const;
    bool isEqual(const EnumType& other) const {
        return value == other.value;
    }
    operator T() {
        return value;
    }
    operator UnderLyingType() {
        return static_cast<int>(value);
    }
    bool operator==(const EnumType& other) const {
        return isEqual(other);
    }
    bool operator!=(const EnumType& other) const {
        return !isEqual(other);
    }
    EnumType& operator=(const EnumType& other) {
        if (this != &other)
            value = other.value;
        return *this;
    }
};
