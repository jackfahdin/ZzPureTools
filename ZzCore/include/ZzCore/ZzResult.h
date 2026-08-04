#pragma once

#include <cstddef>
#include <exception>
#include <optional>
#include <utility>
#include <variant>

#include <QtCore/QtGlobal>

#include <ZzCore/ZzError.h>

namespace ZzCore {

/**
 * @brief 保存成功值或预期错误的互斥结果。
 * @tparam ZzValue 成功值类型，可以是仅移动类型。
 *
 * 错误状态访问 value() 或成功状态访问 error() 属于程序员错误：Debug 构建触发
 * 断言，Release 构建终止进程，不会抛出 variant 访问异常。
 */
template<typename ZzValue>
class [[nodiscard]] ZzResult final
{
public:
    /**
     * @brief 构造成功结果。
     * @param value 成功值。
     * @return 持有成功值的结果。
     */
    [[nodiscard]] static ZzResult success(ZzValue value)
    {
        return ZzResult(std::in_place_index<0>, std::move(value));
    }

    /**
     * @brief 构造失败结果。
     * @param error 错误码不得为 None 的错误值。
     * @return 持有错误的结果。
     */
    [[nodiscard]] static ZzResult failure(ZzError error)
    {
        Q_ASSERT(error.isError());
        if (!error.isError()) {
            std::terminate();
        }
        return ZzResult(std::in_place_index<1>, std::move(error));
    }

    /**
     * @brief 查询是否持有成功值。
     * @return 持有成功值时返回 true。
     */
    [[nodiscard]] bool hasValue() const noexcept
    {
        return storage_.index() == 0;
    }

    /**
     * @brief 查询是否成功。
     * @return 与 hasValue() 相同。
     */
    [[nodiscard]] explicit operator bool() const noexcept
    {
        return hasValue();
    }

    /**
     * @brief 获取可修改的成功值。
     * @return 成功值引用。
     */
    [[nodiscard]] ZzValue &value() &
    {
        Q_ASSERT(hasValue());
        auto *value = std::get_if<0>(&storage_);
        if (value == nullptr) {
            std::terminate();
        }
        return *value;
    }

    /**
     * @brief 获取只读成功值。
     * @return 成功值常量引用。
     */
    [[nodiscard]] const ZzValue &value() const &
    {
        Q_ASSERT(hasValue());
        const auto *value = std::get_if<0>(&storage_);
        if (value == nullptr) {
            std::terminate();
        }
        return *value;
    }

    /**
     * @brief 从右值结果中转移成功值。
     * @return 成功值右值引用。
     */
    [[nodiscard]] ZzValue &&value() &&
    {
        Q_ASSERT(hasValue());
        auto *value = std::get_if<0>(&storage_);
        if (value == nullptr) {
            std::terminate();
        }
        return std::move(*value);
    }

    /**
     * @brief 获取只读错误值。
     * @return 错误值常量引用。
     */
    [[nodiscard]] const ZzError &error() const & noexcept
    {
        Q_ASSERT(!hasValue());
        const auto *error = std::get_if<1>(&storage_);
        if (error == nullptr) {
            std::terminate();
        }
        return *error;
    }

    /**
     * @brief 从右值结果中转移错误值。
     * @return 错误值右值引用。
     */
    [[nodiscard]] ZzError &&error() && noexcept
    {
        Q_ASSERT(!hasValue());
        auto *error = std::get_if<1>(&storage_);
        if (error == nullptr) {
            std::terminate();
        }
        return std::move(*error);
    }

private:
    template<std::size_t ZzIndex, typename ZzArgument>
    explicit ZzResult(
        std::in_place_index_t<ZzIndex>,
        ZzArgument &&argument)
        : storage_(
              std::in_place_index<ZzIndex>,
              std::forward<ZzArgument>(argument))
    {
    }

    std::variant<ZzValue, ZzError> storage_;
};

/**
 * @brief 不携带成功值的结果特化。
 *
 * 成功状态不进行动态分配；成功状态访问 error() 属于程序员错误。
 */
template<>
class [[nodiscard]] ZzResult<void> final
{
public:
    /**
     * @brief 构造成功结果。
     * @return 不携带错误的成功结果。
     */
    [[nodiscard]] static ZzResult success() noexcept
    {
        return ZzResult(std::nullopt);
    }

    /**
     * @brief 构造失败结果。
     * @param error 错误码不得为 None 的错误值。
     * @return 持有错误的结果。
     */
    [[nodiscard]] static ZzResult failure(ZzError error)
    {
        Q_ASSERT(error.isError());
        if (!error.isError()) {
            std::terminate();
        }
        return ZzResult(std::optional<ZzError>(
            std::in_place, std::move(error)));
    }

    /**
     * @brief 查询是否成功。
     * @return 未持有错误时返回 true。
     */
    [[nodiscard]] bool hasValue() const noexcept
    {
        return !error_.has_value();
    }

    /**
     * @brief 查询是否成功。
     * @return 与 hasValue() 相同。
     */
    [[nodiscard]] explicit operator bool() const noexcept
    {
        return hasValue();
    }

    /**
     * @brief 获取只读错误值。
     * @return 错误值常量引用。
     */
    [[nodiscard]] const ZzError &error() const & noexcept
    {
        Q_ASSERT(!hasValue());
        if (!error_) {
            std::terminate();
        }
        return *error_;
    }

    /**
     * @brief 从右值结果中转移错误值。
     * @return 错误值右值引用。
     */
    [[nodiscard]] ZzError &&error() && noexcept
    {
        Q_ASSERT(!hasValue());
        if (!error_) {
            std::terminate();
        }
        return std::move(*error_);
    }

private:
    explicit ZzResult(std::optional<ZzError> error) noexcept
        : error_(std::move(error))
    {
    }

    std::optional<ZzError> error_;
};

} // namespace ZzCore
