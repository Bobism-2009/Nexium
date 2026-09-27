// HTTP requests on a background thread (WinHTTP), polled from the frame loop
// so the IDE keeps drawing while the agent waits for a model. Included from
// native.hpp.
#pragma once
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>

struct NxHttp {
    std::thread worker;
    std::atomic<bool> done{false};
    std::atomic<bool> cancel{false};
    int status = 0;
    std::string body;
    std::string error;
};

static std::vector<std::unique_ptr<NxHttp>> nx__http;

typedef HINTERNET(WINAPI* nx_WinHttpOpen_t)(LPCWSTR, DWORD, LPCWSTR, LPCWSTR, DWORD);
typedef BOOL(WINAPI* nx_WinHttpCrackUrl_t)(LPCWSTR, DWORD, DWORD, LPURL_COMPONENTS);
typedef HINTERNET(WINAPI* nx_WinHttpConnect_t)(HINTERNET, LPCWSTR, INTERNET_PORT, DWORD);
typedef HINTERNET(WINAPI* nx_WinHttpOpenRequest_t)(HINTERNET, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR*, DWORD);
typedef BOOL(WINAPI* nx_WinHttpSendRequest_t)(HINTERNET, LPCWSTR, DWORD, LPVOID, DWORD, DWORD, DWORD_PTR);
typedef BOOL(WINAPI* nx_WinHttpReceiveResponse_t)(HINTERNET, LPVOID);
typedef BOOL(WINAPI* nx_WinHttpQueryHeaders_t)(HINTERNET, DWORD, LPCWSTR, LPVOID, LPDWORD, LPDWORD);
typedef BOOL(WINAPI* nx_WinHttpReadData_t)(HINTERNET, LPVOID, DWORD, LPDWORD);
typedef BOOL(WINAPI* nx_WinHttpCloseHandle_t)(HINTERNET);
typedef BOOL(WINAPI* nx_WinHttpSetTimeouts_t)(HINTERNET, int, int, int, int);

// WinHTTP is loaded at run time, so the build needs no import library for it.
struct NxWinHttp {
    bool ok = false;
    nx_WinHttpOpen_t Open;
    nx_WinHttpCrackUrl_t CrackUrl;
    nx_WinHttpConnect_t Connect;
    nx_WinHttpOpenRequest_t OpenRequest;
    nx_WinHttpSendRequest_t SendRequest;
    nx_WinHttpReceiveResponse_t ReceiveResponse;
    nx_WinHttpQueryHeaders_t QueryHeaders;
    nx_WinHttpReadData_t ReadData;
    nx_WinHttpCloseHandle_t CloseHandle;
    nx_WinHttpSetTimeouts_t SetTimeouts;
};

static NxWinHttp& nx__winhttp() {
    static NxWinHttp w;
    static bool tried = false;
    if (tried) return w;
    tried = true;
    HMODULE m = LoadLibraryA("winhttp.dll");
    if (!m) return w;
    w.Open = (nx_WinHttpOpen_t)(void*)GetProcAddress(m, "WinHttpOpen");
    w.CrackUrl = (nx_WinHttpCrackUrl_t)(void*)GetProcAddress(m, "WinHttpCrackUrl");
    w.Connect = (nx_WinHttpConnect_t)(void*)GetProcAddress(m, "WinHttpConnect");
    w.OpenRequest = (nx_WinHttpOpenRequest_t)(void*)GetProcAddress(m, "WinHttpOpenRequest");
    w.SendRequest = (nx_WinHttpSendRequest_t)(void*)GetProcAddress(m, "WinHttpSendRequest");
    w.ReceiveResponse = (nx_WinHttpReceiveResponse_t)(void*)GetProcAddress(m, "WinHttpReceiveResponse");
    w.QueryHeaders = (nx_WinHttpQueryHeaders_t)(void*)GetProcAddress(m, "WinHttpQueryHeaders");
    w.ReadData = (nx_WinHttpReadData_t)(void*)GetProcAddress(m, "WinHttpReadData");
    w.CloseHandle = (nx_WinHttpCloseHandle_t)(void*)GetProcAddress(m, "WinHttpCloseHandle");
    w.SetTimeouts = (nx_WinHttpSetTimeouts_t)(void*)GetProcAddress(m, "WinHttpSetTimeouts");
    w.ok = w.Open && w.CrackUrl && w.Connect && w.OpenRequest && w.SendRequest && w.ReceiveResponse &&
           w.QueryHeaders && w.ReadData && w.CloseHandle && w.SetTimeouts;
    return w;
}

