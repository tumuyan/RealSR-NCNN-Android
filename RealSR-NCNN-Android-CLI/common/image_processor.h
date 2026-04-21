#ifndef IMAGE_PROCESSOR_H
#define IMAGE_PROCESSOR_H

#include "filesystem_utils.h"
#include <vector>
#include <set>

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

static void list_directory_recursive(const path_t& dirpath,
                                      std::vector<ImageFile>& image_files,
                                      const path_t& base_input_dir,
                                      const path_t& base_output_dir,
                                      const path_t& output_format,
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
            list_directory_recursive(full_path, image_files, base_input_dir, base_output_dir, output_format, rel_path);
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
            path_t out_name;
            if (output_format.empty())
            {
                out_name = name_noext + PATHSTR('.') + ext;
            }
            else
            {
                out_name = name_noext + PATHSTR('.') + output_format;
            }

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
            list_directory_recursive(full_path, image_files, base_input_dir, base_output_dir, output_format, rel_path);
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
            path_t out_name;
            if (output_format.empty())
            {
                out_name = name_noext + PATHSTR('.') + ext;
            }
            else
            {
                out_name = name_noext + PATHSTR('.') + output_format;
            }

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

        std::vector<ImageFile> image_files;
        list_directory_recursive(inputpath, image_files, inputpath, output_dir, output_format);

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

        input_files.push_back(inputpath);
        output_files.push_back(outputpath);
    }

    return 0;
}

#endif // IMAGE_PROCESSOR_H
