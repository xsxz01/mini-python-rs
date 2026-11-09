#include "../cffi.hpp"

#include <string>
#include <vector>

namespace minipy {

namespace {

// 依赖图节点
// Dependency graph node
// ᠬᠠᠷᠢᠴᠠᠭ᠎ᠠ ᠶᠢᠨ ᠲᠥᠪ
struct GraphNode{
    value node;
    std::vector<value> predecessors;
    bool done = false;
    bool returned = false;
};

// 拓扑排序器状态
// Topological sorter state
// ᠲᠣᠭ᠎ᠠ ᠶᠢᠨ ᠡᠷᠡᠮᠪᠢᠯᠡᠯ ᠦᠨ ᠪᠠᠶᠢᠳᠠᠯ
struct GraphState{
    std::vector<GraphNode> nodes;
    bool prepared = false;
};

// 取实例状态
// Get the instance state
// ᠬᠢᠶ᠎ᠡ ᠶᠢᠨ ᠪᠠᠶᠢᠳᠠᠯ ᠢ ᠠᠪᠬᠤ
GraphState* stateOf(cffi& a){
    value self = a.self();
    if (self.type == "native_object" && self.ptr){
        auto* inst = static_cast<ffi_instance*>(self.ptr);
        return static_cast<GraphState*>(inst->handle);
    }
    return nullptr;
}

// 查找节点下标
// Find a node index
// ᠲᠥᠪ ᠦᠨ ᠳᠤᠭ᠎ᠠᠷ ᠢ ᠬᠠᠯᠠᠬᠤ
long long findNode(cffi& a, GraphState& state, const value& node){
    for (size_t i = 0; i < state.nodes.size(); ++i){
        if (a.vm->valuesEqual(state.nodes[i].node, node)) return static_cast<long long>(i);
    }
    return -1;
}

// 添加或取得节点
// Add or fetch a node
// ᠲᠥᠪ ᠨᠡᠮᠡᠭᠳᠡᠬᠦ ᠪᠤᠶᠤ ᠣᠯᠬᠤ
long long ensureNode(cffi& a, GraphState& state, const value& node){
    long long index = findNode(a, state, node);
    if (index >= 0) return index;
    GraphNode entry;
    entry.node = node;
    state.nodes.push_back(entry);
    return static_cast<long long>(state.nodes.size() - 1);
}

// 构建节点列表值
// Build a list of nodes
// ᠲᠥᠪ ᠦᠨ ᠵᠢᠭᠰᠠᠭ᠎ᠠᠯ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
value nodeList(cffi& a, const std::vector<value>& nodes){
    return a.mklist(nodes);
}
}

// TopologicalSorter 构造器
// TopologicalSorter constructor
// TopologicalSorter ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠭᠴᠢ
static value fnSorterConstructor(cffi& a){
    GraphState* state = new GraphState();
    if (a.argc() >= 1 && a.arg(0).type == "dict" && a.arg(0).ptr){
        auto* items = static_cast<dict_t*>(a.arg(0).ptr);
        for (auto& item : *items){
            long long index = ensureNode(a, *state, item.first);
            std::vector<value> preds;
            a.vm->materializeIterable(item.second, preds);
            for (auto& pred : preds){
                long long predIndex = ensureNode(a, *state, pred);
                state->nodes[static_cast<size_t>(predIndex)].done = false;
                state->nodes[static_cast<size_t>(index)].predecessors.push_back(pred);
            }
        }
    }
    return a.mkobject(state);
}

// add(node, *predecessors)
// add(node, *predecessors)
// add(node, *predecessors)
static value fnSorterAdd(cffi& a){
    GraphState* state = stateOf(a);
    if (!state) return a.none();
    if (state->prepared){
        a.vm->raiseError("ValueError", "cannot add nodes after prepare() was called");
        return a.none();
    }
    if (a.argc() < 1){
        a.vm->raiseError("TypeError", "add() missing required argument: 'node'");
        return a.none();
    }
    value node = a.arg(0);
    long long index = ensureNode(a, *state, node);
    for (size_t i = 1; i < a.argc(); ++i){
        value pred = a.arg(i);
        ensureNode(a, *state, pred);
        state->nodes[static_cast<size_t>(index)].predecessors.push_back(pred);
    }
    return a.none();
}

// prepare
// prepare
// prepare
static value fnSorterPrepare(cffi& a){
    GraphState* state = stateOf(a);
    if (!state) return a.none();
    if (state->prepared) return a.none();
    for (auto& node : state->nodes){
        for (auto& pred : node.predecessors) ensureNode(a, *state, pred);
        node.returned = false;
        node.done = false;
    }
    state->prepared = true;

    std::vector<bool> visiting(state->nodes.size(), false);
    std::vector<bool> visited(state->nodes.size(), false);
    bool hasCycle = false;
    auto visit = [&](auto&& self, size_t index) -> void {
        if (hasCycle || visited[index]) return;
        if (visiting[index]){
            hasCycle = true;
            return;
        }
        visiting[index] = true;
        for (auto& pred : state->nodes[index].predecessors){
            long long predIndex = findNode(a, *state, pred);
            if (predIndex >= 0) self(self, static_cast<size_t>(predIndex));
        }
        visiting[index] = false;
        visited[index] = true;
    };
    for (size_t i = 0; i < state->nodes.size(); ++i) visit(visit, i);
    if (hasCycle){
        a.vm->raiseError("CycleError", "nodes are in a cycle");
        return a.none();
    }
    return a.none();
}

// get_ready
// get_ready
// get_ready
static value fnSorterGetReady(cffi& a){
    GraphState* state = stateOf(a);
    if (!state) return a.none();
    if (!state->prepared){
        a.vm->raiseError("ValueError", "prepare() must be called before get_ready()");
        return a.none();
    }
    std::vector<value> ready;
    for (size_t i = 0; i < state->nodes.size(); ++i){
        GraphNode& node = state->nodes[i];
        if (node.done || node.returned) continue;
        bool blocked = false;
        for (auto& pred : node.predecessors){
            long long predIndex = findNode(a, *state, pred);
            if (predIndex >= 0 && !state->nodes[static_cast<size_t>(predIndex)].done){
                blocked = true;
                break;
            }
        }
        if (!blocked){
            node.returned = true;
            ready.push_back(node.node);
        }
    }
    return nodeList(a, ready);
}

// done(*nodes)
// done(*nodes)
// done(*nodes)
static value fnSorterDone(cffi& a){
    GraphState* state = stateOf(a);
    if (!state) return a.none();
    if (!state->prepared){
        a.vm->raiseError("ValueError", "prepare() must be called before done()");
        return a.none();
    }
    for (size_t i = 0; i < a.argc(); ++i){
        value node = a.arg(i);
        long long index = findNode(a, *state, node);
        if (index < 0){
            a.vm->raiseError("ValueError", "node " + a.vm->toString(node) +
                                           " was not passed to add()");
            return a.none();
        }
        state->nodes[static_cast<size_t>(index)].done = true;
    }
    return a.none();
}

// is_active
// is_active
// is_active
static value fnSorterIsActive(cffi& a){
    GraphState* state = stateOf(a);
    if (!state) return a.mkbool(false);
    if (!state->prepared){
        a.vm->raiseError("ValueError", "prepare() must be called before is_active()");
        return a.none();
    }
    for (auto& node : state->nodes){
        if (!node.done) return a.mkbool(true);
    }
    return a.mkbool(false);
}

// static_order
// static_order
// static_order
static value fnSorterStaticOrder(cffi& a){
    GraphState* state = stateOf(a);
    if (!state) return a.none();
    if (state->prepared){
        a.vm->raiseError("ValueError", "cannot call static_order() after prepare()");
        return a.none();
    }
    value prepareResult = fnSorterPrepare(a);
    if (a.vm->hasError()) return prepareResult;
    std::vector<value> order;
    while (true){
        std::vector<value> ready;
        for (auto& node : state->nodes){
            if (node.done || node.returned) continue;
            bool blocked = false;
            for (auto& pred : node.predecessors){
                long long predIndex = findNode(a, *state, pred);
                if (predIndex >= 0 && !state->nodes[static_cast<size_t>(predIndex)].done){
                    blocked = true;
                    break;
                }
            }
            if (!blocked){
                node.returned = true;
                ready.push_back(node.node);
            }
        }
        if (ready.empty()) break;
        for (auto& node : ready){
            order.push_back(node);
            long long index = findNode(a, *state, node);
            if (index >= 0) state->nodes[static_cast<size_t>(index)].done = true;
        }
    }
    return nodeList(a, order);
}

// cffi graphlib lib
// cffi graphlib lib
// cffi graphlib lib
ffilib cffi_graphlib_lib(){
    ffi_class sorterClass;
    sorterClass.name = "TopologicalSorter";
    sorterClass.constructor = (void*)fnSorterConstructor;
    sorterClass.methods = {
        {"add", (void*)fnSorterAdd},
        {"prepare", (void*)fnSorterPrepare},
        {"get_ready", (void*)fnSorterGetReady},
        {"done", (void*)fnSorterDone},
        {"is_active", (void*)fnSorterIsActive},
        {"static_order", (void*)fnSorterStaticOrder}
    };
    return cffi_lib(
        "graphlib",
        {},
        {},
        {
            {"TopologicalSorter", sorterClass}
        });
}

}
