package com.tumuyan.ncnn.realsr;

import java.util.ArrayList;
import java.util.List;

/**
 * 辅助类，用于安全地构建 CLI 命令。
 * 避免直接使用字符串拼接，减少错误。
 */
public class CommandBuilder {
    private final List<String> commandParts;

    public CommandBuilder() {
        this.commandParts = new ArrayList<>();
    }

    public CommandBuilder append(String part) {
        if (part != null && !part.isEmpty()) {
            commandParts.add(part);
        }
        return this;
    }

    public CommandBuilder append(String key, String value) {
        if (key != null && !key.isEmpty() && value != null && !value.isEmpty()) {
            commandParts.add(key);
            commandParts.add(value);
        }
        return this;
    }

    public CommandBuilder append(String key, int value) {
        if (key != null && !key.isEmpty()) {
            commandParts.add(key);
            commandParts.add(String.valueOf(value));
        }
        return this;
    }
    
    public CommandBuilder appendIf(boolean condition, String part) {
        if (condition) {
            append(part);
        }
        return this;
    }

    public CommandBuilder appendIf(boolean condition, String key, String value) {
        if (condition) {
            append(key, value);
        }
        return this;
    }

    public String build() {
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < commandParts.size(); i++) {
            sb.append(commandParts.get(i));
            if (i < commandParts.size() - 1) {
                sb.append(" ");
            }
        }
        return sb.toString();
    }
    
    public String[] buildArray() {
        return commandParts.toArray(new String[0]);
    }

    public void clear() {
        commandParts.clear();
    }
}
