// Random utility functions that have nothing to do with apt. -*-c++-*-
//
//   Copyright (C) 2005 Daniel Burrows
//
//   This program is free software; you can redistribute it and/or
//   modify it under the terms of the GNU General Public License as
//   published by the Free Software Foundation; either version 2 of
//   the License, or (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//   General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program; see the file COPYING.  If not, write to
//   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
//   Boston, MA 02111-1307, USA.

#ifndef UTIL_H
#define UTIL_H

#include <string>
#include <utility>
#include <cstdarg>

struct tm;

// Strip whitespace from the beginning and end of a string.
void stripws(std::string &s);

// Printf for std::string.
#ifdef __GNUG__
__attribute__ ((format (printf, 1, 2)))
#endif
std::string ssprintf(const char *format, ...);
std::string vssprintf(const char *format, va_list ap);

std::wstring swsprintf(const wchar_t *format, ...);

std::wstring vswsprintf(const wchar_t *format, va_list ap);

/** Like strftime, but handles all memory allocation and returns a C++
 *  string.
 */
std::string sstrftime(const char *format, const tm *tm);

/** Like strerror_r, but handles all memory allocation and returns a
 *  C++ string.
 */
std::string sstrerror(int errnum);

/** \return the home directory of the current user as given in the
 *     password database.  If no home directory can be looked up,
 *     returns an empty string.
 */
std::string get_homedir();

/** Compare pairs, with (a,b) considered eqivalent to (b,a). */
template<typename T>
struct orderless_lt
{
  bool operator()(const typename std::pair<T, T> &p1,
		  const typename std::pair<T, T> &p2)
  {
    const std::pair<const T, const T> p1sort
      = (p1.first < p1.second) ? std::pair<const T, const T>(p1.first, p1.second) : std::pair<const T, const T>(p1.second, p1.first);
    const std::pair<const T, const T> p2sort
      = (p2.first < p2.second) ? std::pair<const T, const T>(p2.first, p2.second) : std::pair<const T, const T>(p2.second, p2.first);

    return p1sort < p2sort;
  }
};

/** A function object representing the identity operation. */
template<typename T>
struct identity
{
  T operator()(const T &t) const
  {
    return t;
  }
};

/** A function object that returns its first argument. */
template<typename T1, typename T2>
struct project1st
{
  T1 operator()(const T1 &t1, const T2 &t2) const
  {
    return t1;
  }
};

/** A function object that returns its second argument. */
template<typename T1, typename T2>
struct project2nd
{
  T2 operator()(const T1 &t1, const T2 &t2) const
  {
    return t2;
  }
};

/** A function object that extracts the first element from a pair. */
template<typename T1, typename T2>
struct select1st
{
  T1 operator()(const std::pair<T1, T2> &p) const
  {
    return p.first;
  }
};

/** A function object that extracts the second element from a pair. */
template<typename T1, typename T2>
struct select2nd
{
  T2 operator()(const std::pair<T1, T2> &p) const
  {
    return p.second;
  }
};

#endif
