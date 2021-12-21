/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2020 - 2021 Pionix GmbH and Contributors to EVerest
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <chrono>
#include <iostream>
#include <thread>

#include <boost/exception/diagnostic_information.hpp>
#include <boost/program_options.hpp>
#include <date/date.h>

#include <everest/timer.hpp>

namespace po = boost::program_options;

using date::operator<<;
using namespace std::chrono_literals;

int main(int argc, char* argv[]) {
    po::options_description desc("EVerest::time example");
    desc.add_options()("help,h", "produce help message");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help") != 0) {
        std::cout << desc << "\n";
        return 1;
    }

    boost::asio::io_service io_service;

    std::cout << "start time: " << std::chrono::system_clock::now() << std::endl;

    int count_t1 = 0;
    int count_t2 = 0;
    Everest::SteadyTimer* t0 = new Everest::SteadyTimer(&io_service);
    t0->timeout([]() { std::cout << "Goodbye after 25s" << std::endl; }, 25s);
    Everest::SteadyTimer* t1 = new Everest::SteadyTimer(&io_service);
    t1->at(
        [&]() {
            std::cout << "t1 (asio) after 5s: " << std::chrono::system_clock::now() << std::endl;
            t1->interval(
                [&]() {
                    std::cout << "t1 (asio) interval (1s): " << std::chrono::system_clock::now() << std::endl;
                    count_t1++;
                    if (count_t1 > 3) {
                        t1->timeout(
                            []() {
                                std::cout << "t1 (asio) timeout (3s): " << std::chrono::system_clock::now()
                                          << std::endl;
                            },
                            3s);
                    }
                },
                1s);
        },
        std::chrono::steady_clock::now() + 5s);

    Everest::SteadyTimer* t2 = new Everest::SteadyTimer();
    t2->at(
        [&]() {
            std::cout << "t2 (thread) after 12s: " << std::chrono::system_clock::now() << std::endl;
            t2->interval(
                [&]() {
                    std::cout << "t2 (thread) interval (1s): " << std::chrono::system_clock::now() << std::endl;
                    count_t2++;
                    if (count_t2 > 3) {
                        t2->timeout(
                            []() {
                                std::cout << "t2 (thread) timeout (3s): " << std::chrono::system_clock::now()
                                          << std::endl;
                            },
                            3s);
                    }
                },
                1s);
        },
        std::chrono::steady_clock::now() + 12s);

    io_service.run();

    return 0;
}
