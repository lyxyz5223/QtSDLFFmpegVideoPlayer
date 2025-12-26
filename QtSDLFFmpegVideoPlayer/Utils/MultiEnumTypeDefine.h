#pragma once
// 枚举量的多值组合类型定义
template <typename T>
class MultiEnumTypeDefine {
private:
    T value;
public:
    constexpr MultiEnumTypeDefine() = default;
    constexpr MultiEnumTypeDefine(T initValue) : value(initValue) {}
    ~MultiEnumTypeDefine() = default;

    constexpr operator T() const {
        return value;
    }
    constexpr MultiEnumTypeDefine& operator=(const MultiEnumTypeDefine& other) {
        value = other.value;
        return *this;
    }
    constexpr MultiEnumTypeDefine& operator=(T v) {
        value = v;
        return *this;
    }
    constexpr MultiEnumTypeDefine& operator|=(T v) {
        value = static_cast<T>(value | v);
        return *this;
    }
    constexpr MultiEnumTypeDefine& operator&=(T v) {
        value = static_cast<T>(value & v);
        return *this;
    }
    constexpr MultiEnumTypeDefine operator|(T v) const {
        return MultiEnumTypeDefine(static_cast<T>(value | v));
    }
    constexpr MultiEnumTypeDefine operator&(T v) const {
        return MultiEnumTypeDefine(static_cast<T>(value & v));
    }

    constexpr bool testFlag(T v) const {
        return (value & v) == v;
    }
};