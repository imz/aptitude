// cmdline_simulate.h               -*-c++-*-
//
//  Copyright 2004 Daniel Burrows

#ifndef CMDLINE_SIMULATE_H
#define CMDLINE_SIMULATE_H

#include "cmdline_common.h"

/** Simulate an install run.  to_install and friends are meant to be
 *  the sets the user explicitly selected (so the prompt can be
 *  displayed only when extra stuff is added or removed).
 *
 *  \param as_upgrade currently ignored; meant to control how the
 *                    preview and prompting are handled.
 *  \param to_install a set of packages to install.
 *  \param to_hold a set of packages to hold.
 *  \param to_remove a set of packages to remove.
 *  \param showvers \b true to show version information in the preview.
 *  \param showdeps \b true to show dependency information in the preview.
 *  \param showsize \b true to show size information in the preview.
 *  \param verbose the current verbosity level
 *  \param assume_yes if \b true, assume the user entered "yes"
 *                    at the prompt.
 *  \param fornce_no_change if \b true, make an effort to avoid
 *                          undoing the user's explicit requests as
 *                          given in to_install et al.
 */

int cmdline_simulate(bool as_upgrade,
		     pkgset &to_install, pkgset &to_hold, pkgset &to_remove,
		     bool showvers, bool showdeps, bool showsize,
		     bool always_prompt, int verbose,
		     bool assume_yes, bool force_no_change);


#endif // CMDLINE_SIMULATE_H
