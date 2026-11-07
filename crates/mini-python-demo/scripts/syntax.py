# ==============================================
# Python 新手综合示例
# 知识点：变量、类型、运算符、if、循环、列表、字典、函数、异常、文件操作
# ==============================================

print("=" * 50)
print("【1. 变量与基础数据类型】")
print("=" * 50)

# 变量定义，不用声明类型
name = "小明"       # 字符串 str
age = 18           # 整数 int
height = 1.75      # 浮点数 float
is_student = True  # 布尔 bool

print(f"姓名：{name}, 年龄：{age}, 身高：{height}, 是否学生：{is_student}")
print(f"name类型: {type(name)}, age类型: {type(age)}")

# 算术运算
a = 10
b = 3
print(f"\n算术运算：")
print(f"{a} + {b} = {a + b}")
print(f"{a} - {b} = {a - b}")
print(f"{a} * {b} = {a * b}")
print(f"{a} / {b} = {a / b}")    # 除法得到小数
print(f"{a} // {b} = {a // b}")  # 整数除法（向下取整）
print(f"{a} % {b} = {a % b}")    # 取余数
print(f"{a} ** {b} = {a ** b}")  # 幂运算 10^3


print("\n" + "=" * 50)
print("【2. 条件判断 if / elif / else】")
print("=" * 50)

score = 78
print(f"你的分数：{score}")
if score >= 90:
    print("成绩等级：优秀")
elif score >= 80:
    print("成绩等级：良好")
elif score >= 60:
    print("成绩等级：及格")
else:
    print("成绩等级：不及格")


print("\n" + "=" * 50)
print("【3. 循环 for 和 while】")
print("=" * 50)

# for循环：遍历range
print("for循环输出 1~5：")
for i in range(1, 6):
    print(i, end=" ")
print()

# while循环
print("\nwhile循环：倒计时")
count = 3
while count > 0:
    print(count)
    count -= 1
print("倒计时结束！")

# break 跳出循环；continue 跳过本次
print("\n带break的循环：到4停止")
for num in range(1, 10):
    if num == 4:
        break
    print(num, end=" ")
print()


print("\n" + "=" * 50)
print("【4. 容器：列表list、字典dict】")
print("=" * 50)

# 列表：有序，可修改
fruit_list = ["苹果", "香蕉", "橙子"]
print(f"列表：{fruit_list}")
print(f"取第1个元素：{fruit_list[0]}")
fruit_list.append("葡萄")  # 追加元素
print(f"追加后：{fruit_list}")

# 字典：键值对，适合存信息
student_info = {
    "name": "小红",
    "age": 17,
    "hobby": "画画"
}
print(f"\n字典：{student_info}")
print(f"学生爱好：{student_info['hobby']}")
student_info["age"] = 18  # 修改值
print(f"修改年龄后：{student_info}")


print("\n" + "=" * 50)
print("【5. 函数定义 def】")
print("=" * 50)

# 定义函数，带参数，有返回值
def calc_area(width, height):
    """计算矩形面积（文档字符串，说明函数作用）"""
    area = width * height
    return area

w = 5
h = 3
res = calc_area(w, h)
print(f"矩形宽{w},高{h},面积={res}")

# 函数默认参数
def greet(name="访客"):
    print(f"你好，{name}!")

greet()
greet("小李")


print("\n" + "=" * 50)
print("【6. 异常捕获 try-except，防止程序崩溃】")
print("=" * 50)

try:
    num1 = 8
    num2 = 2
    result = num1 / num2
    print(f"相除结果 = {result}")
except ValueError:
    print("错误！你输入的不是数字！")
except ZeroDivisionError:
    print("错误！不能除以0！")
except Exception as e:
    print(f"其他异常：{e}")


print("\n" + "=" * 50)
print("【7. 文件读写】")
print("=" * 50)

# 写入文件
with open("test.txt", "w", encoding="utf-8") as f:
    f.write("这是写入的第一行\n")
    f.write("Python新手示例文件\n")
print("已经写入 test.txt")

# 读取文件
with open("test.txt", "r", encoding="utf-8") as f:
    content = f.read()
print("读取文件内容：")
print(content)


print("\n" + "=" * 50)
print("【8. 简单小项目：成绩统计】")
print("=" * 50)

def get_avg_score(scores):
    """求平均分"""
    total = sum(scores)
    avg = total / len(scores)
    return round(avg, 2)

score_list = [85, 92, 76, 88, 59]
avg_score = get_avg_score(score_list)
print(f"成绩列表：{score_list}")
print(f"最高分：{max(score_list)}")
print(f"最低分：{min(score_list)}")
print(f"平均分：{avg_score}")


print("\n===== 全部示例运行完毕 =====")

# 以下语法由 syntax runner 使用 MiniPython 逐项执行验证；失败项目与 traceback
# 由 Rust runner 报告，避免一个已知不支持的形式中断整份示例。
print("\n【9. Python 语法补充示例】")

# 解包、关键字参数、位置专用/关键字专用参数与装饰器。
syntax_args = [2, 3]
syntax_kwargs = {"scale": 4}
def syntax_decorator(function):
    return function

@syntax_decorator
def syntax_calculate(left, /, right=1, *items, scale=1, **options):
    return (left + right + len(items) + len(options)) * scale

syntax_answer = syntax_calculate(*syntax_args, **syntax_kwargs)

# lambda、条件表达式、海象表达式、集合推导式和字典展开。
syntax_increment = lambda value: value + 1
syntax_choice = syntax_increment(syntax_answer) if syntax_answer else 0
if (syntax_bound := syntax_choice) > 0:
    syntax_mapping = {**{"answer": syntax_bound}, "ok": True}
syntax_set = {value for value in [1, 2, 2]}

# 循环 else、match guard / or-pattern、异常链及异常组语法。
for syntax_item in [1, 2]:
    pass
else:
    syntax_loop_else = True

match syntax_bound:
    case 0:
        syntax_match = "zero"
    case 1 | 2 if syntax_bound > 0:
        syntax_match = "small"
    case _:
        syntax_match = "other"

try:
    raise ValueError("sample") from TypeError("cause")
except ValueError as syntax_error:
    syntax_caught = str(syntax_error)
finally:
    syntax_finished = True


type SyntaxAlias = int
print("语法补充示例执行完毕：", syntax_match)
syntax_script_complete = "yes"