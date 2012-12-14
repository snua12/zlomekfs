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


//! @brief Dynamic dialog management.
//! @param str_Id Identifier to set to the dialog. null if not used.
//! @param str_Class Class to set to the dialog.
//! @param str_Title Title to set to the dialog.
//!
//! Creates a lastDialog member to the window object, in order to ease interactions with DynamicMenu objects.
function DynamicDialog(str_Id, str_Class, str_Title) {
    //! DIV element corresponding to the dialog.
    this.div = null;

    //! Indicates whether the dialog is fully created.
    this.bFullyCreated = false;

    //! @brief Dialog creation.
    this.create = function() {
        // Create a div.
        var p_div = createXmlNode("div"); {
            if (str_Id != null) { p_div.setAttribute("id", str_Id); }
            if (str_Class != null) { p_div.setAttribute("class", str_Class); }
            p_div.setAttribute("title", str_Title);
        } document.body.appendChild(p_div.node);
        this.div = p_div.node;

        // Ensure this new div at the end of the document is not visible by default.
        $(this.div).css({ display: "none" });
    }

    this.createResource = function() {
        // Create the dialog.
        var p_div = new XmlNode(this.div);
        $(function() {
            $(p_div.node).dialog({
                autoOpen: false,
                width: 600,
                modal: true,
                dialogClass: str_Class
            });
        });

        // Override jQuery UI Dialog styles
        $(".ui-dialog").css({ fontFamily: "Arial", fontSize: "medium" });

        // This style is applicable to all A elements of the page.
        // In case a dialog is currently displayed, it is discarded.
        $("a").click(function() { // 'click' effect
            // If a dialog is currently displayed, discard it.
            if (window.lastDialog != null) {
                window.lastDialog.close();
            }
        });
    }

    //! @brief Dialog display.
    this.open = function() {
        log.debug("DynamicDialog.open()");

        if (! this.bFullyCreated) {
            this.createResource();
            this.bFullyCreated = true;
        }

        window.lastDialog = this;
        $(this.div).dialog("open");
    }

    //! @brief Dialog closure.
    this.close = function() {
        log.debug("DynamicDialog.close()");
        window.lastDialog = null;
        $(this.div).dialog("close");
    }


    // Initialization calls.
    this.create();
}
