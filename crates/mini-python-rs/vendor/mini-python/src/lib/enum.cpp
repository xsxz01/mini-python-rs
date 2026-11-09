#include "../cffi.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace minipy {

namespace {

// 枚举成员状态
// enum member state
// enum ᠬᠡᠰᠢᠭᠦᠨ ᠦᠨ ᠪᠠᠶᠢᠳᠠᠯ
struct EnumMemberState{
    std::string name;
    value val;
    std::string clsName;
    std::string kind; // Enum / IntEnum / StrEnum / Flag / IntFlag
    bool composite = false;
    class_t* owner = nullptr;
};

// auto() 占位标记
// auto() placeholder marker
// auto() ᠰᠣᠯᠢᠭᠤᠯᠤᠭᠴᠢ
struct EnumAutoState{
    int dummy = 0;
};

const ffilib* g_enumLib = nullptr;

const ffi_class* memberClass(){
    if (!g_enumLib) return nullptr;
    auto it = g_enumLib->classes.find("EnumMember");
    return it == g_enumLib->classes.end() ? nullptr : &it->second;
}

const ffi_class* autoClass(){
    if (!g_enumLib) return nullptr;
    auto it = g_enumLib->classes.find("auto");
    return it == g_enumLib->classes.end() ? nullptr : &it->second;
}

EnumMemberState* memberStateOf(const value& v){
    if (v.type != "native_object" || !v.ptr) return nullptr;
    auto* inst = static_cast<ffi_instance*>(v.ptr);
    if (!inst->klass || inst->klass->name != "EnumMember") return nullptr;
    return static_cast<EnumMemberState*>(inst->handle);
}

// 判定值是否为 auto 标记
// Whether value is an auto marker
// auto ᠲᠣᠳᠣᠷᠬᠠᠶᠢᠯᠠᠭᠴᠢ ᠶᠤᠮ ᠤᠤ
bool isAutoMarker(const value& v){
    if (v.type != "native_object" || !v.ptr) return false;
    auto* inst = static_cast<ffi_instance*>(v.ptr);
    return inst->klass && inst->klass->name == "auto";
}

value makeMember(vm& v, const std::string& cls, const std::string& kind,
                 const std::string& name, const value& val, bool composite,
                 class_t* owner = nullptr){
    const ffi_class* mc = memberClass();
    if (!mc) return v.makeNone();
    auto* st = new EnumMemberState();
    st->name = name;
    st->val = val;
    st->clsName = cls;
    st->kind = kind;
    st->composite = composite;
    st->owner = owner;
    value obj = v.makeNativeObject(g_enumLib, mc, st);
    if (obj.type == "native_object" && obj.ptr){
        auto* inst = static_cast<ffi_instance*>(obj.ptr);
        inst->typeName = cls;
        inst->attributes["name"] = v.makeString(name);
        inst->attributes["value"] = val;
        inst->attributes["_name_"] = v.makeString(name);
        inst->attributes["_value_"] = val;
    }
    return obj;
}

std::string kindOf(const class_t& klass){
    if (klass.builtinBase.rfind("enum:", 0) == 0) return klass.builtinBase.substr(5);
    return "Enum";
}

bool isIntKind(const std::string& kind){ return kind == "IntEnum" || kind == "IntFlag"; }
bool isFlagKind(const std::string& kind){ return kind == "Flag" || kind == "IntFlag"; }

// 收集成员名 (按类体赋值顺序, 过滤下划线与方法)
// Collect member names in class-body order (skip dunders/methods)
// ᠬᠡᠰᠢᠭᠦᠨ ᠦᠨ ᠨᠡᠷᠡ ᠢ ᠳᠠᠷᠠᠭᠠᠯᠠᠯ ᠢᠶᠠᠷ ᠲᠣᠭᠲᠠᠭᠠᠨ᠎ᠠ
std::vector<std::string> collectMemberOrder(class_t& klass,
                                            const std::map<std::string, value>& attrs){
    std::vector<std::string> order;
    auto acceptable = [&](const std::string& name){
        if (name.empty() || name[0] == '_') return false;
        auto it = attrs.find(name);
        if (it == attrs.end()) return false;
        if (it->second.type == "function") return false;
        if (it->second.type == "native_object" && it->second.ptr){
            auto* inst = static_cast<ffi_instance*>(it->second.ptr);
            if (inst->klass && inst->klass->name == "EnumMember") return false;
        }
        return true;
    };
    for (auto& ins : klass.initBody){
        if (ins.operation == eop_assign && !ins.operands.empty()){
            const std::string& target = ins.operands[0];
            bool plain = !target.empty() &&
                (std::isalpha(static_cast<unsigned char>(target[0])) || target[0] == '_');
            for (char c : target){
                if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')){
                    plain = false;
                    break;
                }
            }
            if (plain && acceptable(target) &&
                std::find(order.begin(), order.end(), target) == order.end()){
                order.push_back(target);
            }
        }
    }
    for (auto& kv : attrs){
        if (acceptable(kv.first) &&
            std::find(order.begin(), order.end(), kv.first) == order.end()){
            order.push_back(kv.first);
        }
    }
    return order;
}

