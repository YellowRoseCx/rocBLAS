/* ************************************************************************
 * Copyright (C) 2018-2024 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell cop-
 * ies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IM-
 * PLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNE-
 * CTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * ************************************************************************ */

#pragma once

#include "testing_common.hpp"

template <typename T>
void testing_nrm2_bad_arg(const Arguments& arg)
{
    auto rocblas_nrm2_fn = arg.api & c_API_FORTRAN ? rocblas_nrm2<T, true> : rocblas_nrm2<T, false>;
    auto rocblas_nrm2_fn_64
        = arg.api & c_API_FORTRAN ? rocblas_nrm2_64<T, true> : rocblas_nrm2_64<T, false>;

    int64_t             N         = 100;
    int64_t             incx      = 1;
    static const size_t safe_size = 100;

    rocblas_local_handle handle{arg};

    // Allocate device memory
    DEVICE_MEMCHECK(device_vector<T>, dx, (N, incx));
    DEVICE_MEMCHECK(device_vector<real_t<T>>, d_rocblas_result, (1));

    CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_device));

    DAPI_EXPECT(
        rocblas_status_invalid_handle, rocblas_nrm2_fn, (nullptr, N, dx, incx, d_rocblas_result));
    DAPI_EXPECT(rocblas_status_invalid_pointer,
                rocblas_nrm2_fn,
                (handle, N, nullptr, incx, d_rocblas_result));
    DAPI_EXPECT(rocblas_status_invalid_pointer, rocblas_nrm2_fn, (handle, N, dx, incx, nullptr));
}

