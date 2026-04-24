#ifndef IMAGE_PROCESSOR_H
#define IMAGE_PROCESSOR_H

#include "filesystem_utils.h"
#include <vector>
#include <set>
#include <ctime>
#include <sstream>
#include <iomanip>

struct ImageFile {
    path_t relative_path;
    path_t input_abs_path;
    path_t output_abs_path;
};

#if _WIN32
static const std::set<path_t> SUPPORTED_DECODE_EXTENSIONS = {
    PATHSTR("jpg"), PATHSTR("jpeg"),
    PATHSTR("png"),
    PATHSTR("bmp"),
    PATHSTR("webp"),
    PATHSTR("tif"), PATHSTR("tiff")
};

static const std::set<path_t> SUPPORTED_ENCODE_EXTENSIONS = {
    PATHSTR("png"),
    PATHSTR("jpg"), PATHSTR("jpeg"),
    PATHSTR("webp"),
    PATHSTR("bmp")
};
#else
static const std::set<path_t> SUPPORTED_DECODE_EXTENSIONS = {
    PATHSTR("jpg"), PATHSTR("jpeg"),
    PATHSTR("png"),
    PATHSTR("bmp"),
    PATHSTR("webp")
};

static const std::set<path_t> SUPPORTED_ENCODE_EXTENSIONS = {
    PATHSTR("png"),
    PATHSTR("jpg"), PATHSTR("jpeg"),
    PATHSTR("webp"),
    PATHSTR("bmp")
};
#endif

static bool is_supported_decode_format(const path_t& ext) {
    path_t lower_ext = ext;
    std::transform(lower_ext.begin(), lower_ext.end(), lower_ext.begin(), ::tolower);
    return SUPPORTED_DECODE_EXTENSIONS.find(lower_ext) != SUPPORTED_DECODE_EXTENSIONS.end();
}

static bool is_supported_encode_format(const path_t& ext) {
    path_t lower_ext = ext;
    std::transform(lower_ext.begin(), lower_ext.end(), lower_ext.begin(), ::tolower);
    return SUPPORTED_ENCODE_EXTENSIONS.find(lower_ext) != SUPPORTED_ENCODE_EXTENSIONS.end();
}

static path_t apply_name_pattern(const path_t& pattern,
                                  const path_t& name_noext,
                                  const path_t& prog_name,
                                  int index,
                                  const path_t& ts_timestamp,
                                  const path_t& ts_datetime,
                                  const path_t& ts_date,
                                  const path_t& ts_time)
{
    path_t result = pattern;
    path_t index_str;
    {
#if _WIN32
        wchar_t buf[32];
        swprintf(buf, 32, L"%d", index);
        index_str = buf;
#else
        char buf[32];
        sprintf(buf, "%d", index);
        index_str = buf;
#endif
    }
    size_t pos = 0;
    while ((pos = result.find(PATHSTR("{name}"), pos)) != path_t::npos)
        result.replace(pos, 6, name_noext);
    pos = 0;
    while ((pos = result.find(PATHSTR("{prog}"), pos)) != path_t::npos)
        result.replace(pos, 6, prog_name);
    pos = 0;
    while ((pos = result.find(PATHSTR("{index}"), pos)) != path_t::npos)
        result.replace(pos, 7, index_str);
    pos = 0;
    while ((pos = result.find(PATHSTR("{timestamp}"), pos)) != path_t::npos)
        result.replace(pos, 11, ts_timestamp);
    pos = 0;
    while ((pos = result.find(PATHSTR("{datetime}"), pos)) != path_t::npos)
        result.replace(pos, 10, ts_datetime);
    pos = 0;
    while ((pos = result.find(PATHSTR("{date}"), pos)) != path_t::npos)
        result.replace(pos, 6, ts_date);
    pos = 0;
    while ((pos = result.find(PATHSTR("{time}"), pos)) != path_t::npos)
        result.replace(pos, 6, ts_time);
    return result;
}

