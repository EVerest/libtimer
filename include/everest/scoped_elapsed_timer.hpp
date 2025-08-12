#pragma once

#include <chrono>
#include <iostream>

namespace Everest {

struct ScopedElapsedTime {

	ScopedElapsedTime(std::string tag) : tag(tag)
	{
        start = std::chrono::high_resolution_clock::now();
	}

	~ScopedElapsedTime()
	{
		auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

		// TODO: add some logging capabilities here
        std::cout << "Elapsed time: " << duration.count() << "ms for operation: " << tag;
	}

private:
    std::chrono::system_clock::time_point start;	
	std::string tag;
};

}