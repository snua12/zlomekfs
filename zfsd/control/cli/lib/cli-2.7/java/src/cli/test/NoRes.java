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

package cli.test;

import java.util.Iterator;
import java.util.NoSuchElementException;


public class NoRes extends cli.Cli
{
    // Sub-menus.
    private class MyMenu extends cli.Menu {
        private cli.Keyword cli_Do;
            private cli.Keyword cli_Nothing;
                private cli.Endl cli_DoNothing;

        public MyMenu() {
            // Initialization.
            super("MyMenu", new cli.Help().addHelp(cli.Help.LANG_EN, "NoRes sample menu"));

            // Create sub-menus.
        }

        public void populate() {
            cli_Do = (cli.Keyword) this.addElement(new cli.Keyword("do", new cli.Help().addHelp(cli.Help.LANG_EN, "Do nothing"))); {
                cli_Nothing = (cli.Keyword) cli_Do.addElement(new cli.Keyword("nothing", new cli.Help().addHelp(cli.Help.LANG_EN, "Do nothing"))); {
                    cli_DoNothing = (cli.Endl) cli_Nothing.addElement(new cli.Endl(new cli.Help().addHelp(cli.Help.LANG_EN, "Do nothing")));
                }
            }
        }

        public boolean execute(cli.CommandLine CLI_CmdLine) {
            try {
                Iterator<cli.Element> cli_Elements = CLI_CmdLine.iterator();
                cli.Element cli_Element = cli_Element = cli_Elements.next();

                if (cli_Element == cli_Do) {
                    cli_Element = cli_Elements.next();
                    if (cli_Element == cli_Nothing) {
                        cli_Element = cli_Elements.next();
                        if (cli_Element == cli_DoNothing) {
                            getOutputStream().put("Nothing to do...").endl();
                            return true;
                        }
                    }
                }
            } catch (NoSuchElementException e1) {
            } catch (Exception e) {
                getErrorStream().printStackTrace(e);
            }

            getOutputStream().put("NoRes execution error....").endl();
            return false;
        }
    };
    private cli.Menu cli_MyMenu;

    // Internal commands.
    private cli.Keyword cli_Show;
        private cli.Keyword cli_All;
            private cli.Endl cli_DoShowAll;
        private cli.Keyword cli_Param;
            private cli.ParamInt cli_ParamValue;
                private cli.Endl cli_DoShowParam;
    private cli.Keyword cli_Enter;
        private cli.Keyword cli_Menu;
            private cli.Endl cli_EnterMenu;

    public NoRes() {
        // Initialization.
        super("NoRes", new cli.Help().addHelp(cli.Help.LANG_EN, "No resource test"));

        // Create sub-menus.
        cli_MyMenu = this.addMenu(new MyMenu());

        // Populate.
        this.populate();
    }

    public void populate() {
        // Populate the menu.
        cli_Show = (cli.Keyword) this.addElement(new cli.Keyword("show", new cli.Help().addHelp(cli.Help.LANG_EN, "Show parameters"))); {
            cli_All = (cli.Keyword) cli_Show.addElement(new cli.Keyword("all", new cli.Help().addHelp(cli.Help.LANG_EN, "Show all parameters"))); {
                cli_DoShowAll = (cli.Endl) cli_All.addElement(new cli.Endl(new cli.Help().addHelp(cli.Help.LANG_EN, "Show all parameters")));
            }
            cli_Param = (cli.Keyword) cli_Show.addElement(new cli.Keyword("param", new cli.Help().addHelp(cli.Help.LANG_EN, "Parameter <id>"))); {
                cli_ParamValue = (cli.ParamInt) cli_Param.addElement(new cli.ParamInt(new cli.Help().addHelp(cli.Help.LANG_EN, "Parameter id"))); {
                    cli_DoShowParam = (cli.Endl) cli_ParamValue.addElement(new cli.Endl(new cli.Help().addHelp(cli.Help.LANG_EN, "Show the given parameter value")));
                }
            }
        }
        cli_Enter = (cli.Keyword) this.addElement(new cli.Keyword("enter", new cli.Help().addHelp(cli.Help.LANG_EN, "Enter NoRes sample menu"))); {
            cli_Menu = (cli.Keyword) cli_Enter.addElement(new cli.Keyword("menu", new cli.Help().addHelp(cli.Help.LANG_EN, "Enter NoRes sample menu"))); {
                cli_EnterMenu = (cli.Endl) cli_Menu.addElement(new cli.Endl(new cli.Help().addHelp(cli.Help.LANG_EN, "Enter NoRes sample menu"))); {
                    cli_EnterMenu.addMenuRef(cli_MyMenu);
                }
            }
        }

        // Populate the sub-menus.
        cli_MyMenu.populate();
    }

