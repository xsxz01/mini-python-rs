#include "cffi.hpp"

namespace minipy {

namespace {

value invokeNative(void* pointer, vm& vm, const ffilib* library,
                     const ffi_class* klass, const std::vector<value>& args,
                     const value* selfValue,
                     const std::map<std::string, value>& kwargs = {}){
    if (pointer == nullptr){
        return vm.makeNone();
    }
    cffi context;
    context.vm = &vm;
    context.library = library;
    context.currentClass = klass;
    context.argv = args;
    context.kwargv = kwargs;
    context.nativeEntry = pointer;
    if (selfValue) context.selfValue = *selfValue;
    auto function = reinterpret_cast<cffi_native_function>(pointer);
    return function(context);
}

// as 实例
// as instance
// as ᠶᠢᠨᠢᠰ
ffi_instance* asInstance(const value& v){
    if (v.type != "native_object" || v.ptr == nullptr) return nullptr;
    return static_cast<ffi_instance*>(v.ptr);
}
}


size_t cffi::argc() const{
    return argv.size();
}

value cffi::arg(size_t index) const{
    if (index >= argv.size()) return vm ? vm->makeNone() : value{};
    return argv[index];
}

std::string cffi::arg_str(size_t index) const{
    return vm ? vm->toString(arg(index)) : "";
}

long long cffi::arg_int(size_t index) const{
    return vm ? vm->toInt(arg(index)) : 0;
}

double cffi::arg_float(size_t index) const{
    return vm ? vm->toFloat(arg(index)) : 0.0;
}

bool cffi::arg_bool(size_t index) const{
    return vm ? vm->truthy(arg(index)) : false;
}

bool cffi::has_kwarg(const std::string& name) const{
    return kwargv.find(name) != kwargv.end();
}

value cffi::kwarg(const std::string& name) const{
    auto it = kwargv.find(name);
    if (it == kwargv.end()) return none();
    return it->second;
}

long long cffi::kwarg_int(const std::string& name, long long fallback) const{
    auto it = kwargv.find(name);
    if (it == kwargv.end() || !vm) return fallback;
    return vm->toInt(it->second);
}

std::string cffi::kwarg_str(const std::string& name, const std::string& fallback) const{
    auto it = kwargv.find(name);
    if (it == kwargv.end() || !vm) return fallback;
    return vm->toString(it->second);
}

bool cffi::kwarg_bool(const std::string& name, bool fallback) const{
    auto it = kwargv.find(name);
    if (it == kwargv.end() || !vm) return fallback;
    return vm->truthy(it->second);
}


value cffi::none() const{
    return vm ? vm->makeNone() : value{};
}

value cffi::mkint(long long v) const{
    return vm ? vm->makeInt(v) : value{};
}

value cffi::mkfloat(double v) const{
    return vm ? vm->makeFloat(v) : value{};
}

value cffi::mkbool(bool v) const{
    return vm ? vm->makeBool(v) : value{};
}

value cffi::mkstr(const std::string& v) const{
    return vm ? vm->makeString(v) : value{};
}

value cffi::mklist(const std::vector<value>& elements) const{
    return vm ? vm->makeList(elements) : value{};
}

value cffi::mkdict(const std::vector<std::pair<value, value>>& items) const{
    return vm ? vm->makeDict(items) : value{};
}

value cffi::mkobject(void* handle) const{
    if (!vm || !library || !currentClass) return none();
    return vm->makeNativeObject(library, currentClass, handle);
}


value cffi::self() const{
    return selfValue;
}

value cffi::attr(const std::string& name) const{
    if (auto* instance = asInstance(selfValue)){
        auto it = instance->attributes.find(name);
        if (it != instance->attributes.end()) return it->second;
    }
    return none();
}

void cffi::setattr(const std::string& name, const value& v){
    if (auto* instance = asInstance(selfValue)){
        instance->attributes[name] = v;
    }
}

value cffi::attr(const value& object, const std::string& name) const{
    if (auto* instance = asInstance(object)){
        auto it = instance->attributes.find(name);
        if (it != instance->attributes.end()) return it->second;
    }
    return none();
}

void cffi::setattr(const value& object, const std::string& name, const value& v){
    if (auto* instance = asInstance(object)){
        instance->attributes[name] = v;
    }
}


ffilib cffi_lib(const std::string& name,
                const std::map<std::string, void*>& functions,
                const std::map<std::string, void*>& data,
                const std::map<std::string, ffi_class>& classes){
    ffilib library;
    library.name = name;
    library.functions = functions;
    library.data = data;
    library.classes = classes;
    return library;
}

ffi_class cffi_class(const std::string& name,
                     void* constructor,
                     const std::map<std::string, void*>& methods,
                     const std::map<std::string, void*>& data){
    ffi_class klass;
    klass.name = name;
    klass.constructor = constructor;
    klass.methods = methods;
    klass.data = data;
    return klass;
}


bool cffiCallFunction(const ffilib& library, const std::string& name,
                      vm& vm, const std::vector<value>& args, value& result,
                      const std::map<std::string, value>& kwargs){
    auto it = library.functions.find(name);
    if (it == library.functions.end() || it->second == nullptr) return false;
    result = invokeNative(it->second, vm, &library, nullptr, args, nullptr, kwargs);
    return true;
}

bool cffiCallData(const ffilib& library, const std::string& name,
                  vm& vm, value& result){
    auto it = library.data.find(name);
    if (it == library.data.end() || it->second == nullptr) return false;
    result = invokeNative(it->second, vm, &library, nullptr, {}, nullptr);
    return true;
}

bool cffiCallClassData(const ffilib& library, const ffi_class& klass,
                       const std::string& name, vm& vm, value& result){
    auto it = klass.data.find(name);
    if (it == klass.data.end() || it->second == nullptr) return false;
    result = invokeNative(it->second, vm, &library, &klass, {}, nullptr);
    return true;
}

bool cffiCallConstructor(const ffilib& library, const ffi_class& klass,
                         vm& vm, const std::vector<value>& args, value& result,
                         const std::map<std::string, value>& kwargs){
    if (klass.constructor != nullptr){
        result = invokeNative(klass.constructor, vm, &library, &klass, args, nullptr, kwargs);
        if (result.type == "native_object") return true;
    }

    result = vm.makeNativeObject(&library, &klass, nullptr);
    return true;
}

bool cffiCallMethod(vm& vm, const value& self, const std::string& name,
                    const std::vector<value>& args, value& result,
                    const std::map<std::string, value>& kwargs){
    ffi_instance* instance = asInstance(self);
    if (!instance || !instance->klass) return false;
    auto it = instance->klass->methods.find(name);
    if (it == instance->klass->methods.end() || it->second == nullptr) return false;
    result = invokeNative(it->second, vm, instance->library, instance->klass, args, &self, kwargs);
    return true;
}

}
