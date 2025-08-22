# RouterManager 测试

这个目录包含了 RouterManager 的完整单元测试，使用 Google Test 框架编写。

## 📋 测试覆盖范围

### HttpRouter 测试
- ✅ 有效路由解析
- ✅ 无效路径处理
- ✅ API前缀验证
- ✅ 多路由测试
- ✅ 边界条件处理

### ServiceRouter 测试
- ✅ 有效服务查找
- ✅ 无效服务处理
- ✅ 空服务名处理
- ✅ 多服务测试
- ✅ 服务配置验证

### RouterManager 集成测试
- ✅ 完整路由流程
- ✅ HTTP路由失败处理
- ✅ 服务路由失败处理
- ✅ 配置重新加载
- ✅ 统计信息获取

### 错误处理测试
- ✅ 不存在的配置文件
- ✅ 格式错误的配置文件
- ✅ 无效配置内容

### 性能测试
- ✅ 多次路由查找性能测试
- ✅ 边界条件测试

## 🛠️ 环境要求

### 必需的依赖
- **CMake** >= 3.16
- **vcpkg** 包管理器
- **C++20** 编译器 (GCC 10+ 或 Clang 12+)

### vcpkg 包依赖
- `gtest` - Google Test 框架
- `nlohmann-json` - JSON 处理库
- `spdlog` - 日志库
- `httplib` - HTTP 库

## 🚀 快速开始

### 1. 安装 vcpkg (如果还没有)
```bash
# 克隆 vcpkg
git clone https://github.com/Microsoft/vcpkg.git $HOME/vcpkg
cd $HOME/vcpkg

# 引导 vcpkg
./bootstrap-vcpkg.sh

# 安装必需的包
./vcpkg install gtest nlohmann-json spdlog httplib
```

### 2. 构建和运行测试

#### 使用自动化脚本 (推荐)
```bash
# 进入测试目录
cd test/router

# 安装依赖、构建并运行所有测试
./build_and_test.sh

# 或者分步骤执行：
./build_and_test.sh -i          # 只安装依赖
./build_and_test.sh -b          # 只构建
./build_and_test.sh -t          # 只运行测试
```

#### 手动构建
```bash
# 进入测试目录
cd test/router

# 创建构建目录
mkdir build && cd build

# 配置 CMake
cmake .. -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake

# 构建
make -j$(nproc)

# 运行测试
./test_router_manager
```

## 📊 测试选项

### 运行特定测试
```bash
# 只运行 HttpRouter 相关测试
./build_and_test.sh -f "*HttpRouter*"

# 只运行 ServiceRouter 相关测试
./build_and_test.sh -f "*ServiceRouter*"

# 只运行完整路由测试
./build_and_test.sh -f "*CompleteRoute*"

# 运行性能测试
./build_and_test.sh -f "*Performance*"
```

### 详细输出
```bash
# 启用详细输出
./build_and_test.sh -v

# 直接运行测试程序获得更多控制
cd build
./test_router_manager --gtest_color=yes --gtest_output=xml:results.xml
```

### 清理构建
```bash
# 清理构建目录
./build_and_test.sh -c
```

## 📁 文件结构

```
test/router/
├── test_router_manager.cpp    # 主测试文件
├── CMakeLists.txt             # CMake 构建配置
├── build_and_test.sh          # 自动化构建脚本
├── README.md                  # 本文档
└── build/                     # 构建目录 (自动生成)
    ├── test_router_manager    # 测试可执行文件
    └── test_results.xml       # 测试结果 XML
```

## 🧪 测试用例详解

### 基础功能测试
- **HttpRouter_ValidRoutes_ShouldParseSuccessfully**: 测试有效路由解析
- **ServiceRouter_ValidService_ShouldFindSuccessfully**: 测试有效服务查找
- **CompleteRoute_ValidRequest_ShouldRouteSuccessfully**: 测试完整路由流程

### 错误处理测试
- **HttpRouter_InvalidPath_ShouldFail**: 测试无效路径处理
- **ServiceRouter_InvalidService_ShouldFail**: 测试无效服务处理
- **CompleteRoute_ServiceNotFound_ShouldFail**: 测试服务不存在的情况

### 配置管理测试
- **ConfigReload_ValidConfig_ShouldSucceed**: 测试配置重新加载
- **GetStats_ShouldReturnCorrectInfo**: 测试统计信息获取

### 性能测试
- **Performance_MultipleRouteLookups_ShouldBeEfficient**: 测试路由查找性能

### 边界条件测试
- **EdgeCases_EmptyPath_ShouldHandleGracefully**: 测试空路径处理
- **EdgeCases_VeryLongPath_ShouldHandleGracefully**: 测试超长路径处理

## 🔧 故障排除

### 常见问题

1. **vcpkg 包未找到**
   ```
   解决方案：确保 vcpkg 正确安装并且包已安装
   ./vcpkg install gtest nlohmann-json spdlog httplib
   ```

2. **编译错误**
   ```
   解决方案：检查 C++ 标准和编译器版本
   确保使用 C++20 和 GCC 10+ 或 Clang 12+
   ```

3. **链接错误**
   ```
   解决方案：确保所有依赖的源文件都在 CMakeLists.txt 中
   检查 common/utils/ 下的源文件是否存在
   ```

4. **测试失败**
   ```
   解决方案：检查日志输出，确保配置文件格式正确
   运行 ./test_router_manager --gtest_color=yes 获得详细输出
   ```

### 调试技巧

1. **启用详细日志**
   ```cpp
   // 在测试中添加
   spdlog::set_level(spdlog::level::debug);
   ```

2. **单独运行失败的测试**
   ```bash
   ./test_router_manager --gtest_filter="FailedTestName"
   ```

3. **使用 GDB 调试**
   ```bash
   gdb ./test_router_manager
   (gdb) run --gtest_filter="FailedTestName"
   ```

## 📈 测试报告

测试运行后会生成以下输出：

1. **控制台输出**: 实时测试结果和详细信息
2. **XML 报告**: `test_results.xml` 文件，可用于 CI/CD 集成
3. **性能数据**: 路由查找平均耗时等性能指标

## 🤝 贡献指南

添加新测试时请遵循以下规范：

1. **命名规范**: `[Component]_[Scenario]_[ExpectedResult]`
2. **测试结构**: 使用 Arrange-Act-Assert 模式
3. **错误消息**: 提供清晰的失败消息
4. **性能测试**: 为性能敏感的功能添加性能测试

## 📚 参考资料

- [Google Test 文档](https://google.github.io/googletest/)
- [vcpkg 文档](https://vcpkg.io/)
- [CMake 文档](https://cmake.org/documentation/)
- [C++20 标准](https://en.cppreference.com/w/cpp/20)