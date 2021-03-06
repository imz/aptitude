// download_manager.h                             -*-c++-*-
//
//   Copyright (C) 2005 Daniel Burrows
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
//  An abstract interface for download processes, and two
//  implementations.

#ifndef DOWNLOAD_MANAGER_H
#define DOWNLOAD_MANAGER_H

// For RunResult
#include <apt-pkg/acquire.h>

#include <sigc++/trackable.h>

class OpProgress;
class download_signal_log;

/** The generic interface for a task involving a download and possibly
 *  some post-download operations.  (for instance, downloading and
 *  installing packages)
 */
class download_manager : public sigc::trackable
{
public:
  /** Used to determine what the client should do after a download
   *  completes.
   */
  enum result {success, failure, do_again};

protected:
  /** The object doing the actual download.  Initialized to \b NULL
   *  and deleted in the destructor; it is expected that subclasses
   *  will provide a prepare() routine that sets this to an actual
   *  value.
   */
  pkgAcquire *fetcher;

public:
  download_manager();
  virtual ~download_manager();

  /** Do any preliminary work needed to set up a download.
   *
   *  \param progress a status object used to monitor tasks that
   *                  execute while this method is running.
   *
   *  \param acqlog a pkgAcqStatus object that will be used to do the
   *                actual download.
   *
   *  \param download_signal_log a signal logger that will be used
   *                             to Complete the download.
   *
   *  \return \b true if the preparation succeeded, \b false otherwise
   */
  virtual bool prepare(OpProgress &progress,
		       pkgAcquireStatus &acqlog,
		       download_signal_log *signallog) = 0;

  /** Perform the actual download.  This may execute in a background
   *  thread.
   */
  virtual pkgAcquire::RunResult do_download();

  /** Similar, but with a particular pulse interval. */
  virtual pkgAcquire::RunResult do_download(int PulseInterval);

  /** Perform any post-download tasks that need to be handled.  This
   *  should be called from the main thread.
   *
   *  \param result the result of do_download().
   *
   *  \param progress a progress bar that may be used by the
   *                  finishing process.
   *
   *  \return the result of the post-download operations; if this is
   *  do_again, then the frontend code is expected to repeat the
   *  download and post-download tasks (as many times as needed).
   */
  virtual result finish(pkgAcquire::RunResult result,
			OpProgress &progress) = 0;
};

#endif
