// vs_passthrough.h                -*-c++-*-
//
//  A widget that by default passes focus and cursor handling through to
// a "currently focussed" widget.

#ifndef VS_PASSTHROUGH_H
#define VS_PASSTHROUGH_H

#include "vs_container.h"

class vs_passthrough:public vs_container
{
  void gained_focus();
  void lost_focus();

protected:
  virtual bool handle_key(const key &k) override;

  // These call focussed() and unfocussed() on the result of get_focus().
  // (convenience methods)
  //
  // Provided to make it easier to manage focus simply.
  void defocus();
  void refocus();

protected:
  vs_passthrough();

public:
  // Returns the currently focussed widget, if any.
  virtual vs_widget_ref get_focus()=0;

  vs_widget_ref get_active_widget() override;

  virtual void dispatch_mouse(short id, int x, int y, int z, mmask_t bstate) override;

  virtual bool focus_me() override;
  virtual bool get_cursorvisible() override;
  virtual point get_cursorloc() override;
};

typedef ref_ptr<vs_passthrough> vs_passthrough_ref;

#endif
