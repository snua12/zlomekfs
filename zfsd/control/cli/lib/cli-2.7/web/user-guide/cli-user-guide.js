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


//! @brief Dynamic menu for the user-guide.
function UserGuideMenu() {

    var userGuideMenu = this;

    //! Generic dynamic menu reference.
    this.menu = null;
    //! Menu item receiving the table of content.
    this.toc = null;
    //! Menu item receiving example references.
    this.examples = null;
    //! Menu item receiving figure references.
    this.figures = null;
    //! Menu item receiving table references.
    this.tables = null;
    //! Manu item receiving document changes.
    this.changes = null;

    //! @brief Create main menu items
    this.create = function() {
        try {
            log.profile("UserGuideMenu.create()");

            userGuideMenu.menu = new DynamicMenu("nav");

            function reStyle() {
                userGuideMenu.menu.applyStyles();
                userGuideMenu.menu.animate();
                userGuideMenu.applyMoreStyles();
            }

            // The main items of the menu are created right away, but sub-items are populated on mouseover event.
            userGuideMenu.toc = userGuideMenu.menu.createItem(null, "Table of content", "navtoc", "#");
            $(userGuideMenu.toc.a).hover(function() {
                if (userGuideMenu.populateToc) {
                    userGuideMenu.populateToc();
                    reStyle();
                    userGuideMenu.populateToc = null;
                }
            });
            userGuideMenu.examples = userGuideMenu.menu.createItem(null, "Examples", "navexamples", "#");
            $(userGuideMenu.examples.a).hover(function() {
                if (userGuideMenu.populateExamples) {
                    userGuideMenu.populateExamples();
                    reStyle();
                    userGuideMenu.populateExamples = null;
                }
            });
            userGuideMenu.figures = userGuideMenu.menu.createItem(null, "Figures", "navfigures", "#");
            $(userGuideMenu.figures.a).hover(function() {
                if (userGuideMenu.populateFigures) {
                    userGuideMenu.populateFigures();
                    reStyle();
                    userGuideMenu.populateFigures = null;
                }
            });
            userGuideMenu.tables = userGuideMenu.menu.createItem(null, "Tables", "navtables", "#");
            $(userGuideMenu.tables.a).hover(function() {
                if (userGuideMenu.populateTables) {
                    userGuideMenu.populateTables();
                    reStyle();
                    userGuideMenu.populateTables = null;
                }
            });
            userGuideMenu.changes = userGuideMenu.menu.createItem(null, "Changes", "navchanges", "#");
            $(userGuideMenu.changes.a).hover(function() {
                if (userGuideMenu.populateChanges) {
                    userGuideMenu.populateChanges();
                    reStyle();
                    userGuideMenu.populateChanges = null;
                }
            });
            log.profile("UserGuideMenu.create()");
        } catch (err) {
            log.error("UserGuideMenu.create(): " + err); throw err;
        }
    }

    //! @brief Create TOC menu items.
    this.populateToc = function() {
        try {
            log.profile("UserGuideMenu.populateToc()");
            var ar_TocItems = new Array(userGuideMenu.toc);
            $(".titlepage").each(function() {
                try {
                    var p_Node = new XmlNode(this);
                    var str_Title = p_Node.child("div").child("div").child(null).fullText();
                    var str_Id = null;
                    if (/Frequently Asked Questions/.exec(str_Title)) {
                        str_Id = "faq-menu";
                    }
                    var str_Anchor = p_Node.child("div").child("div").child(null).child("a").attribute("name");
                    var i_HeaderLevel = p_Node.child("div").child("div").child(null).name().substring(1);
                    if (i_HeaderLevel >= 2) {
                        ar_TocItems[i_HeaderLevel - 1] = userGuideMenu.menu.createItem(ar_TocItems[i_HeaderLevel - 2], str_Title, str_Id, "#" + str_Anchor);
                    }
                } catch (err) {
                    log.warn("UserGuideMenu.populateToc(): title processing error: this = " + this + ", err = " + err);
                }
            });
            log.profile("UserGuideMenu.populateToc()");
        } catch (err) {
            log.error("UserGuideMenu.populateToc(): " + err); throw err;
        }
    }

    //! @brief Create examples menu items.
    this.populateExamples = function() {
        try {
            log.profile("UserGuideMenu.populateExamples()");
            $(".example").each(function() {
                try {
                    var p_Node = new XmlNode(this);
                    var str_Title = p_Node.child("p").fullText();
                    var str_Anchor = p_Node.child("a").attribute("name");
                    userGuideMenu.menu.createItem(userGuideMenu.examples, str_Title, null, "#" + str_Anchor);
                } catch (err) {
                    log.warn("UserGuideMenu.populateExamples(): example processing error: this = " + this + ", err = " + err);
                }
            });
            log.profile("UserGuideMenu.populateExamples()");
        } catch (err) {
            log.error("UserGuideMenu.populateExamples(): " + err); throw err;
        }
    }

    //! @brief Create figure menu items.
    this.populateFigures = function() {
        try {
            log.profile("UserGuideMenu.populateFigures()");
            var i_FigureCount = 0;
            $(".informalfigure").each(function() {
                try {
                    var p_Node = new XmlNode(this);
                    var str_Title = p_Node.child("div").child("div").child("p").fullText();
                    i_FigureCount ++;
                    var str_Anchor = "Figure_" + i_FigureCount; {
                        // Runtime A element creation.
                        var p_a = createXmlNode("a"); {
                            // Create @name attribute.
                            p_a.setAttribute("name", str_Anchor);
                        } p_Node.node.insertBefore(p_a.node, p_Node.node.firstChild);
                    }
                    userGuideMenu.menu.createItem(userGuideMenu.figures, str_Title, null, "#" + str_Anchor);
                } catch (err) {
                    log.warn("UserGuideMenu.populateFigures(): figure processing error: this = " + this + ", err = " + err);
                }
            });
            log.profile("UserGuideMenu.populateFigures()");
        } catch (err) {
            log.error("UserGuideMenu.populateExamples(): " + err); throw err;
        }
    }

    //! @brief Create table menu items.
    this.populateTables = function() {
        try {
            log.profile("UserGuideMenu.populateTables()");
            $(".table").each(function() {
                try {
                    var p_Node = new XmlNode(this);
                    var str_Title = p_Node.child("p").fullText();
                    var str_Anchor = p_Node.child("a").attribute("name");
                    userGuideMenu.menu.createItem(userGuideMenu.tables, str_Title, null, "#" + str_Anchor);
                } catch (err) {
                    log.warn("UserGuideMenu.populateTables(): table processing error: this = " + this + ", err = " + err);
                }
            });
            log.profile("UserGuideMenu.populateTables()");
        } catch (err) {
            log.err("UserGuideMenu.populateTables(): " + err); throw err;
        }
    }

    //! @brief Create change menu items.
    this.populateChanges = function() {
        try {
            log.profile("UserGuideMenu.populateChanges()");
            $(".revhistory").each(function() {
                var xml_trs = new XmlNode(this).child("table").node.getElementsByTagName("tr");
                // Scan changes from the end of the table, in order to restore a chronological order.
                for (var i = 1; i + 1 < xml_trs.length; i += 2) {
                    try {
                        var xml_tr1 = xml_trs[i];
                        var xml_tr1tds = xml_tr1.getElementsByTagName("td");
                        var xml_tr2 = xml_trs[i+1];
                        var xml_tr2tds = xml_tr2.getElementsByTagName("td");
                        if ((xml_tr1tds.length >= 2) && (xml_tr2tds.length >= 1)) {
                            var str_Title = new XmlNode(xml_tr1tds[0]).fullText();
                            var str_Date = new XmlNode(xml_tr1tds[1]).fullText();
                            var str_Details = new XmlNode(xml_tr2tds[0]).fullText();
                            var p_Item = userGuideMenu.menu.createItem(userGuideMenu.changes, str_Title, null, "#");

                            // Create a dialogbox
                            p_Item.a.dlg = new DynamicDialog("change" + i, "change", str_Title); {
                                // Add date
                                var p_p = createXmlNode("p"); {
                                    var xml_Text = document.createTextNode(str_Date);
                                    p_p.node.appendChild(xml_Text);
                                } p_Item.a.dlg.div.appendChild(p_p.node);

                                // Copy content
                                for (var j = 0; j < xml_tr2tds[0].childNodes.length; j++) {
                                    var xml_Child = xml_tr2tds[0].childNodes[j];
                                    xml_Child = xml_Child.cloneNode(true);
                                    p_Item.a.dlg.div.appendChild(xml_Child);
                                }
                            }
                            // Make it visible on click
                            p_Item.a.showDlg = function() {
                                var xml_a = this;
                                // Differ the dialog display a little bit,
                                // in order not to have it hidden when the menu hides because of being clicked.
                                window.setTimeout(function() { xml_a.dlg.open(); }, 10);
                            }
                            new XmlNode(p_Item.a).setAttribute("onclick", "this.showDlg();");
                        }
                    } catch (err) {
                        log.warn("UserGuideMenu.populateChanges(): change processing error: i = " + i + ", xml_trs[i] = " + xml_trs[i] + ", err = " + err);
                    }
                }

                // Eventually remove the change table from the body.
                this.parentNode.removeChild(this);
            });
            log.profile("UserGuideMenu.populateChanges()");
        } catch (err) {
            log.error("UserGuideMenu.populateChanges(): " + err); throw err;
        }
    }

    //! @brief Apply more styles.
    this.applyMoreStyles = function() {
        try {
            // 'Table of content' menu item in red.
            userGuideMenu.menu.colorMenu(userGuideMenu.toc, "#ff0000");
            // 'Examples' menu item in green.
            userGuideMenu.menu.colorMenu(userGuideMenu.examples, "#006600");
            // 'Figures' menu item in blue.
            userGuideMenu.menu.colorMenu(userGuideMenu.figures, "#000066");
            // 'Tables' menu item in orange.
            userGuideMenu.menu.colorMenu(userGuideMenu.tables, "#ff8800");
            // 'Changes' menu item in purple.
            userGuideMenu.menu.colorMenu(userGuideMenu.changes, "#880088");

            // 'FAQ' menu items width
            $("#faq-menu ul").css({ width: "35em" });
            $("#faq-menu ul li").css({ width: "35em" });
            $("#faq-menu ul li a").css({ width: "35em" });

            // 'Examples' menu items width.
            $("#" + userGuideMenu.examples.li.id + " ul").css({ width: "35em" });
            $("#" + userGuideMenu.examples.li.id + " ul li").css({ width: "35em" });
            $("#" + userGuideMenu.examples.li.id + " ul li a").css({ width: "35em" });
        } catch (err) {
            log.error("UserGuideMenu.applyMoreStyles(): " + err); throw err;
        }
    }

    // Initialization calls.
    try {
        userGuideMenu.create();
        userGuideMenu.menu.applyStyles();
        //userGuideMenu.menu.animate(); // Do not animate right now. That will be done with sub-items creation.
        userGuideMenu.applyMoreStyles();
        userGuideMenu.menu.show();
    } catch (err) {
        log.error("UserGuideMenu.&lt;init&gt;: " + err); throw err;
    }
}

