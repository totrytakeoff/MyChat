1. 服务 http请求 url 应该基于配置文件! 而不是硬编码在code当中! (gateway_server 中的 register_xxx_on_server)
2. 优化其中初始化逻辑职责(这写的什么玩意儿,一堆的宏if包着的各个模块的初始化逻辑全都往init_server中塞,也不拆分成多个函数封装)