static void list_directory_recursive(const path_t& dirpath,
                                      std::vector<ImageFile>& image_files,
                                      const path_t& base_input_dir,
                                      const path_t& base_output_dir,
                                      const path_t& output_format,
                                      const path_t& name_pattern,
                                      const path_t& prog_name,
                                      int& file_index,
                                      const path_t& ts_timestamp,
                                      const path_t& ts_datetime,
                                      const path_t& ts_date,
                                      const path_t& ts_time,
                                      const path_t& current_relative = path_t())
{
#if _WIN32
    _WDIR* dir = _wopendir(dirpath.c_str());
    if (!dir) return;

    struct _wdirent* ent = 0;
    while ((ent = _wreaddir(dir)))
    {
        path_t name(ent->d_name);
        if (name == PATHSTR(".") || name == PATHSTR(".."))
            continue;

        path_t full_path = dirpath + PATHSTR('\\') + name;
        path_t rel_path = current_relative.empty() ? name : current_relative + PATHSTR('\\') + name;

        DWORD attr = GetFileAttributesW(full_path.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES)
            continue;

        if (attr & FILE_ATTRIBUTE_DIRECTORY)
        {
            list_directory_recursive(full_path, image_files, base_input_dir, base_output_dir, output_format, name_pattern, prog_name, file_index, ts_timestamp, ts_datetime, ts_date, ts_time, rel_path);
        }
        else
        {
            path_t ext = get_file_extension(name);
            if (!is_supported_decode_format(ext))
                continue;

            ImageFile img;
            img.relative_path = rel_path;
            img.input_abs_path = full_path;

            path_t name_noext = get_file_name_without_extension(name);
            file_index++;
            path_t out_name = apply_name_pattern(name_pattern, name_noext, prog_name, file_index, ts_timestamp, ts_datetime, ts_date, ts_time) + PATHSTR('.') + (output_format.empty() ? ext : output_format);

            img.output_abs_path = base_output_dir + PATHSTR('\\') + (current_relative.empty() ? out_name : current_relative + PATHSTR('\\') + out_name);

            image_files.push_back(img);
        }
    }

    _wclosedir(dir);
#else
    DIR* dir = opendir(dirpath.c_str());
    if (!dir) return;

    struct dirent* ent = 0;
    while ((ent = readdir(dir)))
    {
        path_t name(ent->d_name);
        if (name == PATHSTR(".") || name == PATHSTR(".."))
            continue;

        path_t full_path = dirpath + PATHSTR('/') + name;
        path_t rel_path = current_relative.empty() ? name : current_relative + PATHSTR('/') + name;

        struct stat st;
        if (stat(full_path.c_str(), &st) != 0)
            continue;

        if (S_ISDIR(st.st_mode))
        {
            list_directory_recursive(full_path, image_files, base_input_dir, base_output_dir, output_format, name_pattern, prog_name, file_index, ts_timestamp, ts_datetime, ts_date, ts_time, rel_path);
        }
        else
        {
            path_t ext = get_file_extension(name);
            if (!is_supported_decode_format(ext))
                continue;

            ImageFile img;
            img.relative_path = rel_path;
            img.input_abs_path = full_path;

            path_t name_noext = get_file_name_without_extension(name);
            file_index++;
            path_t out_name = apply_name_pattern(name_pattern, name_noext, prog_name, file_index, ts_timestamp, ts_datetime, ts_date, ts_time) + PATHSTR('.') + (output_format.empty() ? ext : output_format);

            img.output_abs_path = base_output_dir + PATHSTR('/') + (current_relative.empty() ? out_name : current_relative + PATHSTR('/') + out_name);

            image_files.push_back(img);
        }
    }

    closedir(dir);
#endif
}

