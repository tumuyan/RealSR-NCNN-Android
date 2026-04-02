package com.tumuyan.ncnn.realsr;

/**
 * RecyclerView 的数据模型，用于模型备注编辑界面。
 */
public class LabelItem {
    public final String command;
    public final String fingerprint;
    public final String defaultLabel;
    public String customLabel;

    public LabelItem(String command, String fingerprint, String defaultLabel, String customLabel) {
        this.command = command;
        this.fingerprint = fingerprint;
        this.defaultLabel = defaultLabel;
        this.customLabel = (customLabel != null) ? customLabel : "";
    }
}
