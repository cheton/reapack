/* ReaPack: Package manager for REAPER
 * Copyright (C) 2015-2018  Christian Fillion
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

#include "progress.hpp"

// #include "thread.hpp"
#include "resource.hpp"
#include "string.hpp"
#include "win32.hpp"

#include <sstream>

using namespace std;

Progress::Progress()
  : Dialog(IDD_PROGRESS_DIALOG), m_done(0), m_total(0)
{
}

void Progress::onInit()
{
  Dialog::onInit();

  m_label = getControl(IDC_LABEL);
  m_progress = getControl(IDC_PROGRESS);
  m_cancel = getControl(IDCANCEL);

  Win32::setWindowText(m_label, "Initializing...");

  disable(m_cancel);
  startTimer(100);
}

void Progress::onCommand(const int id, int)
{
  switch(id) {
  case IDCANCEL:
    onCancel();

    // don't wait until the current downloads are finished
    // before getting out of the user way
    // hide();
    break;
  }
}

void Progress::onTimer(const int id)
{
#ifdef _WIN32
  if(!IsWindowEnabled(handle()))
    return;
#endif

  show();
  stopTimer(id);
}

void Progress::setStatus(const std::string &status)
{
  m_current = status;
  updateProgress();
}

void Progress::addStep()
{
  m_total++;
  updateProgress();
}

void Progress::stepFinished()
{
  m_done++;
  updateProgress();
}

void Progress::setCancellable(const bool cancellable)
{
  setEnabled(cancellable, m_cancel);
}

void Progress::updateProgress()
{
  Win32::setWindowText(m_label, m_current.c_str());

  const double pos = static_cast<double>(min(m_done+1, m_total)) / max(2, m_total);
  const int percent = static_cast<int>(pos * 100);

  SendMessage(m_progress, PBM_SETPOS, percent, 0);
  Win32::setWindowText(handle(), String::format(
    "ReaPack: Operation in progress (%d%%)", percent
  ).c_str());
}
