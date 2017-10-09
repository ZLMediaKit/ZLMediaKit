// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains macros and macro-like constructs (e.g., templates) that
// are commonly used throughout Chromium source. (It may also contain things
// that are closely related to things that are commonly used that belong in this
// file.)

#ifndef BASE_MACROS_H_
#define BASE_MACROS_H_

#include <stddef.h>  // For size_t.

// Put this in the declarations for a class to be uncopyable.
#define DISALLOW_COPY(TypeName) \
TypeName(const TypeName&) = delete

// Put this in the declarations for a class to be unassignable.
#define DISALLOW_ASSIGN(TypeName) \
void operator=(const TypeName&) = delete

// A macro to disallow the copy constructor and operator= functions.
// This should be used in the private: declarations for a class.
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
TypeName(const TypeName&) = delete;      \
void operator=(const TypeName&) = delete

// A macro to disallow all the implicit constructors, namely the
// default constructor, copy constructor and operator= functions.
//
// This should be used in the private: declarations for a class
// that wants to prevent anyone from instantiating it. This is
// especially useful for classes containing only static methods.
#define DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName) \
TypeName() = delete;                           \
DISALLOW_COPY_AND_ASSIGN(TypeName)

// The arraysize(arr) macro returns the # of elements in an array arr.  The
// expression is a compile-time constant, and therefore can be used in defining
// new arrays, for example.  If you use arraysize on a pointer by mistake, you
// will get a compile-time error.  For the technical details, refer to
// http://blogs.msdn.com/b/the1/archive/2004/05/07/128242.aspx.

// This template function declaration is used in defining arraysize.
// Note that the function doesn't need an implementation, as we only
// use its type.
template <typename T, size_t N> char (&ArraySizeHelper(T (&array)[N]))[N];
#define arraysize(array) (sizeof(ArraySizeHelper(array)))

// Used to explicitly mark the return value of a function as unused. If you are
// really sure you don't want to do anything with the return value of a function
// that has been marked WARN_UNUSED_RESULT, wrap it with this. Example:
//
//   std::unique_ptr<MyType> my_var = ...;
//   if (TakeOwnership(my_var.get()) == SUCCESS)
//     ignore_result(my_var.release());
//
template<typename T>
inline void ignore_result(const T&) {
}

// The following enum should be used only as a constructor argument to indicate
// that the variable has static storage class, and that the constructor should
// do nothing to its state.  It indicates to the reader that it is legal to
// declare a static instance of the class, provided the constructor is given
// the base::LINKER_INITIALIZED argument.  Normally, it is unsafe to declare a
// static variable that has a constructor or a destructor because invocation
// order is undefined.  However, IF the type can be initialized by filling with
// zeroes (which the loader does for static variables), AND the destructor also
// does nothing to the storage, AND there are no virtual methods, then a
// constructor declared as
//       explicit MyClass(base::LinkerInitialized x) {}
// and invoked as
//       static MyClass my_variable_name(base::LINKER_INITIALIZED);
namespace base {
    enum LinkerInitialized { LINKER_INITIALIZED };
    
    // Use these to declare and define a static local variable (static T;) so that
    // it is leaked so that its destructors are not called at exit. If you need
    // thread-safe initialization, use base/lazy_instance.h instead.
#define CR_DEFINE_STATIC_LOCAL(type, name, arguments) \
static type& name = *new type arguments
    
}  // base

#define MEDIA_EXPORT

#include <assert.h>
#include "Util/logger.h"
using namespace ZL::Util;

#define DCHECK(x) if(!(x)) { ErrorL << "DCHECK " << #x <<endl; }
#define DCHECK_GT(x,y) if(!((x) > (y))) { ErrorL << "DCHECK_GT:" << #x << #y << endl; }
#define DCHECK_GE(x,y) if(!((x) >= (y))) { ErrorL << "DCHECK_GE:" << #x << #y << endl; }
#define DCHECK_LT(x,y) if(!((x) < (y))) { ErrorL << "DCHECK_LT:" << #x << #y << endl; }
#define NOTREACHED() ErrorL << "NOTREACHED" << endl;


#endif  // BASE_MACROS_H_
