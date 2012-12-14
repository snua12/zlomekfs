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


//! @brief Dynamic menu item class.
//! @param menu Dynamic menu owning this item.
//! @param p_ParentMenuItem Parent menu item reference.
//! @param str_Text Text of the item.
//! @param str_Url URL of the item.
function DynamicMenuItem(menu, p_ParentMenuItem, str_Text, str_Url) {
    var menuItem = this;

    //! Parent menu item.
    this.parent = p_ParentMenuItem;
    //! LI top element.
    this.li = null;
    //! A link element.
    this.a = null;
    //! UL element ready for receiving child nodes.
    this.ul = null;

    this.create = function() {
        try {
            var p_li = createXmlNode("li"); this.li = p_li.node; {
                // Set the internal hyperlink.
                var p_a = createXmlNode("a"); this.a = p_a.node; {
                    // Create @href attribute.
                    if ((str_Url != null) && (typeof str_Url != undefined) && (str_Url != "#")) {
                        p_a.setAttribute("href", str_Url);
                    } else {
                        p_a.setAttribute("href", "#_");
                    }
                    // Link with menu.onItemMouseOver.
                    $(p_a.node).hover(function() { menu.onItemMouseOver(menuItem); });
                    // Set the item text.
                    var xml_Text = document.createTextNode(str_Text);
                    p_a.node.appendChild(xml_Text);
                } p_li.node.appendChild(p_a.node);
                // Add an unsigned list node for possible future sub-items.
                var p_ul = createXmlNode("ul"); this.ul = p_ul.node; {
                } p_li.node.appendChild(p_ul.node);
            }
            if (this.parent != null) {
                this.parent.ul.appendChild(p_li.node);
            } else {
                menu.ulMain.appendChild(p_li.node);
            }
        } catch (err) {
            log.error("DynamicMenu.DynamicMenuItem.create(): " + err); throw err;
        }
    }

    this.debug = function() {
        try {
            window.setTimeout(function() {
                log.info("menuItem.li.ulParent.zIndex = " + menuItem.li.parentNode.style.zIndex);
                log.info("menuItem.li.zIndex = " + menuItem.li.style.zIndex);
                log.info("menuItem.a.zIndex = " + menuItem.a.style.zIndex);
                log.info("menuItem.ul.zIndex = " + menuItem.ul.style.zIndex);
            }, 10);
        } catch (err) {
            log.error("DynamicMenu.DynamicMenuItem.debug(): " + err); throw err;
        }
    }

    // Initialization calls.
    try {
        this.create();
    } catch (err) {
        log.error("DynamicMenu.DynamicMenuItem.&lt;init&gt;: " + err); throw err;
    }
}


