#include "IOService_pool.hpp"
#include <iostream>


IOServicePool::IOServicePool(std::size_t pool_size = std::thread::hardware_concurrency()): pool_size_(pool_size),works_(pool_size), next_io_service_(0), io_services_(pool_size) {
    for (std::size_t i = 0; i < pool_size; ++i) {
        io_services_.emplace_back(std::make_shared<IOService>());
        works_[i] = std::make_unique<Work>(boost::asio::make_work_guard(*io_services_[i]));
    }

    // 启动线程池
    for (std::size_t i = 0; i < pool_size; ++i) {
        threads_.emplace_back([this, i]() {
            io_services_[i]->run();
        });
    }



}


IOServicePool::~IOServicePool() {
    Stop();
    std::cout<< "IOServicePool destroyed." << std::endl;
}

/**
 * @brief      从IOService池中获取一个IOService实例
 *
 * @param[in]  参数名  参数描述
 * @return     返回一个IOService实例的引用
 */

boost::asio::io_context& IOServicePool::GetIOService() {
    auto & service = io_services_[(next_io_service_++) % pool_size_];
    return *service;
}

void IOServicePool::Stop() {
    //因为仅仅执行work.reset并不能让iocontext从run的状态中退出
	//当iocontext已经绑定了读或写的监听事件后，还需要手动stop该服务。
    
    for(auto& service: io_services_){
        //把服务先停止
        service->stop();
    } 
        
        
    for(auto& work: works_){
        work.reset();
    }

    for(auto& thread: threads_){
        if(thread.joinable()){
            thread.join();
        }
    }
    io_services_.clear();
    works_.clear();
    io_services_.clear();
        

}
