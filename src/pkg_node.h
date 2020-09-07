// pkg_node.h	-*-c++-*-
//
//  Copyright 1999-2000, 2002, 2005 Daniel Burrows
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
//  In the fine tradition of breaking header files up into their smallest
// usable components..

#ifndef PKG_NODE_H
#define PKG_NODE_H

#include "menu_redirect.h"

#include <vscreen/vs_treeitem.h>

class undo_group;
class keybindings;

class pkg_tree_node:virtual public vs_treeitem, public menu_redirect
// Provides some extra package-related interfaces.
{
  /** Used to convert calls via the menu interface to wrapped
   *  invocations of the low-level action methods.
   */
  bool package_action(void (pkg_tree_node::* action)(undo_group *));
public:
  virtual void select(undo_group *undo)=0;
  virtual void hold(undo_group *undo)=0;
  virtual void keep(undo_group *undo)=0;
  virtual void remove(undo_group *undo)=0;
  // set_auto is idempotent!  No stupid toggling stuff.
  virtual void set_auto(bool isauto, undo_group *undo)=0;

  void mark_auto(undo_group *undo) {set_auto(true, undo);}
  void unmark_auto(undo_group *undo) {set_auto(false, undo);}

  bool dispatch_key(const key &k, vs_tree *owner) override;
  // IMPORTANT NOTE: pkg_tree_node::dispatch_char() does NOT call
  // vs_treeitem::dispatch_char!

  static keybindings *bindings;
  static void init_bindings();

  // Menu redirections:
  bool package_enabled() override;
  bool package_install() override;
  bool package_remove() override;
  bool package_hold() override;
  bool package_keep() override;
  bool package_mark_auto() override;
  bool package_unmark_auto() override;
};

#endif