template <typename T>
void testing_nrm2(const Arguments& arg)
{
    auto rocblas_nrm2_fn = arg.api & c_API_FORTRAN ? rocblas_nrm2<T, true> : rocblas_nrm2<T, false>;
    auto rocblas_nrm2_fn_64
        = arg.api & c_API_FORTRAN ? rocblas_nrm2_64<T, true> : rocblas_nrm2_64<T, false>;

    int64_t N    = arg.N;
    int64_t incx = arg.incx;

    double error_host_ptr;
    double error_device_ptr;

    rocblas_local_handle handle{arg};

    // check to prevent undefined memory allocation error
    if(N <= 0 || incx <= 0)
    {
        DEVICE_MEMCHECK(device_vector<real_t<T>>, d_rocblas_result_0, (1));
        HOST_MEMCHECK(host_vector<real_t<T>>, h_rocblas_result_0, (1));

        rocblas_init_nan(h_rocblas_result_0, 1, 1, 1);
        CHECK_HIP_ERROR(hipMemcpy(
            d_rocblas_result_0, h_rocblas_result_0, sizeof(real_t<T>), hipMemcpyHostToDevice));

        CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_device));
        DAPI_CHECK(rocblas_nrm2_fn, (handle, N, nullptr, incx, d_rocblas_result_0));

        CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_host));
        DAPI_CHECK(rocblas_nrm2_fn, (handle, N, nullptr, incx, h_rocblas_result_0));

        HOST_MEMCHECK(host_vector<real_t<T>>, cpu_0, (1));
        HOST_MEMCHECK(host_vector<real_t<T>>, gpu_0, (1));

        CHECK_HIP_ERROR(
            hipMemcpy(gpu_0, d_rocblas_result_0, sizeof(real_t<T>), hipMemcpyDeviceToHost));
        unit_check_general<real_t<T>>(1, 1, 1, cpu_0, gpu_0);
        unit_check_general<real_t<T>>(1, 1, 1, cpu_0, h_rocblas_result_0);
        return;
    }

    // Naming: `h` is in CPU (host) memory(eg hx), `d` is in GPU (device) memory (eg dx).
    // Allocate host memory
    HOST_MEMCHECK(host_vector<T>, hx, (N, incx));
    HOST_MEMCHECK(host_vector<real_t<T>>, rocblas_result, (1, 1));
    HOST_MEMCHECK(host_vector<real_t<T>>, cpu_result, (1, 1));

    // Allocate device memory
    DEVICE_MEMCHECK(device_vector<T>, dx, (N, incx));
    DEVICE_MEMCHECK(device_vector<real_t<T>>, d_rocblas_result, (1));

    // Initial Data on CPU
    rocblas_init_vector(hx, arg, rocblas_client_alpha_sets_nan, true, true);

    // copy data from CPU to device
    CHECK_HIP_ERROR(dx.transfer_from(hx));

    double cpu_time_used;

    if(arg.unit_check || arg.norm_check)
    {
        if(arg.pointer_mode_host)
        {
            CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_host));
            DAPI_CHECK(rocblas_nrm2_fn, (handle, N, dx, incx, rocblas_result));
        }

        if(arg.pointer_mode_device)
        {
            CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_device));
            handle.pre_test(arg);
            DAPI_CHECK(rocblas_nrm2_fn, (handle, N, dx, incx, d_rocblas_result));
            handle.post_test(arg);

            if(arg.repeatability_check)
            {
                HOST_MEMCHECK(host_vector<real_t<T>>, rocblas_result_copy, (1, 1));
                CHECK_HIP_ERROR(rocblas_result.transfer_from(d_rocblas_result));

                // multi-GPU support
                int device_id, device_count;
                CHECK_HIP_ERROR(hipGetDeviceCount(&device_count));
                for(int dev_id = 0; dev_id < device_count; dev_id++)
                {
                    CHECK_HIP_ERROR(hipGetDevice(&device_id));
                    if(device_id != dev_id)
                        CHECK_HIP_ERROR(hipSetDevice(dev_id));

                    //New rocblas handle for new device
                    rocblas_local_handle handle_copy{arg};

                    // Allocate device memory in new device
                    DEVICE_MEMCHECK(device_vector<T>, dx_copy, (N, incx));
                    DEVICE_MEMCHECK(device_vector<real_t<T>>, d_rocblas_result_copy, (1));

                    // copy data from CPU to device
                    CHECK_HIP_ERROR(dx_copy.transfer_from(hx));

                    CHECK_ROCBLAS_ERROR(
                        rocblas_set_pointer_mode(handle_copy, rocblas_pointer_mode_device));

                    for(int runs = 0; runs < arg.iters; runs++)
                    {
                        DAPI_CHECK(rocblas_nrm2_fn,
                                   (handle_copy, N, dx_copy, incx, d_rocblas_result_copy));
                        CHECK_HIP_ERROR(rocblas_result_copy.transfer_from(d_rocblas_result_copy));
                        unit_check_general<real_t<T>, real_t<T>>(
                            1, 1, 1, rocblas_result, rocblas_result_copy);
                    }
                }
                return;
            }
        }

        // CPU BLAS
        cpu_time_used = get_time_us_no_sync();
        ref_nrm2<T>(N, hx, incx, cpu_result);
        cpu_time_used = get_time_us_no_sync() - cpu_time_used;

        auto compare_to_gold = [&] {
            if(!rocblas_isnan(arg.alpha))
            {
                if(arg.unit_check)
                {
                    double abs_error = sum_near_tolerance<T>(N, cpu_result[0]);
                    near_check_general<real_t<T>, real_t<T>>(
                        1, 1, 1, cpu_result, rocblas_result, abs_error);
                }
            }

            double error = 0.0;
            if(arg.norm_check)
            {
                error = rocblas_abs((cpu_result[0] - rocblas_result[0]) / cpu_result[0]);
            }
            return error;
        };

        if(arg.pointer_mode_host)
        {
            error_host_ptr = compare_to_gold();
        }

        if(arg.pointer_mode_device)
        {
            CHECK_HIP_ERROR(rocblas_result.transfer_from(d_rocblas_result));
            error_device_ptr = compare_to_gold();
        }
    }

    if(arg.timing)
    {
        double gpu_time_used;
        int    number_cold_calls = arg.cold_iters;
        int    total_calls       = number_cold_calls + arg.iters;

        CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_host));
        hipStream_t stream;
        CHECK_ROCBLAS_ERROR(rocblas_get_stream(handle, &stream));

        for(int iter = 0; iter < total_calls; iter++)
        {
            if(iter == number_cold_calls)
                gpu_time_used = get_time_us_sync(stream); // in microseconds

            DAPI_DISPATCH(rocblas_nrm2_fn, (handle, N, dx, incx, d_rocblas_result));
        }

        gpu_time_used = get_time_us_sync(stream) - gpu_time_used;

        ArgumentModel<e_N, e_incx>{}.log_args<T>(rocblas_cout,
                                                 arg,
                                                 gpu_time_used,
                                                 nrm2_gflop_count<T>(N),
                                                 nrm2_gbyte_count<T>(N),
                                                 cpu_time_used,
                                                 error_host_ptr,
                                                 error_device_ptr);
    }
}