//! @brief Info object (note or tip).
//! @param xml_div Note or tip DIV element.
//! @param str_Icon jquery UI icon identifier.
//! @param str_DlgTitle Title of the dialog attached to the info.
function UserGuideInfo(xml_div, str_Icon, str_DlgTitle) {

    var info = this;

    //! Short text.
    this.shortText = null;

    //! Dialog attached with the info.
    this.dlg = null;

    //! DIV element of the info.
    this.div = null;
    // UL element of the info.
    this.ul = null;
    //! LI element of the info.
    this.li = null;
    // SPAN(icon) element of the info.
    this.icon = null;

    //! @brief Info creation.
    this.create = function() {
        // First of all, remove the H3 title (only contains "Note").
        var xml_h3 = xml_div.removeChild(new XmlNode(xml_div).child("h3").node);

        // Set short text.
        info.shortText = new XmlNode(xml_div).fullText();
        if (info.shortText.length > 100) {
            info.shortText = info.shortText.substr(0, 100) + "...";
        }

        // Create the dialog.
        info.dlg = new DynamicDialog(null, null, str_DlgTitle);
        while (xml_div.childNodes.length > 0) {
            var xml_FirstChild = xml_div.firstChild;
            xml_div.removeChild(xml_FirstChild);
            info.dlg.div.appendChild(xml_FirstChild);
        }

        // Create XML elements.
        info.div = xml_div;
        var p_ul = createXmlNode("ul"); info.ul = p_ul.node; {
            var p_li = createXmlNode("li"); info.li = p_li.node; {
                p_li.setAttribute("class", "ui-state-default ui-corner-all");
                p_li.setAttribute("title", info.shortText);
                var p_icon = createXmlNode("span"); info.icon = p_icon.node; {
                    p_icon.setAttribute("class", "ui-icon " + str_Icon);
                } p_li.node.appendChild(p_icon.node);
                var xml_Text = document.createTextNode(new XmlNode(xml_h3).fullText()); {
                } p_li.node.appendChild(xml_Text);
            } p_ul.node.appendChild(p_li.node);
        } xml_div.appendChild(p_ul.node);
    }

    //! @brief Apply styles to info items.
    this.applyStyles = function() {
        // Apply styles to the icon.
        $(info.ul).css({ margin: "0", padding: "0" });
        $(info.li).css({ margin: "2px", position: "relative", padding: "4px 0", cursor: "pointer", float: "right", listStyle: "none", width: 70 });
        $(info.icon).css({ float: "left", margin: "0 4px" });
        // Ensure infos do not mask menu items.
        $(info.div).css({ zIndex: 0 });
        $(info.ul).css({ zIndex: 0 });
        $(info.li).css({ zIndex: 0 });
        $(info.icon).css({ zIndex: 0 });
    }

    //! @brief Animate info items.
    this.animate = function() {
        // Make the info change of color when mouse is over.
        $(info.li).hover(
            function() { $(this).addClass('ui-state-hover'); }, 
            function() { $(this).removeClass('ui-state-hover'); }
        );
        // Display the dialog on click.
        info.li.dlg = this.dlg;
        $(info.li).click(
            function() { info.dlg.open(); }
        );
    }

    this.debug = function() {
        window.setTimeout(function() {
            log.info("p_Info.shortText = " + info.shortText);
            log.info("p_Info.div.zIndex = " + info.div.style.zIndex);
            log.info("p_Info.ul.zIndex = " + info.ul.style.zIndex);
            log.info("p_Info.li.zIndex = " + info.li.style.zIndex);
            log.info("p_Info.icon.zIndex = " + info.icon.style.zIndex);
        }, 10);
    }

    // Initialization calls.
    try {
        this.create();
        this.applyStyles();
        this.animate();
    } catch (err) {
        log.error("UserGuideInfo.&lt;init&gt;: " + err); throw err;
    }
}


