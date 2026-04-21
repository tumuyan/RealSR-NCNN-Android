package com.tumuyan.ncnn.realsr;

import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/**
 * 管理命令列表和标签列表的生成，以及自定义标签的映射。
 * 被 MainActivity、SettingActivity、LabelEditorActivity 共用。
 */
public class CommandListManager {

    public static final String[] COMMAND_0 = new String[] {
            "./realsr-ncnn -i input.png -o output.png  -m models-Real-ESRGAN-anime",
            "./realsr-ncnn -i input.png -o output.png  -m models-Real-ESRGAN",
            "./realsr-ncnn -i input.png -o output.png  -m models-Real-ESRGANv3-general -s 4",
            "./realsr-ncnn -i input.png -o output.png  -m models-Real-ESRGANv3-anime -s 2",
            "./realsr-ncnn -i input.png -o output.png  -m models-Real-ESRGANv3-anime -s 3",
            "./realsr-ncnn -i input.png -o output.png  -m models-Real-ESRGANv3-anime -s 4",
            "./realsr-ncnn -i input.png -o output.png  -m models-Real-ESRGANv2-anime -s 2",
            "./realsr-ncnn -i input.png -o output.png  -m models-Real-ESRGANv2-anime -s 4",
            "./mnnsr-ncnn -i input.png -o output.png  -m models-MNN/ESRGAN-MoeSR-jp_Illustration-x4.mnn -s 4",
            "./mnnsr-ncnn -i input.png -o output.png  -m models-MNN/ESRGAN-MoeSR-jp_Illustration-x4.mnn -d 0 -s 4",
            "./realsr-ncnn -i input.png -o output.png  -m models-ESRGAN-Nomos8kSC -s 4",
            "./mnnsr-ncnn -i input.png -o output.png  -m models-MNN/ESRGAN-Nomos8kSC-x4.mnn -s 4",
            "./realsr-ncnn -i input.png -o output.png  -m models-Real-ESRGAN-SourceBook -s 2",
            "./realcugan-ncnn -i input.png -o output.png  -m models-nose -s 2  -n 0",
            "./realcugan-ncnn -i input.png -o output.png  -m models-se -s 2  -n -1",
            "./realcugan-ncnn -i input.png -o output.png  -m models-se -s 2  -n 0",
            "./realcugan-ncnn -i input.png -o output.png  -m models-se -s 2  -n 1",
            "./realcugan-ncnn -i input.png -o output.png  -m models-se -s 2  -n 2",
            "./realcugan-ncnn -i input.png -o output.png  -m models-se -s 2  -n 3",
            "./realcugan-ncnn -i input.png -o output.png  -m models-se -s 4  -n -1",
            "./realcugan-ncnn -i input.png -o output.png  -m models-se -s 4  -n 0",
            "./realcugan-ncnn -i input.png -o output.png  -m models-se -s 4  -n 3",
            "./realcugan-ncnn -i input.png -o output.png  -m models-pro -s 2  -n -1",
            "./realcugan-ncnn -i input.png -o output.png  -m models-pro -s 2  -n 0",
            "./realcugan-ncnn -i input.png -o output.png  -m models-pro -s 2  -n 3",
            "./realcugan-ncnn -i input.png -o output.png  -m models-pro -s 3  -n -1",
            "./realcugan-ncnn -i input.png -o output.png  -m models-pro -s 3  -n 0",
            "./realcugan-ncnn -i input.png -o output.png  -m models-pro -s 3  -n 3",
            "./Anime4k -i input.png -o output.png -z 2 -A",
            "./Anime4k -i input.png -o output.png -z 2 -A -a -e 48",
            "./Anime4k -i input.png -o output.png -z 2 -A -b -r 48",
            "./Anime4k -i input.png -o output.png -z 2 -A -w",
            "./Anime4k -i input.png -o output.png -z 2 -A -w -H",
            "./Anime4k -i input.png -o output.png -z 4 -A ",
            "./Anime4k -i input.png -o output.png -z 4 -A -a -e 40",
            "./Anime4k -i input.png -o output.png -z 4 -A -b -r 40",
            "./Anime4k -i input.png -o output.png -z 4 -A -w",
            "./Anime4k -i input.png -o output.png -z 4 -A -w -H",
    };

    /** 所有命令（COMMAND_0 + 动态生成的命令） */
    public final String[] commandList;
    /** 所有默认标签（presetLabels + 动态生成的标签） */
    public final String[] defaultLabels;

    private Map<String, String> customLabelMap = new HashMap<>();

