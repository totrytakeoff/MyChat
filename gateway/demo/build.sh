#!/bin/bash

# MyChat Gateway Demo 构建脚本
set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 打印带颜色的信息
print_info() {
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

# 项目信息
PROJECT_NAME="MyChat Gateway Demo"
BUILD_DIR="build"
INSTALL_DIR="bin"

print_info "=========================================="
print_info "  $PROJECT_NAME - Build Script"
print_info "=========================================="

# 检查必要的依赖
print_info "Checking dependencies..."

# 检查CMake
if ! command -v cmake &> /dev/null; then
    print_error "CMake not found. Please install CMake 3.16 or later."
    exit 1
fi

# 检查编译器
if ! command -v g++ &> /dev/null && ! command -v clang++ &> /dev/null; then
    print_error "C++ compiler not found. Please install g++ or clang++."
    exit 1
fi

# 检查pkg-config
if ! command -v pkg-config &> /dev/null; then
    print_warning "pkg-config not found. Some dependencies may not be found."
fi

# 检查protobuf
if ! pkg-config --exists protobuf; then
    print_error "Protobuf not found. Please install libprotobuf-dev."
    print_error "Ubuntu/Debian: sudo apt-get install libprotobuf-dev protobuf-compiler"
    exit 1
fi

# 检查boost
if ! ldconfig -p | grep -q libboost_system; then
    print_error "Boost libraries not found. Please install libboost-all-dev."
    print_error "Ubuntu/Debian: sudo apt-get install libboost-all-dev"
    exit 1
fi

print_success "All required dependencies found!"

# 下载httplib如果不存在
HTTPLIB_DIR="third_party/httplib"
HTTPLIB_FILE="$HTTPLIB_DIR/httplib.h"

if [ ! -f "$HTTPLIB_FILE" ]; then
    print_info "Downloading httplib.h..."
    mkdir -p "$HTTPLIB_DIR"
    
    if command -v wget &> /dev/null; then
        wget -O "$HTTPLIB_FILE" https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h
    elif command -v curl &> /dev/null; then
        curl -o "$HTTPLIB_FILE" https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h
    else
        print_error "Neither wget nor curl found. Please download httplib.h manually:"
        print_error "  mkdir -p $HTTPLIB_DIR"
        print_error "  wget -O $HTTPLIB_FILE https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h"
        exit 1
    fi
    
    if [ -f "$HTTPLIB_FILE" ]; then
        print_success "Downloaded httplib.h successfully"
    else
        print_error "Failed to download httplib.h"
        exit 1
    fi
else
    print_info "httplib.h already exists"
fi

# 解析命令行参数
BUILD_TYPE="Release"
CLEAN_BUILD=false
RUN_AFTER_BUILD=false

while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        -c|--clean)
            CLEAN_BUILD=true
            shift
            ;;
        -r|--run)
            RUN_AFTER_BUILD=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  -d, --debug    Build in Debug mode (default: Release)"
            echo "  -c, --clean    Clean build directory before building"
            echo "  -r, --run      Run the gateway after successful build"
            echo "  -h, --help     Show this help message"
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            exit 1
            ;;
    esac
done

print_info "Build configuration:"
print_info "  Build Type: $BUILD_TYPE"
print_info "  Clean Build: $CLEAN_BUILD"
print_info "  Run After Build: $RUN_AFTER_BUILD"
print_info ""

# 清理构建目录
if [ "$CLEAN_BUILD" = true ] || [ ! -d "$BUILD_DIR" ]; then
    print_info "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR"
fi

# 进入构建目录
cd "$BUILD_DIR"

# 运行CMake配置
print_info "Configuring project with CMake..."
cmake .. -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
         -DCMAKE_INSTALL_PREFIX=../install

# 编译项目
print_info "Building project..."
make -j$(nproc)

print_success "Build completed successfully!"

# 返回项目根目录
cd ..

# 显示构建结果
print_info ""
print_info "Build results:"
print_info "  Executable: $BUILD_DIR/bin/gateway_server"
print_info "  Config: $BUILD_DIR/bin/config/gateway_config.json"
print_info "  Logs: $BUILD_DIR/bin/logs/"
print_info ""

# 测试可执行文件
if [ -x "$BUILD_DIR/bin/gateway_server" ]; then
    print_success "Gateway server binary is ready!"
    
    # 运行版本检查
    cd "$BUILD_DIR/bin"
    ./gateway_server --help > /dev/null 2>&1 || true
    cd ../..
    
else
    print_error "Gateway server binary not found or not executable"
    exit 1
fi

# 创建快捷启动脚本
print_info "Creating convenience scripts..."
cat > run_demo.sh << 'EOF'
#!/bin/bash
cd build/bin
exec ./gateway_server config/gateway_config.json
EOF
chmod +x run_demo.sh

print_success "Created run_demo.sh for easy testing"

# 显示使用说明
print_info ""
print_info "🎉 Build completed successfully!"
print_info ""
print_info "To run the gateway:"
print_info "  ./run_demo.sh"
print_info "  OR"
print_info "  cd build/bin && ./gateway_server config/gateway_config.json"
print_info ""
print_info "Quick test commands:"
print_info "  curl http://localhost:8080/health"
print_info "  curl -X POST http://localhost:8080/api/v1/login \\"
print_info "       -H 'Content-Type: application/json' \\"
print_info "       -d '{\"username\":\"test\",\"password\":\"test\"}'"
print_info ""

# 如果指定了运行选项，启动网关
if [ "$RUN_AFTER_BUILD" = true ]; then
    print_info "Starting gateway server..."
    exec ./run_demo.sh
fi

print_success "Build script completed!"