static void nx__http_run(NxHttp* r, std::string method, std::string url, std::string headers, std::string body) {
    NxWinHttp& w = nx__winhttp();
    if (!w.ok) {
        r->error = "WinHTTP is not available";
        r->done = true;
        return;
    }
    std::wstring wurl = nx__wide(url);
    URL_COMPONENTS uc = {};
    uc.dwStructSize = sizeof uc;
    wchar_t host[256];
    wchar_t path[4096];
    uc.lpszHostName = host;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = 4096;
    if (!w.CrackUrl(wurl.c_str(), 0, 0, &uc)) {
        r->error = "Bad URL: " + url;
        r->done = true;
        return;
    }
    HINTERNET s = w.Open(L"Nexium/2", 0 /* WINHTTP_ACCESS_TYPE_DEFAULT_PROXY */, nullptr, nullptr, 0);
    HINTERNET c = s ? w.Connect(s, host, uc.nPort, 0) : nullptr;
    DWORD flags = uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET q = c ? w.OpenRequest(c, nx__wide(method).c_str(), path, nullptr, nullptr, nullptr, flags) : nullptr;
    if (!q) {
        r->error = "Could not connect to " + url;
    } else {
        w.SetTimeouts(q, 15000, 15000, 30000, 300000);
        std::wstring hdr = nx__wide(headers);
        BOOL ok = w.SendRequest(q, hdr.empty() ? nullptr : hdr.c_str(), hdr.empty() ? 0 : (DWORD)-1,
                                body.empty() ? nullptr : (LPVOID)body.data(), (DWORD)body.size(),
                                (DWORD)body.size(), 0);
        if (ok) ok = w.ReceiveResponse(q, nullptr);
        if (!ok) {
            DWORD e = GetLastError();
            r->error = e == 12002 ? "The request timed out" : (e == 12007 ? "Could not resolve the host name"
                     : (e == 12029 ? "Could not connect to the server" : "Request failed (WinHTTP error " + std::to_string(e) + ")"));
        } else {
            DWORD code = 0, len = sizeof code;
            w.QueryHeaders(q, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &code, &len, nullptr);
            r->status = (int)code;
            char buf[16384];
            DWORD n = 0;
            while (!r->cancel && w.ReadData(q, buf, sizeof buf, &n) && n > 0) r->body.append(buf, n);
            if (r->cancel) r->error = "Stopped";
        }
    }
    if (q) w.CloseHandle(q);
    if (c) w.CloseHandle(c);
    if (s) w.CloseHandle(s);
    r->done = true;
}

// Starts a request. headers are "Name: value" lines separated by "\n".
// Returns an id for the nx_http_* calls.
static int nx_http_start(const std::string& method, const std::string& url, const std::string& headers,
                         const std::string& body) {
    std::unique_ptr<NxHttp> r(new NxHttp());
    std::string hdr;
    for (char ch : headers) {
        if (ch == '\n') hdr += "\r\n";
        else hdr += ch;
    }
    NxHttp* raw = r.get();
    raw->worker = std::thread(nx__http_run, raw, method, url, hdr, body);
    for (size_t i = 0; i < nx__http.size(); i++) {
        if (!nx__http[i]) {
            nx__http[i] = std::move(r);
            return (int)i;
        }
    }
    nx__http.push_back(std::move(r));
    return (int)nx__http.size() - 1;
}

static NxHttp* nx__http_at(int id) {
    if (id < 0 || id >= (int)nx__http.size()) return nullptr;
    return nx__http[(size_t)id].get();
}

static int nx_http_done(int id) {
    NxHttp* r = nx__http_at(id);
    return !r || r->done ? 1 : 0;
}
static int nx_http_status(int id) {
    NxHttp* r = nx__http_at(id);
    return r && r->done ? r->status : 0;
}
static std::string nx_http_body(int id) {
    NxHttp* r = nx__http_at(id);
    return r && r->done ? r->body : std::string();
}
static std::string nx_http_error(int id) {
    NxHttp* r = nx__http_at(id);
    return r && r->done ? r->error : std::string();
}
static void nx_http_cancel(int id) {
    NxHttp* r = nx__http_at(id);
    if (r) r->cancel = true;
}
// Frees a finished request (a running one is left to finish on its own).
static void nx_http_free(int id) {
    NxHttp* r = nx__http_at(id);
    if (!r) return;
    if (!r->done) {
        r->cancel = true;
        r->worker.detach();
        nx__http[(size_t)id].release();
        nx__http[(size_t)id].reset();
        return;
    }
    if (r->worker.joinable()) r->worker.join();
    nx__http[(size_t)id].reset();
}

// An environment variable as UTF-8 ("" when unset).
static std::string nx_env(const std::string& name) {
    DWORD n = GetEnvironmentVariableW(nx__wide(name).c_str(), nullptr, 0);
    if (n == 0) return std::string();
    std::wstring w((size_t)n, L'\0');
    GetEnvironmentVariableW(nx__wide(name).c_str(), &w[0], n);
    w.resize(n - 1);
    return nx__utf8(w.c_str());
}
