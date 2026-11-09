#include "../cffi.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

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

std::map<std::string, bool> g_cmpCache;

bool byteCompare(const fs::path& x, const fs::path& y){
    std::ifstream fx(x, std::ios::binary);
    std::ifstream fy(y, std::ios::binary);
    if (!fx.is_open() || !fy.is_open()) return false;
    char bx[8192];
    char by[8192];
    while (true){
        fx.read(bx, sizeof(bx));
        fy.read(by, sizeof(by));
        std::streamsize nx = fx.gcount();
        std::streamsize ny = fy.gcount();
        if (nx != ny) return false;
        if (nx == 0) return true;
        for (std::streamsize i = 0; i < nx; ++i){
            if (bx[i] != by[i]) return false;
        }
    }
}

bool cmpFiles(const std::string& f1, const std::string& f2, bool shallow){
    fs::path p1 = fsPathFromUtf8(f1);
    fs::path p2 = fsPathFromUtf8(f2);
    std::error_code ec;
    if (!fs::exists(p1, ec) || !fs::exists(p2, ec)) return false;
    if (shallow){
        auto s1 = fs::file_size(p1, ec);
        if (ec) return false;
        auto s2 = fs::file_size(p2, ec);
        if (ec) return false;
        if (s1 != s2) return false;
    }
    std::string key = f1 + "\x00" + f2;
    auto it = g_cmpCache.find(key);
    if (it != g_cmpCache.end()) return it->second;
    bool same = byteCompare(p1, p2);
    g_cmpCache[key] = same;
    return same;
}

// ---------- dircmp ----------
struct DirCmpState{
    std::string left;
    std::string right;
    std::set<std::string> ignore;
    std::set<std::string> hide;
    std::vector<std::string> leftList;
    std::vector<std::string> rightList;
    std::vector<std::string> common;
    std::vector<std::string> leftOnly;
    std::vector<std::string> rightOnly;
    std::vector<std::string> commonDirs;
    std::vector<std::string> commonFiles;
    std::vector<std::string> commonFunny;
    std::vector<std::string> sameFiles;
    std::vector<std::string> diffFiles;
    std::vector<std::string> funnyFiles;
    std::map<std::string, value> subdirs;
};

DirCmpState* dirCmpOf(const value& v){
    if (v.type != "native_object" || !v.ptr) return nullptr;
    auto* inst = static_cast<ffi_instance*>(v.ptr);
    if (!inst->klass || inst->klass->name != "dircmp") return nullptr;
    return static_cast<DirCmpState*>(inst->handle);
}

std::vector<std::string> listDir(const std::string& dir, const std::set<std::string>& ignore,
                                 const std::set<std::string>& hide){
    std::vector<std::string> out;
    std::error_code ec;
    fs::directory_iterator it(fsPathFromUtf8(dir), ec);
    if (ec) return out;
    for (auto& entry : it){
        std::string name = u8Text(entry.path().filename());
        if (hide.count(name) || ignore.count(name)) continue;
        out.push_back(name);
    }
    std::sort(out.begin(), out.end());
    return out;
}

value makeStringList(cffi& a, const std::vector<std::string>& items){
    std::vector<value> out;
    for (auto& s : items) out.push_back(a.mkstr(s));
    return a.mklist(out);
}

std::set<std::string> readStringSet(cffi& a, const value& v){
    std::set<std::string> out;
    if (v.type == "list" || v.type == "tuple"){
        for (auto& item : *static_cast<std::vector<value>*>(v.ptr))
            out.insert(a.vm->toString(item));
    }
    return out;
}

void computeDirCmp(cffi& a, DirCmpState* st);

