static void FillText(const cli::OutputDevice& CLI_UI, const unsigned int UI_Count) {
    for (unsigned int i=1; (i<=UI_Count) && (i<1024); i++) {
        CLI_UI << i << ": ";
        for (unsigned int j=1; j<=i; j++) CLI_UI << "*";
        CLI_UI << cli::endl;
    }
}
