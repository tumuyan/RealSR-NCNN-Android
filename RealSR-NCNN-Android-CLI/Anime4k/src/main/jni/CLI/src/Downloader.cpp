#ifdef ENABLE_LIBCURL

#include <stdexcept>

#include <curl/curl.h>

#include "Downloader.hpp"

#define CURL_ERROR_CHECK(code) if (code != CURLcode::CURLE_OK) throw std::runtime_error(curl_easy_strerror(code))

static std::size_t dataHandler(std::uint8_t* data, std::size_t size, std::size_t nmemb, std::vector<std::uint8_t>* buf)
{
    std::size_t length = size * nmemb;

    buf->insert(buf->end(), data, data + length);

    return length;
}

void Downloader::init()
{
    CURLcode code = CURLcode::CURLE_OK;

    code = curl_global_init(CURL_GLOBAL_ALL);
    CURL_ERROR_CHECK(code);

    curl = curl_easy_init();

    code = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, dataHandler);
    CURL_ERROR_CHECK(code);
}

void Downloader::release() noexcept
{
    if (curl != nullptr)
    {
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        curl = nullptr;
    }
}

void Downloader::download(std::string url, const std::vector<std::uint8_t>& buf)
{
    CURLcode code = CURLcode::CURLE_OK;

    code = curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    CURL_ERROR_CHECK(code);

    code = curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    CURL_ERROR_CHECK(code);

    code = curl_easy_perform(curl);
    CURL_ERROR_CHECK(code);
}

Downloader::~Downloader()
{
    release();
}

#endif // ENABLE_LIBCURL
