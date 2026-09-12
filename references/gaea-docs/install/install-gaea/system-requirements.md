Source: https://docs.gaea.app/install/install-gaea/system-requirements.html

# System Requirements

You will require [.NET 8.0](https://download.visualstudio.microsoft.com/download/pr/7f4d5cbc-4449-4ea5-9578-c467821f251f/b9b19f89d0642bf78f4b612c6a741637/dotnet-runtime-8.0.0-win-x64.exe) and [Visual C++ Runtime](https://aka.ms/vs/17/release/vc_redist.x64.exe).

The Gaea installer will download and install those if needed. An internet connection will be required to do that.

Depending on your hardware, you may also need [Intel® CPU Runtime for OpenCL Applications with SYCL Support](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-cpu-runtime-for-opencl-applications-with-sycl-support.html)

If you are going to install in an air-gapped/offline environment, then you may want to install the prerequisites beforehand.

## Hardware Requirements

Before running Gaea, make sure your computer meets the minimum system requirements and that you have the latest CPU and GPU drivers.

| Minimum | Recommended |
| --- | --- |
| Windows 10 (April 2018 - Build 1803) | Windows 11 - 23H2 |
| Intel Core i5-4200U / AMD FX-8370 | Intel Core i7-4770K / AMD Ryzen-5 1500X |
| 16GB RAM | 32GB RAM |
| NVIDIA GeForce GTX 1080  AMD Radeon RX Vega 64 | NVIDIA GeForce RTX 3070 Ti  AMD Radeon RX 6800 XT |
| 10GB HDD space | 50GB HDD space |

For CPU compute: The CPU must support AVX2 instruction set.

For GPU compute: NVIDIA cards need CUDA 12.5; AMD cards need HIP 6.0.

Hardware and Driver requirements often change by different manufacturers. We highly recommend using the Community Edition to ensure your hardware meets the needs for running Gaea.

RAM may depend on project requirements. See [Calculating Memory Requirements](https://docs.gaea.app/using/advanced-topics/technical-information/calculating-memory-requirements.html).
