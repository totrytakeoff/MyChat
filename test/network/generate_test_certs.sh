#!/bin/bash

# 生成测试用的SSL证书和私钥
# 注意：这些证书仅用于测试，不应在生产环境中使用

echo "Generating test SSL certificates..."

# 生成私钥
openssl genrsa -out test_key.pem 2048

# 生成自签名证书
openssl req -new -x509 -key test_key.pem -out test_cert.pem -days 365 -subj "/C=CN/ST=Test/L=Test/O=TestOrg/CN=localhost"

echo "Test certificates generated:"
echo "  - test_key.pem (private key)"
echo "  - test_cert.pem (certificate)"
echo ""
echo "Note: These certificates are for testing only!"