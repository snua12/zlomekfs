/*
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
*/


function onLoad() {
    try {
        log.profile("onLoad()");

        // Display blackbird logger.
        window.setTimeout(function() {
            try {
                var b_Display = false;
                $("#blackbird").css({ display: (b_Display ? "block" : "none") });
            } catch (err) {
                log.error("onLoad()/timeout[display blackbird]: " + err);
                throw err;
            }
        }, 10);

        // Create a dynamic menu.
        var p_Menu = new DynamicMenu("nav");
            var p_MainMenu = p_Menu.createItem(null, "CLI toolkit", null, "#");
                var p_News = p_Menu.createItem(p_MainMenu, "News", null, "#news");
                var p_Presentation = p_Menu.createItem(p_MainMenu, "Overview", null, "#overview");
                var p_License = p_Menu.createItem(p_MainMenu, "License", null, "#license");
            var p_Changes = p_Menu.createItem(null, "Changes", null, "#changes");
                var p_History = p_Menu.createItem(p_Changes, "History", null, "#history");
                var p_NextChanges = p_Menu.createItem(p_Changes, "Next evolutions", null, "#evolutions");
            var p_Download = p_Menu.createItem(null, "Download", null, "#download");
                var p_Latest = p_Menu.createItem(p_Download, "Latest", null, "#download");
                var p_AllVersions = p_Menu.createItem(p_Download, "All versions", null, "#history");
            var p_Documentation = p_Menu.createItem(null, "Documentation", null, "#");
                var p_UserGuide = p_Menu.createItem(p_Documentation, "User guide", null, "user-guide/cli-user-guide.html");
                var p_Doxygen = p_Menu.createItem(p_Documentation, "Doxygen", null, "doxygen/html/index.html");
                var p_Javadoc = p_Menu.createItem(p_Documentation, "Javadoc", null, "javadoc/html/index.html");
                var p_FAQ = p_Menu.createItem(p_Documentation, "FAQ", null, "user-guide/cli-user-guide.html#faq");
            var p_Contact = p_Menu.createItem(null, "Contact", null, "#contact");
        p_Menu.applyStyles();
        p_Menu.colorMenu(p_MainMenu, "#ff0000"); // red
        p_Menu.colorMenu(p_Changes, "#006600"); // green
        p_Menu.colorMenu(p_Download, "#000066"); // blue
        p_Menu.colorMenu(p_Documentation, "#ff8800"); // orange
        p_Menu.colorMenu(p_Contact, "#880088"); // purple
        p_Menu.animate();
        p_Menu.show();

        // Create an accordion for history changes.
        $("#version-list").accordion({ header: "h4" });

        // Create level 2 tabs for-each cli version.
        $("#version-list .version-details").each(function() {
            var xml_div = this;
            var p_ul = createXmlNode("ul");
            if (xml_div.firstChild) {
                xml_div.insertBefore(p_ul.node, xml_div.firstChild);
            } else {
                xml_div.appendChild(p_ul.node);
            }
        });
        var i_TabCount = 0;
        $("#version-list .version-details h5").each(function() {
            try {
                var p_h5 = new XmlNode(this);
                var p_div = p_h5.parent();
                var p_ul = p_div.child("ul");
                var p_a = p_h5.child("a");
                for (var p_tab = p_h5; p_tab.name() != "div"; p_tab = new XmlNode(p_tab.node.nextSibling)) {}

                var str_TabId = "tab-" + i_TabCount;
                i_TabCount ++;

                p_tab.setAttribute("id", str_TabId);
                p_a.setAttribute("href", "#" + str_TabId);
                var p_li = createXmlNode("li"); {
                    p_h5.node.removeChild(p_a.node);
                    p_li.node.appendChild(p_a.node);
                    p_div.node.removeChild(p_h5.node);
                } p_ul.node.appendChild(p_li.node);
            } catch (err) {
                log.error("for-each('#version-list .version-details h5'): node processing error (" + this + "), " + err);
            }
        });
        $("#history .version-details").tabs();

        // Fix accordion/tabs heights.
        $("#version-list div").css({ height: "auto" });
        $("#version-list *").click(function() { $("#version-list div").css({ height: "auto" }); }); // Remind on every click to avoid vertical sliders.

        // Make download buttons change of color when mouse is over.
        $("p.download").hover(
            function() { $(this).addClass('ui-state-hover'); }, 
            function() { $(this).removeClass('ui-state-hover'); }
        );

        log.profile("onLoad()");
    } catch (err) {
        log.error("onLoad(): " + err); throw err;
    }
}

function mailTo(str_Text) {
    try {
        var v2 = "FDBVQRYZW82SPA2VXBBJVU";
        var v7 = unescape("%27%28%27.8%21w%288AW%21%10%26_71.l%2998");
        var v5 = v2.length;
        var v1 = "";
        for (var v4 = 0; v4 < v5; v4 ++) {
            v1 += String.fromCharCode(v2.charCodeAt(v4) ^ v7.charCodeAt(v4));
        }
        document.write(
            '<a href="javascript:void(0)" onclick="window.location=\'mail\u0074o\u003a'
            + v1
            + '?subject=Email%20Alexis%20Royer'
            + '\'">'
            + str_Text
            + '</a>'
        );
    } catch (err) {
        log.error("mailTo(): " + err); throw err;
    }
}