value buildDirCmp(cffi& a, const std::string& left, const std::string& right,
                  const std::set<std::string>& ignore, const std::set<std::string>& hide){
    auto* st = new DirCmpState();
    st->left = left;
    st->right = right;
    st->ignore = ignore;
    st->hide = hide;
    value obj = a.mkobject(st);
    computeDirCmp(a, st);
    if (obj.type == "native_object" && obj.ptr){
        auto* inst = static_cast<ffi_instance*>(obj.ptr);
        inst->attributes["left"] = a.mkstr(st->left);
        inst->attributes["right"] = a.mkstr(st->right);
        inst->attributes["left_list"] = makeStringList(a, st->leftList);
        inst->attributes["right_list"] = makeStringList(a, st->rightList);
        inst->attributes["common"] = makeStringList(a, st->common);
        inst->attributes["left_only"] = makeStringList(a, st->leftOnly);
        inst->attributes["right_only"] = makeStringList(a, st->rightOnly);
        inst->attributes["common_dirs"] = makeStringList(a, st->commonDirs);
        inst->attributes["common_files"] = makeStringList(a, st->commonFiles);
        inst->attributes["common_funny"] = makeStringList(a, st->commonFunny);
        inst->attributes["same_files"] = makeStringList(a, st->sameFiles);
        inst->attributes["diff_files"] = makeStringList(a, st->diffFiles);
        inst->attributes["funny_files"] = makeStringList(a, st->funnyFiles);
        std::vector<std::pair<value, value>> subs;
        for (auto& kv : st->subdirs) subs.push_back({a.mkstr(kv.first), kv.second});
        value dict = a.vm->makeDict(subs);
        inst->attributes["subdirs"] = dict;
        inst->attributes["ignore"] = makeStringList(a, std::vector<std::string>(st->ignore.begin(), st->ignore.end()));
        inst->attributes["hide"] = makeStringList(a, std::vector<std::string>(st->hide.begin(), st->hide.end()));
    }
    return obj;
}

void computeDirCmp(cffi& a, DirCmpState* st){
    st->leftList = listDir(st->left, st->ignore, st->hide);
    st->rightList = listDir(st->right, st->ignore, st->hide);
    std::set<std::string> ls(st->leftList.begin(), st->leftList.end());
    std::set<std::string> rs(st->rightList.begin(), st->rightList.end());
    st->common.clear();
    st->leftOnly.clear();
    st->rightOnly.clear();
    for (auto& name : ls){
        if (rs.count(name)) st->common.push_back(name);
        else st->leftOnly.push_back(name);
    }
    for (auto& name : rs){
        if (!ls.count(name)) st->rightOnly.push_back(name);
    }
    std::sort(st->common.begin(), st->common.end());
    st->commonDirs.clear();
    st->commonFiles.clear();
    st->commonFunny.clear();
    st->sameFiles.clear();
    st->diffFiles.clear();
    st->funnyFiles.clear();
    std::error_code ec;
    for (auto& name : st->common){
        fs::path lp = fsPathFromUtf8(st->left) / fsPathFromUtf8(name);
        fs::path rp = fsPathFromUtf8(st->right) / fsPathFromUtf8(name);
        bool ldir = fs::is_directory(lp, ec);
        bool rdir = fs::is_directory(rp, ec);
        bool lfile = fs::is_regular_file(lp, ec);
        bool rfile = fs::is_regular_file(rp, ec);
        if (ldir && rdir){
            st->commonDirs.push_back(name);
            st->subdirs[name] = buildDirCmp(a, u8Text(lp), u8Text(rp), st->ignore, st->hide);
        } else if (lfile && rfile){
            st->commonFiles.push_back(name);
            if (cmpFiles(u8Text(lp), u8Text(rp), true)) st->sameFiles.push_back(name);
            else st->diffFiles.push_back(name);
        } else {
            st->commonFunny.push_back(name);
            if (lfile && rfile) st->funnyFiles.push_back(name);
        }
    }
}

std::string pathName(const std::string& text){
    fs::path p = fsPathFromUtf8(text);
    return u8Text(p.filename());
}

static value fnDircmpCtor(cffi& a){
    std::string left = a.vm->toString(a.arg(0));
    std::string right = a.vm->toString(a.arg(1));
    std::set<std::string> ignore = {"RCS", "CVS", ".svn", ".git", "__pycache__"};
    std::set<std::string> hide;
    hide.insert(pathName(left));
    if (a.has_kwarg("ignore") && a.kwarg("ignore").type != "none")
        ignore = readStringSet(a, a.kwarg("ignore"));
    if (a.has_kwarg("hide") && a.kwarg("hide").type != "none"){
        hide.clear();
        hide = readStringSet(a, a.kwarg("hide"));
    }
    return buildDirCmp(a, left, right, ignore, hide);
}

static void mergeLists(std::vector<std::string>& into, const std::vector<std::string>& src){
    for (auto& s : src) into.push_back(s);
}

