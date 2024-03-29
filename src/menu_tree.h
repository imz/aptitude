// menu_tree.h                                      -*-c++-*-
//
//   Copyright (C) 2005, 2007 Daniel Burrows
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
//
// A vs_tree augmented with the ability to act as a menu redirector.

#ifndef MENU_TREE
#define MENU_TREE

#include "menu_redirect.h"

#include <vscreen/vs_editline.h>

#include <vscreen/vs_tree.h>

class pkg_matcher;
class pkg_tree_node;
class undo_group;

/** A vs_tree that can be generically used as a menu redirector.  All
 *  the menu redirection routines are filled in; case analysis on the
 *  currently selected tree item is used to implement them.  In
 *  addition, the tree will pop up a search dialog in response to
 *  either the Search menu command or the corresponding keybinding.
 *
 *  \todo eliminate some of the dynamic_casting by making
 *  pkg_tree_node and solution_item subclasses of menu_redirect; then
 *  you can just cast the active item to a menu_redirect and proxy for
 *  it.
 */
class menu_tree:public vs_tree, public menu_redirect
{
  /** Proxy the given call to the currently selected item, if it
   *  implements the menu_redirect interface.
   */
  bool proxy_redirect(bool (menu_redirect::*)());

  /** Return the selected node, if any, or \b NULL if no node is selected. */
  pkg_tree_node *pkg_node_selection();

  /** A precompiled matcher representing the last search that was performed. */
  pkg_matcher *last_search_matcher;

  /** The string that was compiled to produce the above matcher. */
  std::wstring last_search_term;

  /** If \b true, the last search was a backwards search. */
  bool last_search_backwards;

  /** \b true if an incremental search is in progress. */
  bool doing_incsearch;

  /** The iterator that was selected prior to the incremental search. */
  vs_treeiterator pre_incsearch_selected;

  void do_search(std::wstring s, bool backward);
  void do_incsearch(std::wstring s, bool backward);
  void do_cancel_incsearch();

  /** Execute the given action; this is needed because some "wrapper"
   *  code is used to handle undo and so on.
   */
  bool package_action(void (pkg_tree_node::* action)(undo_group *));

  /** \return \b true if a package or a package version is selected. */
  bool pkg_or_ver_selected();

  static vs_editline::history_list search_history;
protected:
  /** Reset all information about the incremental search.  This must be
   *  performed whenever the root is changed.
   */
  void reset_incsearch();

  menu_tree();
public:
  static ref_ptr<menu_tree> create()
  {
    ref_ptr<menu_tree> rval(new menu_tree);
    rval->decref();
    return rval;
  }

  ~menu_tree();

  /** \return \b true iff a pkg_node descendant is currently selected. */
  bool package_enabled() override;

  /** If a pkg_node is currently selected, execute its "install" operation. */
  bool package_install() override;

  /** If a pkg_node is currently selected, execute its "remove" operation. */
  bool package_remove() override;

  /** If a pkg_node is currently selected, execute its "keep" operation. */
  bool package_keep() override;

  /** If a pkg_node is currently selected, execute its "hold" operation. */
  bool package_hold() override;

  /** If a pkg_node is currently selected, execute its "set auto" operation. */
  bool package_mark_auto() override;

  /** If a pkg_node is currently selected, execute its "set manual" operation. */
  bool package_unmark_auto() override;

  /** \return \b true if a package or a package version is selected. */
  bool package_forbid_enabled() override;

  /** If a package or a version is selected, perform a "forbid"
   *  operation on it.
   */
  bool package_forbid() override;

  /** \return \b true if a package or a package version is selected. */
  bool package_changelog_enabled() override;

  /** If a package or version is selected, show its changelog. */
  bool package_changelog() override;

  /** \return \b true if a package or a package version is selected. */
  bool package_information_enabled() override;

  /** If a package or version is selected, show its information. */
  bool package_information() override;


  /** \return \b true; all package trees know how to search. */
  bool find_search_enabled() override;

  /** \return \b true; all package trees know how to search. */
  bool find_search_back_enabled() override;

  /** Execute the 'search' menu command. */
  bool find_search() override;

  /** Execute the 'search backwards' menu command. */
  bool find_search_back() override;

  /** \return \b true if there is a "previous search". */
  bool find_research_enabled() override;

  /** Execute the 're-search' menu command. */
  bool find_research() override;

  /** \return \b true if there is a "previous search". */
  bool find_repeat_search_back_enabled() override;

  /** Execute the 'repeat search in opposite direction' menu command. */
  bool find_repeat_search_back() override;

  /** \return \b false. */
  bool find_limit_enabled() override;

  /** Does nothing. */
  bool find_limit() override;

  /** \return \b false. */
  bool find_reset_limit_enabled() override;

  /** Does nothing. */
  bool find_reset_limit() override;

  /** \return \b true if this view is active. */
  bool find_broken_enabled() override;

  /** Find the next broken package (searches for '~b'). */
  bool find_broken() override;

  bool handle_key(const key &k) override;
};

typedef ref_ptr<menu_tree> menu_tree_ref;

#endif
