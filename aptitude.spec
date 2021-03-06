# vim: set ft=spec: -*- rpm-spec -*-

Name: aptitude
Version: 0.4.5
Release: alt3

Summary: Terminal-based apt frontend
Group: System/Configuration/Packaging
License: GPL
Url: http://people.debian.org/~dburrows/aptitude
Packager: Sir Raorn <raorn@altlinux.ru>

# git://git.altlinux.org/people/raorn/packages/%name.git
Source: %name-%version.tar
Patch: %name-%version-%release.patch

Requires: %{get_dep libapt}

BuildRequires(pre): libapt-devel >= 0.5.15cnc5-alt1

# Automatically added by buildreq on Sat Oct 07 2006
BuildRequires: cppunit-devel docbook-dtds docbook-style-xsl gcc-c++ html2text libapt-devel libncursesw-devel libsigc++2.0-devel xsltproc

%description
aptitude is a terminal-based apt frontend.  This is a heavily
emasculated version of Debian software, besause some features
can't be ported to apt-rpm.

%package doc
Summary: English manual for aptitude, a terminal-based apt frontend
Group: Books/Computer books
Conflicts: %name < %version-%release
Conflicts: %name > %version-%release

%description doc
aptitude is a terminal-based apt frontend.  This package contains
the English version of the aptitude user's manual in HTML format.

%prep
%setup -q
%patch -p1

%build
# gettext uses mkinstalldirs...
touch mkinstalldirs
# STFU, automake!
touch ChangeLog
%autoreconf

%configure \
  --disable-werror \
  --disable-docs
%make_build
%make_build -C doc/en

%install
mkdir -p %buildroot%_localstatedir/%name
%make_install DESTDIR=%buildroot install
rm -rf %buildroot%_mandir

%make_install DESTDIR=%buildroot -C doc/en install-man

%find_lang %name

