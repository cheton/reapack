/* ReaPack: Package manager for REAPER
 * Copyright (C) 2015-2017  Christian Fillion
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef REAPACK_STRING_HPP
#define REAPACK_STRING_HPP

// This whole file is a huge hack to support unicode on Windows
// MultiByteToWideCharT is required to make file path like
// "C:\Users\Test\Downloads\Новая папка" work on Windows...

#include <string>

#ifdef _WIN32
  #include <windows.h>

  typedef wchar_t CharT;
  #define L(str) L##str

  #define strtoutf8(str) (static_cast<std::string>(String(static_cast<const CharT *>(str))).c_str())
  #define strfromutf8(str) (String(static_cast<const char *>(str)).c_str())

  #define snprintf(...) snprintf_auto(__VA_ARGS__)
  extern int snprintf_auto(char *buf, size_t size, const char *fmt, ...);
  extern int snprintf_auto(wchar_t *buf, size_t size, const wchar_t *fmt, ...);
#else
  typedef char CharT;
  #define L(str) str

  #define strtoutf8(str) (str)
  #define strfromutf8(str) (str)
#endif

typedef std::basic_string<CharT> BasicString;

class String : public BasicString {
public:
  using BasicString::basic_string;

  String() : basic_string() {}
  String(const BasicString &s) : basic_string(s) {}
  String(const BasicString &&s) : basic_string(move(s)) {}

#ifdef _WIN32
  // reference to pointer to deny string literals (forcing the use of the L macro)
  String(const char *&str, UINT codepage = CP_UTF8) { mbtowide(str, codepage); }
  String(const std::string &utf8) { mbtowide(utf8.c_str(), CP_UTF8); }
  template<typename T> explicit String(T v) : basic_string(std::to_wstring(v)) {}

  operator std::string() const;
  inline std::string toUtf8() const { return *this; }

protected:
  void mbtowide(const char *, UINT codepage);
#else
  template<typename T> explicit String(T v) : basic_string(std::to_string(v)) {}
  inline const std::string &toUtf8() const { return *this; }
#endif

  inline std::string toUtf8() { return *this; }
};

template<> struct std::hash<String> {
  size_t operator()(const String &str) const {
    return std::hash<BasicString>{}(str);
  }
};

#include <sstream>
typedef std::basic_stringstream<CharT> StringStream;
typedef std::basic_ostringstream<CharT> StringStreamO;
typedef std::basic_istringstream<CharT> StringStreamI;

#include <boost/format/format_fwd.hpp>
typedef boost::basic_format<CharT> StringFormat;

#include <regex>
typedef std::basic_regex<CharT> Regex;
typedef std::regex_iterator<String::const_iterator> RegexIterator;
typedef std::match_results<String::const_iterator> MatchResults;

#define lengthof(arr) (sizeof(arr) / sizeof(*arr))

#endif