// 生成组合成员名 (Flag 位分解)
// Build composite member name (Flag bit decomposition)
// ᠨᠢᠶᠢᠯᠦᠭᠡ ᠬᠡᠰᠢᠭᠦᠨ ᠦᠨ ᠨᠡᠷᠡ
std::string compositeName(class_t& klass, long long bits){
    auto* attrs = static_cast<std::map<std::string, value>*>(klass.attributes);
    if (!attrs) return std::to_string(bits);
    auto mapIt = attrs->find("_member_map_");
    if (mapIt == attrs->end() || mapIt->second.type != "dict" || !mapIt->second.ptr)
        return std::to_string(bits);
    auto* map = static_cast<dict_t*>(mapIt->second.ptr);
    std::map<std::string, long long> values;
    for (auto& kv : *map){
        if (kv.first.type != "str" || !kv.first.ptr) continue;
        auto* ms = memberStateOf(kv.second);
        if (!ms || ms->composite) continue;
        if (ms->val.type == "int" || ms->val.type == "bool")
            values[*static_cast<std::string*>(kv.first.ptr)] = *static_cast<long long*>(ms->val.ptr);
    }
    std::vector<std::string> ordered;
    auto namesIt = attrs->find("_member_names_");
    if (namesIt != attrs->end() && namesIt->second.type == "list" && namesIt->second.ptr){
        for (auto& nv : *static_cast<std::vector<value>*>(namesIt->second.ptr)){
            if (nv.type == "str" && nv.ptr)
                ordered.push_back(*static_cast<std::string*>(nv.ptr));
        }
    }
    if (ordered.empty()){
        for (auto& kv : values) ordered.push_back(kv.first);
    }
    long long remaining = bits;
    std::vector<std::string> parts;
    for (auto& name : ordered){
        auto it = values.find(name);
        if (it == values.end()) continue;
        long long val = it->second;
        if (val != 0 && (remaining & val) == val){
            parts.push_back(name);
            remaining &= ~val;
        }
    }
    if (remaining != 0 || parts.empty()) return std::to_string(bits);
    std::string out;
    for (size_t i = 0; i < parts.size(); ++i){
        if (i > 0) out += "|";
        out += parts[i];
    }
    return out;
}

}

// 枚举成员 __repr__: <Color.RED: 1>
// enum member __repr__: <Color.RED: 1>
// enum ᠬᠡᠰᠢᠭᠦᠨ __repr__
static value fnMemberRepr(cffi& a){
    auto* st = memberStateOf(a.self());
    if (!st) return a.mkstr("<enum member>");
    std::string text = "<" + st->clsName + "." + st->name + ": " +
                       pythonRepr(*a.vm, st->val) + ">";
    return a.mkstr(text);
}