%files -f %name.lang
%doc AUTHORS FAQ NEWS README* TODO %name-hackers-guide.txt
%_bindir/*
%_datadir/%name
%_man8dir/%name.8*
%_localstatedir/*

%files doc
%doc doc/en/output-html/*

%changelog
* Sun Apr 26 2009 Alexey I. Froloff <raorn@altlinux.org> 0.4.5-alt3
- Fixed segfault when Aptitude::UI::Minibuf-Prompts set to true
- Fixed "reinstall" action, disabled UI reinstall (closes: #17164)

* Fri Dec 12 2008 Dmitry V. Levin <ldv@altlinux.org> 0.4.5-alt2.1
- Fixed build with g++ 4.3.x.

* Wed Aug 13 2008 Sir Raorn <raorn@altlinux.ru> 0.4.5-alt2
- New option "Aptitude::UI::Disable-Mouse" - ignore mouse
  events (for use with b0rken terminals)
- Support for command-line package file installation
- Better "autoinstalled" status assignment for packages that
  obsolete other packages

* Sat May 17 2008 Sir Raorn <raorn@altlinux.ru> 0.4.5-alt1
- [0.4.5]
- Fix CLI "update" and disable UI "UpdatePackageList" (closes: #10204)
- Use libapt-pkg when calculating dist-upgrade instead of dburrows-invented
  square-wheeled bicycle (this means you can use aptitude dist-upgrade and
  get sane results)

* Thu Dec 14 2006 Sir Raorn <raorn@altlinux.ru> 0.4.4-alt1
- [0.4.4]

* Sat Oct 14 2006 Sir Raorn <raorn@altlinux.ru> 0.4.1-alt1.1
- Rebuilt with new apt

* Sat Oct 07 2006 Sir Raorn <raorn@altlinux.ru> 0.4.1-alt1
- [0.4.1]
- Updated build requires
- All pathes merged into GIT repository
- Removed some of aptitude functionality:
 + changelog viewer (unusable outside Debian)
 + "Tasks" support (unusable outside Debian)
 + "Tags" support (unusable outside Debian)
 + hierarchy support (unusable outside Debian)
 + "Audit recommendations" support (not applicable for RPM)
 + "Become root" functionality (closes: #7332)
 + trust checks (not available in apt < 0.6)
 + "purge" support (not applicable for RPM)
 + "Problem Resolver" because it only generates problems
- Changed some defaults:
 + default group policy is "status,section(none)"
 + "section" group method set to "groups" concept, not
   "sections" (not applicable for RPM)
 + "Security updates" are coming from "updates" component,
   not "securyty.debian.org" site
- Closes: #7333 (fixed in upstream)

* Fri Apr 14 2006 Sir Raorn <raorn@altlinux.ru> 0.3.1deb3-alt2
- Rebuilt with new apt
- Fixed --as-needed link

* Sun Jan 30 2005 Sir Raorn <raorn@altlinux.ru> 0.3.1deb3-alt1
- [0.3.1-3]
- Match Description parsing with apt-rpm's LongDesc format
- Put packages coming from updates.altlinux.org into "Security updates" section
- Enabled documentation (manpage and user's manual)
- Merge package descriptions from debian/control
- Updated build requires

* Sat Jan 29 2005 Sir Raorn <raorn@altlinux.ru> 0.2.15.8deb1-alt2.1
- Rebuilt with new libapt-pkg

* Thu Jan 20 2005 Dmitry V. Levin <ldv@altlinux.org> 0.2.15.8deb1-alt2
- Fixed compilation issues detected by g++-3.4.3.

* Fri Oct 08 2004 Sir Raorn <raorn@altlinux.ru> 0.2.15.8deb1-alt1
- [0.2.15.8-1]
- Do not hide Maintainer and Source package from package wiew.

* Mon Sep 20 2004 Sir Raorn <raorn@altlinux.ru> 0.2.15.7deb1-alt1
- [0.2.15.7-1]
- cnc-rpm patch updated from conectiva (thanx, boiko)
- Documentation removed (totally b0rken)

* Thu Jul 15 2004 Sir Raorn <raorn@altlinux.ru> 0.2.15.2deb1-alt1
- [0.2.15.2-1]

* Thu May 13 2004 ALT QA Team Robot <qa-robot@altlinux.org> 0.2.13-alt2.1
- Rebuilt with libapt-0.5.15cnc6-alt1.

* Fri Jan 16 2004 Dmitry V. Levin <ldv@altlinux.org> 0.2.13-alt2
- Updated rpm-apt patch.
- Fixed build.
- Rebuilt with apt-0.5.15cnc5.

* Sat Oct 11 2003 Sir Raorn <raorn@altlinux.ru> 0.2.13-alt1
- New maintainer
- [0.2.13]
- Removed patches:
  + alt-gcc3
- Updated patches:
  + alt-cnc-apt-rpm (was: cnc-rpm)
- Default package grouping policy set to
  "filter(missing),status,section(none,nopassthrough)"
- Updated buildreqs

* Wed Dec 04 2002 Dmitry V. Levin <ldv@altlinux.org> 0.2.11.1-alt1
- Updated to aptitude-0.2.11.1-2cl.

* Wed Sep 11 2002 Stanislav Ievlev <inger@altlinux.ru> 0.2.8-alt3
- rebuld with gcc3

* Wed Mar 27 2002 Dmitry V. Levin <ldv@alt-linux.org> 0.2.8-alt2
- Added librpm-4.0.4 build support.
- Built with librpm-4.0.4, updated buildrequires.

* Fri Mar 22 2002 Rider <rider@altlinux.ru> 0.2.8-alt1
- 0.2.8 with Conectiva patches.

* Tue Nov 06 2001 Dmitry V. Levin <ldv@alt-linux.org> 0.0.7.13-ipl11mdk
- Initial build with rpm4.

* Thu Aug 09 2001 Dmitry V. Levin <ldv@altlinux.ru> 0.0.7.13-ipl10mdk
- Corrected requires and buildrequires lists.
- Reworked compilation options again: we add only '-fno-exceptions' now.

* Wed Aug 08 2001 Dmitry V. Levin <ldv@altlinux.ru> 0.0.7.13-ipl9mdk
- Added %%optflags_nocpp to compilation options.
- Rebuilt with apt-0.3.19cnc52-alt2, corrected apt requires.

* Thu Jul 26 2001 Alexander Bokovoy <ab@altlinux.ru> 0.0.7.13-ipl8mdk
- rebuild against apt-get cnc51

* Sun Apr 14 2001 Alexander Bokovoy <ab@avilink.net> 0.0.7.13-ipl7mdk
- rebuild against apt-get cnc38

* Tue Feb 20 2001 Alexander Bokovoy <ab@avilink.net> 0.0.7.13-ipl6mdk
- Wrong group fixed

* Mon Feb 19 2001 Alexander Bokovoy <ab@avilink.net> 0.0.7.13-ipl5mdk
- rebuild against apt-get cnc36

* Tue Jan 24 2001 Alexander Bokovoy <ab@avilink.net> 0.0.7.13-ipl4mdk
- Incompabilities between libapt-pkg and aptitude sources fixed

* Tue Jan 22 2001 Alexander Bokovoy <ab@avilink.net> 0.0.7.13-ipl3mdk
- New upstream version
- rebuild with new apt

* Tue Jan 09 2001 Dmitry V. Levin <ldv@fandra.org> 0.0.7.6-ipl2mdk
- Specfile cleanup.

* Sun Dec 02 2000 Alexander Bokovoy <ab@avilink.net>
- MDK RE-fication
- Patch for incorrect class cast under gcc 2.96

* Mon Oct 16 2000 Claudio Matsuoka <claudio@conectiva.com>
- new upstream release: 0.0.7.6

* Fri Oct 13 2000 Claudio Matsuoka <claudio@conectiva.com>
- new upstream release: 0.0.7.4
- minor RPM cleanups
- specfile fixes

* Thu Oct 12 2000 Claudio Matsuoka <claudio@conectiva.com>
- added missing help file, manpage
- changed /var/state/aptitude to FHS-compliant /var/lib/aptitude

* Tue Oct 10 2000 Claudio Matsuoka <claudio@conectiva.com>
- upgraded to 0.0.7.3
- some RPM cleanup
- package quality: 50-60%%

* Mon Oct  2 2000 Claudio Matsuoka <claudio@conectiva.com>
- package created