#if _WIN32
static int create_directory_recursive(const path_t& dirpath)
{
    if (dirpath.empty()) return -1;

    size_t pos = 0;
    do
    {
        pos = dirpath.find_first_of(PATHSTR("\\/"), pos + 1);
        path_t sub = (pos == path_t::npos) ? dirpath : dirpath.substr(0, pos);

        DWORD attr = GetFileAttributesW(sub.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES)
        {
            BOOL ret = CreateDirectoryW(sub.c_str(), NULL);
            if (!ret) return -1;
        }
        else if (!(attr & FILE_ATTRIBUTE_DIRECTORY))
        {
            return -1;
        }
    } while (pos != path_t::npos);

    return 0;
}
#else
static int create_directory_recursive(const path_t& dirpath)
{
    std::string cmd = "mkdir -p \"" + dirpath + "\"";
    return system(cmd.c_str());
}
#endif

static int prepare_output_paths(const std::vector<ImageFile>& image_files)
{
    for (size_t i = 0; i < image_files.size(); i++)
    {
        path_t out_dir;
        size_t last_sep = image_files[i].output_abs_path.find_last_of(PATHSTR("/\\"));
        if (last_sep != path_t::npos)
        {
            out_dir = image_files[i].output_abs_path.substr(0, last_sep);
        }

        if (!out_dir.empty() && !path_is_directory(out_dir))
        {
            int ret = create_directory_recursive(out_dir);
            if (ret != 0)
            {
#if _WIN32
                fwprintf(stderr, L"failed to create output directory %ls\n", out_dir.c_str());
#else
                fprintf(stderr, "failed to create output directory %s\n", out_dir.c_str());
#endif
                return ret;
            }
        }
    }
    return 0;
}