void reportOne(cffi& a, const DirCmpState* st, bool printHeader){
    if (printHeader && !st->diffFiles.empty()){
        std::cout << "diff " << st->left << " " << st->right << "\n";
        std::cout << "  ";
        for (size_t i = 0; i < st->diffFiles.size(); ++i){
            if (i) std::cout << " ";
            std::cout << st->diffFiles[i];
        }
        std::cout << "\n";
    }
    std::cout << "Identical files: ";
    for (size_t i = 0; i < st->sameFiles.size(); ++i){
        if (i) std::cout << " ";
        std::cout << st->sameFiles[i];
    }
    std::cout << "\n";
    std::cout << "Different files: ";
    for (size_t i = 0; i < st->diffFiles.size(); ++i){
        if (i) std::cout << " ";
        std::cout << st->diffFiles[i];
    }
    std::cout << "\n";
    std::cout << "Files only in " << st->left << ": ";
    for (size_t i = 0; i < st->leftOnly.size(); ++i){
        if (i) std::cout << " ";
        std::cout << st->leftOnly[i];
    }
    std::cout << "\n";
    std::cout << "Files only in " << st->right << ": ";
    for (size_t i = 0; i < st->rightOnly.size(); ++i){
        if (i) std::cout << " ";
        std::cout << st->rightOnly[i];
    }
    std::cout << "\n";
    std::cout << "Common subdirectories: ";
    for (size_t i = 0; i < st->commonDirs.size(); ++i){
        if (i) std::cout << " ";
        std::cout << st->commonDirs[i];
    }
    std::cout << "\n";
}

static value fnDircmpReport(cffi& a){
    auto* st = dirCmpOf(a.self());
    if (!st) return a.none();
    reportOne(a, st, true);
    return a.none();
}

static value fnDircmpReportPartial(cffi& a){
    auto* st = dirCmpOf(a.self());
    if (!st) return a.none();
    reportOne(a, st, true);
    for (auto& kv : st->subdirs){
        auto* sub = dirCmpOf(kv.second);
        if (sub) reportOne(a, sub, true);
    }
    return a.none();
}

void reportFullRecursive(cffi& a, const DirCmpState* st, bool header){
    reportOne(a, st, header);
    for (auto& kv : st->subdirs){
        auto* sub = dirCmpOf(kv.second);
        if (sub) reportFullRecursive(a, sub, true);
    }
}

static value fnDircmpReportFull(cffi& a){
    auto* st = dirCmpOf(a.self());
    if (!st) return a.none();
    reportFullRecursive(a, st, true);
    return a.none();
}

// ---------- 模块函数 ----------
static value fnCmp(cffi& a){
    std::string f1 = a.vm->toString(a.arg(0));
    std::string f2 = a.vm->toString(a.arg(1));
    bool shallow = a.has_kwarg("shallow") ? a.kwarg_bool("shallow", true) : true;
    return a.mkbool(cmpFiles(f1, f2, shallow));
}

static value fnCmpfiles(cffi& a){
    std::string dir1 = a.vm->toString(a.arg(0));
    std::string dir2 = a.vm->toString(a.arg(1));
    value common = a.arg(2);
    bool shallow = a.has_kwarg("shallow") ? a.kwarg_bool("shallow", true) : true;
    std::vector<value> match, mismatch, errors;
    if (common.type == "list" || common.type == "tuple"){
        for (auto& item : *static_cast<std::vector<value>*>(common.ptr)){
            std::string name = a.vm->toString(item);
            fs::path p1 = fsPathFromUtf8(dir1) / fsPathFromUtf8(name);
            fs::path p2 = fsPathFromUtf8(dir2) / fsPathFromUtf8(name);
            std::error_code ec;
            if (!fs::exists(p1, ec) || !fs::exists(p2, ec)){
                errors.push_back(a.mkstr(name));
                continue;
            }
            if (cmpFiles(u8Text(p1), u8Text(p2), shallow)) match.push_back(a.mkstr(name));
            else mismatch.push_back(a.mkstr(name));
        }
    }
    std::vector<value> result;
    result.push_back(a.mklist(match));
    result.push_back(a.mklist(mismatch));
    result.push_back(a.mklist(errors));
    return a.vm->makeTuple(result);
}

static value fnClearCache(cffi& a){
    (void)a;
    g_cmpCache.clear();
    return a.none();
}

}

ffilib cffi_filecmp_lib(){
    std::map<std::string, void*> functions = {
        {"cmp", (void*)fnCmp},
        {"cmpfiles", (void*)fnCmpfiles},
        {"clear_cache", (void*)fnClearCache}
    };
    std::map<std::string, ffi_class> classes;
    ffi_class dc;
    dc.name = "dircmp";
    dc.constructor = (void*)fnDircmpCtor;
    dc.methods = {
        {"report", (void*)fnDircmpReport},
        {"report_partial_closure", (void*)fnDircmpReportPartial},
        {"report_full_closure", (void*)fnDircmpReportFull}
    };
    classes["dircmp"] = dc;
    return cffi_lib("filecmp", functions, {}, classes);
}

}
