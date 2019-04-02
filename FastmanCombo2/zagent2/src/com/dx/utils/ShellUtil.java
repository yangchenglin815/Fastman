package com.dx.utils;

import java.io.BufferedReader;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;

public class ShellUtil {

    public static final String COMMAND_SH = "sh";
    public static final String COMMAND_EXIT = "exit\n";
    public static final String COMMAND_LINE_END = "\n";

    private ShellUtil() {
        throw new AssertionError();
    }

    public static String execCommand(String[] commands) {
        if (commands == null || commands.length == 0) {
            return null;
        }

        Process process = null;
        BufferedReader brResult = null;
        StringBuilder sbMsg = null;

        DataOutputStream os = null;
        try {
            process = Runtime.getRuntime().exec(COMMAND_SH);
            os = new DataOutputStream(process.getOutputStream());
            for (String command : commands) {
                if (command == null) {
                    continue;
                }

                // donnot use os.writeBytes(commmand), avoid chinese charset
                // error
                os.write(command.getBytes());
                os.writeBytes(COMMAND_LINE_END);
                os.flush();
            }
            os.writeBytes(COMMAND_EXIT);
            os.flush();

            process.waitFor();
            // get command result
            sbMsg = new StringBuilder();
            brResult = new BufferedReader(new InputStreamReader(process.getInputStream()));
            String s;
            while ((s = brResult.readLine()) != null) {
                sbMsg.append(s);
            }
        } catch (IOException e) {
            e.printStackTrace();
        } catch (Exception e) {
            e.printStackTrace();
        } finally {
            try {
                if (os != null) {
                    os.close();
                }
                if (brResult != null) {
                    brResult.close();
                }
            } catch (IOException e) {
                e.printStackTrace();
            }

            if (process != null) {
                process.destroy();
            }
        }
        return sbMsg == null ? null : sbMsg.toString();
    }
}
