#include "../cffi.hpp"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

namespace minipy {

namespace {

// 匹配块
// A matching block
// ᠲᠠᠭᠠᠷᠠᠯ ᠬᠡᠰᠡᠭ
struct MatchBlock{
    long long a = 0;
    long long b = 0;
    long long size = 0;
};

// 操作码
// An edit opcode
// ᠦᠢᠯᠡ ᠶᠢᠨ ᠲᠡᠮᠳᠡᠭ
struct OpCode{
    std::string tag;
    long long i1 = 0;
    long long i2 = 0;
    long long j1 = 0;
    long long j2 = 0;
};

// 序列匹配器状态
// Sequence matcher state
// ᠳᠠᠪᠬᠠᠷᠢᠶ᠎ᠠ ᠲᠠᠭᠠᠷᠠᠯ ᠦᠨ ᠪᠠᠶᠢᠳᠠᠯ
struct MatcherState{
    value a;
    value b;
    std::vector<value> aItems;
    std::vector<value> bItems;
    std::unordered_map<std::string, std::vector<long long>> b2j;
    bool autojunk = true;
};

// 元素键
// Key of an element for hashing
// ᠬᠡᠰᠡᠭ ᠦᠨ ᠲᠦᠯᠬᠢᠭᠦᠷ
std::string elementKey(cffi& a, const value& v){
    return pythonRepr(*a.vm, v);
}

// 取得实例状态
// Get the instance state
// ᠬᠢᠶ᠎ᠡ ᠶᠢᠨ ᠪᠠᠶᠢᠳᠠᠯ ᠢ ᠠᠪᠬᠤ
MatcherState* stateOf(cffi& a){
    value self = a.self();
    if (self.type == "native_object" && self.ptr){
        auto* inst = static_cast<ffi_instance*>(self.ptr);
        return static_cast<MatcherState*>(inst->handle);
    }
    return nullptr;
}

// 读取序列为元素列表
// Read a sequence into an element list
// ᠳᠠᠪᠬᠠᠷᠢᠶ᠎ᠠ ᠢ ᠬᠡᠰᠡᠭ ᠦᠨ ᠵᠢᠭᠰᠠᠭ᠎ᠠᠯ ᠪᠣᠯᠭᠠᠨ᠎ᠠ
bool readSequence(cffi& a, const value& source, std::vector<value>& items){
    a.vm->materializeIterable(source, items);
    return !a.vm->hasError();
}

// 重建 b 索引
// Rebuild the b-side index
// b ᠲᠠᠯᠠ ᠶᠢᠨ ᠲᠦᠯᠬᠢᠭᠦᠷ ᠢ ᠰᠢᠨᠡᠴᠢᠯᠡᠬᠦ
void rebuildIndex(cffi& a, MatcherState& state){
    state.b2j.clear();
    std::unordered_map<std::string, long long> counts;
    for (size_t j = 0; j < state.bItems.size(); ++j){
        std::string key = elementKey(a, state.bItems[j]);
        state.b2j[key].push_back(static_cast<long long>(j));
        counts[key]++;
    }
    if (state.autojunk && state.bItems.size() >= 200){
        long long threshold = static_cast<long long>(state.bItems.size()) / 100 + 1;
        for (auto& entry : counts){
            if (entry.second > threshold) state.b2j.erase(entry.first);
        }
    }
}

// 设置两个序列
// Set both sequences
// ᠬᠣᠶᠠᠷ ᠳᠠᠪᠬᠠᠷᠢᠶ᠎ᠠ ᠲᠣᠭᠲᠠᠭᠤᠯᠬᠤ
bool setSeqs(cffi& a, MatcherState& state, const value& left, const value& right){
    state.a = left;
    state.b = right;
    state.aItems.clear();
    state.bItems.clear();
    if (!readSequence(a, left, state.aItems)) return false;
    if (!readSequence(a, right, state.bItems)) return false;
    rebuildIndex(a, state);
    return true;
}

// 最长匹配
// Find the longest matching block
// ᠤᠲᠠᠭᠠᠷ ᠲᠠᠭᠠᠷᠠᠯ ᠢ ᠣᠯᠬᠤ
MatchBlock longestMatch(cffi& a, MatcherState& state,
                        long long alo, long long ahi, long long blo, long long bhi){
    MatchBlock best;
    best.a = alo;
    best.b = blo;
    best.size = 0;
    std::unordered_map<long long, long long> previous;
    for (long long i = alo; i < ahi; ++i){
        std::unordered_map<long long, long long> current;
        auto found = state.b2j.find(elementKey(a, state.aItems[static_cast<size_t>(i)]));
        if (found != state.b2j.end()){
            for (long long j : found->second){
                if (j < blo) continue;
                if (j >= bhi) break;
                long long length = 1;
                auto prev = previous.find(j - 1);
                if (prev != previous.end()) length = prev->second + 1;
                current[j] = length;
                if (length > best.size){
                    best.size = length;
                    best.a = i - length + 1;
                    best.b = j - length + 1;
                }
            }
        }
        previous.swap(current);
    }
    while (best.a > alo && best.b > blo &&
           elementKey(a, state.aItems[static_cast<size_t>(best.a - 1)]) ==
           elementKey(a, state.bItems[static_cast<size_t>(best.b - 1)])){
        best.a--;
        best.b--;
        best.size++;
    }
    while (best.a + best.size < ahi && best.b + best.size < bhi &&
           elementKey(a, state.aItems[static_cast<size_t>(best.a + best.size)]) ==
           elementKey(a, state.bItems[static_cast<size_t>(best.b + best.size)])){
        best.size++;
    }
    return best;
}

// 全部匹配块
// Compute all matching blocks
// ᠪᠦᠭᠦᠳᠡ ᠲᠠᠭᠠᠷᠠᠯ ᠬᠡᠰᠡᠭ
std::vector<MatchBlock> matchingBlocks(cffi& a, MatcherState& state){
    long long la = static_cast<long long>(state.aItems.size());
    long long lb = static_cast<long long>(state.bItems.size());
    std::vector<MatchBlock> blocks;
    std::vector<std::vector<long long>> queue;
    queue.push_back({0, la, 0, lb});
    while (!queue.empty()){
        std::vector<long long> range = queue.back();
        queue.pop_back();
        MatchBlock match = longestMatch(a, state, range[0], range[1], range[2], range[3]);
        if (match.size){
            blocks.push_back(match);
            if (range[0] < match.a && range[2] < match.b){
                queue.push_back({range[0], match.a, range[2], match.b});
            }
            if (match.a + match.size < range[1] && match.b + match.size < range[3]){
                queue.push_back({match.a + match.size, range[1],
                                 match.b + match.size, range[3]});
            }
        }
    }
    std::sort(blocks.begin(), blocks.end(), [](const MatchBlock& x, const MatchBlock& y){
        return x.a < y.a;
    });
    std::vector<MatchBlock> merged;
    for (auto& block : blocks){
        if (!merged.empty() && merged.back().a + merged.back().size == block.a &&
            merged.back().b + merged.back().size == block.b){
            merged.back().size += block.size;
        } else {
            merged.push_back(block);
        }
    }
    MatchBlock sentinel;
    sentinel.a = la;
    sentinel.b = lb;
    sentinel.size = 0;
    merged.push_back(sentinel);
    return merged;
}

// 全部操作码
// Compute edit opcodes
// ᠪᠦᠭᠦᠳᠡ ᠦᠢᠯᠡ ᠶᠢᠨ ᠲᠡᠮᠳᠡᠭ
std::vector<OpCode> opCodes(cffi& a, MatcherState& state){
    std::vector<MatchBlock> blocks = matchingBlocks(a, state);
    std::vector<OpCode> codes;
    long long i = 0, j = 0;
    for (auto& block : blocks){
        std::string tag;
        if (i < block.a && j < block.b) tag = "replace";
        else if (i < block.a) tag = "delete";
        else if (j < block.b) tag = "insert";
        if (!tag.empty()){
            OpCode code;
            code.tag = tag;
            code.i1 = i;
            code.i2 = block.a;
            code.j1 = j;
            code.j2 = block.b;
            codes.push_back(code);
        }
        i = block.a + block.size;
        j = block.b + block.size;
        if (block.size){
            OpCode code;
            code.tag = "equal";
            code.i1 = block.a;
            code.i2 = i;
            code.j1 = block.b;
            code.j2 = j;
            codes.push_back(code);
        }
    }
    return codes;
}

// 分组操作码
// Group opcodes with context
// ᠬᠠᠮᠲᠤ ᠦᠢᠯᠡ ᠶᠢᠨ ᠲᠡᠮᠳᠡᠭ
std::vector<std::vector<OpCode>> groupedOpcodes(cffi& a, MatcherState& state, long long n){
    if (n < 0) n = 0;
    std::vector<OpCode> codes = opCodes(a, state);
    if (codes.empty()){
        OpCode code;
        code.tag = "equal";
        code.i1 = 0;
        code.i2 = 1;
        code.j1 = 0;
        code.j2 = 1;
        codes.push_back(code);
    }
    if (codes.front().tag == "equal"){
        OpCode& first = codes.front();
        if (first.i2 - n > first.i1) first.i1 = first.i2 - n;
        if (first.j2 - n > first.j1) first.j1 = first.j2 - n;
    }
    if (codes.back().tag == "equal"){
        OpCode& last = codes.back();
        if (last.i1 + n < last.i2) last.i2 = last.i1 + n;
        if (last.j1 + n < last.j2) last.j2 = last.j1 + n;
    }
    std::vector<std::vector<OpCode>> groups;
    std::vector<OpCode> group;
    long long nn = n + n;
    for (auto& code : codes){
        if (code.tag == "equal" && code.i2 - code.i1 > nn){
            OpCode head = code;
            head.i2 = code.i1 + n;
            head.j2 = code.j1 + n;
            group.push_back(head);
            groups.push_back(group);
            group.clear();
            code.i1 = code.i2 - n;
            code.j1 = code.j2 - n;
        }
        group.push_back(code);
    }
    if (!group.empty() && !(group.size() == 1 && group.front().tag == "equal")){
        groups.push_back(group);
    }
    return groups;
}

// 操作码转元组
// Convert an opcode to a tuple
// ᠲᠡᠮᠳᠡᠭ ᠢ ᠪᠣᠯᠪᠣᠰᠤᠨ ᠪᠣᠯᠭᠠᠨ᠎ᠠ
value codeTuple(cffi& a, const OpCode& code){
    std::vector<value> items = {
        a.mkstr(code.tag), a.mkint(code.i1), a.mkint(code.i2),
        a.mkint(code.j1), a.mkint(code.j2)
    };
    return a.vm->makeTuple(items);
}

// 匹配块转元组
// Convert a match block to a tuple
// ᠲᠠᠭᠠᠷᠠᠯ ᠬᠡᠰᠡᠭ ᠢ ᠪᠣᠯᠪᠣᠰᠤᠨ ᠪᠣᠯᠭᠠᠨ᠎ᠠ
value blockTuple(cffi& a, const MatchBlock& block){
    std::vector<value> items = {a.mkint(block.a), a.mkint(block.b), a.mkint(block.size)};
    return a.vm->makeTuple(items);
}

// 读取行列表
// Read a list of lines
// ᠮᠥᠷᠢ ᠶᠢᠨ ᠵᠢᠭᠰᠠᠭ᠎ᠠᠯ ᠢ ᠦᠨᠰᠢᠬᠦ
bool readLines(cffi& a, const value& source, std::vector<std::string>& lines){
    std::vector<value> items;
    a.vm->materializeIterable(source, items);
    if (a.vm->hasError()) return false;
    for (auto& item : items) lines.push_back(a.vm->toString(item));
    return true;
}

// 统一差异范围格式
// Format a unified diff range
// ᠨᠢᠭᠡᠳᠦᠭᠰᠡᠨ ᠶᠢᠯᠭ᠎ᠠ ᠶᠢᠨ ᠬᠦᠷᠢᠶ᠎ᠡ
std::string unifiedRange(long long start, long long stop){
    long long beginning = start + 1;
    long long length = stop - start;
    if (length == 1) return std::to_string(beginning);
    if (length == 0) beginning--;
    return std::to_string(beginning) + "," + std::to_string(length);
}

// 上下文差异范围格式
// Format a context diff range
// ᠠᠭᠤᠷ ᠦᠨ ᠶᠢᠯᠭ᠎ᠠ ᠶᠢᠨ ᠬᠦᠷᠢᠶ᠎ᠡ
std::string contextRange(long long start, long long stop){
    long long beginning = start + 1;
    long long length = stop - start;
    if (length == 0) beginning--;
    if (length <= 1) return std::to_string(beginning);
    return std::to_string(beginning) + "," + std::to_string(length);
}

// 打开序列匹配器
// Prepare a matcher over two line lists
// ᠬᠣᠶᠠᠷ ᠮᠥᠷᠢ ᠶᠢᠨ ᠵᠢᠭᠰᠠᠭ᠎ᠠᠯ ᠳ᠋ᠡᠭᠡᠷ ᠡᠴᠡ ᠲᠠᠭᠠᠷᠠᠯ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
MatcherState buildMatcher(cffi& a, const std::vector<std::string>& left,
                          const std::vector<std::string>& right){
    MatcherState state;
    for (auto& line : left) state.aItems.push_back(a.mkstr(line));
    for (auto& line : right) state.bItems.push_back(a.mkstr(line));
    rebuildIndex(a, state);
    return state;
}

// 构建字符串列表值
// Build a list of strings
// ᠦᠰᠦᠭ ᠵᠢᠭᠰᠠᠭ᠎ᠠᠯ ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠬᠤ
value stringList(cffi& a, const std::vector<std::string>& lines){
    std::vector<value> items;
    for (auto& line : lines) items.push_back(a.mkstr(line));
    return a.mklist(items);
}
}

// SequenceMatcher 构造器
// SequenceMatcher constructor
// SequenceMatcher ᠪᠠᠶᠢᠨᠳᠠᠭᠤᠯᠤᠭᠴᠢ
static value fnMatcherConstructor(cffi& a){
    MatcherState* state = new MatcherState();
    value first = a.arg(1);
    value second = a.arg(2);
    if (!setSeqs(a, *state, first, second)) return a.none();
    value obj = a.mkobject(state);
    a.setattr(obj, "a", first);
    a.setattr(obj, "b", second);
    return obj;
}

// set_seqs
// set_seqs
// set_seqs
static value fnMatcherSetSeqs(cffi& a){
    MatcherState* state = stateOf(a);
    if (!state) return a.none();
    value first = a.arg(0), second = a.arg(1);
    if (!setSeqs(a, *state, first, second)) return a.none();
    a.setattr("a", first);
    a.setattr("b", second);
    return a.none();
}

// set_seq1
// set_seq1
// set_seq1
static value fnMatcherSetSeq1(cffi& a){
    MatcherState* state = stateOf(a);
    if (!state) return a.none();
    value first = a.arg(0);
    state->a = first;
    state->aItems.clear();
    if (!readSequence(a, first, state->aItems)) return a.none();
    a.setattr("a", first);
    return a.none();
}

// set_seq2
// set_seq2
// set_seq2
static value fnMatcherSetSeq2(cffi& a){
    MatcherState* state = stateOf(a);
    if (!state) return a.none();
    value second = a.arg(0);
    state->b = second;
    state->bItems.clear();
    if (!readSequence(a, second, state->bItems)) return a.none();
    rebuildIndex(a, *state);
    a.setattr("b", second);
    return a.none();
}

// find_longest_match
// find_longest_match
// find_longest_match
static value fnMatcherFindLongest(cffi& a){
    MatcherState* state = stateOf(a);
    if (!state) return a.none();
    long long alo = (a.argc() > 0) ? a.arg_int(0) : 0;
    long long ahi = (a.argc() > 1) ? a.arg_int(1) : static_cast<long long>(state->aItems.size());
    long long blo = (a.argc() > 2) ? a.arg_int(2) : 0;
    long long bhi = (a.argc() > 3) ? a.arg_int(3) : static_cast<long long>(state->bItems.size());
    return blockTuple(a, longestMatch(a, *state, alo, ahi, blo, bhi));
}

// get_matching_blocks
// get_matching_blocks
// get_matching_blocks
static value fnMatcherMatchingBlocks(cffi& a){
    MatcherState* state = stateOf(a);
    if (!state) return a.none();
    std::vector<value> blocks;
    for (auto& block : matchingBlocks(a, *state)) blocks.push_back(blockTuple(a, block));
    return a.mklist(blocks);
}

// get_opcodes
// get_opcodes
// get_opcodes
static value fnMatcherOpcodes(cffi& a){
    MatcherState* state = stateOf(a);
    if (!state) return a.none();
    std::vector<value> codes;
    for (auto& code : opCodes(a, *state)) codes.push_back(codeTuple(a, code));
    return a.mklist(codes);
}

// get_grouped_opcodes
// get_grouped_opcodes
// get_grouped_opcodes
static value fnMatcherGroupedOpcodes(cffi& a){
    MatcherState* state = stateOf(a);
    if (!state) return a.none();
    long long n = (a.argc() > 0) ? a.arg_int(0) : 3;
    std::vector<value> groups;
    for (auto& group : groupedOpcodes(a, *state, n)){
        std::vector<value> lines;
        for (auto& code : group) lines.push_back(codeTuple(a, code));
        groups.push_back(a.mklist(lines));
    }
    return a.mklist(groups);
}

// ratio
// ratio
// ratio
static value fnMatcherRatio(cffi& a){
    MatcherState* state = stateOf(a);
    if (!state) return a.mkfloat(0.0);
    long long matches = 0;
    for (auto& block : matchingBlocks(a, *state)) matches += block.size;
    long long total = static_cast<long long>(state->aItems.size() + state->bItems.size());
    if (total == 0) return a.mkfloat(1.0);
    return a.mkfloat(2.0 * static_cast<double>(matches) / static_cast<double>(total));
}

// quick_ratio
// quick_ratio
// quick_ratio
static value fnMatcherQuickRatio(cffi& a){
    MatcherState* state = stateOf(a);
    if (!state) return a.mkfloat(0.0);
    std::unordered_map<std::string, long long> counts;
    for (auto& item : state->aItems) counts[elementKey(a, item)]++;
    long long matches = 0;
    for (auto& item : state->bItems){
        auto found = counts.find(elementKey(a, item));
        if (found != counts.end() && found->second > 0){
            matches++;
            found->second--;
        }
    }
    long long total = static_cast<long long>(state->aItems.size() + state->bItems.size());
    if (total == 0) return a.mkfloat(1.0);
    return a.mkfloat(2.0 * static_cast<double>(matches) / static_cast<double>(total));
}

// real_quick_ratio
// real_quick_ratio
// real_quick_ratio
static value fnMatcherRealQuickRatio(cffi& a){
    MatcherState* state = stateOf(a);
    if (!state) return a.mkfloat(0.0);
    long long la = static_cast<long long>(state->aItems.size());
    long long lb = static_cast<long long>(state->bItems.size());
    if (la + lb == 0) return a.mkfloat(1.0);
    long long smallest = la < lb ? la : lb;
    return a.mkfloat(2.0 * static_cast<double>(smallest) / static_cast<double>(la + lb));
}

// fn Get Close Matches
// fn get_close_matches
// fn Get Close Matches
static value fnGetCloseMatches(cffi& a){
    std::string word = a.arg_str(0);
    std::vector<std::string> possibilities;
    if (!readLines(a, a.arg(1), possibilities)) return a.none();
    long long n = (a.argc() > 2) ? a.arg_int(2) : 3;
    double cutoff = (a.argc() > 3) ? a.arg_float(3) : 0.6;
    std::vector<std::pair<double, std::string>> scored;
    for (auto& candidate : possibilities){
        MatcherState state;
        if (!readSequence(a, a.mkstr(word), state.aItems)) return a.none();
        if (!readSequence(a, a.mkstr(candidate), state.bItems)) return a.none();
        rebuildIndex(a, state);
        long long matches = 0;
        for (auto& block : matchingBlocks(a, state)) matches += block.size;
        long long total = static_cast<long long>(state.aItems.size() + state.bItems.size());
        double ratio = total ? 2.0 * static_cast<double>(matches) / static_cast<double>(total) : 1.0;
        if (ratio >= cutoff) scored.push_back({ratio, candidate});
    }
    std::stable_sort(scored.begin(), scored.end(),
                     [](const std::pair<double, std::string>& x,
                        const std::pair<double, std::string>& y){
                         return x.first > y.first;
                     });
    std::vector<std::string> result;
    for (auto& item : scored){
        if (static_cast<long long>(result.size()) >= n) break;
        result.push_back(item.second);
    }
    return stringList(a, result);
}

// fn Unified Diff
// fn unified_diff
// fn Unified Diff
static value fnUnifiedDiff(cffi& a){
    std::vector<std::string> left, right;
    if (!readLines(a, a.arg(0), left)) return a.none();
    if (!readLines(a, a.arg(1), right)) return a.none();
    std::string fromfile = (a.argc() > 2) ? a.arg_str(2) : "";
    std::string tofile = (a.argc() > 3) ? a.arg_str(3) : "";
    std::string fromdate = (a.argc() > 4) ? a.arg_str(4) : "";
    std::string todate = (a.argc() > 5) ? a.arg_str(5) : "";
    long long n = (a.argc() > 6) ? a.arg_int(6) : 3;
    std::string lineterm = (a.argc() > 7) ? a.arg_str(7) : "\n";
    MatcherState state = buildMatcher(a, left, right);
    std::vector<std::string> out;
    bool started = false;
    for (auto& group : groupedOpcodes(a, state, n)){
        if (!started){
            started = true;
            std::string fromLine = "--- " + fromfile + (fromdate.empty() ? "" : "\t" + fromdate) + lineterm;
            std::string toLine = "+++ " + tofile + (todate.empty() ? "" : "\t" + todate) + lineterm;
            out.push_back(fromLine);
            out.push_back(toLine);
        }
        OpCode first = group.front();
        OpCode last = group.back();
        out.push_back("@@ -" + unifiedRange(first.i1, last.i2) + " +" +
                      unifiedRange(first.j1, last.j2) + " @@" + lineterm);
        for (auto& code : group){
            if (code.tag == "equal"){
                for (long long i = code.i1; i < code.i2; ++i) out.push_back(" " + left[static_cast<size_t>(i)]);
            } else if (code.tag == "replace"){
                for (long long i = code.i1; i < code.i2; ++i) out.push_back("-" + left[static_cast<size_t>(i)]);
                for (long long j = code.j1; j < code.j2; ++j) out.push_back("+" + right[static_cast<size_t>(j)]);
            } else if (code.tag == "delete"){
                for (long long i = code.i1; i < code.i2; ++i) out.push_back("-" + left[static_cast<size_t>(i)]);
            } else if (code.tag == "insert"){
                for (long long j = code.j1; j < code.j2; ++j) out.push_back("+" + right[static_cast<size_t>(j)]);
            }
        }
    }
    return stringList(a, out);
}

// fn Context Diff
// fn context_diff
// fn Context Diff
static value fnContextDiff(cffi& a){
    std::vector<std::string> left, right;
    if (!readLines(a, a.arg(0), left)) return a.none();
    if (!readLines(a, a.arg(1), right)) return a.none();
    std::string fromfile = (a.argc() > 2) ? a.arg_str(2) : "";
    std::string tofile = (a.argc() > 3) ? a.arg_str(3) : "";
    std::string fromdate = (a.argc() > 4) ? a.arg_str(4) : "";
    std::string todate = (a.argc() > 5) ? a.arg_str(5) : "";
    long long n = (a.argc() > 6) ? a.arg_int(6) : 3;
    std::string lineterm = (a.argc() > 7) ? a.arg_str(7) : "\n";
    MatcherState state = buildMatcher(a, left, right);
    std::vector<std::string> out;
    bool started = false;
    for (auto& group : groupedOpcodes(a, state, n)){
        if (!started){
            started = true;
            out.push_back("*** " + fromfile + (fromdate.empty() ? "" : "\t" + fromdate) + lineterm);
            out.push_back("--- " + tofile + (todate.empty() ? "" : "\t" + todate) + lineterm);
        }
        OpCode first = group.front();
        OpCode last = group.back();
        out.push_back("***************" + lineterm);
        out.push_back("*** " + contextRange(first.i1, last.i2) + " ****" + lineterm);
        for (auto& code : group){
            if (code.tag == "equal"){
                for (long long i = code.i1; i < code.i2; ++i) out.push_back("  " + left[static_cast<size_t>(i)]);
            } else if (code.tag == "replace"){
                for (long long i = code.i1; i < code.i2; ++i) out.push_back("! " + left[static_cast<size_t>(i)]);
                for (long long j = code.j1; j < code.j2; ++j) out.push_back("! " + right[static_cast<size_t>(j)]);
            } else if (code.tag == "delete"){
                for (long long i = code.i1; i < code.i2; ++i) out.push_back("- " + left[static_cast<size_t>(i)]);
            } else if (code.tag == "insert"){
                for (long long j = code.j1; j < code.j2; ++j) out.push_back("+ " + right[static_cast<size_t>(j)]);
            }
        }
        out.push_back("--- " + contextRange(first.j1, last.j2) + " ----" + lineterm);
    }
    return stringList(a, out);
}

// fn Ndiff
// fn ndiff
// fn Ndiff
static value fnNdiff(cffi& a){
    std::vector<std::string> left, right;
    if (!readLines(a, a.arg(0), left)) return a.none();
    if (!readLines(a, a.arg(1), right)) return a.none();
    std::string lineterm = (a.argc() > 2) ? a.arg_str(2) : "\n";
    MatcherState state = buildMatcher(a, left, right);
    std::vector<std::string> out;
    for (auto& code : opCodes(a, state)){
        if (code.tag == "equal"){
            for (long long i = code.i1; i < code.i2; ++i) out.push_back("  " + left[static_cast<size_t>(i)] + lineterm);
        } else if (code.tag == "replace" || code.tag == "delete"){
            for (long long i = code.i1; i < code.i2; ++i) out.push_back("- " + left[static_cast<size_t>(i)] + lineterm);
            if (code.tag == "replace"){
                for (long long j = code.j1; j < code.j2; ++j) out.push_back("+ " + right[static_cast<size_t>(j)] + lineterm);
            }
        } else if (code.tag == "insert"){
            for (long long j = code.j1; j < code.j2; ++j) out.push_back("+ " + right[static_cast<size_t>(j)] + lineterm);
        }
    }
    return stringList(a, out);
}

// cffi difflib lib
// cffi difflib lib
// cffi difflib lib
ffilib cffi_difflib_lib(){
    ffi_class matcherClass;
    matcherClass.name = "SequenceMatcher";
    matcherClass.constructor = (void*)fnMatcherConstructor;
    matcherClass.methods = {
        {"set_seqs", (void*)fnMatcherSetSeqs},
        {"set_seq1", (void*)fnMatcherSetSeq1},
        {"set_seq2", (void*)fnMatcherSetSeq2},
        {"find_longest_match", (void*)fnMatcherFindLongest},
        {"get_matching_blocks", (void*)fnMatcherMatchingBlocks},
        {"get_opcodes", (void*)fnMatcherOpcodes},
        {"get_grouped_opcodes", (void*)fnMatcherGroupedOpcodes},
        {"ratio", (void*)fnMatcherRatio},
        {"quick_ratio", (void*)fnMatcherQuickRatio},
        {"real_quick_ratio", (void*)fnMatcherRealQuickRatio}
    };
    return cffi_lib(
        "difflib",
        {
            {"get_close_matches", (void*)fnGetCloseMatches},
            {"unified_diff", (void*)fnUnifiedDiff},
            {"context_diff", (void*)fnContextDiff},
            {"ndiff", (void*)fnNdiff}
        },
        {},
        {
            {"SequenceMatcher", matcherClass}
        });
}

}