// 枚举成员 __str__: Color.RED
// enum member __str__: Color.RED
// enum ᠬᠡᠰᠢᠭᠦᠨ __str__
static value fnMemberStr(cffi& a){
    auto* st = memberStateOf(a.self());
    if (!st) return a.mkstr("");
    if (isIntKind(st->kind) && (st->val.type == "int" || st->val.type == "bool"))
        return a.mkstr(std::to_string(*static_cast<long long*>(st->val.ptr)));
    if (st->kind == "StrEnum" && st->val.type == "str" && st->val.ptr)
        return a.mkstr(*static_cast<std::string*>(st->val.ptr));
    return a.mkstr(st->clsName + "." + st->name);
}

static value fnMemberEq(cffi& a){
    auto* st = memberStateOf(a.self());
    if (!st) return a.mkbool(false);
    value other = a.arg(0);
    auto* os = memberStateOf(other);
    if (os) return a.mkbool(st == os);
    if (isIntKind(st->kind) && (other.type == "int" || other.type == "bool")){
        if (st->val.type == "int" || st->val.type == "bool"){
            return a.mkbool(*static_cast<long long*>(st->val.ptr) ==
                            *static_cast<long long*>(other.ptr));
        }
    }
    if (st->kind == "StrEnum" && other.type == "str" && st->val.type == "str"){
        return a.mkbool(*static_cast<std::string*>(st->val.ptr) ==
                        *static_cast<std::string*>(other.ptr));
    }
    return a.mkbool(false);
}

static value fnMemberInt(cffi& a){
    auto* st = memberStateOf(a.self());
    if (st && (st->val.type == "int" || st->val.type == "bool"))
        return a.mkint(*static_cast<long long*>(st->val.ptr));
    return a.mkint(0);
}

static value fnMemberNe(cffi& a){
    auto* st = memberStateOf(a.self());
    if (!st) return a.mkbool(true);
    value other = a.arg(0);
    auto* os = memberStateOf(other);
    if (os) return a.mkbool(st != os);
    if (isIntKind(st->kind) && (other.type == "int" || other.type == "bool")){
        if (st->val.type == "int" || st->val.type == "bool"){
            return a.mkbool(*static_cast<long long*>(st->val.ptr) !=
                            *static_cast<long long*>(other.ptr));
        }
    }
    if (st->kind == "StrEnum" && other.type == "str" && st->val.type == "str"){
        return a.mkbool(*static_cast<std::string*>(st->val.ptr) !=
                        *static_cast<std::string*>(other.ptr));
    }
    return a.mkbool(true);
}

// IntEnum 比较: __lt__/__le__/__gt__/__ge__ (与 int 或同类成员比较值)
// IntEnum comparison (compare value against int or same-class member)
// IntEnum ᠬᠠᠷᠢᠴᠠᠭᠤᠯᠭ᠎ᠠ
static value memberIntCmp(cffi& a, const std::string& op){
    auto* st = memberStateOf(a.self());
    if (!st || !isIntKind(st->kind)) return a.mkbool(false);
    if (st->val.type != "int" && st->val.type != "bool") return a.mkbool(false);
    long long lv = *static_cast<long long*>(st->val.ptr);
    value other = a.arg(0);
    long long rv = 0;
    auto* os = memberStateOf(other);
    if (os && (os->val.type == "int" || os->val.type == "bool")){
        rv = *static_cast<long long*>(os->val.ptr);
    } else if (other.type == "int" || other.type == "bool"){
        rv = *static_cast<long long*>(other.ptr);
    } else {
        return a.mkbool(false);
    }
    if (op == "<") return a.mkbool(lv < rv);
    if (op == "<=") return a.mkbool(lv <= rv);
    if (op == ">") return a.mkbool(lv > rv);
    if (op == ">=") return a.mkbool(lv >= rv);
    return a.mkbool(false);
}

static value fnMemberLt(cffi& a){ return memberIntCmp(a, "<"); }
static value fnMemberLe(cffi& a){ return memberIntCmp(a, "<="); }
static value fnMemberGt(cffi& a){ return memberIntCmp(a, ">"); }
static value fnMemberGe(cffi& a){ return memberIntCmp(a, ">="); }

