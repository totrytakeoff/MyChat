#!/bin/bash

# RouterManager测试构建和运行脚本

set -e  # 遇到错误立即退出

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 函数：打印彩色消息
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 检查vcpkg环境
check_vcpkg() {
    if [ -z "$HOME/vcpkg" ] || [ ! -d "$HOME/vcpkg" ]; then
        print_error "vcpkg not found at $HOME/vcpkg"
        print_error "Please install vcpkg first:"
        print_error "  git clone https://github.com/Microsoft/vcpkg.git $HOME/vcpkg"
        print_error "  cd $HOME/vcpkg && ./bootstrap-vcpkg.sh"
        exit 1
    fi
    
    print_status "vcpkg found at $HOME/vcpkg"
}

# 安装必需的包
install_dependencies() {
    print_status "Installing dependencies via vcpkg..."
    
    cd $HOME/vcpkg
    
    # 安装必需的包
    packages=("gtest" "nlohmann-json" "spdlog" "httplib")
    
    for package in "${packages[@]}"; do
        print_status "Installing $package..."
        ./vcpkg install $package || {
            print_warning "Failed to install $package, it might already be installed"
        }
    done
    
    print_success "Dependencies installation completed"
}

# 构建测试
build_tests() {
    print_status "Building RouterManager tests..."
    
    # 返回到测试目录
    cd "$(dirname "$0")"
    
    # 创建构建目录
    if [ -d "build" ]; then
        print_status "Cleaning existing build directory..."
        rm -rf build
    fi
    
    mkdir build
    cd build
    
    # 配置CMake
    print_status "Configuring CMake with vcpkg toolchain..."
    cmake .. -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake \
             -DCMAKE_BUILD_TYPE=Debug \
             -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    
    # 构建
    print_status "Building..."
    make -j$(nproc) || {
        print_error "Build failed!"
        exit 1
    }
    
    print_success "Build completed successfully"
}

# 运行测试
run_tests() {
    print_status "Running RouterManager tests..."
    
    # 确保在build目录中
    cd "$(dirname "$0")/build"
    
    # 运行测试
    print_status "Executing test_router_manager..."
    ./test_router_manager --gtest_output=xml:test_results.xml --gtest_color=yes || {
        print_error "Some tests failed!"
        return 1
    }
    
    print_success "All tests passed!"
    
    # 显示测试结果摘要
    if [ -f "test_results.xml" ]; then
        print_status "Test results saved to test_results.xml"
    fi
}

# 运行特定测试
run_specific_test() {
    local test_filter="$1"
    print_status "Running specific test: $test_filter"
    
    cd "$(dirname "$0")/build"
    ./test_router_manager --gtest_filter="$test_filter" --gtest_color=yes
}

# 显示帮助
show_help() {
    echo "RouterManager Test Build Script"
    echo ""
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -h, --help              Show this help message"
    echo "  -i, --install-deps      Install dependencies via vcpkg"
    echo "  -b, --build-only        Only build, don't run tests"
    echo "  -t, --test-only         Only run tests (assume already built)"
    echo "  -c, --clean             Clean build directory"
    echo "  -f, --filter PATTERN    Run only tests matching PATTERN"
    echo "  -v, --verbose           Verbose output"
    echo ""
    echo "Examples:"
    echo "  $0                      # Build and run all tests"
    echo "  $0 -i                   # Install dependencies first"
    echo "  $0 -f '*HttpRouter*'    # Run only HttpRouter tests"
    echo "  $0 -c                   # Clean build directory"
}

# 清理构建目录
clean_build() {
    print_status "Cleaning build directory..."
    cd "$(dirname "$0")"
    if [ -d "build" ]; then
        rm -rf build
        print_success "Build directory cleaned"
    else
        print_status "Build directory doesn't exist"
    fi
}

# 主函数
main() {
    local install_deps=false
    local build_only=false
    local test_only=false
    local clean_only=false
    local test_filter=""
    local verbose=false
    
    # 解析命令行参数
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                show_help
                exit 0
                ;;
            -i|--install-deps)
                install_deps=true
                shift
                ;;
            -b|--build-only)
                build_only=true
                shift
                ;;
            -t|--test-only)
                test_only=true
                shift
                ;;
            -c|--clean)
                clean_only=true
                shift
                ;;
            -f|--filter)
                test_filter="$2"
                shift 2
                ;;
            -v|--verbose)
                verbose=true
                shift
                ;;
            *)
                print_error "Unknown option: $1"
                show_help
                exit 1
                ;;
        esac
    done
    
    # 设置详细输出
    if [ "$verbose" = true ]; then
        set -x
    fi
    
    print_status "Starting RouterManager test script..."
    
    # 只清理
    if [ "$clean_only" = true ]; then
        clean_build
        exit 0
    fi
    
    # 检查vcpkg
    check_vcpkg
    
    # 安装依赖
    if [ "$install_deps" = true ]; then
        install_dependencies
    fi
    
    # 只运行测试
    if [ "$test_only" = true ]; then
        if [ -n "$test_filter" ]; then
            run_specific_test "$test_filter"
        else
            run_tests
        fi
        exit 0
    fi
    
    # 构建
    if [ "$build_only" = false ]; then
        build_tests
    else
        build_tests
        exit 0
    fi
    
    # 运行测试
    if [ -n "$test_filter" ]; then
        run_specific_test "$test_filter"
    else
        run_tests
    fi
    
    print_success "RouterManager test script completed successfully!"
}

# 运行主函数
main "$@"