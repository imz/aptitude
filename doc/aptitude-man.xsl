<?xml version="1.0" encoding="utf-8"?>

<!-- Magic: -->

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" 
                version="1.0">

<xsl:import href="http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl"/>

<xsl:param name="chunker.output.method" select="'text'"/>
<xsl:param name="chunker.output.encoding" select="'UTF-8'"/>
<xsl:param name="man.charmap.use.subset" select="0"/>

<xsl:output method="text" encoding="UTF-8" indent="no"/>

<xsl:template match='replaceable'>
  <xsl:text>&lt;</xsl:text><xsl:apply-imports/><xsl:text>&gt;</xsl:text>
</xsl:template>


<xsl:template match="literal">
  <xsl:text>\fB</xsl:text><xsl:apply-templates/><xsl:text>\fR</xsl:text>
</xsl:template>

<xsl:param name="preferred.mediaobject.role">text</xsl:param>

</xsl:stylesheet>
