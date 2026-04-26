package com.tumuyan.ncnn.realsr;

public class ProgressLogHelper {

    private final StringBuilder logBuilder = new StringBuilder();
    private String lastProgressLine = "";
    private long startTime;

    public void reset() {
        logBuilder.setLength(0);
        lastProgressLine = "";
        startTime = System.currentTimeMillis();
    }

    public static boolean isProgressLine(String line) {
        return line != null && line.matches("\\s*\\d([0-9.]*)%(\\s.+)?");
    }

    public void appendLine(String line) {
        if (line == null || line.isEmpty()) return;

        if (isProgressLine(line)) {
            lastProgressLine = line;
        } else {
            logBuilder.append(line).append("\n");
            lastProgressLine = "";
        }
    }

    public String getDisplayText() {
        if (lastProgressLine.isEmpty()) {
            return logBuilder.toString();
        } else {
            return logBuilder.toString() + lastProgressLine;
        }
    }

    public String getFullLog() {
        return logBuilder.toString();
    }

    public String getProgressText() {
        if (!lastProgressLine.isEmpty()) {
            return lastProgressLine.trim().split("\\s")[0];
        }
        return "";
    }

    public boolean hasProgress() {
        return !lastProgressLine.isEmpty();
    }

    public float getElapsedTimeSeconds() {
        return (System.currentTimeMillis() - startTime) / 1000f;
    }

    public String getCompletionSummary(boolean success, String modelName, boolean isNcnnCommand) {
        StringBuilder summary = new StringBuilder();

        if (!success) {
            summary.append("\nfail, use ").append(getElapsedTimeSeconds()).append(" second");
        } else {
            summary.append("\nfinish, use ").append(getElapsedTimeSeconds()).append(" second");
        }

        if (isNcnnCommand && modelName != null && !modelName.isEmpty()) {
            summary.append(", ").append(modelName);
        }

        summary.append("\n");
        return summary.toString();
    }
}