//! @brief Handler called when the page is loaded.
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

        // Abort javascript if required.
        if (! /nojs/.exec(location)) {
            // Create dynamic menu.
            document.menu = new UserGuideMenu();

            // Other transformations.
            function makeInfos(str_Class, str_Icon, str_DlgTitle) {
                try {
                    log.profile("onLoad()/makeInfos(str_Class = " + str_Class + ")");
                    // For each note, create a UserGuideInfo object that will transform the page.
                    $("." + str_Class).each(function() {
                        try {
                            this.info = new UserGuideInfo(this, str_Icon, str_DlgTitle);
                        } catch (err) {
                            log.warn("onLoad()/makeInfos(): div processing error: this = " + this + ", err = " + err);
                        }
                    });
                    log.profile("onLoad()/makeInfos(str_Class = " + str_Class + ")");
                } catch (err) {
                    log.error("onLoad()/makeInfos(): " + err); throw err;
                }
            }
            window.setTimeout(function() {
                makeInfos("note", "ui-icon-document", "Note");
                makeInfos("tip", "ui-icon-lightbulb", "Tip");
            }, 100);

            if (document.all) {
                // IE fixes: make the scrolling move in order to ensure the menu to be displayed.
                window.setTimeout(function() { document.body.scrollTop = document.body.scrollTop - 1; }, 5000);
            }
        }

        log.profile("onLoad()");
    } catch (err) {
        log.error("onLoad(): " + err); throw err;
    }
}

//! @brief Handler called when the page is scrolled.
function onScroll() {
    try {
        log.profile("onScroll()");

        if (document.all) {
            var xml_BlackBird = document.getElementById("blackbird");
            if (xml_BlackBird) {
                xml_BlackBird.style.position = "absolute";
                xml_BlackBird.style.top = document.body.scrollTop + window.height() - xml_BlackBird.offsetHeight;
                xml_BlackBird.style.left = document.body.scrollLeft + window.width() - xml_BlackBird.offsetWidth;
            }
        }

        log.profile("onScroll()");
    } catch (err) {
        log.error("onScroll(): " + err);
        throw err;
    }
}