// IntEnum 算术: 结果退化为 int (CPython 行为)
// IntEnum arithmetic: result degrades to int (CPython behavior)
// IntEnum ᠲᠣᠬᠢᠶᠠᠯᠠᠯ
static value memberIntArith(cffi& a, const std::string& op){
    auto* st = memberStateOf(a.self());
    if (!st || !isIntKind(st->kind)) return a.none();
    if (st->val.type != "int" && st->val.type != "bool") return a.none();
    long long lv = *static_cast<long long*>(st->val.ptr);
    value other = a.arg(0);
    auto* os = memberStateOf(other);
    long long rv = 0;
    bool isInt = false;
    if (os && (os->val.type == "int" || os->val.type == "bool")){
        rv = *static_cast<long long*>(os->val.ptr);
        isInt = true;
    } else if (other.type == "int" || other.type == "bool"){
        rv = *static_cast<long long*>(other.ptr);
        isInt = true;
    }
    if (isInt){
        if (op == "+") return a.mkint(lv + rv);
        if (op == "-") return a.mkint(lv - rv);
        if (op == "*") return a.mkint(lv * rv);
        if (op == "%"){
            if (rv == 0){ a.vm->raiseError("ZeroDivisionError", "integer division or modulo by zero"); return a.none(); }
            long long q = lv % rv;
            if (q != 0 && ((q < 0) != (rv < 0))) q += rv;
            return a.mkint(q);
        }
        if (op == "//"){
            if (rv == 0){ a.vm->raiseError("ZeroDivisionError", "integer division or modulo by zero"); return a.none(); }
            long long q = lv / rv;
            if ((lv % rv) != 0 && ((lv < 0) != (rv < 0))) q -= 1;
            return a.mkint(q);
        }
    }
    if (other.type == "float"){
        double rvf = a.vm->toFloat(other);
        if (op == "+") return a.mkfloat(lv + rvf);
        if (op == "-") return a.mkfloat(lv - rvf);
        if (op == "*") return a.mkfloat(lv * rvf);
    }
    a.vm->raiseError("TypeError", "unsupported operand type(s) for " + op + ": '" +
                     st->clsName + "' and '" +
                     (other.type == "none" ? "NoneType" : other.type) + "'");
    return a.none();
}

static value fnMemberAdd(cffi& a){
    auto* st = memberStateOf(a.self());
    if (st && st->kind == "StrEnum" && st->val.type == "str" && st->val.ptr &&
        a.argc() > 0){
        value other = a.arg(0);
        if (other.type == "str" && other.ptr)
            return a.mkstr(*static_cast<std::string*>(st->val.ptr) +
                           *static_cast<std::string*>(other.ptr));
    }
    return memberIntArith(a, "+");
}
static value fnMemberSub(cffi& a){ return memberIntArith(a, "-"); }
static value fnMemberMul(cffi& a){ return memberIntArith(a, "*"); }
static value fnMemberMod(cffi& a){ return memberIntArith(a, "%"); }
static value fnMemberFloorDiv(cffi& a){ return memberIntArith(a, "//"); }

static value fnMemberStrVal(cffi& a){
    auto* st = memberStateOf(a.self());
    if (st && st->val.type == "str" && st->val.ptr) return a.mkstr(*static_cast<std::string*>(st->val.ptr));
    return a.mkstr("");
}

// 从位值得到 Flag 成员: 优先复用已有成员, 否则生成组合伪成员
// Resolve a Flag member from bits: reuse an existing member, else composite
// ᠪᠢᠲ ᠡᠴᠡ Flag ᠬᠡᠰᠢᠭᠦᠨ
static value memberFromBits(vm& v, EnumMemberState* st, long long bits){
    if (st && st->owner && st->owner->attributes){
        auto* attrs = static_cast<std::map<std::string, value>*>(st->owner->attributes);
        auto mapIt = attrs->find("_member_map_");
        if (mapIt != attrs->end() && mapIt->second.type == "dict" && mapIt->second.ptr){
            for (auto& kv : *static_cast<dict_t*>(mapIt->second.ptr)){
                auto* ms = memberStateOf(kv.second);
                if (ms && (ms->val.type == "int" || ms->val.type == "bool") &&
                    *static_cast<long long*>(ms->val.ptr) == bits)
                    return kv.second;
            }
            std::string name = compositeName(*st->owner, bits);
            return makeMember(v, st->clsName, st->kind, name, v.makeInt(bits), true, st->owner);
        }
    }
    return v.makeInt(bits);
}

