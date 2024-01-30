#ifndef ANIME4KCPP_CLI_DOWNLOADER_HPP
#define ANIME4KCPP_CLI_DOWNLOADER_HPP

#ifdef ENABLE_LIBCURL

#include <cstdint>
#include <string>
#include <vector>

class Downloader
{
public:
    ~Downloader();

    void init();
    void release() noexcept;
    void download(std::string url, const std::vector<std::uint8_t>& buf);

private:
    void* curl = nullptr;
};

#endif // ENABLE_LIBCURL

#endif // !ANIME4KCPP_CLI_DOWNLOADER_HPP
