# automake doesn't play nicely with DocBook :(

SUBDIRS=images
EXTRA_DIST=aptitude-html-ja.xsl aptitude-man-ja.xsl

LANGCODE=ja
MANPAGE=aptitude.$(LANGCODE).8
# Automake automagically distributes README without asking..
README=README.$(LANGCODE)
XMLSOURCES=aptitude.xml manpage.xml

# Put documentation in /usr/share/doc/aptitude (by default)
docdir=$(datadir)/doc/$(PACKAGE)
localemandir=$(mandir)/$(LANGCODE)
htmldir=$(DESTDIR)$(docdir)/html/$(LANGCODE)

IMAGES = $(wildcard images/*.png)

if USE_PO4A
all-local: doc-stamp
else
all-local:
endif

clean-local:
	-rm -fr output-html/ output-txt/ output-man/
	-rm -f doc-stamp doc-css-stamp doc-html-stamp doc-txt-stamp doc-man-stamp
	-rm -fr $(MANPAGE) $(README)

install-data-hook:
	$(mkinstalldirs) $(DESTDIR)$(localemandir)/man8
	$(INSTALL_DATA) $(MANPAGE) $(DESTDIR)$(localemandir)/man8/aptitude.8

	$(mkinstalldirs) $(htmldir)/images
	$(INSTALL_DATA) output-html/*.html output-html/*.css $(htmldir)
	$(INSTALL_DATA) output-html/images/* $(htmldir)/images

#	$(INSTALL_DATA) $(README) $(DESTDIR)$(pkgdatadir)/$(README)

doc-stamp: doc-html-stamp doc-css-stamp $(README) $(MANPAGE)
	touch doc-stamp

aptitude.xml: $(srcdir)/../en/aptitude.xml $(srcdir)/../po4a/po/$(LANGCODE).po 
	po4a-translate -M utf-8 -f docbook -m $(srcdir)/../en/aptitude.xml -p $(srcdir)/../po4a/po/$(LANGCODE).po -l aptitude.xml -k 50

manpage.xml: $(srcdir)/../en/manpage.xml $(srcdir)/../po4a/po/$(LANGCODE).po
	po4a-translate -M utf-8 -f docbook -m $(srcdir)/../en/manpage.xml -p $(srcdir)/../po4a/po/$(LANGCODE).po -l manpage.xml

db2latex: doc-db2latex-stamp

$(MANPAGE): $(XMLSOURCES) aptitude-man-$(LANGCODE).xsl $(srcdir)/../aptitude-man.xsl
	-rm -fr output-man
	xsltproc -o output-man/aptitude.8 aptitude-man-$(LANGCODE).xsl $(srcdir)/aptitude.xml
	mv output-man/aptitude.8 $(MANPAGE)

$(README): $(XMLSOURCES) $(srcdir)/../aptitude-txt.xsl $(srcdir)/../aptitude-common.xsl $(srcdir)/../aptitude-txt.style
	-rm -fr output-txt/
	xsltproc -o output-txt/index.html $(srcdir)/../aptitude-txt.xsl $(srcdir)/aptitude.xml
	html2text -width 80 -ascii -nobs -rcfile $(srcdir)/../aptitude-txt.style output-txt/index.html | sed 's/&#8212/--/g' > $(README)

doc-db2latex-stamp: output-db2latex/aptitude.tex $(IMAGES)
	ln -s ../images output-db2latex/images
# Run pdflatex 3 times just to be extra sure that it converges.
	cd output-db2latex && pdflatex -interaction nonstopmode aptitude.tex && pdflatex -interaction nonstopmode aptitude.tex && pdflatex -interaction nonstopmode aptitude.tex
	touch doc-db2latex-stamp

output-db2latex/aptitude.tex: $(XMLSOURCES) $(srcdir)/../aptitude-db2latex.xsl $(srcdir)/../aptitude-common.xsl
	-rm -fr output-db2latex
	mkdir output-db2latex
	xsltproc -o output-db2latex/aptitude.tex $(srcdir)/../aptitude-db2latex.xsl $(srcdir)/aptitude.xml


doc-css-stamp: doc-html-stamp $(srcdir)/../aptitude.css
	rm -f output-html/aptitude.css
	cp $(srcdir)/../aptitude.css output-html/
	touch doc-css-stamp

doc-html-stamp: $(XMLSOURCES) aptitude-html-$(LANGCODE).xsl $(srcdir)/../aptitude-common.xsl $(IMAGES)
	-rm -fr output-html/

	xsltproc -o output-html/ aptitude-html-$(LANGCODE).xsl $(srcdir)/aptitude.xml

	mkdir output-html/images/
	ln -f $(srcdir)/images/*.png output-html/images/
	for x in caution important note tip warning; do ln -s /usr/share/xml/docbook/stylesheet/nwalsh/images/$$x.png output-html/images/; done
	for x in home next prev up; do ln -s /usr/share/xml/docbook/stylesheet/nwalsh/images/$$x.gif output-html/images/; done

	touch doc-html-stamp