static bool memberBitOperands(EnumMemberState* st, const value& other,
                              long long& lv, long long& rv){
    if (!st) return false;
    if (st->val.type != "int" && st->val.type != "bool") return false;
    lv = *static_cast<long long*>(st->val.ptr);
    auto* os = memberStateOf(other);
    if (os && (os->val.type == "int" || os->val.type == "bool"))
        rv = *static_cast<long long*>(os->val.ptr);
    else if (other.type == "int" || other.type == "bool")
        rv = *static_cast<long long*>(other.ptr);
    else return false;
    return true;
}

static value memberBitOp(cffi& a, const std::string& op){
    auto* st = memberStateOf(a.self());
    long long lv = 0, rv = 0;
    if (!memberBitOperands(st, a.arg(0), lv, rv)) return a.none();
    long long r = 0;
    if (op == "|") r = lv | rv;
    else if (op == "&") r = lv & rv;
    else if (op == "^") r = lv ^ rv;
    if (st && isFlagKind(st->kind)) return memberFromBits(*a.vm, st, r);
    return a.vm->makeInt(r);
}

static value fnMemberOr(cffi& a){ return memberBitOp(a, "|"); }

static value fnMemberAnd(cffi& a){ return memberBitOp(a, "&"); }

static value fnMemberXor(cffi& a){ return memberBitOp(a, "^"); }

// Flag 成员测试: x in flag → (x & flag) == x
// Flag membership: x in flag → (x & flag) == x
// Flag ᠪᠠᠶᠢᠭᠤᠯᠭ᠎ᠠ ᠰᠢᠯᠭᠠᠬᠤ
static value fnMemberContains(cffi& a){
    auto* st = memberStateOf(a.self());
    if (!st || !isFlagKind(st->kind)) return a.mkbool(false);
    if (st->val.type != "int" && st->val.type != "bool") return a.mkbool(false);
    long long selfBits = *static_cast<long long*>(st->val.ptr);
    value other = a.arg(0);
    long long obits = 0;
    auto* os = memberStateOf(other);
    if (os && (os->val.type == "int" || os->val.type == "bool"))
        obits = *static_cast<long long*>(os->val.ptr);
    else if (other.type == "int" || other.type == "bool")
        obits = *static_cast<long long*>(other.ptr);
    else
        return a.mkbool(false);
    return a.mkbool((obits & selfBits) == obits);
}

// auto() 构造
// auto() constructor
// auto() ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠭᠴᠢ
static value fnAutoCtor(cffi& a){
    return a.mkobject(new EnumAutoState());
}

static value fnEnumCtor(cffi& a){
    return a.mkobject(nullptr);
}

// unique 装饰器: 检查无别名
// unique decorator: reject aliases
// unique ᠨᠢᠭᠡᠨᠲᠡ ᠪᠠᠷᠢᠮᠲᠠ
static value fnUnique(cffi& a){
    if (a.argc() < 1) return a.none();
    value cls = a.arg(0);
    if (cls.type != "class" || !cls.ptr) return cls;
    auto* klass = static_cast<class_t*>(cls.ptr);
    auto* attrs = klass->attributes
                      ? static_cast<std::map<std::string, value>*>(klass->attributes)
                      : nullptr;
    if (!attrs) return cls;
    auto it = attrs->find("_member_map_");
    if (it == attrs->end() || it->second.type != "dict" || !it->second.ptr) return cls;
    auto* map = static_cast<dict_t*>(it->second.ptr);
    for (size_t i = 0; i < map->size(); ++i){
        for (size_t j = i + 1; j < map->size(); ++j){
            auto* si = memberStateOf((*map)[i].second);
            auto* sj = memberStateOf((*map)[j].second);
            if (si && sj && a.vm->valuesEqual(si->val, sj->val)){
                std::string ni = (*map)[i].first.type == "str" && (*map)[i].first.ptr
                                     ? *static_cast<std::string*>((*map)[i].first.ptr)
                                     : si->name;
                std::string nj = (*map)[j].first.type == "str" && (*map)[j].first.ptr
                                     ? *static_cast<std::string*>((*map)[j].first.ptr)
                                     : sj->name;
                a.vm->raiseError("ValueError",
                    "duplicate values found in <enum '" + klass->name + "'>: " +
                    nj + " -> " + ni);
                return a.none();
            }
        }
    }
    return cls;
}

