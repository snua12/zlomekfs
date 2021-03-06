// ----- Extra java (option='head') -----


// ----- Imports -----
import java.util.Iterator;
import java.util.NoSuchElementException;

// ----- Extra java (option='import') -----


/**
 * Class auto-generated by 'javaclic.xsl' - Do not edit!
 * @author javaclic.xsl - CLI library 2.7 (Alexis Royer, http://alexis.royer.free.fr/CLI/)
 */

// ----- Cli class definition -----
 class Empty extends cli.Cli {
    // ----- Sub-menus -----

    // ----- Owner CLI -----
    private Empty m_cliOwnerCli;
    // ----- Menus -----
    private Empty m_clicli_id4485827;
    // ----- Node members -----
    // ----- Extra java (option='members') -----


    // ----- Constructor -----
    public Empty() {
        super("myCLI", new cli.Help());
        this.populate();
        // ----- Extra java (option='constructor') -----

    }

    // ----- Populate -----
    public void populate() {
        // CLI reference
        m_cliOwnerCli = (Empty) getCli();
        // Create menus and populate
        m_cliOwnerCli.m_clicli_id4485827 = this;
        // Local nodes
    }

    // ----- Menu execution -----
    public boolean execute(cli.CommandLine CLI_CmdLine) {
        try {
            cli.TraceClass CLI_EXECUTION = new cli.TraceClass("CLI_EXECUTION", new cli.Help().addHelp(cli.Help.LANG_EN, "CLI Execution traces").addHelp(cli.Help.LANG_FR, "Traces d'ex�cution du CLI"));
            Iterator<cli.Element> cli_Elements = CLI_CmdLine.iterator();
            cli.Element cli_Element = null;
            // myCLI>
            m_clicli_id4485827_lbl:
            {
                cli_Element = cli_Elements.next();
                if (cli_Element == null) return false;
                cli.Traces.trace(CLI_EXECUTION, "context = \"myCLI>\", "+ "word = " + (cli_Element instanceof cli.Endl ? "<CR>" : cli_Element.getKeyword()));
                return false;
            }
        } catch (NoSuchElementException e1) {
        } catch (Exception e) {
            getErrorStream().printStackTrace(e);
        }
        return false;
    }

    public void onExit() {
    }

    public String onPrompt() {
        return super.onPrompt();
    }

}

// ----- Extra java (option='tail') -----


