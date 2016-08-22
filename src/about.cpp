/* ReaPack: Package manager for REAPER
 * Copyright (C) 2015-2016  Christian Fillion
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

#include "about.hpp"

#include "browser.hpp"
#include "encoding.hpp"
#include "errors.hpp"
#include "index.hpp"
#include "listview.hpp"
#include "menu.hpp"
#include "ostream.hpp"
#include "reapack.hpp"
#include "registry.hpp"
#include "remote.hpp"
#include "report.hpp"
#include "resource.hpp"
#include "richedit.hpp"
#include "tabbar.hpp"
#include "transaction.hpp"

#include <boost/algorithm/string/replace.hpp>
#include <iomanip>
#include <sstream>

using namespace std;

enum { ACTION_ABOUT_PKG = 300, ACTION_FIND_IN_BROWSER, ACTION_COPY_URL };

About::About() : Dialog(IDD_ABOUT_DIALOG)
{
  RichEdit::Init();
}

void About::onInit()
{
  Dialog::onInit();

  m_tabs = createControl<TabBar>(IDC_TABS);
  m_desc = createControl<RichEdit>(IDC_ABOUT);

  m_menu = createControl<ListView>(IDC_MENU);
  m_menu->onSelect(bind(&About::updateList, this));

  m_list = createControl<ListView>(IDC_LIST);
  m_list->onContextMenu([=] (Menu &m, int i) { return m_delegate->fillContextMenu(m, i); });
  m_list->onActivate([=] { m_delegate->itemActivated(); });

  m_report = getControl(IDC_REPORT);
}

void About::onCommand(const int id, int)
{
  switch(id) {
  case IDOK:
  case IDCANCEL:
    close();
    break;
  default:
    if(m_links.count(id))
      selectLink(id);
    else if(m_delegate)
      m_delegate->onCommand(id);
    break;
  }
}

void About::setDelegate(const DelegatePtr &delegate)
{
#ifdef _WIN32
  // preventing fast blinking on windows
  SendMessage(handle(), WM_SETREDRAW, false, 0);
#endif

  m_tabs->clear();
  m_menu->reset();
  m_menu->sortByColumn(0);
  m_list->reset();
  m_list->sortByColumn(0);

  m_delegate = nullptr;
  m_links.clear();

  const int controls[] = {
    IDC_ABOUT,
    IDC_MENU,
    IDC_LIST,
    IDC_REPORT,
    IDC_WEBSITE,
    IDC_SCREENSHOT,
    IDC_DONATE,
    IDC_ACTION,
  };

  for(const int control : controls)
    hide(getControl(control));

  m_delegate = delegate;
  m_delegate->init(this);
  m_menu->sort();

  m_currentIndex = -255;
  updateList();

#ifdef LVSCW_AUTOSIZE_USEHEADER
  m_menu->resizeColumn(m_menu->columnCount() - 1, LVSCW_AUTOSIZE_USEHEADER);
  m_list->resizeColumn(m_list->columnCount() - 1, LVSCW_AUTOSIZE_USEHEADER);
#endif

#ifdef _WIN32
  SendMessage(handle(), WM_SETREDRAW, true, 0);

  // This is required on Windows to get the first tab to be fully draw,
  // but I have no idea why...
  InvalidateRect(handle(), nullptr, true);
#endif
}

void About::setTitle(const string &what)
{
  auto_char title[255] = {};
  auto_snprintf(title, auto_size(title),
    AUTO_STR("About %s"), make_autostring(what).c_str());
  SetWindowText(handle(), title);
}

void About::setMetadata(const Metadata *metadata, const bool substitution)
{
  string aboutText = metadata->about();

  if(substitution) {
    boost::replace_all(aboutText, "[[REAPACK_VERSION]]", ReaPack::VERSION);
    boost::replace_all(aboutText, "[[REAPACK_BUILDTIME]]", ReaPack::BUILDTIME);
  }

  if(m_desc->setRichText(aboutText))
    m_tabs->addTab({AUTO_STR("About"), {m_desc->handle()}});

  const auto &getLinkControl = [](const Metadata::LinkType type) {
    switch(type) {
    case Metadata::WebsiteLink:
      return IDC_WEBSITE;
    case Metadata::DonationLink:
      return IDC_DONATE;
    case Metadata::ScreenshotLink:
      return IDC_SCREENSHOT;
    }

    return IDC_WEBSITE; // make MSVC happy
  };

  RECT rect;
  GetWindowRect(getControl(IDC_WEBSITE), &rect);
  ScreenToClient(handle(), (LPPOINT)&rect);
  ScreenToClient(handle(), ((LPPOINT)&rect)+1);

  const int shift = (rect.right - rect.left) + 4;

  for(const auto &pair : metadata->links()) {
    const int control = getLinkControl(pair.first);

    if(!m_links.count(control)) {
      HWND handle = getControl(control);

      SetWindowPos(handle, nullptr, rect.left, rect.top, 0, 0,
        SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);

      show(handle);
      rect.left += shift;
      m_links[control] = {};
    }

    m_links[control].push_back(&pair.second);
  }
}

void About::setAction(const string &label)
{
  HWND btn = getControl(IDC_ACTION);
  SetWindowText(btn, make_autostring(label).c_str());
  show(btn);
}

void About::selectLink(const int ctrl)
{
  const auto &links = m_links[ctrl];
  const int count = (int)links.size();

  m_tabs->setFocus();

  if(count == 1) {
    openLink(links.front());
    return;
  }

  Menu menu;

  for(int i = 0; i < count; i++) {
    const string &name = boost::replace_all_copy(links[i]->name, "&", "&&");
    menu.addAction(make_autostring(name).c_str(), i | (ctrl << 8));
  }

  const int choice = menu.show(getControl(ctrl), handle());

  if(choice >> 8 == ctrl)
    openLink(links[choice & 0xff]);
}

void About::openLink(const Link *link)
{
  const auto_string &url = make_autostring(link->url);
  ShellExecute(nullptr, AUTO_STR("open"), url.c_str(), nullptr, nullptr, SW_SHOW);
}

void About::updateList()
{
  const int index = m_menu->currentIndex();

  // do nothing when the selection is cleared, except for the initial execution
  if((index < 0 && m_currentIndex != -255) || index == m_currentIndex)
    return;

  InhibitControl lock(m_list);
  m_list->clear();

  m_delegate->updateList(index);
  m_currentIndex = index;

  m_list->sort();
}

AboutIndexDelegate::AboutIndexDelegate(const IndexPtr &index, ReaPack *reapack)
  : m_index(index), m_reapack(reapack)
{
}

void AboutIndexDelegate::init(About *dialog)
{
  m_dialog = dialog;

  dialog->setTitle(m_index->name());
  dialog->setMetadata(m_index->metadata(), m_index->name() == "ReaPack");
  dialog->setAction("Install/update " + m_index->name());

  // restore report size after being possibly modified by AboutPackageDelegate
  RECT rect;
  GetWindowRect(dialog->desc()->handle(), &rect);
  ScreenToClient(dialog->handle(), (LPPOINT)&rect);
  ScreenToClient(dialog->handle(), ((LPPOINT)&rect)+1);
  SetWindowPos(dialog->report(), nullptr, rect.left, rect.top,
    rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOACTIVATE);

  dialog->tabs()->addTab({AUTO_STR("Packages"),
    {dialog->menu()->handle(), dialog->list()->handle()}});
  dialog->tabs()->addTab({AUTO_STR("Installed Files"), {dialog->report()}});

  dialog->menu()->addColumn({AUTO_STR("Category"), 142});

  dialog->menu()->addRow({AUTO_STR("<All Packages>")});

  for(const Category *cat : m_index->categories())
    dialog->menu()->addRow({make_autostring(cat->name())});

  dialog->list()->addColumn({AUTO_STR("Package"), 382});
  dialog->list()->addColumn({AUTO_STR("Version"), 80});
  dialog->list()->addColumn({AUTO_STR("Author"), 90});

  dialog->list()->setSortCallback(1, [&] (const int a, const int b) {
    const Version *l = m_packagesData->at(a)->lastVersion();
    const Version *r = m_packagesData->at(b)->lastVersion();
    return l->compare(*r);
  });

  initInstalledFiles();
}

void AboutIndexDelegate::initInstalledFiles()
{
  set<Registry::File> allFiles;

  try {
    Registry reg(Path::prefixRoot(Path::REGISTRY));
    for(const Registry::Entry &entry : reg.getEntries(m_index->name())) {
      const vector<Registry::File> &files = reg.getFiles(entry);
      allFiles.insert(files.begin(), files.end());
    }
  }
  catch(const reapack_error &e) {
    const auto_string &desc = make_autostring(e.what());
    auto_char msg[255] = {};
    auto_snprintf(msg, auto_size(msg),
      AUTO_STR("The file list is currently unavailable.\x20")
      AUTO_STR("Retry later when all installation task are completed.\r\n")
      AUTO_STR("\r\nError description: %s"),
      desc.c_str());
    SetWindowText(m_dialog->report(), msg);
    return;
  }

  if(allFiles.empty()) {
    SetWindowText(m_dialog->report(),
      AUTO_STR(
      "This repository does not own any file on your computer at this time.\r\n")

      AUTO_STR("It is either not yet installed or it does not provide ")
      AUTO_STR("any package compatible with your system."));
  }
  else {
    stringstream stream;

    for(const Registry::File &file : allFiles) {
      stream << file.path.join();
      if(file.main)
        stream << '*';
      stream << "\r\n";
    }

    SetWindowText(m_dialog->report(), make_autostring(stream.str()).c_str());
  }
}

void AboutIndexDelegate::updateList(const int index)
{
  // -1: all packages, >0 selected category
  const int catIndex = index - 1;

  if(catIndex < 0)
    m_packagesData = &m_index->packages();
  else
    m_packagesData = &m_index->category(catIndex)->packages();

  for(const Package *pkg : *m_packagesData) {
    const Version *lastVer = pkg->lastVersion();
    const auto_string &name = make_autostring(pkg->displayName());
    const auto_string &version = make_autostring(lastVer->name());
    const auto_string &author = make_autostring(lastVer->displayAuthor());

    m_dialog->list()->addRow({name, version, author});
  }
}

bool AboutIndexDelegate::fillContextMenu(Menu &menu, const int index) const
{
  if(index < 0)
    return false;

  menu.addAction(AUTO_STR("Find in the &browser"), ACTION_FIND_IN_BROWSER);
  menu.addAction(AUTO_STR("About this &package"), ACTION_ABOUT_PKG);

  return true;
}

void AboutIndexDelegate::onCommand(const int id)
{
  switch(id) {
  case ACTION_FIND_IN_BROWSER:
    findInBrowser();
    break;
  case ACTION_ABOUT_PKG:
    aboutPackage();
    break;
  case IDC_ACTION:
    install();
    break;
  }
}

const Package *AboutIndexDelegate::currentPackage() const
{
  const int index = m_dialog->list()->currentIndex();

  if(index < 0)
    return nullptr;
  else
    return m_packagesData->at(index);
}

void AboutIndexDelegate::findInBrowser()
{
  Browser *browser = m_reapack->browsePackages();
  if(!browser)
    return;

  const Package *pkg = currentPackage();
  const string &name = pkg->displayName(m_reapack->config()->browser.showDescs);

  ostringstream stream;
  stream << quoted(name) << ' ' << quoted(m_index->name());
  browser->setFilter(stream.str());
}

void AboutIndexDelegate::aboutPackage()
{
  const Package *pkg = currentPackage();
  m_dialog->setDelegate(make_shared<AboutPackageDelegate>(pkg, m_reapack));
}

void AboutIndexDelegate::install()
{
  enum { INSTALL_ALL = 80, UPDATE_ONLY };

  Menu menu;
  menu.addAction(AUTO_STR("Install all packages in this repository"), INSTALL_ALL);
  menu.addAction(AUTO_STR("Update installed packages only"), UPDATE_ONLY);

  const int choice = menu.show(m_dialog->getControl(IDC_ACTION), m_dialog->handle());

  if(!choice)
    return;

  const Remote &remote = m_reapack->remote(m_index->name());

  if(!remote) {
    // In case the user uninstalled the repository while this dialog was opened
    MessageBox(m_dialog->handle(),
      AUTO_STR("This repository cannot be found in your current configuration."),
      AUTO_STR("ReaPack"), MB_OK);
    return;
  }

  Transaction *tx = m_reapack->setupTransaction();

  if(!tx)
    return;

  m_reapack->enable(remote);

  tx->synchronize(remote, choice == INSTALL_ALL);
  tx->runTasks();
}

AboutPackageDelegate::AboutPackageDelegate(const Package *pkg, ReaPack *reapack)
  : m_package(pkg), m_reapack(reapack),
    m_index(pkg->category()->index()->shared_from_this())
{
}

void AboutPackageDelegate::init(About *dialog)
{
  m_dialog = dialog;

  dialog->setTitle(m_package->displayName());
  dialog->setMetadata(m_package->metadata());
  dialog->setAction("About " + m_index->name());

  dialog->tabs()->addTab({AUTO_STR("History"), {dialog->menu()->handle(), dialog->report()}});
  dialog->tabs()->addTab({AUTO_STR("Contents"), {dialog->menu()->handle(), dialog->list()->handle()}});

  RECT rect;
  GetWindowRect(dialog->list()->handle(), &rect);
  ScreenToClient(dialog->handle(), (LPPOINT)&rect);
  ScreenToClient(dialog->handle(), ((LPPOINT)&rect)+1);

  SetWindowPos(dialog->report(), nullptr, rect.left, rect.top,
    rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOACTIVATE);

  dialog->menu()->addColumn({AUTO_STR("Version"), 142});

  dialog->list()->addColumn({AUTO_STR("File"), 502});
  dialog->list()->addColumn({AUTO_STR("Main"), 50});

  for(const Version *ver : m_package->versions())
    dialog->menu()->addRow({make_autostring(ver->name())});

  dialog->menu()->setSortCallback(0, [&] (const int a, const int b) {
    return m_package->version(a)->compare(*m_package->version(b));
  });

  dialog->menu()->sortByColumn(0, ListView::DescendingOrder);
  dialog->menu()->setSelected(dialog->menu()->rowCount() - 1, true);
}

void AboutPackageDelegate::updateList(const int index)
{
  if(index < 0)
    return;

  const Version *ver = m_package->version(index);
  OutputStream stream;
  stream << *ver;
  SetWindowText(m_dialog->report(), make_autostring(stream.str()).c_str());

  vector<const Source *> uniqueSources;

  const auto &sources = ver->sources();
  for(auto it = sources.begin(); it != sources.end();) {
    const Path &path = it->first;
    const Source *src = it->second;

    m_dialog->list()->addRow({make_autostring(path.join()),
      make_autostring(src->isMain() ? "Yes" : "No")});

    uniqueSources.push_back(src);

    // skip duplicate files
    do { it++; } while(it != sources.end() && path == it->first);
  }

  swap(m_sources, uniqueSources);
}

bool AboutPackageDelegate::fillContextMenu(Menu &menu, const int index) const
{
  if(index < 0)
    return false;

  menu.addAction(AUTO_STR("Copy source URL"), ACTION_COPY_URL);

  return true;
}

void AboutPackageDelegate::onCommand(const int id)
{
  switch(id) {
  case IDC_ACTION:
    m_dialog->setDelegate(make_shared<AboutIndexDelegate>(m_index, m_reapack));
    break;
  case ACTION_COPY_URL:
    copySourceUrl();
    break;
  }
}

void AboutPackageDelegate::copySourceUrl()
{
  const int index = m_dialog->list()->currentIndex();

  if(index < 0)
    return;

  m_dialog->setClipboard(m_sources[index]->url());
}