// applyEnumTransform: 类体属性 → 枚举成员
// applyEnumTransform: class attrs → enum members
// enum ᠬᠤᠪᠢᠷᠠᠭᠤᠯᠭ᠎ᠠ
bool applyEnumTransform(vm& v, class_t& klass){
    if (!g_enumLib) return false;
    auto* attrs = klass.attributes
                      ? static_cast<std::map<std::string, value>*>(klass.attributes)
                      : nullptr;
    if (!attrs) return false;
    std::string kind = kindOf(klass);
    std::vector<std::string> order = collectMemberOrder(klass, *attrs);

    std::vector<value> memberNames;
    std::vector<std::pair<value, value>> memberMap;
    std::map<std::string, value> canonical;
    long long nextAuto = 1;
    for (const std::string& name : order){
        value raw = (*attrs)[name];
        value val = raw;
        if (isAutoMarker(raw)){
            if (isFlagKind(kind)){
                long long bits = 1;
                std::vector<long long> used;
                for (auto& kv : canonical){
                    if (kv.second.type == "int" || kv.second.type == "bool")
                        used.push_back(*static_cast<long long*>(kv.second.ptr));
                }
                while (std::find(used.begin(), used.end(), bits) != used.end()) bits <<= 1;
                val = v.makeInt(bits);
            } else {
                while (true){
                    bool used = false;
                    for (auto& kv : canonical){
                        if ((kv.second.type == "int" || kv.second.type == "bool") &&
                            *static_cast<long long*>(kv.second.ptr) == nextAuto){
                            used = true;
                            break;
                        }
                    }
                    if (!used) break;
                    nextAuto++;
                }
                val = v.makeInt(nextAuto);
                nextAuto++;
            }
        }
        // 别名检测: 同值成员指向同一对象
        // alias detection: same value reuses same member object
        // ᠳᠠᠬᠢᠯᠲᠠ ᠨᠡᠷᠡ ᠢ ᠰᠢᠯᠭᠠᠨ᠎ᠠ
        bool aliased = false;
        for (auto& kv : canonical){
            if (v.valuesEqual(kv.second, val)){
                value existing;
                for (auto& mp : memberMap){
                    if (mp.first.type == "str" && kv.first ==
                        *static_cast<std::string*>(mp.first.ptr)){
                        existing = mp.second;
                        break;
                    }
                }
                if (existing.ptr){
                    (*attrs)[name] = existing;
                    memberMap.push_back({v.makeString(name), existing});
                    aliased = true;
                    break;
                }
            }
        }
        if (aliased) continue;
        value member = makeMember(v, klass.name, kind, name, val, false, &klass);
        (*attrs)[name] = member;
        canonical[name] = val;
        memberNames.push_back(v.makeString(name));
        memberMap.push_back({v.makeString(name), member});
    }
    (*attrs)["_member_names_"] = v.makeList(memberNames);
    (*attrs)["_member_map_"] = v.makeDict(memberMap);
    return true;
}