    public boolean execute(cli.CommandLine CLI_CmdLine) {
        try {
            Iterator<cli.Element> cli_Elements = CLI_CmdLine.iterator();
            cli.Element cli_Element = cli_Elements.next();

            //if (cli_Element != null) { getOutputStream().put(cli_Element.getKeyword()).endl(); }
            if (cli_Element == cli_Show) {
                cli_Element = cli_Elements.next();
                if (cli_Element == cli_All) {
                    cli_Element = cli_Elements.next();
                    if (cli_Element == cli_DoShowAll) {
                        getOutputStream().put("Nothing to show.").endl();
                        return true;
                    }
                }
                else if (cli_Element == cli_Param) {
                    cli_Element = cli_Elements.next();
                    if (cli_Element == cli_ParamValue) {
                        cli_Element = cli_Elements.next();
                        if (cli_Element == cli_DoShowParam) {
                            getOutputStream().put("Param = " + cli_ParamValue.getValue()).endl();
                            return true;
                        }
                    }
                }
            }
            else if (cli_Element == cli_Enter) {
                cli_Element = cli_Elements.next();
                if (cli_Element == cli_Menu) {
                    cli_Element = cli_Elements.next();
                    if (cli_Element == cli_EnterMenu) {
                        getOutputStream().put("Getting in NoRes menu").endl();
                        return true;
                    }
                }
            }
        } catch (NoSuchElementException e1) {
        } catch (Exception e) {
            getErrorStream().printStackTrace(e);
        }

        getOutputStream().put("NoRes execution error....").endl();
        return false;
    }

    public void onExit() {
        getOutputStream().put("Exiting NoRes...").endl();
    }

    public static void main(String ARJ_Args[])
    {
        //  cli.Cli cli_NoRes = new cli.Cli("NoRes", new cli.Help().addHelp(cli.Help.LANG_EN, "No resource test")); {
        //      cli.Keyword cli_Show = (cli.Keyword) cli_NoRes.addElement(new cli.Keyword("show", new cli.Help().addHelp(cli.Help.LANG_EN, "Show parameters"))); {
        //          cli.Keyword cli_All = (cli.Keyword) cli_Show.addElement(new cli.Keyword("all", new cli.Help().addHelp(cli.Help.LANG_EN, "Show all parameters"))); {
        //              cli.Endl cli_Endl = (cli.Endl) cli_All.addElement(new cli.Endl(new cli.Help().addHelp(cli.Help.LANG_EN, "Show all parameters")));
        //          }
        //      }
        //      cli.Keyword cli_Param = (cli.Keyword) cli_NoRes.addElement(new cli.Keyword("param", new cli.Help().addHelp(cli.Help.LANG_EN, "Parameter <id>"))); {
        //          cli.ParamInt cli_ParamValue = (cli.ParamInt) cli_Param.addElement(new cli.ParamInt(new cli.Help().addHelp(cli.Help.LANG_EN, "Parameter id")));
        //      }
        //  }

        cli.Traces.setFilter(new cli.TraceClass("CLI_JNI"), true);

        NoRes cli_NoRes = new NoRes();

        cli.Shell cli_Shell = new cli.Shell(cli_NoRes);
        cli_Shell.run(new cli.Console());

        System.runFinalizersOnExit(true);
    }
}
