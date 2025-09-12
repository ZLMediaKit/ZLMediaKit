/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
*/

#ifndef ZLMEDIAKIT_SHELLPARSER_H
#define ZLMEDIAKIT_SHELLPARSER_H

#include <iostream>
#include <string>
#include <vector>
#include <cctype>

// Shell-like command line parser.
// Features:
//  - Whitespace splitting (space, tab, newline)
//  - Quotes: single ('...') and double ("...")
//  - Escapes with backslash (\\) outside quotes
//  - In single quotes: backslash is literal (like POSIX shell)
//  - In double quotes: backslash can escape  "  $  `  \\  and newline (line continuation)
//    Additionally supports common C-style escapes: \n \t \r \0 .. outside and inside double quotes
//  - Line continuation: backslash followed by newline is ignored
//  - Produces argv pointers with stable lifetime backed by std::vector<std::string>
//
// Notes:
//  - This is NOT a full shell (no variable expansion, no globbing, no command substitution).
//  - Behavior aims to be practical and safe for exec* arguments building.

struct ParseResult {
    ParseResult(bool ok, const char *err, size_t pos, std::vector<std::string> args)
        : ok(ok)
        , error_msg(err)
        , error_pos(pos)
        , args(std::move(args)) {}

    bool ok;
    std::string error_msg;
    size_t error_pos = 0;         // index in input when error happens
    std::vector<std::string> args; // parsed arguments
};

namespace detail {

inline bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n';
}

// Returns true if it handled a line continuation ("\\\n").
inline bool handle_line_continuation(const std::string &s, size_t &i) {
    if (i + 1 < s.size() && s[i] == '\\' && s[i + 1] == '\n') {
        i += 2; // consume both and do nothing
        return true;
    }
    return false;
}

inline bool hex_digit(char c) { return std::isxdigit(static_cast<unsigned char>(c)) != 0; }
inline int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return 0;
}

// Parse C-style escapes: \n, \t, \r, \0..\377 (octal), \xHH (hex). Returns std::nullopt if not a known escape.
inline std::pair<bool, char> c_style_escape(const std::string &s, size_t &i) {
    if (i >= s.size()) return std::make_pair(false, '\0');
    char c = s[i];
    switch (c) {
        case 'n': ++i; return std::make_pair(true, '\n');
        case 't': ++i; return std::make_pair(true, '\t');
        case 'r': ++i; return std::make_pair(true, '\r');
        case 'a': ++i; return std::make_pair(true, '\a');
        case 'b': ++i; return std::make_pair(true, '\b');
        case 'f': ++i; return std::make_pair(true, '\f');
        case 'v': ++i; return std::make_pair(true, '\v');
        case '\\': ++i; return std::make_pair(true, '\\');
        case '"': ++i; return std::make_pair(true, '"');
        case '\'': ++i; return std::make_pair(true, '\'');
        case '0': {
            // up to 3 octal digits total (including the first 0 already consumed here?)
            // Here c=='0' means octal sequence starts at current '0'.
            // We'll parse up to 3 octal digits starting at current pos.
            int val = 0; int cnt = 0;
            while (i < s.size() && cnt < 3 && (s[i] >= '0' && s[i] <= '7')) {
                val = (val << 3) + (s[i] - '0');
                ++i; ++cnt;
            }
            return std::make_pair(true, static_cast<char>(val & 0xFF));
        }
        case 'x': {
            ++i; // consume 'x'
            int val = 0; int cnt = 0;
            while (i < s.size() && cnt < 2 && hex_digit(s[i])) {
                val = (val << 4) + hex_val(s[i]);
                ++i; ++cnt;
            }
            if (cnt == 0) return std::make_pair(false, '\0'); // not actually a hex escape
            return std::make_pair(true, static_cast<char>(val & 0xFF));
        }
        default:
            return std::make_pair(false, '\0');
    }
}

}

ParseResult parse_shell_like(const std::string &input) {
    using namespace detail;
    std::vector<std::string> args;
    std::string cur;

    enum class State { Normal, InSingle, InDouble };
    State st = State::Normal;

    size_t i = 0; const size_t N = input.size();
    while (i < N) {
        // line continuation check (\\\n) applies in all states
        if (handle_line_continuation(input, i)) continue;
        if (i >= N) break;

        char c = input[i];
        switch (st) {
            case State::Normal: {
                if (is_space(c)) {
                    if (!cur.empty()) { args.emplace_back(std::move(cur)); cur.clear(); }
                    ++i;
                } else if (c == '\'') {
                    st = State::InSingle; ++i;
                } else if (c == '"') {
                    st = State::InDouble; ++i;
                } else if (c == '\\') {
                    ++i; // consume backslash
                    if (i >= N) {
                        return {false, "结尾处孤立的反斜杠（未转义任何字符）", i, {}};
                    }
                    // Try C-style escapes first
                    auto esc = c_style_escape(input, i);
                    if (esc.first) {
                        cur.push_back(esc.second);
                    } else {
                        // Not a known C escape: take the next char literally
                        cur.push_back(input[i]);
                        ++i;
                    }
                } else {
                    cur.push_back(c); ++i;
                }
            } break;

            case State::InSingle: {
                if (c == '\'') { st = State::Normal; ++i; }
                else { cur.push_back(c); ++i; }
            } break;

            case State::InDouble: {
                if (c == '"') { st = State::Normal; ++i; }
                else if (c == '\\') {
                    ++i; // consume backslash
                    if (i >= N) {
                        return {false, "双引号内以反斜杠结尾，缺少被转义字符", i, {}};
                    }
                    // In POSIX shell, within double quotes, only certain escapes are special.
                    // Here we support both POSIX subset and common C-style escapes for practicality.
                    auto esc = c_style_escape(input, i);
                    if (esc.first) {
                        cur.push_back(esc.second);
                    } else {
                        // If not a C-style escape, allow escaping one char literally (e.g., $ `)
                        cur.push_back(input[i]);
                        ++i;
                    }
                } else {
                    cur.push_back(c); ++i;
                }
            } break;
        }
    }

    if (st == State::InSingle) {
        return {false, "缺少配对的单引号（'）", i, {}};
    }
    if (st == State::InDouble) {
        return {false, "缺少配对的双引号（\"）", i, {}};
    }

    if (!cur.empty()) args.emplace_back(std::move(cur));

    return {true, "", 0, std::move(args)};
}

// Helper: build argv pointers backed by the strings' storage.
// The returned vector includes a trailing nullptr, suitable for execv*.
inline std::vector<const char*> make_argv(const std::vector<std::string>& args) {
    std::vector<const char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto &s : args) argv.push_back(s.c_str());
    argv.push_back(nullptr);
    return argv;
}

#endif // ZLMEDIAKIT_SHELLPARSER_H
