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

#include "remote.hpp"

#include "errors.hpp"

#include <boost/lexical_cast.hpp>
#include <boost/logic/tribool_io.hpp>
#include <regex>

using namespace std;

static char DATA_DELIMITER = '|';

static bool validateName(const String &name)
{
  using namespace std::regex_constants;

  // see https://en.wikipedia.org/wiki/Filename#Reserved%5Fcharacters%5Fand%5Fwords
  static const regex validPattern("[^*\\\\:<>?/|\"[:cntrl:]]{3,}");
  static const regex invalidPattern(
    "[\\.\x20].+|.+[\\.\x20]|CLOCK\\$|COM\\d|LPT\\d", icase);

  smatch valid, invalid;
  regex_match(name, valid, validPattern);
  regex_match(name, invalid, invalidPattern);

  return !valid.empty() && invalid.empty();
}

static bool validateUrl(const String &url)
{
  // see http://tools.ietf.org/html/rfc3986#section-2
  static const regex pattern(
    "(?:[a-zA-Z0-9._~:/?#[\\]@!$&'()*+,;=-]|%[a-f0-9]{2})+");

  smatch match;
  regex_match(url, match, pattern);

  return !match.empty();
}

Remote Remote::fromString(const String &data)
{
  StringStreamI stream(data);

  String name;
  getline(stream, name, DATA_DELIMITER);

  String url;
  getline(stream, url, DATA_DELIMITER);

  String enabled;
  getline(stream, enabled, DATA_DELIMITER);

  String autoInstall;
  getline(stream, autoInstall, DATA_DELIMITER);

  if(!validateName(name) || !validateUrl(url))
    return {};

  Remote remote(name, url);

  try {
    remote.setEnabled(boost::lexical_cast<bool>(enabled));
  }
  catch(const boost::bad_lexical_cast &) {}

  try {
    remote.setAutoInstall(boost::lexical_cast<tribool>(autoInstall));
  }
  catch(const boost::bad_lexical_cast &) {}

  return remote;
}

Remote::Remote()
  : m_enabled(true), m_protected(false), m_autoInstall(boost::logic::indeterminate)
{
}

Remote::Remote(const String &name, const String &url, const bool enabled, const tribool &autoInstall)
  : m_enabled(enabled), m_protected(false), m_autoInstall(autoInstall)
{
  setName(name);
  setUrl(url);
}

void Remote::setName(const String &name)
{
  if(!validateName(name))
    throw reapack_error("invalid name");
  else
    m_name = name;
}

void Remote::setUrl(const String &url)
{
  if(!validateUrl(url))
    throw reapack_error("invalid url");
  else
    m_url = url;
}

String Remote::toString() const
{
  StringStreamO out;
  out << m_name << DATA_DELIMITER;
  out << m_url << DATA_DELIMITER;
  out << m_enabled << DATA_DELIMITER;
  out << m_autoInstall;

  return out.str();
}

void RemoteList::add(const Remote &remote)
{
  if(!remote)
    return;

  size_t index = 0;
  const auto &it = m_map.find(remote.name());

  if(it == m_map.end()) {
    // insert remote
    index = m_remotes.size();
    m_remotes.push_back(remote);
  }
  else {
    // replace remote
    index = it->second;
    m_remotes[index] = remote;
  }

  m_map[remote.name()] = index;
}

void RemoteList::remove(const String &name)
{
  const auto &it = m_map.find(name);

  if(it == m_map.end())
    return;

  m_remotes.erase(m_remotes.begin() + it->second);

  for(auto walk = m_map.begin(); walk != m_map.end(); walk++) {
    if(walk->second > it->second)
      walk->second--;
  }

  m_map.erase(it);
}

Remote RemoteList::get(const String &name) const
{
  const auto &it = m_map.find(name);

  if(it == m_map.end())
    return {};
  else
    return m_remotes[it->second];
}

vector<Remote> RemoteList::getEnabled() const
{
  vector<Remote> list;

  for(const Remote &remote : m_remotes) {
    if(remote.isEnabled())
      list.push_back(remote);
  }

  return list;
}
