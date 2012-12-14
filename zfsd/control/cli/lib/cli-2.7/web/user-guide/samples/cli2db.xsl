<?xml version="1.0" encoding="iso-8859-1"?>

<!--
    Copyright (c) 2006-2011, Alexis Royer, http://alexis.royer.free.fr/CLI

    All rights reserved.

    Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

        * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
        * Neither the name of the CLI library project nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
    CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
    EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
    PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
    PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
    LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
    NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
-->


<xsl:stylesheet
        version="1.0"
        xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
        xmlns:cli="http://alexis.royer.free.fr/CLI"
        xmlns:db="http://docbook.org/docbook-ng">
<xsl:output method="xml" encoding="iso-8859-1"/>


<xsl:variable name="STR_Endl"><xsl:text>
</xsl:text></xsl:variable>


<xsl:template match="/">
    <db:programlisting>
    <xsl:text>&lt;?xml version="1.0" encoding="ISO-8859-1"?&gt;</xsl:text><xsl:value-of select="$STR_Endl"/>
    <xsl:apply-templates/>
    </db:programlisting>
</xsl:template>

<xsl:template match="cli:endl">
    <!-- Start the 'endl' element -->
    <xsl:call-template name="T_Indent"/>
    <xsl:text>&lt;endl</xsl:text><xsl:apply-templates select="@*"/><xsl:text>&gt;</xsl:text>
    <!-- Display native code -->
    <xsl:if test="cli:cpp or cli:java or cli:menu"><xsl:value-of select="$STR_Endl"/></xsl:if>
    <xsl:for-each select="cli:cpp|cli:java|comment()"><xsl:apply-templates select="."/></xsl:for-each>
    <!-- Display menus and menu references -->
    <xsl:if test="(cli:cpp or cli:java) and cli:menu"><xsl:value-of select="$STR_Endl"/></xsl:if>
    <xsl:for-each select="cli:menu"><xsl:apply-templates select="."/></xsl:for-each>
    <!-- Finish the 'endl' element -->
    <xsl:if test="cli:cpp or cli:java or cli:menu"><xsl:call-template name="T_Indent"/></xsl:if>
    <xsl:text>&lt;/endl&gt;</xsl:text><xsl:value-of select="$STR_Endl"/>
</xsl:template>

<xsl:template match="cli:help">
    <xsl:text>&lt;help</xsl:text>
        <xsl:apply-templates select="@*"/>
        <xsl:text>&gt;</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>&lt;/help&gt;</xsl:text>
</xsl:template>

<xsl:template match="cli:cpp|cli:java">
    <xsl:call-template name="T_Indent"/>

    <xsl:text>&lt;</xsl:text><xsl:value-of select="local-name()"/>
        <xsl:apply-templates select="@*"/>
        <xsl:text>&gt;</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>&lt;/</xsl:text><xsl:value-of select="local-name()"/><xsl:text>&gt;</xsl:text>

    <xsl:value-of select="$STR_Endl"/>
</xsl:template>

<xsl:template match="cli:out">
    <xsl:text>&lt;out/&gt;</xsl:text>
</xsl:template>

<xsl:template match="cli:err">
    <xsl:text>&lt;err/&gt;</xsl:text>
</xsl:template>

<xsl:template match="cli:value-of">
    <xsl:text>&lt;value-of</xsl:text>
    <xsl:apply-templates select="@*"/>
    <xsl:text>/&gt;</xsl:text>
</xsl:template>

<xsl:template match="cli:tag[@ref]">
    <xsl:call-template name="T_Indent"/>
    <xsl:text>&lt;tag</xsl:text>
    <xsl:apply-templates select="@*"/>
    <xsl:text>/&gt;</xsl:text>
    <xsl:value-of select="$STR_Endl"/>
</xsl:template>

<xsl:template match="cli:menu[@ref]">
    <xsl:text>&lt;menu</xsl:text>
    <xsl:apply-templates select="@*"/>
    <xsl:text>&gt;</xsl:text>
</xsl:template>

<xsl:template match="cli:*">
    <xsl:if test="self::cli:menu and (count(preceding-sibling::cli:*) &gt; count(preceding-sibling::cli:help))">
        <xsl:value-of select="$STR_Endl"/>
    </xsl:if>
    <xsl:if test="self::cli:handler and not(preceding-sibling::cli:handler) and (count(preceding-sibling::cli:*) &gt; count(preceding-sibling::cli:help))">
        <xsl:value-of select="$STR_Endl"/>
    </xsl:if>

    <xsl:call-template name="T_Indent"/>
    <xsl:text>&lt;</xsl:text>
        <xsl:value-of select="local-name(.)"/>
        <xsl:if test=".=/">
            <xsl:text> xmlns="http://alexis.royer.free.fr/CLI"</xsl:text>
        </xsl:if>
        <xsl:apply-templates select="@*"/>
        <xsl:text>&gt;</xsl:text>
        <xsl:apply-templates select="cli:help"/>
        <xsl:value-of select="$STR_Endl"/>
    <xsl:apply-templates select="cli:*[not(self::cli:help)]"/>
    <xsl:call-template name="T_Indent"/>
        <xsl:text>&lt;/</xsl:text>
        <xsl:value-of select="local-name(.)"/>
        <xsl:text>&gt;</xsl:text>
        <xsl:value-of select="$STR_Endl"/>
</xsl:template>

<xsl:template match="comment()">
    <!-- Do not output copyright comments. -->
    <xsl:if test="not(contains(.,'Copyright'))">
        <xsl:call-template name="T_Indent"/>
            <xsl:text>&lt;!--</xsl:text>
            <xsl:value-of select="."/>
            <xsl:text>--&gt;</xsl:text>
            <xsl:value-of select="$STR_Endl"/>
    </xsl:if>
</xsl:template>

<xsl:template match="@*">
    <xsl:text> </xsl:text>
    <xsl:value-of select="local-name(.)"/>
    <xsl:text>="</xsl:text>
    <xsl:value-of select="."/>
    <xsl:text>"</xsl:text>
</xsl:template>

<xsl:template name="T_Indent">
    <xsl:if test="not(.=/)">
        <xsl:text> </xsl:text>
        <xsl:for-each select="parent::cli:*">
            <xsl:call-template name="T_Indent"/>
        </xsl:for-each>
    </xsl:if>
</xsl:template>

</xsl:stylesheet>