    /**
     * 构建完整的命令列表和标签列表。
     *
     * @param presetLabels   内置命令的默认标签数组（来自 R.array.style_array），长度必须与 COMMAND_0 一致
     * @param extraPath      自定义模型路径
     * @param extraCommand   用户预设命令
     * @param classicalFilters 经典插值算法列表
     * @param magickFilters  Magick算法列表
     */
    public CommandListManager(String[] presetLabels, String extraPath, String extraCommand,
                              String[] classicalFilters, String[] magickFilters) {
        List<String> extraCmdList = new ArrayList<>();
        List<String> extraCmdLabels = buildExtraCommands(extraPath, extraCommand,
                classicalFilters, magickFilters, extraCmdList);

        int l = COMMAND_0.length;
        commandList = new String[extraCmdList.size() + l];
        System.arraycopy(COMMAND_0, 0, commandList, 0, l);
        for (int i = 0; i < extraCmdList.size(); i++)
            commandList[l + i] = extraCmdList.get(i);

        defaultLabels = new String[extraCmdLabels.size() + l];
        // 安全处理：presetLabels 可能与 COMMAND_0 长度不一致（版本更新时）
        int copyLen = Math.min(presetLabels.length, l);
        System.arraycopy(presetLabels, 0, defaultLabels, 0, copyLen);
        // 如果 presetLabels 比 COMMAND_0 短，补充默认值
        for (int i = copyLen; i < l; i++) {
            defaultLabels[i] = commandFingerprint(COMMAND_0[i]);
        }
        for (int i = 0; i < extraCmdLabels.size(); i++)
            defaultLabels[l + i] = extraCmdLabels.get(i);
    }

    public int getCommandCount() {
        return commandList.length;
    }

    public String getCommandAt(int index) {
        if (index >= 0 && index < commandList.length)
            return commandList[index];
        return "";
    }

    /**
     * 判断命令是否支持目录批量处理模式。
     * 支持：realsr-ncnn, srmd-ncnn, waifu2x-ncnn, realcugan-ncnn, mnnsr-ncnn, resize-ncnn
     * 不支持：Anime4k, magick（这些程序只能处理单文件）
     */
    public static boolean supportsDirectoryMode(String command) {
        if (command == null || command.isEmpty()) return false;
        String cmd = command.trim().toLowerCase();
        return cmd.startsWith("./realsr-ncnn") ||
               cmd.startsWith("./srmd-ncnn") ||
               cmd.startsWith("./waifu2x-ncnn") ||
               cmd.startsWith("./realcugan-ncnn") ||
               cmd.startsWith("./mnnsr-ncnn") ||
               cmd.startsWith("./resize-ncnn");
    }

    /**
     * 获取支持目录批量处理的命令列表。
     */
    public String[] getDirectorySupportedCommands() {
        List<String> supported = new ArrayList<>();
        for (String cmd : commandList) {
            if (supportsDirectoryMode(cmd)) {
                supported.add(cmd);
            }
        }
        return supported.toArray(new String[0]);
    }

    /**
     * 获取支持目录批量处理的标签列表。
     * @param useCustomLabel 是否使用自定义标签
     */
    public String[] getDirectorySupportedLabels(boolean useCustomLabel) {
        String[] allLabels = getDisplayLabels(useCustomLabel);
        List<String> supported = new ArrayList<>();
        for (int i = 0; i < commandList.length; i++) {
            if (supportsDirectoryMode(commandList[i])) {
                supported.add(allLabels[i]);
            }
        }
        return supported.toArray(new String[0]);
    }

    /**
     * 获取显示用的标签列表。
     * 如果 useCustomLabel 为 true，则用自定义标签替换默认标签。
     */
    public String[] getDisplayLabels(boolean useCustomLabel) {
        if (!useCustomLabel || customLabelMap.isEmpty()) {
            return defaultLabels.clone();
        }
        String[] result = defaultLabels.clone();
        for (int i = 0; i < commandList.length; i++) {
            String fp = commandFingerprint(commandList[i]);
            String custom = customLabelMap.get(fp);
            if (custom != null && !custom.isEmpty()) {
                result[i] = custom;
            }
        }
        return result;
    }

    /** 加载自定义标签映射（从 JSON 字符串） */
    public void loadCustomLabels(String json) {
        customLabelMap.clear();
        if (json == null || json.trim().isEmpty()) return;
        try {
            JSONObject obj = new JSONObject(json);
            Iterator<String> keys = obj.keys();
            while (keys.hasNext()) {
                String key = keys.next();
                customLabelMap.put(key, obj.getString(key));
            }
        } catch (JSONException e) {
            Log.e("CommandListManager", "Failed to parse customLabels JSON", e);
            customLabelMap.clear();
        }
    }

