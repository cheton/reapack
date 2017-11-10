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

#include "task.hpp"

#include "archive.hpp"
#include "config.hpp"
#include "download.hpp"
#include "errors.hpp"
#include "filesystem.hpp"
#include "index.hpp"
#include "reapack.hpp"
#include "transaction.hpp"

using namespace std;

UninstallTask::UninstallTask(const Registry::Entry &re, Transaction *tx)
  : Task(tx), m_entry(move(re))
{
}

bool UninstallTask::start()
{
  tx()->registry()->getFiles(m_entry).swap(m_files);

  // allow conflicting packages to be installed
  tx()->registry()->forget(m_entry);

  return true;
}

void UninstallTask::commit()
{
  for(const auto &file : m_files) {
    if(!FS::exists(file.path))
      continue;

    if(FS::removeRecursive(file.path))
      tx()->receipt()->addRemoval(file.path);
    else
      tx()->receipt()->addError({FS::lastError(), file.path.join()});

    tx()->registerFile({false, m_entry, file});
  }

  tx()->registry()->forget(m_entry);
}

PinTask::PinTask(const Registry::Entry &re, const bool pin, Transaction *tx)
  : Task(tx), m_entry(move(re)), m_pin(pin)
{
}

void PinTask::commit()
{
  tx()->registry()->setPinned(m_entry, m_pin);
  tx()->receipt()->setPackageChanged();
}
