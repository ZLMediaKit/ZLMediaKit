//     __ _____ _____ _____
//  __|  |   __|     |   | |  JSON for Modern C++
// |  |  |__   |  |  | | | |  version 3.11.2
// |_____|_____|_____|_|___|  https://github.com/nlohmann/json
//
// SPDX-FileCopyrightText: 2013-2022 Niels Lohmann <https://nlohmann.me>
// SPDX-License-Identifier: MIT

#pragma once

#include <utility>

#include <json/detail/abi_macros.hpp>
#include <json/detail/conversions/from_json.hpp>
#include <json/detail/conversions/to_json.hpp>
#include <json/detail/meta/identity_tag.hpp>

NLOHMANN_JSON_NAMESPACE_BEGIN

/// @sa https://json.nlohmann.me/api/adl_serializer/
template<typename ValueType, typename>
struct adl_serializer {
    /// @brief convert a JSON value to any value type
    /// @sa https://json.nlohmann.me/api/adl_serializer/from_json/
    template<typename BasicJsonType, typename TargetType = ValueType>
    static auto from_json(BasicJsonType &&j, TargetType &val) noexcept(
            noexcept(::Leo::from_json(std::forward<BasicJsonType>(j), val)))
            -> decltype(::Leo::from_json(std::forward<BasicJsonType>(j), val), void()) {
        ::Leo::from_json(std::forward<BasicJsonType>(j), val);
    }

    /// @brief convert a JSON value to any value type
    /// @sa https://json.nlohmann.me/api/adl_serializer/from_json/
    template<typename BasicJsonType, typename TargetType = ValueType>
    static auto from_json(BasicJsonType &&j) noexcept(
            noexcept(::Leo::from_json(std::forward<BasicJsonType>(j), detail::identity_tag<TargetType>{})))
            -> decltype(::Leo::from_json(std::forward<BasicJsonType>(j), detail::identity_tag<TargetType>{})) {
        return ::Leo::from_json(std::forward<BasicJsonType>(j), detail::identity_tag<TargetType>{});
    }

    /// @brief convert any value type to a JSON value
    /// @sa https://json.nlohmann.me/api/adl_serializer/to_json/
    template<typename BasicJsonType, typename TargetType = ValueType>
    static auto to_json(BasicJsonType &j, TargetType &&val) noexcept(
            noexcept(::Leo::to_json(j, std::forward<TargetType>(val))))
            -> decltype(::Leo::to_json(j, std::forward<TargetType>(val)), void()) {
        ::Leo::to_json(j, std::forward<TargetType>(val));
    }
};

NLOHMANN_JSON_NAMESPACE_END