static int collect_input_output_files(const path_t& inputpath,
                                       const path_t& outputpath,
                                       const path_t& format,
                                       const path_t& name_pattern,
                                       const path_t& prog_name,
                                       std::vector<path_t>& input_files,
                                       std::vector<path_t>& output_files)
{
    input_files.clear();
    output_files.clear();

    bool input_is_dir = path_is_directory(inputpath);

    if (input_is_dir)
    {
        path_t output_dir = outputpath;
        bool output_exists = path_is_directory(outputpath);

        if (!output_exists)
        {
            int ret = create_directory_recursive(output_dir);
            if (ret != 0)
            {
#if _WIN32
                fwprintf(stderr, L"failed to create output directory %ls\n", output_dir.c_str());
#else
                fprintf(stderr, "failed to create output directory %s\n", output_dir.c_str());
#endif
                return -1;
            }
        }

        path_t output_format = format;
        if (output_format.empty())
        {
            output_format = path_t();
        }

        int file_index = 0;
        std::time_t now = std::time(nullptr);
        std::tm* tm_now = std::localtime(&now);

        path_t ts_timestamp, ts_datetime, ts_date, ts_time;
        {
#if _WIN32
            wchar_t buf[64];
            swprintf(buf, 64, L"%lld", (long long)now);
            ts_timestamp = buf;
            swprintf(buf, 64, L"%04d%02d%02d_%02d%02d%02d",
                     tm_now->tm_year + 1900, tm_now->tm_mon + 1, tm_now->tm_mday,
                     tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec);
            ts_datetime = buf;
            swprintf(buf, 64, L"%04d%02d%02d",
                     tm_now->tm_year + 1900, tm_now->tm_mon + 1, tm_now->tm_mday);
            ts_date = buf;
            swprintf(buf, 64, L"%02d%02d%02d",
                     tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec);
            ts_time = buf;
#else
            char buf[64];
            sprintf(buf, "%lld", (long long)now);
            ts_timestamp = buf;
            sprintf(buf, "%04d%02d%02d_%02d%02d%02d",
                    tm_now->tm_year + 1900, tm_now->tm_mon + 1, tm_now->tm_mday,
                    tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec);
            ts_datetime = buf;
            sprintf(buf, "%04d%02d%02d",
                    tm_now->tm_year + 1900, tm_now->tm_mon + 1, tm_now->tm_mday);
            ts_date = buf;
            sprintf(buf, "%02d%02d%02d",
                    tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec);
            ts_time = buf;
#endif
        }

        std::vector<ImageFile> image_files;
        list_directory_recursive(inputpath, image_files, inputpath, output_dir, output_format, name_pattern, prog_name, file_index, ts_timestamp, ts_datetime, ts_date, ts_time);

        int ret = prepare_output_paths(image_files);
        if (ret != 0) return -1;

        for (size_t i = 0; i < image_files.size(); i++)
        {
            input_files.push_back(image_files[i].input_abs_path);
            output_files.push_back(image_files[i].output_abs_path);
        }

        if (input_files.empty())
        {
#if _WIN32
            fwprintf(stderr, L"no supported image files found in %ls\n", inputpath.c_str());
#else
            fprintf(stderr, "no supported image files found in %s\n", inputpath.c_str());
#endif
            return -1;
        }
    }
    else
    {
        path_t input_ext = get_file_extension(inputpath);
        if (!is_supported_decode_format(input_ext))
        {
#if _WIN32
            fwprintf(stderr, L"unsupported input format: %ls\n", input_ext.c_str());
#else
            fprintf(stderr, "unsupported input format: %s\n", input_ext.c_str());
#endif
            return -1;
        }

        path_t final_output = outputpath;
        bool output_is_dir = path_is_directory(outputpath);

        if (output_is_dir)
        {
            path_t input_name = inputpath;
            size_t last_sep = inputpath.find_last_of(PATHSTR("/\\"));
            if (last_sep != path_t::npos)
                input_name = inputpath.substr(last_sep + 1);

            path_t name_noext = get_file_name_without_extension(input_name);
            path_t out_ext = format.empty() ? input_ext : format;

#if _WIN32
            final_output = outputpath + PATHSTR('\\') + name_noext + PATHSTR('.') + out_ext;
#else
            final_output = outputpath + PATHSTR('/') + name_noext + PATHSTR('.') + out_ext;
#endif
        }
        else
        {
            path_t out_ext = get_file_extension(outputpath);
            if (out_ext.empty())
            {
                path_t effective_format = format.empty() ? input_ext : format;
                final_output = outputpath + PATHSTR('.') + effective_format;
            }
        }

        input_files.push_back(inputpath);
        output_files.push_back(final_output);
    }

    return 0;
}

static int filter_files_by_size_threshold(std::vector<path_t>& input_files,
                                          std::vector<path_t>& output_files,
                                          long long size_threshold,
                                          int verbose)
{
    if (size_threshold <= 0) return 0;

    std::vector<path_t> filtered_input;
    std::vector<path_t> filtered_output;

    for (size_t i = 0; i < output_files.size(); i++)
    {
#if _WIN32
        struct _stat64 file_stat;
        if (_wstati64(output_files[i].c_str(), &file_stat) == 0)
        {
            if (file_stat.st_size >= size_threshold)
            {
                if (verbose)
                {
                    fwprintf(stderr, L"[skip] %ls -> %ls (exists, size=%lld bytes >= threshold)\n",
                              input_files[i].c_str(), output_files[i].c_str(), file_stat.st_size);
                }
                continue;
            }
        }
#else
        struct stat file_stat;
        if (stat(output_files[i].c_str(), &file_stat) == 0)
        {
            if (file_stat.st_size >= size_threshold)
            {
                if (verbose)
                {
                    fprintf(stderr, "[skip] %s -> %s (exists, size=%lld bytes >= threshold)\n",
                            input_files[i].c_str(), output_files[i].c_str(), (long long)file_stat.st_size);
                }
                continue;
            }
        }
#endif

        filtered_input.push_back(input_files[i]);
        filtered_output.push_back(output_files[i]);
    }

    input_files = filtered_input;
    output_files = filtered_output;

    return 0;
}

#endif // IMAGE_PROCESSOR_H
