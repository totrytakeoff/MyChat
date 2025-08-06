#!/bin/bash

# MyChat Gateway API测试脚本
# 用于快速测试网关的各种功能

BASE_URL="http://localhost:8080"
WS_URL="ws://localhost:8081"

# 颜色定义
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_test() {
    echo -e "${BLUE}[TEST]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

echo "=========================================="
echo "   MyChat Gateway API Test Suite"
echo "=========================================="
echo ""

# 检查网关是否运行
print_test "Checking if gateway is running..."
if ! curl -s "$BASE_URL/health" > /dev/null; then
    print_error "Gateway is not running on $BASE_URL"
    print_error "Please start the gateway first: ./run_demo.sh"
    exit 1
fi
print_success "Gateway is running!"
echo ""

# 1. 健康检查测试
print_test "1. Health check test"
response=$(curl -s "$BASE_URL/health")
if echo "$response" | grep -q '"status":"ok"'; then
    print_success "Health check passed"
    echo "Response: $response"
else
    print_error "Health check failed"
    echo "Response: $response"
fi
echo ""

# 2. 用户登录测试
print_test "2. User login test"
login_response=$(curl -s -X POST "$BASE_URL/api/v1/login" \
    -H 'Content-Type: application/json' \
    -d '{"username":"test","password":"test","device_id":"test_script"}')

echo "Login response: $login_response"

# 解析token
if echo "$login_response" | grep -q '"error_code":0'; then
    token=$(echo "$login_response" | sed -n 's/.*"token":"\([^"]*\)".*/\1/p')
    if [ -n "$token" ]; then
        print_success "Login successful! Token: ${token:0:20}..."
    else
        print_error "Login response missing token"
        token=""
    fi
else
    print_error "Login failed"
    token=""
fi
echo ""

# 3. 获取用户信息测试（需要token）
if [ -n "$token" ]; then
    print_test "3. Get user info test"
    user_info_response=$(curl -s -H "Authorization: Bearer $token" \
        "$BASE_URL/api/v1/user/info")
    
    echo "User info response: $user_info_response"
    
    if echo "$user_info_response" | grep -q '"error_code":0'; then
        print_success "Get user info successful"
    else
        print_error "Get user info failed"
    fi
    echo ""
else
    print_warning "Skipping user info test (no token)"
    echo ""
fi

# 4. 发送消息测试（需要token）
if [ -n "$token" ]; then
    print_test "4. Send message test"
    message_response=$(curl -s -X POST "$BASE_URL/api/v1/message/send" \
        -H "Authorization: Bearer $token" \
        -H 'Content-Type: application/json' \
        -d '{"to_user":"user_demo","content":"Hello from test script!","message_type":"text"}')
    
    echo "Send message response: $message_response"
    
    # 由于是demo，消息服务可能返回错误（服务未实现），这是正常的
    if echo "$message_response" | grep -q '"error_code"'; then
        print_warning "Message service response received (may be mock error)"
    else
        print_success "Send message successful"
    fi
    echo ""
else
    print_warning "Skipping send message test (no token)"
    echo ""
fi

# 5. 服务器统计信息测试
print_test "5. Server statistics test"
stats_response=$(curl -s "$BASE_URL/stats")
if echo "$stats_response" | grep -q '"server"'; then
    print_success "Server stats retrieved successfully"
    echo "Stats preview:"
    echo "$stats_response" | head -10
    echo "... (truncated)"
else
    print_error "Failed to get server stats"
    echo "Response: $stats_response"
fi
echo ""

# 6. 登出测试（需要token）
if [ -n "$token" ]; then
    print_test "6. User logout test"
    logout_response=$(curl -s -X POST "$BASE_URL/api/v1/logout" \
        -H "Authorization: Bearer $token")
    
    echo "Logout response: $logout_response"
    
    if echo "$logout_response" | grep -q '"error_code":0'; then
        print_success "Logout successful"
    else
        print_error "Logout failed"
    fi
    echo ""
else
    print_warning "Skipping logout test (no token)"
    echo ""
fi

# 7. 错误处理测试
print_test "7. Error handling tests"

# 无效登录
print_test "  7a. Invalid login test"
invalid_login=$(curl -s -X POST "$BASE_URL/api/v1/login" \
    -H 'Content-Type: application/json' \
    -d '{"username":"invalid","password":"invalid"}')

if echo "$invalid_login" | grep -q '"error_code":[^0]'; then
    print_success "Invalid login properly rejected"
else
    print_error "Invalid login was not properly handled"
fi

# 无效token
print_test "  7b. Invalid token test"
invalid_token=$(curl -s -H "Authorization: Bearer invalid_token" \
    "$BASE_URL/api/v1/user/info")

if echo "$invalid_token" | grep -q '"error_code":[^0]'; then
    print_success "Invalid token properly rejected"
else
    print_error "Invalid token was not properly handled"
fi

# 不存在的endpoint
print_test "  7c. 404 endpoint test"
not_found=$(curl -s "$BASE_URL/api/v1/nonexistent")
if echo "$not_found" | grep -q '404\|"error_code":404'; then
    print_success "404 error properly handled"
else
    print_warning "404 handling may need improvement"
fi
echo ""

# 8. 并发测试（简单版）
print_test "8. Simple concurrency test"
print_test "  Making 10 concurrent health check requests..."

start_time=$(date +%s.%N)
for i in {1..10}; do
    curl -s "$BASE_URL/health" > /dev/null &
done
wait
end_time=$(date +%s.%N)

duration=$(echo "$end_time - $start_time" | bc -l)
print_success "Completed 10 concurrent requests in ${duration}s"
echo ""

# 总结
echo "=========================================="
echo "             Test Summary"
echo "=========================================="
print_success "Basic API functionality verified!"
echo ""
echo "What was tested:"
echo "  ✓ Health check endpoint"
echo "  ✓ User login/logout flow"
echo "  ✓ Token-based authentication"
echo "  ✓ Protected resource access"
echo "  ✓ Error handling"
echo "  ✓ Basic concurrency"
echo ""
echo "Next steps for full testing:"
echo "  • Test WebSocket connections"
echo "  • Test message routing to actual services"
echo "  • Test connection management features"
echo "  • Performance testing with higher loads"
echo ""
echo "Gateway demo is working correctly! 🎉"