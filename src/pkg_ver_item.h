// pkg_ver_item.h (This is -*-c++-*-)
//
//  Copyright 1999-2002, 2004-2005 Daniel Burrows
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
//  Let you create tree nodes which refer to a particular version of a package.

#ifndef PKG_VER_ITEM_H
#define PKG_VER_ITEM_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "apt_info_tree.h"

#include "pkg_node.h"
#include "pkg_grouppolicy.h"
#include "pkg_item_with_subtree.h"

#include <apt-pkg/depcache.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/version.h>

#ifndef HAVE_LIBAPT_PKG3
#define pkgVersionCompare _system->versionCompare
#endif

class pkg_ver_item:public pkg_tree_node
{
  pkgCache::VerIterator version;
  /** Stores the name of this version to avoid problems that can arise
   *  with transcode().
   */
  std::wstring version_name;

  bool show_pkg_name;

  pkg_signal *sig;
public:
  pkg_ver_item(const pkgCache::VerIterator &_version, pkg_signal *_sig,
	       bool _show_pkg_name=false);

  pkgCache::PkgIterator get_package() const {return version.ParentPkg();}
  const pkgCache::VerIterator &get_version() const {return version;}

  virtual style get_normal_style();
  virtual style get_highlighted_style();
  virtual void paint(vs_tree *win, int y, bool hierarchical,
		     const style &st)
    override;

  virtual const wchar_t *tag();
  virtual const wchar_t *label();

  virtual void select(undo_group *undo);
  virtual void hold(undo_group *undo);
  virtual void keep(undo_group *undo);
  virtual void remove(undo_group *undo);
  virtual void set_auto(bool isauto, undo_group *undo);

  virtual void forbid_version(undo_group *undo);

  virtual void highlighted(vs_tree *win);
  virtual void unhighlighted(vs_tree *win);

  void show_information();

  bool dispatch_key(const key &k, vs_tree *owner);

  pkg_ver_item *get_sig();

  static style ver_style(pkgCache::VerIterator version,
			 bool highlighted);


  // Menu redirections:
  bool package_forbid_enabled();
  bool package_forbid();
  bool package_changelog_enabled();
  bool package_changelog();
  bool package_information_enabled();
  bool package_information();
};

class versort:public sortpolicy
{
public:
  inline int compare(const pkg_ver_item *item1, const pkg_ver_item *item2) const
  {
    if(item1->get_version().ParentPkg()!=item2->get_version().ParentPkg())
      return strcmp(item1->get_version().ParentPkg().Name(),
		    item2->get_version().ParentPkg().Name());

    return _system->VS->CmpVersion(item1->get_version().VerStr(), item2->get_version().VerStr());
  }

  bool operator()(vs_treeitem *item1, vs_treeitem *item2)
  {
    // FIXME: this is horrible.
    const pkg_ver_item *pitem1=dynamic_cast<const pkg_ver_item *>(item1);
    const pkg_ver_item *pitem2=dynamic_cast<const pkg_ver_item *>(item2);

    if(pitem1 && pitem2)
      return (compare(pitem1,pitem2)<0);
    else
      return false; // we shouldn't get here!
  }
};

typedef pkg_item_with_subtree<pkg_ver_item, versort> pkg_vertree;
class pkg_vertree_generic:public vs_subtree<pkg_ver_item, versort>
{
  std::wstring name;
public:
  pkg_vertree_generic(std::wstring _name, bool _expanded):vs_subtree<pkg_ver_item, versort>(_expanded),name(_name) {}
  void paint(vs_tree *win, int y, bool hierarchical, const style &st) override
  {paint_text(win, y, hierarchical, name);}
  const wchar_t *tag() {return name.c_str();}
  const wchar_t *label() {return name.c_str();}
};

// The following policy will make each package given to it into a
// subtree containing all versions of the package known to apt-pkg.
class pkg_grouppolicy_ver_factory:public pkg_grouppolicy_factory
{
public:
  virtual pkg_grouppolicy *instantiate(pkg_signal *sig, desc_signal *desc_sig);
};

// The next class displays the available versions of a single package.
// It takes over the screen when instantiated and returns to the previous
// screen when finished.
class pkg_ver_screen:public apt_info_tree
{
protected:
  virtual vs_treeitem *setup_new_root(const pkgCache::PkgIterator &pkg,
				      const pkgCache::VerIterator &ver);
  pkg_ver_screen(const pkgCache::PkgIterator &pkg);
public:
  static ref_ptr<pkg_ver_screen> create(const pkgCache::PkgIterator &pkg)
  {
    ref_ptr<pkg_ver_screen> rval(new pkg_ver_screen(pkg));
    rval->decref();
    return rval;
  }
};

typedef ref_ptr<pkg_ver_screen> pkg_ver_screen_ref;

void setup_package_versions(const pkgCache::PkgIterator &pkg, pkg_vertree *tree, pkg_signal *sig);
void setup_package_versions(const pkgCache::PkgIterator &pkg, pkg_vertree_generic *tree, pkg_signal *sig);
// Adds all versions of a package to the given tree.

#endif
