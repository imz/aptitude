// menu_redirect.h                                 -*-c++-*-
//
//   Copyright (C) 2004-2005, 2007 Daniel Burrows
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
// This class allows some signals to be "redirected" from the menu to
// appropriate widgets.

#ifndef MENU_REDIRECT_H
#define MENU_REDIRECT_H

#include <sigc++/trackable.h>

class menu_redirect:virtual public sigc::trackable
{
public:
  /** \return \b true if the 'undo' command should be enabled. */
  virtual bool undo_undo_enabled();

  /** Perform the 'undo' command. */
  virtual bool undo_undo();

  /** \return \b true iff the package commands should be enabled. */
  virtual bool package_enabled();

  /** Install the selected package (if any).
   *
   *  \return \b true to stop emission of the signal.
   */
  virtual bool package_install();

  /** Remove the selected package (if any). */
  virtual bool package_remove();

  /** Hold the selected package (if any). */
  virtual bool package_hold();

  /** Keep the selected package (if any). */
  virtual bool package_keep();

  /** Set the "automatic" flag on the selected package (if any). */
  virtual bool package_mark_auto();

  /** Clear the "automatic" flag on the selected package (if any). */
  virtual bool package_unmark_auto();

  /** \return \b true if the selected item can be 'forbidden'. */
  virtual bool package_forbid_enabled();

  /** 'Forbid' the selected item: either forbid the candidate
   *  version of a package, or forbid the selected version.
   */
  virtual bool package_forbid();

  /** \return \b true if the selected item has information to view. */
  virtual bool package_information_enabled();

  /** View a package's information.  Only works on a real package or a
   *  package version.
   */
  virtual bool package_information();

  /** \return \b true if the selected item has a changelog. */
  virtual bool package_changelog_enabled();

  /** View the changelog of the selected package. */
  virtual bool package_changelog();

  /** \return \b true if this view knows how to search. */
  virtual bool find_search_enabled();

  /** Execute the 'search' menu command. */
  virtual bool find_search();

  /** \return \b true if this view knows how to search back. */
  virtual bool find_search_back_enabled();

  /** Execute the 'search back' menu command. */
  virtual bool find_search_back();

  /** \return \b true if this view knows how to search and there is
   *  a "previous search".
   */
  virtual bool find_research_enabled();

  /** Execute the 're-search' menu command. */
  virtual bool find_research();

  /** \return \b true if this view knows how to search and there is
   *  a "previous search".
   */
  virtual bool find_repeat_search_back_enabled();

  /** Execute the 'repeat search in the opposite direction' menu command. */
  virtual bool find_repeat_search_back();

  /** \return \b true if this view knows how to set a display limit. */
  virtual bool find_limit_enabled();

  /** Execute the 'limit' menu command. */
  virtual bool find_limit();

  /** \return \b true if this view knows how to clear a display limit. */
  virtual bool find_reset_limit_enabled();

  /** Execute the 'reset limit' menu command. */
  virtual bool find_reset_limit();

  /** \return \b true if this view knows how to find broken packages. */
  virtual bool find_broken_enabled();

  /** Execute the 'find broken' menu command. */
  virtual bool find_broken();

  virtual ~menu_redirect() {}
};

template<typename T> class ref_ptr;
class vscreen_widget;

/** Bind up all the menu-related signals to the given menu handler.
 *  The resulting bindings will cause menu items to be enabled if (a)
 *  valve is the currently active widget, and (b) the underlying
 *  _enabled test returns \b true.
 */
void create_menu_bindings(menu_redirect *menu_handler,
			  const ref_ptr<vscreen_widget> &valve);

#endif
