#include "../cffi.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace minipy {

namespace {

namespace fs = std::filesystem;

std::string u8Text(const fs::path& p){
    std::u8string u8 = p.generic_u8string();
    return std::string(u8.begin(), u8.end());
}

fs::path fsPathFromUtf8(const std::string& text){
    std::u8string u8(text.begin(), text.end());
    return fs::path(u8);
}

bool wildMatchImpl(const std::string& name, size_t ni, const std::string& pat, size_t pi){
    while (pi < pat.size()){
        char pc = pat[pi];
        if (pc == '*'){
            if (pi + 1 == pat.size()) return true;
            for (size_t k = ni; k <= name.size(); ++k){
                if (wildMatchImpl(name, k, pat, pi + 1)) return true;
            }
            return false;
        }
        if (ni >= name.size()) return false;
        if (pc == '?'){
            ++ni; ++pi; continue;
        }
        if (pc == '['){
            size_t end = pat.find(']', pi + 1);
            if (end == std::string::npos) end = pat.size();
            bool negate = false;
            size_t k = pi + 1;
            if (k < end && (pat[k] == '!' || pat[k] == '^')){ negate = true; ++k; }
            bool matched = false;
            while (k < end){
                char lo = pat[k];
                if (k + 2 < end && pat[k + 1] == '-'){
                    char hi = pat[k + 2];
                    if (name[ni] >= lo && name[ni] <= hi) matched = true;
                    k += 3;
                } else {
                    if (name[ni] == lo) matched = true;
                    ++k;
                }
            }
            if (matched == negate) return false;
            ++ni;
            pi = end < pat.size() ? end + 1 : end;
            continue;
        }
        if (name[ni] != pc) return false;
        ++ni; ++pi;
    }
    return ni == name.size();
}

bool wildMatch(const std::string& name, const std::string& pattern){
    return wildMatchImpl(name, 0, pattern, 0);
}

// ---------- 文件复制 ----------
bool copyFileImpl(const fs::path& src, const fs::path& dst){
    std::error_code ec;
    fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
    return !ec;
}

void copyAttrs(const fs::path& src, const fs::path& dst, bool times){
    std::error_code ec;
    auto st = fs::status(src, ec);
    if (!ec) fs::permissions(dst, st.permissions(), ec);
    if (times){
        auto t = fs::last_write_time(src, ec);
        if (!ec) fs::last_write_time(dst, t, ec);
    }
}

void removeTree(const fs::path& path){
    std::error_code ec;
    fs::remove_all(path, ec);
}

// ---------- ZIP (stored) ----------
uint32_t crc32Of(const std::string& data){
    static uint32_t table[256];
    static bool init = false;
    if (!init){
        for (uint32_t i = 0; i < 256; ++i){
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        init = true;
    }
    uint32_t crc = 0xFFFFFFFFu;
    for (unsigned char ch : data){
        crc = table[(crc ^ ch) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

struct ZipRecord{
    std::string name;
    uint32_t crc = 0;
    uint64_t size = 0;
    uint32_t offset = 0;
};

void put16(std::ostringstream& out, uint16_t v){
    out << static_cast<char>(v & 0xFF) << static_cast<char>((v >> 8) & 0xFF);
}

void put32(std::ostringstream& out, uint32_t v){
    out << static_cast<char>(v & 0xFF) << static_cast<char>((v >> 8) & 0xFF)
        << static_cast<char>((v >> 16) & 0xFF) << static_cast<char>((v >> 24) & 0xFF);
}

void writeZip(const std::string& archive, const std::vector<std::pair<std::string, std::string>>& files){
    std::ostringstream out;
    std::vector<ZipRecord> records;
    for (auto& entry : files){
        ZipRecord rec;
        rec.name = entry.first;
        for (auto& ch : rec.name) if (ch == '\\') ch = '/';
        rec.crc = crc32Of(entry.second);
        rec.size = entry.second.size();
        rec.offset = static_cast<uint32_t>(out.str().size());
        put32(out, 0x04034b50);
        put16(out, 20);
        put16(out, 0);
        put16(out, 0);
        put16(out, 0);
        put16(out, 0);
        put32(out, rec.crc);
        put32(out, static_cast<uint32_t>(rec.size));
        put32(out, static_cast<uint32_t>(rec.size));
        put16(out, static_cast<uint16_t>(rec.name.size()));
        put16(out, 0);
        out.write(rec.name.data(), static_cast<std::streamsize>(rec.name.size()));
        out.write(entry.second.data(), static_cast<std::streamsize>(entry.second.size()));
        records.push_back(rec);
    }
    uint32_t cdOffset = static_cast<uint32_t>(out.str().size());
    for (auto& rec : records){
        put32(out, 0x02014b50);
        put16(out, 20);
        put16(out, 20);
        put16(out, 0);
        put16(out, 0);
        put16(out, 0);
        put16(out, 0);
        put32(out, rec.crc);
        put32(out, static_cast<uint32_t>(rec.size));
        put32(out, static_cast<uint32_t>(rec.size));
        put16(out, static_cast<uint16_t>(rec.name.size()));
        put16(out, 0);
        put16(out, 0);
        put16(out, 0);
        put16(out, 0);
        put32(out, 0);
        put32(out, rec.offset);
        out.write(rec.name.data(), static_cast<std::streamsize>(rec.name.size()));
    }
    uint32_t cdSize = static_cast<uint32_t>(out.str().size()) - cdOffset;
    put32(out, 0x06054b50);
    put16(out, 0);
    put16(out, 0);
    put16(out, static_cast<uint16_t>(records.size()));
    put16(out, static_cast<uint16_t>(records.size()));
    put32(out, cdSize);
    put32(out, cdOffset);
    put16(out, 0);
    std::ofstream file(archive, std::ios::binary | std::ios::trunc);
    std::string bytes = out.str();
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

uint16_t read16(const std::string& data, size_t pos){
    if (pos + 2 > data.size()) return 0;
    return static_cast<uint16_t>((static_cast<unsigned char>(data[pos]) |
                                  (static_cast<unsigned char>(data[pos + 1]) << 8)));
}

uint32_t read32(const std::string& data, size_t pos){
    if (pos + 4 > data.size()) return 0;
    return static_cast<uint32_t>((static_cast<unsigned char>(data[pos]) |
                                  (static_cast<unsigned char>(data[pos + 1]) << 8) |
                                  (static_cast<unsigned char>(data[pos + 2]) << 16) |
                                  (static_cast<unsigned char>(data[pos + 3]) << 24)));
}

bool extractZip(const std::string& archive, const std::string& target, std::string& error){
    std::ifstream file(archive, std::ios::binary);
    if (!file.is_open()){
        error = "file not found";
        return false;
    }
    std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    size_t eocd = std::string::npos;
    if (data.size() >= 22){
        for (size_t i = data.size() - 22; i + 4 <= data.size(); --i){
            if (read32(data, i) == 0x06054b50){ eocd = i; break; }
            if (i == 0) break;
        }
    }
    if (eocd == std::string::npos){
        error = "not a zip file";
        return false;
    }
    uint16_t count = read16(data, eocd + 10);
    uint32_t cdOffset = read32(data, eocd + 16);
    size_t pos = cdOffset;
    fs::path root = fsPathFromUtf8(target);
    for (uint16_t i = 0; i < count; ++i){
        if (pos + 46 > data.size()) break;
        if (read32(data, pos) != 0x02014b50) break;
        uint16_t method = read16(data, pos + 10);
        uint32_t crc = read32(data, pos + 16);
        uint32_t csize = read32(data, pos + 20);
        uint16_t nlen = read16(data, pos + 28);
        uint16_t elen = read16(data, pos + 30);
        uint16_t clen = read16(data, pos + 32);
        uint32_t localOffset = read32(data, pos + 42);
        std::string name = data.substr(pos + 46, nlen);
        pos += 46 + nlen + elen + clen;
        if (method != 0){
            error = "unsupported compression in zip";
            return false;
        }
        size_t lp = localOffset;
        if (read32(data, lp) != 0x04034b50) continue;
        uint16_t lnlen = read16(data, lp + 26);
        uint16_t lelen = read16(data, lp + 28);
        size_t dataPos = lp + 30 + lnlen + lelen;
        std::string content = data.substr(dataPos, csize);
        (void)crc;
        fs::path out = root / fsPathFromUtf8(name);
        if (name.size() && (name.back() == '/' || name.back() == '\\')){
            std::error_code ec;
            fs::create_directories(out, ec);
            continue;
        }
        std::error_code ec;
        fs::create_directories(out.parent_path(), ec);
        std::ofstream of(out, std::ios::binary | std::ios::trunc);
        of.write(content.data(), static_cast<std::streamsize>(content.size()));
    }
    return true;
}

// ---------- TAR (ustar, uncompressed) ----------
void tarField(std::ostringstream& out, const std::string& text, size_t width){
    std::string value = text.substr(0, width);
    out.write(value.data(), static_cast<std::streamsize>(value.size()));
    for (size_t i = value.size(); i < width; ++i) out << '\0';
}

void tarOctal(std::ostringstream& out, long long value, size_t width){
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%0*llo", static_cast<int>(width - 1), value);
    std::string text(buffer);
    tarField(out, text, width);
}

void writeTar(const std::string& archive, const std::vector<std::pair<std::string, std::string>>& files){
    std::ostringstream out;
    for (auto& entry : files){
        std::string name = entry.first;
        for (auto& ch : name) if (ch == '\\') ch = '/';
        bool directory = name.size() && name.back() == '/';
        std::ostringstream header;
        tarField(header, name, 100);
        tarOctal(header, 0644, 8);
        tarOctal(header, 0, 8);
        tarOctal(header, 0, 8);
        tarOctal(header, directory ? 0 : static_cast<long long>(entry.second.size()), 12);
        tarOctal(header, 0, 12);
        header << (directory ? '5' : '0');
        header << "ustar";
        header << '\0';
        header << "00";
        tarField(header, "", 32);
        tarField(header, "", 32);
        tarField(header, "", 8);
        tarField(header, "", 8);
        tarField(header, "", 155);
        header << '\0';
        out.write(header.str().data(), static_cast<std::streamsize>(header.str().size()));
        if (!directory){
            out.write(entry.second.data(), static_cast<std::streamsize>(entry.second.size()));
            size_t pad = (512 - (entry.second.size() % 512)) % 512;
            for (size_t i = 0; i < pad; ++i) out << '\0';
        }
    }
    for (int i = 0; i < 2; ++i){
        for (size_t k = 0; k < 512; ++k) out << '\0';
    }
    std::ofstream file(archive, std::ios::binary | std::ios::trunc);
    std::string bytes = out.str();
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

bool extractTar(const std::string& archive, const std::string& target, std::string& error){
    std::ifstream file(archive, std::ios::binary);
    if (!file.is_open()){
        error = "file not found";
        return false;
    }
    std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    size_t pos = 0;
    fs::path root = fsPathFromUtf8(target);
    while (pos + 512 <= data.size()){
        std::string header = data.substr(pos, 512);
        if (header.find('\0') == 0 || read32(header, 0) == 0 && read16(header, 0) == 0) break;
        std::string name = header.substr(0, 100);
        size_t nul = name.find('\0');
        if (nul != std::string::npos) name = name.substr(0, nul);
        if (name.empty()) break;
        std::string sizeText = header.substr(124, 12);
        long long size = std::strtoll(sizeText.c_str(), nullptr, 8);
        char type = header[156];
        pos += 512;
        fs::path out = root / fsPathFromUtf8(name);
        if (type == '5' || name.back() == '/'){
            std::error_code ec;
            fs::create_directories(out, ec);
        } else {
            std::error_code ec;
            fs::create_directories(out.parent_path(), ec);
            std::ofstream of(out, std::ios::binary | std::ios::trunc);
            if (size > 0 && pos + static_cast<size_t>(size) <= data.size())
                of.write(data.data() + pos, static_cast<std::streamsize>(size));
        }
        pos += static_cast<size_t>(size);
        pos += (512 - (static_cast<size_t>(size) % 512)) % 512;
    }
    return true;
}

// ---------- 公共函数 ----------
static value fnCopyFile(cffi& a){
    fs::path src = fsPathFromUtf8(a.vm->toString(a.arg(0)));
    fs::path dst = fsPathFromUtf8(a.vm->toString(a.arg(1)));
    std::error_code ec;
    if (fs::is_directory(dst, ec)){
        a.vm->raiseError("IsADirectoryError",
            "[Errno 21] Is a directory: '" + a.vm->toString(a.arg(1)) + "'");
        return a.none();
    }
    bool same = false;
    if (fs::exists(src, ec) && fs::exists(dst, ec))
        same = fs::equivalent(src, dst, ec);
    if (same) return a.mkstr(a.vm->toString(a.arg(1)));
    if (!copyFileImpl(src, dst)){
        a.vm->raiseError("FileNotFoundError",
            "[Errno 2] No such file or directory: '" + a.vm->toString(a.arg(0)) + "'");
        return a.none();
    }
    return a.mkstr(a.vm->toString(a.arg(1)));
}

static value fnCopy(cffi& a){
    value r = fnCopyFile(a);
    if (a.vm->hasError()) return a.none();
    copyAttrs(fsPathFromUtf8(a.vm->toString(a.arg(0))),
              fsPathFromUtf8(a.vm->toString(a.arg(1))), false);
    return r;
}

static value fnCopy2(cffi& a){
    value r = fnCopyFile(a);
    if (a.vm->hasError()) return a.none();
    copyAttrs(fsPathFromUtf8(a.vm->toString(a.arg(0))),
              fsPathFromUtf8(a.vm->toString(a.arg(1))), true);
    return r;
}

static value fnCopyMode(cffi& a){
    std::error_code ec;
    auto st = fs::status(fsPathFromUtf8(a.vm->toString(a.arg(0))), ec);
    if (!ec) fs::permissions(fsPathFromUtf8(a.vm->toString(a.arg(1))), st.permissions(), ec);
    return a.none();
}

static value fnCopyStat(cffi& a){
    copyAttrs(fsPathFromUtf8(a.vm->toString(a.arg(0))),
              fsPathFromUtf8(a.vm->toString(a.arg(1))), true);
    return a.none();
}

bool isIgnored(cffi& a, value& ignoreFn, const std::string& dir,
               const std::vector<std::string>& names, std::set<std::string>& ignored){
    if (!ignoreFn.ptr) return false;
    std::vector<value> args;
    args.push_back(a.mkstr(dir));
    std::vector<value> nameValues;
    for (auto& n : names) nameValues.push_back(a.mkstr(n));
    args.push_back(a.mklist(nameValues));
    bool hadError = a.vm->hasError();
    value result = a.vm->callValue(ignoreFn, args);
    if (a.vm->hasError()){
        if (!hadError) a.vm->clearErr();
        return false;
    }
    if (result.type == "list" || result.type == "tuple" || result.type == "set"){
        std::vector<value> items;
        a.vm->materializeIterable(result, items);
        for (auto& item : items) ignored.insert(a.vm->toString(item));
    }
    return true;
}

bool copyTreeRec(cffi& a, const fs::path& src, const fs::path& dst, bool symlinks,
                 bool dirsExistOk, value& ignoreFn, bool copy2Mode){
    std::error_code ec;
    if (!fs::exists(src, ec)){
        a.vm->raiseError("FileNotFoundError",
            "[Errno 2] No such file or directory: '" + u8Text(src) + "'");
        return false;
    }
    if (fs::exists(dst, ec) && !dirsExistOk){
        a.vm->raiseError("FileExistsError",
            "[Errno 17] File exists: '" + u8Text(dst) + "'");
        return false;
    }
    fs::create_directories(dst, ec);
    std::vector<std::string> names;
    fs::directory_iterator it(src, ec);
    if (ec) return false;
    for (auto& entry : it) names.push_back(u8Text(entry.path().filename()));
    std::sort(names.begin(), names.end());
    std::set<std::string> ignored;
    isIgnored(a, ignoreFn, u8Text(src), names, ignored);
    for (auto& name : names){
        if (ignored.count(name)) continue;
        fs::path s = src / fsPathFromUtf8(name);
        fs::path d = dst / fsPathFromUtf8(name);
        if (fs::is_directory(s, ec)){
            if (!copyTreeRec(a, s, d, symlinks, dirsExistOk, ignoreFn, copy2Mode)) return false;
        } else {
            if (!copyFileImpl(s, d)) return false;
            copyAttrs(s, d, copy2Mode);
        }
    }
    return true;
}

static value fnCopyTree(cffi& a){
    fs::path src = fsPathFromUtf8(a.vm->toString(a.arg(0)));
    fs::path dst = fsPathFromUtf8(a.vm->toString(a.arg(1)));
    bool symlinks = a.has_kwarg("symlinks") ? a.kwarg_bool("symlinks", false) : false;
    bool dirsExistOk = a.has_kwarg("dirs_exist_ok")
                           ? a.kwarg_bool("dirs_exist_ok", false) : false;
    value ignoreFn = a.has_kwarg("ignore") ? a.kwarg("ignore") : a.none();
    if (!copyTreeRec(a, src, dst, symlinks, dirsExistOk, ignoreFn, true))
        return a.none();
    return a.mkstr(u8Text(dst));
}

static value fnRmtree(cffi& a){
    std::string path = a.vm->toString(a.arg(0));
    bool ignoreErrors = a.has_kwarg("ignore_errors")
                            ? a.kwarg_bool("ignore_errors", false) : false;
    removeTree(fsPathFromUtf8(path));
    if (!ignoreErrors){
        std::error_code ec;
        if (fs::exists(fsPathFromUtf8(path), ec))
            a.vm->raiseError("OSError", "Cannot remove tree: '" + path + "'");
    }
    return a.none();
}

static value fnMove(cffi& a){
    std::string srcText = a.vm->toString(a.arg(0));
    std::string dstText = a.vm->toString(a.arg(1));
    fs::path src = fsPathFromUtf8(srcText);
    fs::path dst = fsPathFromUtf8(dstText);
    std::error_code ec;
    if (fs::exists(dst, ec) && fs::is_directory(dst, ec))
        dst = dst / src.filename();
    fs::rename(src, dst, ec);
    if (ec){
        if (fs::is_directory(src, ec)){
            value noIgnore;
            if (!copyTreeRec(a, src, dst, false, false, noIgnore, true)) return a.none();
            removeTree(src);
        } else {
            if (!copyFileImpl(src, dst)) return a.none();
            copyAttrs(src, dst, true);
            fs::remove(src, ec);
        }
    }
    std::error_code ec2;
    if (!fs::exists(dst, ec2)){
        a.vm->raiseError("FileNotFoundError",
            "[Errno 2] No such file or directory: '" + srcText + "'");
        return a.none();
    }
    return a.mkstr(dstText);
}

static value fnWhich(cffi& a){
    std::string cmd = a.vm->toString(a.arg(0));
#if defined(_WIN32)
    std::vector<std::string> dirs;
    if (a.has_kwarg("path") && a.kwarg("path").type != "none"){
        std::string pathText = a.vm->toString(a.kwarg("path"));
        size_t start = 0;
        while (start <= pathText.size()){
            size_t sep = pathText.find_first_of(";:", start);
            std::string part = sep == std::string::npos ? pathText.substr(start)
                                                        : pathText.substr(start, sep - start);
            if (!part.empty()) dirs.push_back(part);
            if (sep == std::string::npos) break;
            start = sep + 1;
        }
    } else {
        std::string pathText = getenv("PATH") ? getenv("PATH") : "";
        size_t start = 0;
        while (start <= pathText.size()){
            size_t sep = pathText.find(';', start);
            std::string part = sep == std::string::npos ? pathText.substr(start)
                                                        : pathText.substr(start, sep - start);
            if (!part.empty()) dirs.push_back(part);
            if (sep == std::string::npos) break;
            start = sep + 1;
        }
    }
    std::string exts = getenv("PATHEXT") ? getenv("PATHEXT") : ".EXE;.BAT;.CMD";
    std::vector<std::string> extList;
    size_t s2 = 0;
    while (s2 <= exts.size()){
        size_t sep = exts.find(';', s2);
        std::string part = sep == std::string::npos ? exts.substr(s2)
                                                    : exts.substr(s2, sep - s2);
        if (!part.empty()) extList.push_back(part);
        if (sep == std::string::npos) break;
        s2 = sep + 1;
    }
    std::vector<std::string> candidates{cmd};
    std::string lower = cmd;
    for (auto& ch : lower) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    bool hasExt = false;
    for (auto& ext : extList){
        std::string lext = ext;
        for (auto& ch : lext) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        if (lower.size() >= lext.size() &&
            lower.substr(lower.size() - lext.size()) == lext){ hasExt = true; break; }
    }
    if (!hasExt) for (auto& ext : extList) candidates.push_back(cmd + ext);
    for (auto& dir : dirs){
        for (auto& candidate : candidates){
            fs::path p = fsPathFromUtf8(dir) / fsPathFromUtf8(candidate);
            std::error_code ec;
            if (fs::is_regular_file(p, ec)) return a.mkstr(u8Text(p));
        }
    }
#else
    (void)cmd;
#endif
    return a.mkstr("");
}

static value fnDiskUsage(cffi& a){
    fs::space_info info = fs::space(fsPathFromUtf8(a.vm->toString(a.arg(0))));
    std::vector<value> result;
    result.push_back(a.mkint(static_cast<long long>(info.capacity)));
    result.push_back(a.mkint(static_cast<long long>(info.capacity - info.free)));
    result.push_back(a.mkint(static_cast<long long>(info.free)));
    return a.vm->makeTuple(result);
}

static value fnGetTerminalSize(cffi& a){
    long long columns = 80, lines = 24;
    if (a.has_kwarg("fallback")){
        value fb = a.kwarg("fallback");
        if (fb.type == "list" || fb.type == "tuple"){
            auto* items = static_cast<std::vector<value>*>(fb.ptr);
            if (items->size() >= 1) columns = a.vm->toInt((*items)[0]);
            if (items->size() >= 2) lines = a.vm->toInt((*items)[1]);
        }
    }
    if (const char* env = getenv("COLUMNS")){
        long long v = std::strtoll(env, nullptr, 10);
        if (v > 0) columns = v;
    }
    if (const char* env = getenv("LINES")){
        long long v = std::strtoll(env, nullptr, 10);
        if (v > 0) lines = v;
    }
#if defined(_WIN32)
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info)){
        if (columns == 80) columns = info.srWindow.Right - info.srWindow.Left + 1;
        if (lines == 24) lines = info.srWindow.Bottom - info.srWindow.Top + 1;
    }
#endif
    std::vector<value> result;
    result.push_back(a.mkint(columns));
    result.push_back(a.mkint(lines));
    return a.vm->makeTuple(result);
}

// ignore_patterns 返回可调用的忽略函数
// ignore_patterns returns a callable ignore function
// ᠮᠠᠷᠲᠠᠬᠤ ᠦᠢᠯᠡ ᠬᠡᠷᠡᠭᠰᠡᠯ
struct IgnorePatternState{
    std::vector<std::string> patterns;
    std::string contextDir;
};

static bool fnIgnoreCall(vm& v, ffi_instance& inst, const std::vector<value>& args,
                         const std::map<std::string, value>& kwargs, value& result){
    auto* st = static_cast<IgnorePatternState*>(inst.handle);
    std::vector<value> out;
    if (!st){
        result = v.makeList(out);
        return true;
    }
    std::vector<std::string> names;
    if (args.size() > 1 && (args[1].type == "list" || args[1].type == "tuple")){
        std::vector<value> items;
        v.materializeIterable(args[1], items);
        for (auto& item : items) names.push_back(v.toString(item));
    }
    std::set<std::string> ignored;
    for (auto& name : names){
        for (auto& pattern : st->patterns){
            if (wildMatch(name, pattern)){
                ignored.insert(name);
                break;
            }
        }
    }
    for (auto& name : ignored) out.push_back(v.makeString(name));
    result = v.makeList(out);
    return true;
}

static value fnIgnorePatterns(cffi& a){
    auto* st = new IgnorePatternState();
    for (size_t i = 0; i < a.argc(); ++i) st->patterns.push_back(a.vm->toString(a.arg(i)));
    return a.vm->makeDynamicObject("IgnorePatterns", st, nullptr, fnIgnoreCall);
}

static value fnMakeArchive(cffi& a){
    std::string baseName = a.vm->toString(a.arg(0));
    std::string format = a.vm->toString(a.arg(1));
    std::string rootDir = a.vm->toString(a.arg(2));
    std::string baseDir;
    if (a.has_kwarg("base_dir") && a.kwarg("base_dir").type != "none")
        baseDir = a.vm->toString(a.kwarg("base_dir"));
    else if (!rootDir.empty())
        baseDir = fsPathFromUtf8(rootDir).parent_path().u8string().empty()
                      ? "."
                      : u8Text(fsPathFromUtf8(rootDir).parent_path());
    fs::path root = fsPathFromUtf8(rootDir);
    std::vector<std::pair<std::string, std::string>> files;
    fs::path base = baseDir.empty() ? fs::path(".") : fsPathFromUtf8(baseDir);
    std::error_code ec;
    fs::recursive_directory_iterator it(root, ec);
    if (!ec){
        for (auto& entry : it){
            std::string rel = u8Text(fs::relative(entry.path(), base, ec));
            if (ec) continue;
            for (auto& ch : rel) if (ch == '\\') ch = '/';
            std::string content;
            if (!entry.is_directory(ec)){
                std::ifstream in(entry.path(), std::ios::binary);
                content.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
            }
            files.push_back({rel, content});
        }
    }
    std::string archiveName = baseName + "." + (format == "zip" ? "zip" : "tar");
    if (format == "zip") writeZip(archiveName, files);
    else if (format == "tar") writeTar(archiveName, files);
    else {
        a.vm->raiseError("ValueError", "unknown archive format '" + format + "'");
        return a.none();
    }
    return a.mkstr(archiveName);
}

static value fnUnpackArchive(cffi& a){
    std::string archive = a.vm->toString(a.arg(0));
    std::string target;
    if (a.has_kwarg("extract_dir") && a.kwarg("extract_dir").type != "none")
        target = a.vm->toString(a.kwarg("extract_dir"));
    if (target.empty()) target = ".";
    std::error_code ec;
    fs::create_directories(fsPathFromUtf8(target), ec);
    std::string error;
    std::string lower = archive;
    for (auto& ch : lower) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    bool ok = false;
    if (lower.size() >= 4 && lower.substr(lower.size() - 4) == ".zip")
        ok = extractZip(archive, target, error);
    else if (lower.size() >= 4 && lower.substr(lower.size() - 4) == ".tar")
        ok = extractTar(archive, target, error);
    else {
        a.vm->raiseError("ValueError", "Unknown archive format");
        return a.none();
    }
    if (!ok){
        a.vm->raiseError("OSError", error);
        return a.none();
    }
    return a.none();
}

}

ffilib cffi_shutil_lib(){
    std::map<std::string, void*> functions = {
        {"copyfile", (void*)fnCopyFile},
        {"copy", (void*)fnCopy},
        {"copy2", (void*)fnCopy2},
        {"copymode", (void*)fnCopyMode},
        {"copystat", (void*)fnCopyStat},
        {"copytree", (void*)fnCopyTree},
        {"rmtree", (void*)fnRmtree},
        {"move", (void*)fnMove},
        {"which", (void*)fnWhich},
        {"disk_usage", (void*)fnDiskUsage},
        {"get_terminal_size", (void*)fnGetTerminalSize},
        {"ignore_patterns", (void*)fnIgnorePatterns},
        {"make_archive", (void*)fnMakeArchive},
        {"unpack_archive", (void*)fnUnpackArchive}
    };
    return cffi_lib("shutil", functions, {}, {});
}

}