//! @brief Dynamic menu management.
function DynamicMenu(id) {
    log.profile("DynamicMenu(id = " + id + ")");

    //! Self reference.
    var menu = this;

    //! Menu identifier.
    this.id = id;

    //! Hiding timeout.
    this.hHideMenu = null;

    //! Hiding offset.
    this.hideOffset = 23;

    //! Main UL element.
    this.ulMain = null;


    //! @brief Dynamic menu creation.
    this.create = function() {
        try {
            log.profile("DynamicMenu.create()");

            var p_ul = createXmlNode("ul"); {
                p_ul.setAttribute("id", this.id);
            } document.body.insertBefore(p_ul.node, document.body.firstChild);
            $(p_ul.node).css({ display: "none" });

            this.ulMain = p_ul.node;

            log.profile("DynamicMenu.create()");
        } catch (err) {
            log.error("DynamicMenu.create(): " + err); throw err;
        }
    }

    //! @brief Menu item creation.
    //! @param p_ParentMenuItem Parent menu item reference. null for creating an item at the top level.
    //! @param str_Text Text of menu item.
    //! @param str_Id Identifier of menu item. Useful for specific renderings. Optional, may be null.
    //! @param str_Url URL the menu item should redirect to.
    //! @return New DynamicMenuItem object.
    this.createItem = function(p_ParentMenuItem, str_Text, str_Id, str_Url) {
        try {
            // Create a list item.
            var p_NewItem = new DynamicMenuItem(menu, p_ParentMenuItem, str_Text, str_Url);
            if (str_Id != null) { p_NewItem.li.setAttribute("id", str_Id); }

            // Force zIndex through menu hierarchy.
            var i_zIndex = 0;
            for (var p_Item = p_NewItem; p_Item != null; p_Item = p_Item.parent) {
                var i_CurrentzIndex = p_Item.a.style.zIndex;
                if (typeof i_CurrentzIndex == undefined) { i_CurrentzIndex = 0; }
                if (i_CurrentzIndex == "") { i_CurrentzIndex = 0; }
                if (i_CurrentzIndex <= i_zIndex) {
                    i_zIndex ++;
                } else {
                    break;
                }

                //! UL element ready for receiving child nodes.
                p_Item.li.parentNode.style.zIndex = i_zIndex;
                p_Item.li.style.zIndex = i_zIndex;
                p_Item.a.style.zIndex = i_zIndex;
                p_Item.ul.style.zIndex = i_zIndex - 1;
            }

            return p_NewItem;
        } catch (err) {
            log.error("DynamicMenu.createItem(): " + err); throw err;
        }
    }

    //! @brief Shows the menu previously created.
    this.show = function() {
        try {
            $(this.ulMain).css({ display: "block" });
            // Let the menu visible for 1 second, then hide.
            this.hHideMenu = window.setTimeout(function() { menu.hide("DynamicMenu.show()"); }, 1000);
        } catch (err) {
            log.error("DynamicMenu.show(): " + err); throw err;
        }
    }

    //! @brief Apply styles.
    this.applyStyles = function() {
        try {
            log.profile("DynamicMenu.applyStyles()");

            var nav = "#" + this.id;

            // Let's position menu items at the top left corner, attached to the navigator window.
            $(nav + ", " + nav + " ul").css({
                margin: 0,
                padding: 0,
                listStyleType: "none",
                listStylePosition: "outside",
                position: "fixed", // fixed for a fixed position in the navigator window, whatever the scrolling is.
                top: 0,
                left: 0,
                lineHeight: "1.5em" // Line-height defines the height of each list item.
            });
            if (document.all) {
                // IE fixes.
                $(nav + ", " + nav + " ul").css({
                    position: "absolute",
                    top: document.body.scrollTop,
                    left: document.body.scrollLeft
                });
                $(window).scroll(function() {
                    $(menu.ulMain).css({
                        display: "block",
                        position: "absolute",
                        top: document.body.scrollTop - menu.hideOffset,
                        left: document.body.scrollLeft
                    });
                });
            }

            // Style each hyper link in our menu a little bit.
            $(nav + " a").css({
                display: "block",
                padding: "0px 5px",
                border: "1px solid #333333",
                color: "#ffffff",
                textDecoration: "none",
                backgroundColor: "#666666"
                // Using opacity slows a lot menu displays
                // opacity: "0.95",
                // filter: "alpha(opacity='95')" // IE < 8
            });

            // Enlight menu items when the mouse is over.
            $(nav + " a").hover(
                function() { $(this).css({ backgroundColor: "#ffffff", color: "#333333" }); },
                function() { $(this).css({ backgroundColor: "#666666", color: "#ffffff" }); }
            );

            // Align our list elements horizontally.
            $(nav + " li").css({
                float: "left",
                position: "relative"
            });

            // Position the nested Lists right beyond the main menu and give them a width of 18em.
            $(nav + " ul").css({
                position: "absolute",
                width: "18em",
                top: "1.5em",
                display: "none"
            });
            if (document.all) {
                // IE fixes.
                $(nav + " > ul").css({
                    top: document.body.scrollTop + 24
                });
                $(window).scroll(function() {
                    $(nav + " ul").css({
                        top: document.body.scrollTop + 24
                    });
                });
            }

            // Set the width of the hyper links to 18 em
            // (which in combination with the width of the UL set above results in a horizontally displayed sub menu, despite of the ongoing float:left).
            $(nav + " li ul a").css({
                width: "18em",
                float: "left"
            });

            // Define where we display the sub menus.
            $(nav + " ul ul").css({
                top: "auto"
            });
            $(nav + " li ul ul").css({
                left: "18em",
                margin: "0px 0 0 10px"
            });
            if (document.all) {
                // IE fixes.
                $(nav + " li ul ul").css({
                    margin: "0px 0 0 0px"
                });
            }

            log.profile("DynamicMenu.applyStyles()");
        } catch (err) {
            log.error("DynamicMenu.applyStyles(): " + err); throw err;
        }
    }

    //! @brief Colorize a menu item.
    //! @param p_MenuItem Menu item to colorize.
    //! @param str_Color Color to be used.
    this.colorMenu = function(p_MenuItem, str_Color) {
        $(p_MenuItem.a).css({ border: "1px solid " + str_Color, backgroundColor: str_Color });
        $(p_MenuItem.a).hover(
            function() { $(this).css({ backgroundColor: "#ffffff", color: "#333333" }); },
            function() { $(this).css({ backgroundColor: str_Color, color: "#ffffff" }); }
        );
    }

    //! @brief Dynamic menu animation.
    this.animate = function() {
        try {
            log.profile("DynamicMenu.animate()");

            var nav = "#" + this.id;

            // jQuery Code to create the effects.

            // Opera Fix.
            $(nav + " ul").css({ display: "none" });

            // Show/hide sub-menus when mouse in over/out.
            $(nav + " li").hover(
                function() { // 'on' effect
                    // If a timeout for hiding the menu is set, kill it.
                    if (menu.hHideMenu != null) {
                        window.clearTimeout(menu.hHideMenu);
                        menu.hHideMenu = null;
                    }
                    // Ensure the menu is visible at the top position.
                    $(nav).css({ top: 0 }, 0);
                    if (document.all) {
                        $(nav).css({ top: document.body.scrollTop }, 0);
                    }
                    // Display the direct sub-menu.
                    var jq_SubMenu = $(this).find('ul:first');
                    jq_SubMenu.css({ visibility: "visible", display: "none" });
                    if (document.all) {
                        jq_SubMenu.css({ display: "block" });
                    }
                    jq_SubMenu.show(400);
                },
                function() { // 'out' effect
                    // Hide the direct sub-menu.
                    $(this).find('ul:first').css({ visibility: "hidden" });
                    // If a timeout for hiding the menu is set, kill it first.
                    if (menu.hHideMenu != null) {
                        window.clearTimeout(menu.hHideMenu);
                        menu.hHideMenu = null;
                    }
                    // Then set a timer for auto hiding the menu.
                    menu.hHideMenu = window.setTimeout(
                        function() {
                            menu.hide("jQuery.hover(out-effect)");
                            menu.hHideMenu = null;
                        }, 1000
                    );
                }
            );

            // This style is applicable to all A elements of the page.
            // In case it is an inner reference, is scrolls the page as the menu would,
            // and applies the appropriate offset so that the menu does not disturb the elements displayed.
            $("a").click(
                function() { // 'click' effect
                    $(nav + " ul").css({ visibility: "hidden" });
                    menu.hide("onClick");

                    // Inner anchors.
                    if (this.getAttribute("href").charAt(0) == '#') {
                        var str_Anchor = this.getAttribute("href").replace(/#/, "");
                        if (str_Anchor.length > 0) {
                            // Find out the target anchor in the page
                            var p_Anchor = null;
                            var xml_Anchors = document.getElementsByName(str_Anchor);
                            if (xml_Anchors.length > 0) {
                                p_Anchor = new XmlNode(xml_Anchors[0]);
                            } else if (document.all) {
                                // IE fixes.
                                xml_Anchors = document.getElementsByTagName("a");
                                for (var i=0; i<xml_Anchors.length; i++) {
                                    if (xml_Anchors[i].getAttribute("name") == str_Anchor) {
                                        p_Anchor = new XmlNode(xml_Anchors[i]);
                                        break;
                                    }
                                }
                            }

                            // Scroll to the target anchor.
                            if (p_Anchor != null) {
                                // If the anchor is located in a title, move up to the title element.
                                if (p_Anchor.parent().name().charAt(0) == 'h') {
                                    p_Anchor = p_Anchor.parent();
                                }
                                // Apply somme more offset because of the menu.
                                var moreOffset = new XmlNode(menu.ulMain).height() + 2 - menu.hideOffset;
                                // Make the window move smoothly to the given position.
                                $(document.body).animate({ scrollTop: p_Anchor.top() - moreOffset });
                            }
                        }
                    }
                }
            );

            log.profile("DynamicMenu.animate()");
        } catch (err) {
            log.error("DynamicMenu.animate(): " + err); throw err;
        }
    }

    //! @brief Menu item mouse over handler.
    //! @param p_MenuItem A menu item node.
    this.onItemMouseOver = function(p_MenuItem) {
        try {
            log.debug("DynamicMenu.onItemMouseOver(p_MenuItem = " + p_MenuItem.a + ")");

            // Make item list slide up/down if not completely visible.
            if (p_MenuItem.parent != null) {
                var i_TopMenuPos = new XmlNode(menu.ulMain).child("li").bottom() - 2; // Remove two pixels because of border settings in CSS.
                var i_WindowHeight = window.height() - new XmlNode(menu.ulMain).child("li").height(); // Remove one line because of the bottom slider that could mask the last menu.
                if (document.all) {
                    // IE fixes.
                    i_WindowHeight += document.body.scrollTop;
                }

                // First ensure the current item is visible.
                if (true) {
                    var p_Current = new XmlNode(p_MenuItem.a);
                    if (p_Current.top() < i_TopMenuPos)
                    {
                        var i_Translate = i_TopMenuPos - p_Current.top();
                        $(p_MenuItem.parent.ul).animate({ top: p_MenuItem.parent.ul.offsetTop + i_Translate }, 10);
                    }
                    if (p_Current.bottom() > i_WindowHeight) {
                        var i_Translate = p_Current.bottom() - i_WindowHeight;
                        $(p_MenuItem.parent.ul).animate({ top: p_MenuItem.parent.ul.offsetTop - i_Translate }, 10);
                    }
                }
                // Then ensure the previous item is visible.
                if (p_MenuItem.li.previousSibling != null) {
                    var p_Previous = new XmlNode(p_MenuItem.li.previousSibling).child("a");
                    if (p_Previous.top() < i_TopMenuPos) {
                        var i_Translate = i_TopMenuPos - p_Previous.top();
                        $(p_MenuItem.parent.ul).animate({ top: p_MenuItem.parent.ul.offsetTop + i_Translate }, 10);
                    }
                }
                // Enventually ensure the next item visible.
                if (p_MenuItem.li.nextSibling != null) {
                    var p_Next = new XmlNode(p_MenuItem.li.nextSibling).child("a");
                    if (p_Next.bottom() > i_WindowHeight) {
                        var i_Translate = p_Next.bottom() - i_WindowHeight;
                        $(p_MenuItem.parent.ul).animate({ top: p_MenuItem.parent.ul.offsetTop - i_Translate }, 10);
                    }
                }
            }
        } catch (err) {
            log.error("DynamicMenu.onItemMouseOver(): " + err); throw err;
        }
    }

    //! @brief Hides the menu.
    //! @param cause Cause of hiding (useful for debug only)
    //! @param delay Time of effect. Optional, can be not set, default behaviour is then adopted.
    this.hide = function(cause, delay) {
        try {
            log.debug("DynamicMenu.hide(cause = " + cause + ", delay = " + delay + ")");
            if (! document.all) {
                $(menu.ulMain).animate({ top: - this.hideOffset }, delay);
            } else {
                menu.ulMain.style.position = "absolute";
                $(menu.ulMain).animate({
                    top: document.body.scrollTop - this.hideOffset,
                    left: document.body.scrollLeft
                });
            }
        } catch (err) {
            log.error("DynamicMenu.hide(): " + err); throw err;
        }
    }


    // Initialization calls.
    try {
        this.create();
    } catch (err) {
        log.error("DynamicMenu.&lt;int&gt;: " + err); throw err;
    }

    log.profile("DynamicMenu(id = " + id + ")");
}
