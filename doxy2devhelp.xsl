<xsl:stylesheet
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:fo="http://www.w3.org/1999/XSL/Format"
    version="1.0">

<!-- Based on http://bur.st/~eleusis/devhelp/doxy2devhelp.xsl
             (http://bur.st/~eleusis/devhelp/README)
     which is based on http://bugzilla.gnome.org/show_bug.cgi?id=122450
-->

<xsl:output method="xml" version="1.0" indent="yes"/>

<xsl:param name="reference_prefix"></xsl:param>

<xsl:template match="/">
  <book title="Pidgin Documentation"
        name="pidgin"
        link="{$reference_prefix}main.html">
  <chapters>
    <sub name="Modules" link="{$reference_prefix}modules.html">
      <xsl:apply-templates select="doxygenindex/compound[@kind='group']">
        <xsl:sort select="."/>
      </xsl:apply-templates>
    </sub>
    <!-- annotated.html has the short descriptions beside each struct.  is
         that more useful than being grouped alphabetically?
      -->
    <sub name="Structs" link="{$reference_prefix}classes.html">
      <xsl:apply-templates select="doxygenindex/compound[@kind='struct']">
        <xsl:sort select="."/>
      </xsl:apply-templates>
    </sub>
    <!-- This is redundant given Modules -->
    <!--
    <sub name="Directories" link="{$reference_prefix}dirs.html">
      <xsl:apply-templates select="doxygenindex/compound[@kind='dir']">
        <xsl:sort select="."/>
      </xsl:apply-templates>
    </sub>
    -->
    <!-- FIXME: Some files show up here but are broken links; mostly
                files that are under pages...
      -->
    <sub name="Files" link="{$reference_prefix}files.html">
      <xsl:apply-templates select="doxygenindex/compound[@kind='file']">
        <xsl:sort select="."/>
      </xsl:apply-templates>
    </sub>
    <sub name="Signals, HOWTOs, Other" link="{$reference_prefix}pages.html">
      <xsl:apply-templates select="doxygenindex/compound[@kind='page']">
        <xsl:sort select="."/>
      </xsl:apply-templates>
    </sub>
  </chapters>

  <functions>
    <!-- @todo: maybe select only the real functions, ie those with kind=="function"? -->
    <xsl:apply-templates select="doxygenindex/compound/member" mode="as-function"/>
  </functions>
  </book>
</xsl:template>

<xsl:template match="compound">
  <xsl:param name="name"><xsl:value-of select="name"/></xsl:param>
  <xsl:param name="link"><xsl:value-of select="@refid"/>.html</xsl:param>
  <sub name="{$name}" link="{$reference_prefix}{$link}">
  <xsl:apply-templates select="member" mode="as-sub">
    <xsl:sort select="."/>
  </xsl:apply-templates>
  </sub>
</xsl:template>

<xsl:template match="member" mode="as-function">
  <!--
  <function name="atk_set_value" link="atk-atkvalue.html#ATK-SET-VALUE"/>
  -->
  <xsl:param name="name"><xsl:value-of select="name"/></xsl:param>
  <!-- Link is refid attribute of parent element + "#" + diff between refid of parent and own refid -->
  <xsl:param name="refid_parent"><xsl:value-of select="parent::node()/@refid"/></xsl:param>
  <xsl:param name="own_refid"><xsl:value-of select="@refid"/></xsl:param>
  <xsl:param name="offset"><xsl:value-of select="string-length($refid_parent) + 3"/></xsl:param>
  <xsl:param name="ref_diff"><xsl:value-of select="substring($own_refid, $offset, 33)"/></xsl:param>
  <xsl:param name="link"><xsl:value-of select="$refid_parent"/>.html#<xsl:value-of select="$ref_diff"/></xsl:param>
  <function name="{$name}" link="{$reference_prefix}{$link}"/>
</xsl:template>

<xsl:template match="member" mode="as-sub">
  <xsl:param name="name"><xsl:value-of select="name"/></xsl:param>
  <!-- Link is refid attribute of parent element + "#" + diff between refid of parent and own refid -->
  <xsl:param name="refid_parent"><xsl:value-of select="parent::node()/@refid"/></xsl:param>
  <xsl:param name="own_refid"><xsl:value-of select="@refid"/></xsl:param>
  <xsl:param name="offset"><xsl:value-of select="string-length($refid_parent) + 3"/></xsl:param>
  <xsl:param name="ref_diff"><xsl:value-of select="substring($own_refid, $offset, 33)"/></xsl:param>
  <xsl:param name="link"><xsl:value-of select="$refid_parent"/>.html#<xsl:value-of select="$ref_diff"/></xsl:param>
  <sub name="{$name}" link="{$reference_prefix}{$link}"/>
</xsl:template>

</xsl:stylesheet>
