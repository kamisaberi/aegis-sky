#include "aegis/platform/CudaAllocator.h"
#include <cuda_runtime.h>
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace aegis::platform {

    void* CudaAllocator::alloc_pinned(size_t size) {
        void* ptr = nullptr;
        // cudaHostAllocMapped: Maps memory into CUDA address space
        // cudaHostAllocWriteCombined: Faster CPU write, Slower CPU read (Good for Sensors)
        cudaError_t err = cudaHostAlloc(&ptr, size, cudaHostAllocMapped);

        if (err != cudaSuccess) {
            spdlog::error("[Platform] CUDA Pinned Alloc Failed: {}", cudaGetErrorString(err));
            throw std::runtime_error("OOM: Pinned Memory");
        }
        return ptr;
    }

    void CudaAllocator::free_pinned(void* ptr) {
        if (ptr) cudaFreeHost(ptr);
    }

    void* CudaAllocator::alloc_device(size_t size) {
        void* ptr = nullptr;
        cudaError_t err = cudaMalloc(&ptr, size);
        if (err != cudaSuccess) {
            spdlog::error("[Platform] CUDA VRAM Alloc Failed: {}", cudaGetErrorString(err));
            throw std::runtime_error("OOM: VRAM");
        }
        return ptr;
    }

    void CudaAllocator::free_device(void* ptr) {
        if (ptr) cudaFree(ptr);
    }
}