# automake doesn't play nicely with DocBook :(

SUBDIRS=images

EXTRA_DIST=aptitude.xml

LANGCODE=cs

# Put documentation in /usr/share/doc/aptitude (by default)
docdir=$(datadir)/doc/$(PACKAGE)
htmldir=$(docdir)/html/$(LANGCODE)

localemandir=$(mandir)/$(LANGCODE)

README=README.$(LANGCODE)
MANPAGE=aptitude.$(LANGCODE).8

IMAGES = $(wildcard images/*.png)

all-local: doc-stamp

clean-local:
	-rm -fr output-html/ output-txt/ output-man/
	-rm -f doc-stamp doc-css-stamp doc-html-stamp doc-txt-stamp doc-man-stamp
	-rm -fr $(MANPAGE) $(README)

# pkgdata_DATA=$(README)

install-data-hook:
	$(mkinstalldirs) $(DESTDIR)$(localemandir)/man8
	$(INSTALL_DATA) $(MANPAGE) $(DESTDIR)$(localemandir)/man8/aptitude.8

	$(mkinstalldirs) $(DESTDIR)$(htmldir)/images
	$(INSTALL_DATA) output-html/*.html output-html/*.css $(DESTDIR)$(htmldir)
	$(INSTALL_DATA) output-html/images/* $(DESTDIR)$(htmldir)/images

doc-stamp: doc-html-stamp doc-css-stamp $(README) $(MANPAGE)
	touch doc-stamp

$(MANPAGE): aptitude.xml $(srcdir)/../aptitude-man.xsl
	-rm -fr output-man
	xsltproc -o output-man/aptitude.8 $(srcdir)/../aptitude-man.xsl $(srcdir)/aptitude.xml
	mv output-man/aptitude.8 $(MANPAGE)

$(README): aptitude.xml $(srcdir)/../aptitude-txt.xsl $(srcdir)/../aptitude-common.xsl ../aptitude-txt.style
	-rm -fr output-txt/
	xsltproc -o output-txt/index.html $(srcdir)/../aptitude-txt.xsl $(srcdir)/aptitude.xml
	html2text -width 80 -ascii -nobs -rcfile $(srcdir)/../aptitude-txt.style output-txt/index.html | sed 's/&#8212/--/g' > $(README)

doc-css-stamp: doc-html-stamp $(srcdir)/../aptitude.css
	rm -f output-html/aptitude.css
	cp $(srcdir)/../aptitude.css output-html/
	touch doc-css-stamp

doc-html-stamp: aptitude.xml ../aptitude-html.xsl ../aptitude-common.xsl $(IMAGES)
	-rm -fr output-html/

	xsltproc -o output-html/ $(srcdir)/../aptitude-html.xsl $(srcdir)/aptitude.xml

	mkdir output-html/images/
	ln -f $(srcdir)/images/*.png output-html/images/
	for x in caution important note tip warning; do cp -p /usr/share/xml/docbook/xsl-stylesheets/images/$$x.png output-html/images/; done
	for x in home next prev up; do cp -p /usr/share/xml/docbook/xsl-stylesheets/images/$$x.gif output-html/images/; done

	touch doc-html-stamp
