// pkg_subtree.cc
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
//  (trivial :) ) implementations for pkg_subtree.h

#include "pkg_subtree.h"

#include <generic/apt/apt.h>

#include <vscreen/vs_tree.h>

void pkg_subtree::paint(vs_tree *win, int y, bool hierarchical,
			const style &st)
{
  vs_subtree<pkg_tree_node>::paint(win, y, hierarchical, name);
}

const wchar_t *pkg_subtree::tag()
{
  return name.c_str();
}

const wchar_t *pkg_subtree::label()
{
  return name.c_str();
}

bool pkg_subtree::dispatch_key(const key &k, vs_tree *owner)
{
  if(pkg_tree_node::dispatch_key(k, owner))
    return true;
  else
    return vs_subtree<pkg_tree_node>::dispatch_key(k, owner);
}

void pkg_subtree::select(undo_group *undo)
{
  (*apt_cache_file)->begin_action_group();

  for(child_iterator i=get_children_begin(); i!=get_children_end(); i++)
    (*i)->select(undo);

  (*apt_cache_file)->end_action_group(undo);
}

void pkg_subtree::hold(undo_group *undo)
{
  (*apt_cache_file)->begin_action_group();

  for(child_iterator i=get_children_begin(); i!=get_children_end(); i++)
    (*i)->hold(undo);

  (*apt_cache_file)->end_action_group(undo);
}

void pkg_subtree::keep(undo_group *undo)
{
  (*apt_cache_file)->begin_action_group();

  for(child_iterator i=get_children_begin(); i!=get_children_end(); i++)
    (*i)->keep(undo);

  (*apt_cache_file)->end_action_group(undo);
}

void pkg_subtree::remove(undo_group *undo)
{
  (*apt_cache_file)->begin_action_group();

  for(child_iterator i=get_children_begin(); i!=get_children_end(); i++)
    (*i)->remove(undo);

  (*apt_cache_file)->end_action_group(undo);
}

void pkg_subtree::reinstall(undo_group *undo)
{
  (*apt_cache_file)->begin_action_group();

  for(child_iterator i=get_children_begin(); i!=get_children_end(); i++)
    (*i)->reinstall(undo);

  (*apt_cache_file)->end_action_group(undo);
}

void pkg_subtree::set_auto(bool isauto, undo_group *undo)
{
  (*apt_cache_file)->begin_action_group();

  for(child_iterator i=get_children_begin(); i!=get_children_end(); i++)
    (*i)->set_auto(isauto, undo);

  (*apt_cache_file)->end_action_group(undo);
}

void pkg_subtree::highlighted(vs_tree *win)
{
  if(info_signal)
    (*info_signal)(description);
}

void pkg_subtree::unhighlighted(vs_tree *win)
{
  if(info_signal)
    (*info_signal)(L"");
}
