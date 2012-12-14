public class UISampleText {
    public static void fillText(cli.OutputDevice.Interface CLI_UI, int I_Count) {
        for (int i=1; (i<=I_Count) && (i<1024); i++) {
            CLI_UI.put(i + ": ");
            for (int j=1; j<=i; j++) CLI_UI.put("*");
            CLI_UI.endl();
        }
    }
}
