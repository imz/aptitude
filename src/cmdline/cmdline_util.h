// cmdline_util.h                                   -*-c++-*-
//
//   Copyright 2004 Daniel Burrows

#ifndef CMDLINE_UTIL_H
#define CMDLINE_UTIL_H

#include "cmdline_common.h"

// For download_manager::result
#include <generic/apt/download_manager.h>

void cmdline_show_pkglist(pkgvector &items);
void cmdline_show_stringlist(strvector &items);

/** Finds a candidate version for the package using the given source.
 */
pkgCache::VerIterator cmdline_find_ver(pkgCache::PkgIterator pkg,
				       cmdline_version_source source,
				       string sourcestr);

/** Starts up the visual UI in preview mode, and exits with status 0
 *  when the UI shuts down.
 */
void ui_preview();

/** Splits the given input string into a package name/pattern and a
 *  version source.  If the input string is an output string, the
 *  function will still behave sanely.
 *
 *  \param input the string to be split
 *  \param source will be set to the type of source specified
 *  \param package will be set to the package name/pattern
 *  \param sourcestr will be set to the string associated with the source,
 *                   if any
 *
 *  \return \b true if the source was successfully parsed.
 */
bool cmdline_parse_source(const string &input,
			  cmdline_version_source &source,
			  string &package,
			  string &sourcestr);

/** Run the given download and post-download commands using the
 *  standard command-line UI.  Runs the preparation routine, the
 *  actual download, and the post-download commands.
 *
 *  \return the success status of the post-download commands, or
 *  failure if the process failed before they could be run.
 */
download_manager::result cmdline_do_download(download_manager *m);

#endif // CMDLINE_UTIL_H
