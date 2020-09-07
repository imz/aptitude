// cmine.h     -*-c++-*-
//
//  Copyright 2000 Daniel Burrows
//
//  A Curses interface to Minesweeper, using the vscreen architecture.  The
// idea is that it'll be possible to drop this into a vscreen-using program
// and have it Just Work.  Currently vscreen is (aside from the manually
// extracted code in this tree) only used in aptitude, but if I ever get
// around to breaking it into its own library, this'll be useful.  Also, as
// Caldera demonstrated, playing games while your packages are downloading has
// a high reading on the Cool Useless Feature Scale :-)
//
//  (that's also the reason that there's room made for some stuff that's not
// really accessible from the version I'm turning in -- in particular,
// configuration of colors and keybindings.  To make use of them you really
// need a configuration file, and I don't want to write a parser here..aptitude
// just piggybacks on the apt configuration, so I don't really have a parser
// from that that I can borrow either..hmmm..I think the apt parser is
// public-domain, but I'm not sure how that interacts with our collaboration
// policy :) )

#ifndef CMINE_H
#define CMINE_H

#include "board.h"

#include <vscreen/vs_minibuf_win.h>
#include <vscreen/vs_editline.h>

class vs_radiogroup;
class keybindings;

class cmine:public vscreen_widget
{
  //vs_multiplex *status_multiplex;
  mine_board *board;
  int curx, cury; // Cursor location
  int basex, basey; // Where the board starts relative to the window.
  int prevwidth, prevheight;
  // The width and height last time we adjusted basex,basey..needed at the
  // moment, I'm looking for a cleaner solution.
  int timeout_num;

  static vs_editline::history_list load_history, save_history;

  void set_board(mine_board *_board);
  // Deletes the old board, and replaces it with the new one.  cur[xy] and
  // base[xy] are set so that the board appears in the center of the screen,
  // with the cursor approxiomately in its center.
  void paint_square(int x, int y, const style &st);

  void checkend();
  // Checks whether the game is over, prints cute message if so.

  class update_header_event;
  friend class update_header_event;

  void update_header();
  // Updates the displayed header information (time, etc)

  void do_load_game(std::wstring s);
  void do_save_game(std::wstring s);
  void do_new_game();
  void do_continue_new_game(bool start,
			    vscreen_widget &w,
			    vs_radiogroup *grp);
  void do_custom_game();
  void do_start_custom_game(vscreen_widget &w,
			    vs_editline &heightedit,
			    vs_editline &widthedit,
			    vs_editline &minesedit);
protected:
  void paint_header(const style &st);

  cmine();
public:
  static ref_ptr<cmine> create()
  {
    ref_ptr<cmine> rval(new cmine);
    rval->decref();
    return rval;
  }

  bool handle_key(const key &k) override;
  void paint(const style &st) override;
  ~cmine()
  {
    delete board;
    vscreen_deltimeout(timeout_num);
  }

  int width_request() override;
  int height_request(int w) override;

  bool get_cursorvisible() override {return false;}
  point get_cursorloc() override {return point(0,0);}
  bool focus_me() override {return true;}

  static keybindings *bindings;
  static void init_bindings();
};

#endif
