# 工具类测试

本目录包含对项目中通用工具类的完整测试套件。

## 测试概览

### 测试文件

1. **test_signal_handler.cpp** - SignalHandler 类的单元测试
   - 测试信号注册和回调执行
   - 测试优雅关闭功能
   - 测试多重回调和异常处理
   - 测试线程安全性和性能

2. **test_cli_parser.cpp** - CLIParser 类的单元测试
   - 测试命令行参数解析
   - 测试各种数据类型选项
   - 测试帮助和版本信息生成
   - 测试回调函数和错误处理

3. **test_config_mgr_extended.cpp** - ConfigManager 扩展功能测试
   - 测试环境变量加载
   - 测试 .env 文件解析
   - 测试配置优先级
   - 测试类型转换和特殊字符处理

4. **test_utils_integration.cpp** - 工具类集成测试
   - 测试多个工具类的协同工作
   - 模拟真实应用场景
   - 测试复杂配置和信号处理流程

## 依赖要求

### 必需依赖
- **CMake** (>= 3.10)
- **Google Test** (gtest)
- **Google Mock** (gmock)
- **C++20** 兼容编译器

### 安装依赖

#### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install -y cmake libgtest-dev libgmock-dev
```

#### CentOS/RHEL
```bash
sudo yum install -y cmake gtest-devel gmock-devel
```

#### Arch Linux
```bash
sudo pacman -S cmake gtest gmock
```

## 运行测试

### 使用测试脚本（推荐）

我们提供了一个便捷的测试脚本 `run_tests.sh`：

```bash
# 运行所有测试
./run_tests.sh

# 显示帮助信息
./run_tests.sh --help

# 仅编译测试
./run_tests.sh --build-only

# 仅运行测试（假设已编译）
./run_tests.sh --run-only

# 运行特定测试
./run_tests.sh --test test_signal_handler

# 使用 CTest 运行
./run_tests.sh --ctest

# 清理构建文件
./run_tests.sh --clean

# 检查依赖
./run_tests.sh --check-deps

# 生成测试报告
./run_tests.sh --report
```

### 手动编译和运行

如果你想手动控制编译过程：

```bash
# 1. 创建构建目录
mkdir -p ../../build
cd ../../build

# 2. 配置 CMake
cmake .. -DCMAKE_BUILD_TYPE=Debug

# 3. 编译测试
make test_signal_handler test_cli_parser test_config_mgr_extended test_utils_integration

# 4. 运行测试
./test/utils/test_signal_handler
./test/utils/test_cli_parser
./test/utils/test_config_mgr_extended
./test/utils/test_utils_integration
```

### 使用 CTest

```bash
cd ../../build
ctest --output-on-failure -R "SignalHandlerTest|CLIParserTest|ConfigManagerExtendedTest|UtilsIntegrationTest"
```

## 测试详情

### SignalHandler 测试

测试覆盖以下功能：
- ✅ 单例模式验证
- ✅ 信号注册和回调执行
- ✅ 优雅关闭信号处理
- ✅ 多重回调支持
- ✅ 异常处理和恢复
- ✅ 线程安全性
- ✅ 性能测试
- ✅ RAII 包装器
- ✅ 资源清理

### CLIParser 测试

测试覆盖以下功能：
- ✅ 字符串、整数、浮点数、布尔选项
- ✅ 长选项和短选项形式
- ✅ 默认值处理
- ✅ 帮助和版本信息生成
- ✅ 回调函数支持
- ✅ 位置参数解析
- ✅ 错误处理和验证
- ✅ 特殊字符和 Unicode 支持
- ✅ 性能测试

### ConfigManager 扩展测试

测试覆盖以下功能：
- ✅ 环境变量直接访问
- ✅ .env 文件加载和解析
- ✅ 配置优先级（CLI > 环境变量 > 配置文件）
- ✅ 类型转换和验证
- ✅ 特殊字符和 Unicode 处理
- ✅ 批量环境变量加载
- ✅ 错误处理和边界情况
- ✅ 性能测试
- ✅ 线程安全性

### 集成测试

测试覆盖以下场景：
- ✅ CLI + Config 基本集成
- ✅ 信号处理集成
- ✅ 配置优先级验证
- ✅ 复杂真实场景模拟
- ✅ 错误处理集成
- ✅ 并发和线程安全
- ✅ 性能测试
- ✅ 资源清理验证

## 测试输出

测试会生成以下输出：

1. **控制台输出** - 实时测试结果
2. **XML 测试报告** - 用于 CI/CD 集成
3. **HTML 测试报告** - 可视化测试结果

测试报告位置：
- XML: `build/*_results.xml`
- HTML: `build/test_reports/utils_test_report.html`

## 性能基准

各测试的性能基准：

| 测试类型 | 预期执行时间 | 超时设置 |
|---------|-------------|----------|
| SignalHandler | < 10s | 30s |
| CLIParser | < 5s | 30s |
| ConfigManager | < 15s | 30s |
| Integration | < 30s | 60s |

## 故障排除

### 常见问题

1. **Google Test 未找到**
   ```bash
   # 确保安装了开发包
   sudo apt-get install libgtest-dev libgmock-dev
   ```

2. **编译错误**
   ```bash
   # 确保 C++20 支持
   g++ --version  # 应该 >= 10.0
   ```

3. **信号测试失败**
   ```bash
   # 某些系统可能限制信号处理，这是正常的
   # 可以在 Docker 容器中运行测试
   ```

4. **权限问题**
   ```bash
   # 确保测试脚本有执行权限
   chmod +x run_tests.sh
   ```

### 调试测试

使用 GDB 调试测试：

```bash
cd ../../build
gdb ./test/utils/test_signal_handler
(gdb) run
(gdb) bt  # 如果崩溃，查看堆栈跟踪
```

使用 Valgrind 检查内存问题：

```bash
cd ../../build
valgrind --leak-check=full ./test/utils/test_signal_handler
```

## CI/CD 集成

这些测试设计为可以轻松集成到 CI/CD 流水线中：

### GitHub Actions 示例

```yaml
name: Utils Tests
on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v2
    - name: Install dependencies
      run: |
        sudo apt-get update
        sudo apt-get install -y cmake libgtest-dev libgmock-dev
    - name: Run tests
      run: |
        cd test/utils
        ./run_tests.sh --ctest
    - name: Upload test results
      uses: actions/upload-artifact@v2
      with:
        name: test-results
        path: build/test_reports/
```

### Jenkins 示例

```groovy
pipeline {
    agent any
    stages {
        stage('Test Utils') {
            steps {
                sh 'cd test/utils && ./run_tests.sh --ctest'
                publishTestResults testResultsPattern: 'build/*_results.xml'
            }
        }
    }
}
```

## 贡献指南

添加新测试时请遵循以下准则：

1. **测试命名** - 使用描述性名称
2. **测试组织** - 使用 Google Test 的 TEST_F 宏
3. **断言** - 使用适当的 EXPECT_* 和 ASSERT_* 宏
4. **清理** - 在 SetUp/TearDown 中正确清理资源
5. **文档** - 为复杂测试添加注释

### 测试模板

```cpp
class MyNewTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 测试前初始化
    }
    
    void TearDown() override {
        // 测试后清理
    }
};

TEST_F(MyNewTest, DescriptiveTestName) {
    // 安排 (Arrange)
    // 设置测试数据
    
    // 行动 (Act)
    // 执行被测试的功能
    
    // 断言 (Assert)
    // 验证结果
    EXPECT_EQ(expected, actual);
}
```

## 许可证

这些测试文件遵循与主项目相同的许可证。