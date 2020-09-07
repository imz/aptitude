// download_screen.h  (this is -*-c++-*- )
//
//  Copyright 1999-2001, 2003-2005 Daniel Burrows
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; see the file COPYING.  If not, write to
//  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
//  Boston, MA 02111-1307, USA.
//
//  This acts as a progress meter for a download.

#ifndef DOWNLOAD_SCREEN_H
#define DOWNLOAD_SCREEN_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <vscreen/vs_table.h>
#include <vscreen/vs_tree.h>
#include <vscreen/vs_subtree.h>

#include <apt-pkg/acquire.h>

class download_item;

class download_tree:public vs_subtree_generic
{
public:
  download_tree():vs_subtree_generic(true) {}

  void paint(vs_tree *win, int y, bool hierarchical, const style &style) override
  {paint_text(win, y, hierarchical, L"ERROR: SHOULD NOT APPEAR");}
  const wchar_t * tag() override {return L"download tree";}
  const wchar_t * label() override {return L"download tree";}
};

class download_screen:public vs_tree, public pkgAcquireStatus
{
  typedef std::map<void *, download_item *> downloadmap;
  downloadmap active_items;
  // Makes it easy to find a currently downloading item when we get a hit
  // for it.

  vscreen_widget *prev;
  // The screen that was being displayed before we started running the
  // download.

  bool finished;
  // If this is true, the status bar will be displayed as usual (as opposed to
  // being a progress meter)

  bool cancelled;
  // True if the user cancelled the download.

  download_tree *contents;

  download_item *get_itm(pkgAcquire::ItemDesc &itmdesc)
  {
    downloadmap::iterator found=active_items.find(itmdesc.Owner);
    eassert(found!=active_items.end());

    return found->second;
  }

protected:
  bool handle_key(const key &k) override;

public:
  download_screen():prev(NULL),finished(false),cancelled(false) {contents=new download_tree; set_root(contents);}

  bool MediaChange(const string &media, const string &drive) override;
  void IMSHit(pkgAcquire::ItemDesc &itmdesc) override;
  void Fetch(pkgAcquire::ItemDesc &itmdesc) override;
  void Done(pkgAcquire::ItemDesc &itmdesc) override;
  void Fail(pkgAcquire::ItemDesc &itmdesc) override;
  bool Pulse(pkgAcquire *Owner) override;
  void Start() override;
  void Stop() override;

  bool get_cursorvisible() override {return false;}

  //void paint_status();

  virtual ~download_screen();
};

#endif
