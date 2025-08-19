# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a C++ based instant messaging system called MyChat. The project implements a distributed IM system with a gateway service that handles WebSocket connections, message routing, and communication with backend microservices.

## Architecture

The system follows a microservices architecture:

1. **Gateway Service** - Handles WebSocket connections and message routing
2. **Backend Services** - User, Friend, Group, Message, Push services
3. **Common Components** - Shared network utilities, protobuf definitions, and utilities

### Key Components

- **Gateway Server** (`services/gateway/`) - Main entry point for client connections
- **WebSocket Server/Session** (`common/network/`) - Handles WebSocket communication
- **Protobuf Codec** (`common/network/protobuf_codec.hpp`) - Message encoding/decoding
- **Connection Manager** (`services/gateway/connection_manager.hpp`) - Manages client connections
- **Message Router** (`services/gateway/message_router.hpp`) - Routes messages to appropriate services

### Message Format

Messages use a custom binary protocol with Protobuf:
```
[header_size(varint)][type_name_size(varint)][type_name_string][header_data][message_data][CRC32]
```

The header is defined in `common/proto/base.proto` as `IMHeader`.

## Common Development Commands

### Building Tests
```bash
cd test/network
mkdir -p build && cd build
cmake ..
make
```

### Running Specific Tests
```bash
# Run protobuf codec tests
./protobufcodec_test

# Run WebSocket tests
./websocket_test

# Run Authentication tests
cd gateway/auth/build
make
./test_multi_platform_auth
```

### Generating Protobuf Files
```bash
# From the proto directory
protoc --cpp_out=. *.proto
```

## Code Structure

- `common/` - Shared components (network, proto, utils)
- `services/` - Microservices (gateway, user, friend, group, message, push)
- `test/` - Unit tests for various components
- `docs/` - Documentation files

## Key Classes

1. **ProtobufCodec** - Encodes/decodes messages with CRC32 validation
2. **WebSocketServer/WebSocketSession** - Handles WebSocket connections
3. **GatewayServer** - Main gateway service
4. **ConnectionManager** - Manages client connections
5. **MessageRouter** - Routes messages to backend services
6. **MultiPlatformAuthManager** - Handles multi-platform authentication with JWT-based dual token system
7. **PlatformTokenStrategy** - Manages platform-specific token configurations

## Documentation

- **Redis Structure** - See `docs/docs/redis.md` for detailed Redis data structure design

## Development Guidelines

- Follow existing code style and patterns
- Use the existing logging system (LogManager)
- Maintain thread safety in network components
- Use smart pointers for memory management
- Follow the existing error handling patterns