#!/bin/bash

# 快速测试脚本 - 用于日常开发测试

set -e

echo "🚀 RouterManager 快速测试"
echo "=========================="

# 检查是否在正确目录
if [ ! -f "test_router_manager.cpp" ]; then
    echo "❌ 请在 test/router 目录下运行此脚本"
    exit 1
fi

# 创建或进入构建目录
mkdir -p build
cd build

# 快速构建（假设依赖已安装）
echo "🔨 构建中..."
cmake .. -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Debug > /dev/null 2>&1
make -j$(nproc) > /dev/null 2>&1

if [ $? -ne 0 ]; then
    echo "❌ 构建失败，运行完整构建脚本："
    echo "   ./build_and_test.sh -i"
    exit 1
fi

echo "✅ 构建成功"

# 运行测试
echo "🧪 运行测试..."
echo ""

./test_router_manager --gtest_color=yes

echo ""
echo "✅ 测试完成！"