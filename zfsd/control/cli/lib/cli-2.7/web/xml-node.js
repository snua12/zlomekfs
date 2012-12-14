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


//! @brief HTML node toolkit class.
//! @param xml_Node HTML node to create a toolkit class for.
function XmlNode(xml_Node) {

    //! @brief HTML node accessor.
    //! @return HTML node.
    this.node = xml_Node;

    //! @brief Name of the HTML node.
    //! @return Name of the HTML node.
    this.name = function() {
        return this.node.nodeName.toLowerCase();
    }

    //! @brief Parent node accessor.
    //! @return HTML toolkit object for the parent.
    this.parent = function() {
        return new XmlNode(this.node.parentNode);
    }

    //! @brief First child of the kind accessor.
    //! @param str_NodeName Name of the child node to search.
    //!                     null for child of any kind.
    //! @return HTML toolkit object for the child if found, null otherwise.
    this.child = function(str_NodeName) {
        for (var i = 0; i < this.node.childNodes.length; i++) {
            if (this.node.childNodes[i].nodeType != 3) {
                if ((str_NodeName == null) || (this.node.childNodes[i].nodeName.toLowerCase() == str_NodeName.toLowerCase())) {
                    return new XmlNode(this.node.childNodes[i]);
                }
            }
        }
        return null;
    };

    //! @brief Attribute accessor.
    //! @param str_AttributeName Name of the attribute.
    //! @return Value of the attribute.
    this.attribute = function(str_AttributeName) {
        return this.node.getAttribute(str_AttributeName);
    };

    //! @brief Attribute setter.
    //! @param str_AttributeName Name of the attribute.
    //! @param str_AttributeValue Value of the attribute.
    this.setAttribute = function(str_AttributeName, str_AttributeValue) {
        var xml_Attribute = document.createAttribute(str_AttributeName);
        xml_Attribute.nodeValue = str_AttributeValue;
        this.node.setAttributeNode(xml_Attribute);
    }

    //! @brief Node text accessor.
    //! @return Text of node and sub-nodes concatenation.
    this.fullText = function() {
        var str_Text = "";
        for (var i = 0; i < this.node.childNodes.length; i++) {
            if (this.node.childNodes[i].nodeType == 1) {
                var str_ChildText = new XmlNode(this.node.childNodes[i]).fullText();
                if (str_ChildText.length > 0) {
                    if (str_Text.length > 0) str_Text += " ";
                    str_Text += str_ChildText;
                }
            }
            if (this.node.childNodes[i].nodeType == 3) {
                var str_ChildText = this.node.childNodes[i].nodeValue.replace(/\n/g," ").replace(/^ */,"").replace(/ *$/,"").replace(/ +/g," ");
                if (str_ChildText.length > 0) {
                    if (str_Text.length > 0) str_Text += " ";
                    str_Text += str_ChildText;
                }
            }
        }
        return str_Text;
    };

    //! @brief Top position computation.
    //! @return Current top position.
    this.top = function() {
        var i_Top = 0;
        for (var node = this.node; node != null; node = node.offsetParent) {
            i_Top += node.offsetTop;
        }
        return i_Top;
    }

    //! @brief Bottom position computation.
    //! @return Current bottom position.
    this.bottom = function() {
        return this.top() + this.height();
    }

    //! @brief Height accessor.
    //! @return Height.
    this.height = function() {
        return this.node.offsetHeight;
    }

    //! @brief Left position computation.
    //! @return Current left position.
    this.left = function() {
        var i_left = 0;
        for (var node = this.node; node != null; node = node.offsetParent) {
            i_left += node.offsetLeft;
        }
        return i_left;
    }

    //! @brief Right position computation.
    //! @return Current right position.
    this.right = function() {
        return this.left() + this.width();
    }

    //! @brief Width accessor.
    //! @return Width.
    this.width = function() {
        return this.node.offsetWidth;
    }
}

//! @brief XmlNode creation from nothing.
//! @param str_NodeName Tag of HTML node to create.
function createXmlNode(str_NodeName) {
    var xml_Node = document.createElement(str_NodeName);
    return new XmlNode(xml_Node);
}

//! @brief Window height computation.
//! @return Window height.
window.height = function() {
    if (typeof(window.innerHeight) == 'number') {
        // Non-IE
        return window.innerHeight;
    } else if (document.documentElement && document.documentElement.clientHeight) {
        // IE 6+ in 'standards compliant mode'
        return document.documentElement.clientHeight;
    } else if (document.body && document.body.clientHeight) {
        // IE 4 compatible
        return document.body.clientHeight;
    }
    return 0;
}

//! @brief Window width computation.
//! @return Window width.
window.width = function() {
    if (typeof(window.innerWidth) == 'number') {
        // Non-IE
        return window.innerWidth;
    } else if (document.documentElement && document.documentElement.clientWidth) {
        // IE 6+ in 'standards compliant mode'
        return document.documentElement.clientWidth;
    } else if (document.body && document.body.clientWidth) {
        // IE 4 compatible
        return document.body.clientWidth;
    }
    return 0;
}