    /** 设置自定义标签映射 */
    public void setCustomLabelMap(Map<String, String> map) {
        customLabelMap.clear();
        if (map != null) {
            customLabelMap.putAll(map);
        }
    }

    /** 获取当前自定义标签映射的副本 */
    public Map<String, String> getCustomLabelMap() {
        return new HashMap<>(customLabelMap);
    }

    /** 导出为 JSON 字符串 */
    public String toCustomLabelJson() {
        if (customLabelMap.isEmpty()) return "";
        return new JSONObject(customLabelMap).toString();
    }

    /**
     * 导出全部为 TSV 格式文本（用于复制全部）。
     * 格式：fingerprint\tcustomLabel，每行一条。
     * 没有自定义标签的项也输出（只有 fingerprint）。
     */
    public String exportAllText() {
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < commandList.length; i++) {
            String fp = commandFingerprint(commandList[i]);
            String custom = customLabelMap.get(fp);
            sb.append(fp);
            if (custom != null && !custom.isEmpty()) {
                sb.append('\t').append(custom);
            }
            sb.append('\n');
        }
        return sb.toString();
    }

    /**
     * 从 TSV 格式文本导入（用于粘贴全部）。
     * 格式：每行 fingerprint\tcustomLabel，自定义标签为空时可以只有 fingerprint。
     *
     * @return 成功导入的条数
     */
    public int importFromText(String text) {
        if (text == null || text.trim().isEmpty()) return 0;
        int count = 0;
        // 构建指纹到索引的查找表
        Map<String, Integer> fpIndexMap = new HashMap<>();
        for (int i = 0; i < commandList.length; i++) {
            fpIndexMap.put(commandFingerprint(commandList[i]), i);
        }
        String[] lines = text.split("\n");
        for (String line : lines) {
            line = line.trim();
            if (line.isEmpty()) continue;
            String[] parts = line.split("\t", 2);
            String fp = parts[0].trim();
            if (fpIndexMap.containsKey(fp)) {
                if (parts.length > 1 && !parts[1].trim().isEmpty()) {
                    customLabelMap.put(fp, parts[1].trim());
                } else {
                    customLabelMap.remove(fp);
                }
                count++;
            }
        }
        return count;
    }

    /**
     * 生成命令指纹：去掉 -i/-o 参数部分，保留命令核心。
     */
    public static String commandFingerprint(String cmd) {
        if (cmd == null) return "";
        return cmd.replaceAll("\\s+-i\\s+\\S+\\s+-o\\s+\\S+", "").trim();
    }

    /**
     * 从用户自定义模型路径和滤镜配置生成额外的命令和标签。
     * 逻辑从 MainActivity.getExtraCommands() 迁移。
     */
    private static List<String> buildExtraCommands(String extraPath, String extraCommand,
                                                   String[] classicalFilters, String[] magickFilters,
                                                   List<String> cmdList) {
        List<String> cmdLabel = new ArrayList<>();

        String[] classicalResize = { "2", "4", "10" };
        for (String f : classicalFilters) {
            for (String s : classicalResize) {
                cmdList.add("./resize-ncnn -i input.png -o output.png  -m " + f + " -s " + s);
                cmdLabel.add("Classical-" + f + "-x" + s);
            }
        }

        String[] magickResize = { "200%", "400%", "1000%" };
        for (String f : magickFilters) {
            for (String s : magickResize) {
                cmdList.add("./magick input.png -filter " + f + " -resize " + s + " output.png ");
                cmdLabel.add("Magick-" + f + "-x" + s.replaceFirst("(\\d+)00%", "$1"));
            }
        }

        if (!extraPath.isEmpty()) {
            File[] folders = new File(extraPath).listFiles();
            if (folders == null)
                Log.e("buildExtraCommands", "extraPath folders is null");
            else {
                Arrays.sort(folders, Comparator.comparing(File::getName));
                for (File folder : folders) {
                    String name = folder.getName();
                    if (name.endsWith(".mnn") || name.startsWith("models-MNN")) {
                        if (folder.isDirectory()) {
                            File[] files = folder.listFiles();
                            if (files != null && files.length > 0) {
                                Arrays.sort(files, Comparator.comparing(File::getName));
                                for (File file : files) {
                                    if (file.getName().endsWith(".mnn")) {
                                        String[] v = getNameFromModelPath(file.getAbsolutePath(), "MNNSR");
                                        cmdList.add("./mnnsr-ncnn -i input.png -o output.png  -m "
                                                + file.getAbsolutePath() + " -s " + v[1]);
                                        cmdLabel.add(v[0]);
                                    }
                                }
                            }
                        } else {
                            String[] v = getNameFromModelPath(folder.getAbsolutePath(), "MNNSR");
                            cmdList.add("./mnnsr-ncnn -i input.png -o output.png  -m " + folder.getAbsolutePath()
                                    + " -s " + v[1]);
                            cmdLabel.add(v[0]);
                        }
                    } else if (folder.isDirectory() && name.startsWith("models")) {
                        String model = name.replace("models-", "");
                        String scaleMatcher = ".*x(\\d+).*";
                        String noiseMatcher = "";
                        String command = "./realsr-ncnn -i input.png -o output.png  -m " + folder.getAbsolutePath()
                                + " -s ";

                        if (name.matches("models-(cugan|cunet|upconv).*")) {
                            model = name.replace("models-", "Waifu2x-");
                            scaleMatcher = ".*scale(\\d+).*";
                            command = "./waifu2x-ncnn -i input.png -o output.png  -m " + folder.getAbsolutePath()
                                    + " -s ";
                            noiseMatcher = "noise(\\d+).*";
                        } else if (name.matches("models-srmd.*")) {
                            if (name.equals("models-srmd"))
                                model = "SRMD";
                            else
                                model = name.replace("models-srmd", "SRMD-");
                            command = "./srmd-ncnn -i input.png -o output.png  -m " + folder.getAbsolutePath() + " -s ";
                        } else if (name.startsWith("models-DF2K")) {
                            model = name.replace("models-", "RealSR-");
                        } else if (name.startsWith("models-mnn")) {
                        }

                        List<String> suffix = genCmdFromModel(folder, scaleMatcher, noiseMatcher);
                        for (String s : suffix) {
                            cmdList.add(command + s);
                            cmdLabel.add(model + "-x" + s.replace(" -n ", "-noise"));
                        }
                    }
                }
            }
        }

        if (!extraCommand.isEmpty()) {
            String[] cmds = extraCommand.split("\n");
            cmdLabel.addAll(Arrays.asList(cmds));
        }

        return cmdLabel;
    }

    /**
     * 从用户自定义模型路径加载文件，自动列出可用命令
     */
    private static List<String> genCmdFromModel(File folder, String scaleMatcher, String noiseMatcher) {
        List<String> list = new ArrayList<>();
        File[] files = folder.listFiles();

        List<String> names = new ArrayList<>();
        if (files != null) {
            for (File f : files) {
                String name = f.getName().toLowerCase(Locale.ROOT);
                if (name.endsWith("bin"))
                    names.add(name);
            }
        }
        String[] fileNames = names.toArray(new String[0]);
        Arrays.sort(fileNames);

        for (String name : fileNames) {
            String s;
            if (name.matches(scaleMatcher))
                s = (name.replaceFirst(scaleMatcher, "$1"));
            else
                s = "1";

            if (!noiseMatcher.isEmpty()) {
                String noise = name.replaceFirst(noiseMatcher, "$1");
                if (noise.matches("\\d+")) {
                    int n = Integer.parseInt(noise);
                    s = s + " -n " + n;
                }
            }
            if (!list.contains(s))
                list.add(s);
        }
        return list;
    }

    /**
     * 从模型文件路径提取名称和倍率
     */
    public static String[] getNameFromModelPath(String path, String type) {
        String scaleMatcher = "([xX]\\d+|\\d+[xX])";
        String s = "", name = "";
        String[] splitedPath = path.split("[/\\\\]+");

        if (splitedPath.length > 1) {
            if (splitedPath[splitedPath.length - 1].matches(scaleMatcher + "\\..+")) {
                s = splitedPath[splitedPath.length - 1].replaceFirst(scaleMatcher, "$1");
                name = splitedPath[splitedPath.length - 2];
            } else {
                String m = "[-_.\\s]+";
                name = splitedPath[splitedPath.length - 1].replaceFirst("\\.(.{1,4})$", "");
                if (name.matches("(\\d+)[xX].+"))
                    s = name.replaceFirst("(\\d+[xX]).+", "$1");
                else {
                    String[] fileTags = name.split(m);
                    for (String tag : fileTags) {
                        if (tag.matches(scaleMatcher)) {
                            s = tag;
                            break;
                        }
                    }
                }
            }
        }
        int scale = s.isEmpty() ? 1 : Integer.parseInt((s.replaceFirst("[xX]", "")));
        if (scale < 1)
            scale = 1;
        if (!name.contains(s)) {
            name = name + "-x" + scale;
        }
        name = name.replaceFirst("(models-|model-)", "");
        if (!type.isEmpty()) {
            name = type + "-" + name;
        }

        return new String[] { name, "" + scale };
    }
}
