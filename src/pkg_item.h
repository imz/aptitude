// pkg_item.h  -*-c++-*-
//
//  Copyright 1999-2005 Daniel Burrows
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
//  A tree item which represents a package.

#ifndef PKG_ITEM_H
#define PKG_ITEM_H

#include "pkg_node.h"

#include <apt-pkg/pkgcache.h>

#include <sigc++/signal.h>

/** A tree item which represents a package. */
class pkg_item:public pkg_tree_node
{
  pkgCache::PkgIterator package;

  sigc::signal2<void,
		const pkgCache::PkgIterator &,
		const pkgCache::VerIterator &> *info_signal;
  // To be called when we're highlighted or unhighlighted..

  // These perform the given action.  They are idempotent, like dselect's
  // keyboard commands.
  void do_select(undo_group *undo);
  void do_hold(undo_group *undo);
  void do_remove(undo_group *undo);
public:
  class pkg_columnizer;
  static pkgCache::VerIterator visible_version(const pkgCache::PkgIterator &pkg);
  pkg_item(const pkgCache::PkgIterator &_package,
	   sigc::signal2<void,
	   const pkgCache::PkgIterator &,
	   const pkgCache::VerIterator &> *sig);

  virtual void paint(vs_tree *win, int y, bool hierarchical, const style &st) override;

  virtual const wchar_t *tag() override;
  virtual const wchar_t *label() override;
  virtual bool matches(const std::wstring &s) const override;

  virtual void select(undo_group *undo) override;
  virtual void hold(undo_group *undo) override;
  virtual void keep(undo_group *undo) override;
  virtual void remove(undo_group *undo) override;
  virtual void set_auto(bool isauto, undo_group *undo) override;
  virtual void forbid_upgrade(undo_group *undo);

  virtual void highlighted(vs_tree *win) override;
  virtual void unhighlighted(vs_tree *win) override;

  virtual style get_highlight_style() override;
  virtual style get_normal_style() override;

  void show_changelog();
  void show_information();

  const pkgCache::PkgIterator &get_package() const;
  pkgCache::VerIterator visible_version() const;

  bool dispatch_key(const key &k, vs_tree *owner) override;
  void dispatch_mouse(short id, int x, mmask_t bstate, vs_tree *owner) override;

  /** Returns the style that would be used to display the given
   *  package.
   *
   *  \param package the package to generate a style for
   *  \param highlighted if \b true, the package is highlighted
   */
  static style pkg_style(pkgCache::PkgIterator package, bool highlighted);

  // Menu redirections:
  bool package_forbid_enabled() override;
  bool package_forbid() override;
  bool package_changelog_enabled() override;
  bool package_changelog() override;
  bool package_information_enabled() override;
  bool package_information() override;
};

#endif
