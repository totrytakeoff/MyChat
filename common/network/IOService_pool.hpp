#pragma once
#ifndef IOSERVICE_POOL_HPP
#define IOSERVICE_POOL_HPP

/******************************************************************************
 *
 * @file       IOService_pool.hpp
 * @brief      IOService池类，用于管理多个IOService实例,提高IOService的效率
 *
 * @author     myself
 * @date       2025/07/22
 *
 *****************************************************************************/

#include <boost/asio.hpp>
#include <vector>
#include "../utils/singleton.hpp"

class IOServicePool : public Singleton<IOServicePool> {
    friend class Singleton<IOServicePool>;

public:
    using IOService = boost::asio::io_context;
    using IOServicePtr = std::shared_ptr<IOService>;
    using Work = boost::asio::executor_work_guard<IOService::executor_type>;
    using WorkPtr = std::unique_ptr<Work>;

    ~IOServicePool();

    IOServicePool(const IOServicePool&) = delete;
    IOServicePool& operator=(const IOServicePool&) = delete;

    boost::asio::io_context& GetIOService();

    void Stop();

private:
    IOServicePool(std::size_t pool_size = std::thread::hardware_concurrency());
    
    std::vector<IOServicePtr> io_services_;
    std::vector<WorkPtr> works_;
    std::vector<std::thread> threads_;
    std::size_t next_io_service_;
    std::size_t pool_size_;

};



#endif  // IOSERVICE_POOL_HPP