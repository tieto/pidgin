<?xml version='1.0' ?>
<xsl:stylesheet version='2.0' xmlns:xsl='http://www.w3.org/1999/XSL/Transform'>
		<xsl:output
		method="html"
		omit-xml-declaration="yes"
		doctype-public="-//W3C//DTD XHTML 1.0 Strict//EN"
		doctype-system="http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd"
		indent="yes"
		/>
	<xsl:template match='/project'>
		<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en">
			<head>
				<title><xsl:value-of select='@name'/> translation statistics</title>
				<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
				<style type="text/css">
					.bargraph {
						width: 200px;
						height: 20px;
						background-color: red;
						border-collapse: collapse;
						border-spacing: 0px;
						margin: 0px;
						border: 0px;
						padding: 0px;
					}
					.translated { 
						background-color: green; 
						padding: 0px;
					}
					.fuzzy { 
						background-color: blue; 
						padding: 0px;
					}
					.untranslated { 
						background-color: red; 
						padding: 0px;
					}
					td.sep {
						padding-right: 10px;
					}
					th {
						text-align: left;
					}
				</style>
			</head>
			<body>
				<!-- <div id="content"> -->
				<h1><xsl:value-of select='@name' /> translation statistics</h1>
				<table>
					<tr><th>Language</th><th colspan='2'>Translated</th><th colspan='2'>Fuzzy</th><th colspan='2'>Untranslated</th></tr>
					<xsl:for-each select="lang">
						<xsl:sort select='@code' />
						<tr>
							<td><a><xsl:attribute name='href'><xsl:value-of select='@code'/>.po</xsl:attribute><xsl:value-of select='@name'/> (<xsl:value-of select='@code'/>)</a></td>
							<td><xsl:value-of select='@translated'/></td>
							<td class='sep'><xsl:value-of select="format-number(@translated div ../@strings * 100,'#.##')"/> %</td>
							<td><xsl:value-of select='@fuzzy'/></td>
							<td class='sep'><xsl:value-of select="format-number(@fuzzy div ../@strings * 100,'#.##')"/> %</td>
							<td><xsl:value-of select='../@strings - (@translated + @fuzzy)'/></td>
							<td><xsl:value-of select="format-number((../@strings - (@translated + @fuzzy)) div ../@strings * 100,'#.##')"/> %</td>
						<td>
							<table class='bargraph'><tr>
									<td class="translated"><xsl:attribute name='style'>width:<xsl:value-of select='round(@translated div ../@strings * 200)'/>px;</xsl:attribute></td>
									<td class="fuzzy"><xsl:attribute name='style'>width:<xsl:value-of select='round(@fuzzy div ../@strings * 200)'/>px;</xsl:attribute></td>
									<td class="untranslated"><xsl:attribute name='style'>width:<xsl:value-of select='round((../@strings - @translated - @fuzzy) div ../@strings * 200)'/>px;</xsl:attribute></td>
							</tr></table>
						</td>
						</tr>
					</xsl:for-each>
				</table>
				<p><a><xsl:attribute name='href'><xsl:value-of select='@pofile'/></xsl:attribute><xsl:value-of select='@pofile'/></a> generated on <xsl:value-of select='@generated'/></p>
				<!-- </div> -->
			</body>
		</html>
	</xsl:template>
</xsl:stylesheet>
