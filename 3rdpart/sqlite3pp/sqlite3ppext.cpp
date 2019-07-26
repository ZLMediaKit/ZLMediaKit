// sqlite3ppext.cpp
//
// The MIT License
//
// Copyright (c) 2015 Wongoo Lee (iwongu at gmail dot com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include <cstring>

#include "sqlite3ppext.h"

namespace sqlite3pp
{
  namespace ext
  {

    namespace
    {

      void function_impl(sqlite3_context* ctx, int nargs, sqlite3_value** values)
      {
        auto f = static_cast<function::function_handler*>(sqlite3_user_data(ctx));
        context c(ctx, nargs, values);
        (*f)(c);
      }

      void step_impl(sqlite3_context* ctx, int nargs, sqlite3_value** values)
      {
        auto p = static_cast<std::pair<aggregate::pfunction_base, aggregate::pfunction_base>*>(sqlite3_user_data(ctx));
        auto s = static_cast<aggregate::function_handler*>((*p).first.get());
        context c(ctx, nargs, values);
        ((function::function_handler&)*s)(c);
      }

      void finalize_impl(sqlite3_context* ctx)
      {
        auto p = static_cast<std::pair<aggregate::pfunction_base, aggregate::pfunction_base>*>(sqlite3_user_data(ctx));
        auto f = static_cast<aggregate::function_handler*>((*p).second.get());
        context c(ctx);
        ((function::function_handler&)*f)(c);
      }

    } // namespace

    database borrow(sqlite3* pdb) {
      return database(pdb);
    }

    context::context(sqlite3_context* ctx, int nargs, sqlite3_value** values)
      : ctx_(ctx), nargs_(nargs), values_(values)
    {
    }

    int context::args_count() const
    {
      return nargs_;
    }

    int context::args_bytes(int idx) const
    {
      return sqlite3_value_bytes(values_[idx]);
    }

    int context::args_type(int idx) const
    {
      return sqlite3_value_type(values_[idx]);
    }

    int context::get(int idx, int) const
    {
      return sqlite3_value_int(values_[idx]);
    }

    double context::get(int idx, double) const
    {
      return sqlite3_value_double(values_[idx]);
    }

    long long int context::get(int idx, long long int) const
    {
      return sqlite3_value_int64(values_[idx]);
    }

    char const* context::get(int idx, char const*) const
    {
      return reinterpret_cast<char const*>(sqlite3_value_text(values_[idx]));
    }

    std::string context::get(int idx, std::string) const
    {
      return get(idx, (char const*)0);
    }

    void const* context::get(int idx, void const*) const
    {
      return sqlite3_value_blob(values_[idx]);
    }



    void context::result(int value)
    {
      sqlite3_result_int(ctx_, value);
    }

    void context::result(double value)
    {
      sqlite3_result_double(ctx_, value);
    }

    void context::result(long long int value)
    {
      sqlite3_result_int64(ctx_, value);
    }

    void context::result(std::string const& value)
    {
      result(value.c_str(), false);
    }

    void context::result(char const* value, bool fcopy)
    {
      sqlite3_result_text(ctx_, value, std::strlen(value), fcopy ? SQLITE_TRANSIENT : SQLITE_STATIC);
    }

    void context::result(void const* value, int n, bool fcopy)
    {
      sqlite3_result_blob(ctx_, value, n, fcopy ? SQLITE_TRANSIENT : SQLITE_STATIC );
    }

    void context::result()
    {
      sqlite3_result_null(ctx_);
    }

    void context::result(null_type)
    {
      sqlite3_result_null(ctx_);
    }

    void context::result_copy(int idx)
    {
      sqlite3_result_value(ctx_, values_[idx]);
    }

    void context::result_error(char const* msg)
    {
      sqlite3_result_error(ctx_, msg, std::strlen(msg));
    }

    void* context::aggregate_data(int size)
    {
      return sqlite3_aggregate_context(ctx_, size);
    }

    int context::aggregate_count()
    {
      return sqlite3_aggregate_count(ctx_);
    }

    function::function(database& db) : db_(db.db_)
    {
    }

    int function::create(char const* name, function_handler h, int nargs)
    {
      fh_[name] = pfunction_base(new function_handler(h));
      return sqlite3_create_function(db_, name, nargs, SQLITE_UTF8, fh_[name].get(), function_impl, 0, 0);
    }

    aggregate::aggregate(database& db) : db_(db.db_)
    {
    }

    int aggregate::create(char const* name, function_handler s, function_handler f, int nargs)
    {
      ah_[name] = std::make_pair(pfunction_base(new function_handler(s)), pfunction_base(new function_handler(f)));
      return sqlite3_create_function(db_, name, nargs, SQLITE_UTF8, &ah_[name], 0, step_impl, finalize_impl);
    }

  } // namespace ext

} // namespace sqlite3pp