// enumClassCall: Color(1) 按值查找
// enumClassCall: Color(1) lookup by value
// enum ᠬᠢᠶ᠎ᠡ ᠪᠢᠲ᠋ᠢ ᠶᠢᠨ ᠳᠤᠭᠤᠳᠠᠯᠭ᠎ᠠ
value enumClassCall(vm& v, class_t& klass, const std::vector<value>& args){
    std::string kind = kindOf(klass);
    if (args.empty()){
        v.raiseError("TypeError", klass.name + "() missing 1 required positional argument: 'value'");
        return v.makeNone();
    }
    value target = args[0];
    auto* attrs = klass.attributes
                      ? static_cast<std::map<std::string, value>*>(klass.attributes)
                      : nullptr;
    if (attrs){
        auto mapIt = attrs->find("_member_map_");
        if (mapIt != attrs->end() && mapIt->second.type == "dict" && mapIt->second.ptr){
            auto* map = static_cast<dict_t*>(mapIt->second.ptr);
            for (auto& kv : *map){
                auto* ms = memberStateOf(kv.second);
                if (!ms) continue;
                if (v.valuesEqual(ms->val, target)) return kv.second;
            }
            // Flag: 未匹配的整数值 → 组合伪成员
            // Flag: unmatched int value → composite pseudo-member
            // Flag: ᠲᠣ᠂ ᠲᠠᠠᠷᠠᠬᠤ ᠦᠭᠡᠢ → ᠨᠢᠶᠢᠯᠦᠭᠡ ᠬᠡᠰᠢᠭᠦᠨ
            if (isFlagKind(kind) && (target.type == "int" || target.type == "bool")){
                long long bits = *static_cast<long long*>(target.ptr);
                std::string name = compositeName(klass, bits);
                return makeMember(v, klass.name, kind, name, target, true, &klass);
            }
        }
    }
    v.raiseError("ValueError", pythonRepr(v, target) + " is not a valid " + klass.name);
    return v.makeNone();
}

// cffi enum 库
// cffi enum library
// cffi enum ᠨᠣᠮ
ffilib cffi_enum_lib(){
    ffi_class memberCls;
    memberCls.name = "EnumMember";
    memberCls.constructor = nullptr;
    memberCls.methods = {
        {"__repr__", (void*)fnMemberRepr},
        {"__str__", (void*)fnMemberStr},
        {"__eq__", (void*)fnMemberEq},
        {"__ne__", (void*)fnMemberNe},
        {"__lt__", (void*)fnMemberLt},
        {"__le__", (void*)fnMemberLe},
        {"__gt__", (void*)fnMemberGt},
        {"__ge__", (void*)fnMemberGe},
        {"__add__", (void*)fnMemberAdd},
        {"__sub__", (void*)fnMemberSub},
        {"__mul__", (void*)fnMemberMul},
        {"__mod__", (void*)fnMemberMod},
        {"__floordiv__", (void*)fnMemberFloorDiv},
        {"__int__", (void*)fnMemberInt},
        {"__strval__", (void*)fnMemberStrVal},
        {"__or__", (void*)fnMemberOr},
        {"__and__", (void*)fnMemberAnd},
        {"__xor__", (void*)fnMemberXor},
        {"__contains__", (void*)fnMemberContains}
    };

    ffi_class autoCls;
    autoCls.name = "auto";
    autoCls.constructor = (void*)fnAutoCtor;

    ffi_class enumCls;
    enumCls.name = "Enum";
    enumCls.constructor = (void*)fnEnumCtor;

    ffi_class intEnumCls;
    intEnumCls.name = "IntEnum";
    intEnumCls.constructor = (void*)fnEnumCtor;

    ffi_class strEnumCls;
    strEnumCls.name = "StrEnum";
    strEnumCls.constructor = (void*)fnEnumCtor;

    ffi_class flagCls;
    flagCls.name = "Flag";
    flagCls.constructor = (void*)fnEnumCtor;

    ffi_class intFlagCls;
    intFlagCls.name = "IntFlag";
    intFlagCls.constructor = (void*)fnEnumCtor;

    ffi_class metaCls;
    metaCls.name = "EnumMeta";
    metaCls.constructor = (void*)fnEnumCtor;

    ffi_class typeCls;
    typeCls.name = "EnumType";
    typeCls.constructor = (void*)fnEnumCtor;

    static ffilib lib = cffi_lib(
        "enum",
        {
            {"unique", (void*)fnUnique}
        },
        {},
        {
            {"EnumMember", memberCls},
            {"auto", autoCls},
            {"Enum", enumCls},
            {"IntEnum", intEnumCls},
            {"StrEnum", strEnumCls},
            {"Flag", flagCls},
            {"IntFlag", intFlagCls},
            {"EnumMeta", metaCls},
            {"EnumType", typeCls}
        });
    g_enumLib = &lib;
    return lib;
}

}
