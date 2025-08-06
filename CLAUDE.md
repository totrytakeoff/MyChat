# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a C++ based instant messaging (IM) system with a modular architecture. The project uses:
- C++17 for core services
- Boost.Asio for network programming
- Protocol Buffers for message serialization
- spdlog for logging
- Google Test for unit testing
- CMake as the build system

## Architecture

The codebase follows a layered architecture:
1. **Network Layer**: TCP/WebSocket communication (in `common/network/`)
2. **Message Codec**: Protobuf encoding/decoding (in `common/network/protobuf_codec.*`)
3. **Core Services**: Business logic modules (in `services/`)
4. **Utilities**: Common utilities like logging, config management, thread pools (in `common/utils/`)
5. **Protocol Definitions**: Protobuf message definitions (in `common/proto/`)
6. **Tests**: Unit tests for various components (in `test/`)

Key components:
- `TCPSession`: Handles individual TCP connections with heartbeat and timeout mechanisms
- `ProtobufCodec`: Encodes/decodes messages with CRC32 validation and type checking
- `IMHeader`: Standard message header containing metadata (version, sequence, command ID, etc.)

## Build and Test Commands

### Building Tests

Navigate to a test directory and use CMake to build:

```bash
cd test/network
mkdir -p build && cd build
cmake ..
make
```

Or for utils tests:
```bash
cd test/utils
mkdir -p build && cd build
cmake ..
make
```

### Running Tests

After building, run the test executables directly:
```bash
# For network tests
cd test/network/build
./protobufcodec_test

# For utils tests (after building)
cd test/utils/build
./test_utils
```

### Key Test Commands

1. Build and run protobuf codec tests:
```bash
cd test/network && mkdir -p build && cd build && cmake .. && make && ./protobufcodec_test
```

2. Build utils tests:
```bash
cd test/utils && mkdir -p build && cd build && cmake .. && make
```

## Development Guidelines

1. **Message Format**: All network messages use the ProtobufCodec with format:
   `[header_size(varint)][type_name_size(varint)][type_name_string][header_data][message_data][CRC32]`

2. **Error Handling**: Use the standardized ErrorCode enum from `base.proto` for consistent error reporting

3. **Logging**: Use the LogManager class from `common/utils/log_manager.hpp` for consistent logging

4. **Testing**: Add unit tests for new functionality in the appropriate test directory

5. **Network Protocol**: TCP sessions use 4-byte length prefix for message framing to handle packet fragmentation

## Common Development Tasks

1. **Adding new message types**: 
   - Define in appropriate `.proto` file in `common/proto/`
   - Run protobuf compiler to generate code
   - Update `ProtobufCodec` tests if needed

2. **Creating new services**:
   - Add new directory under `services/`
   - Implement business logic
   - Add corresponding tests in `test/`

3. **Extending network functionality**:
   - Work in `common/network/` directory
   - Follow existing patterns for TCPSession/WebSocketSession
   - Add unit tests in `test/network/`