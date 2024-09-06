#pragma once

#include <defs/assert.hpp>
#include <defs/minimal_geode.hpp>
#include <stdint.h>
#include <matjson.hpp>
#include <Geode/utils/Result.hpp>
#include <Geode/utils/Task.hpp>

#include <util/time.hpp>
#include <util/singleton.hpp>

struct CurlRequest;

struct GLOBED_DLL CurlResponse {
    CurlResponse() {}
    CurlResponse(CurlResponse&&) = default;
    CurlResponse& operator=(CurlResponse&&) = default;
    CurlResponse(const CurlResponse&) = default;
    CurlResponse& operator=(const CurlResponse&) = default;

    static CurlResponse fatalError(std::string_view message);

    int getCode() const;
    bool ok() const;

    std::string header(std::string_view key) const;
    const std::unordered_map<std::string, std::string>& headers(std::string_view key) const;

    Result<std::vector<uint8_t>> data();
    Result<std::string> text();
    Result<matjson::Value> json();

    template <typename T>
    Result<T> json() {
        GLOBED_UNWRAP_INTO(this->json(), auto parsed);

        if (!parsed.is<T>()) {
            auto tname = std::string(typeid(T).name());
#ifdef GEODE_IS_WINDOWS
            tname = tname.substr(tname.find(' ') + 1);
#endif

            return Err(fmt::format("failed to parse JSON as {}", tname));
        }

        return Ok(parsed.as<T>());
    }

    // Gets either the fatal error message, or formats the response in format "code X: data"
    std::string getError();

private:
    int m_code = 0;
    std::string m_fatalMessage;
    std::vector<uint8_t> m_rawResponse;
    std::unordered_map<std::string, std::string> m_headers;

    friend class CurlManager;
};

class GLOBED_DLL CurlManager : public SingletonBase<CurlManager> {
    friend class SingletonBase;

public:
    using Task = geode::Task<CurlResponse>;

    const char* getCurlVersion();
    Task send(CurlRequest& req);

private:
};

struct CurlRequest {
    CurlRequest();
    CurlRequest& header(std::string_view name, std::string_view value);
    CurlRequest& param(std::string_view name, std::string_view value);

    template <std::integral T>
    CurlRequest& param(std::string_view name, T value) {
        return this->param(name, std::to_string(value));
    }

    CurlRequest& userAgent(std::string_view name);
    CurlRequest& timeout(size_t seconds);

    template <typename Rep, typename Period>
    CurlRequest& timeout(const util::time::duration<Rep, Period>& duration) {
        return this->timeout(static_cast<size_t>(util::time::asSeconds(duration)));
    }

    CurlRequest& followRedirects(bool follow);
    CurlRequest& body(const std::vector<uint8_t>& data);
    CurlRequest& body(std::vector<uint8_t>&& data);
    CurlRequest& body(std::string_view str);
    CurlRequest& bodyJSON(const matjson::Value& json);

    CurlRequest& get(std::string_view url);
    CurlRequest& post(std::string_view url);
    CurlRequest& put(std::string_view url);
    CurlRequest& patch(std::string_view url);
    CurlRequest& delete_(std::string_view url);
    CurlRequest& customMethod(std::string_view url, std::string_view method);
    CurlRequest& encrypted(bool enc);

    CurlManager::Task send();

    struct Data;
    std::shared_ptr<Data> m_data;
};

