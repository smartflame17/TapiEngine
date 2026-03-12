# TapiEngine <img width="48" height="48" alt="tapiengine_logo" src="https://github.com/user-attachments/assets/a454d340-ef25-4178-b578-b205978240da" />

TapiEngine is a basic 3D engine written in C++ with minimal external dependencies. It leverages the WinAPI and Direct3D 11 to provide a lightweight yet functional environment for 3D graphics development on Windows.

## Features

- Minimal external dependencies for ease of setup and portability
- Core rendering capabilities using Direct3D 11
- Windows-native application loop and input handling via WinAPI
- Object-Component system for flexible scene management
- Designed for learning, prototyping, and extending

## Getting Started

### Prerequisites

- Windows OS
- Visual Studio 2022 (recommended) or any C++ compiler with Direct3D 11 SDK support

### Building

1. Clone the repository:
   ```sh
   git clone https://github.com/smartflame17/TapiEngine.git
   ```
2. Open the project in Visual Studio.
3. Build the solution.

### Running

After building, run the executable from your build output directory. You should see a window initialized by WinAPI and rendered via Direct3D 11.


## Acknowledgements

- Microsoft for WinAPI and Direct3D 11
- The [imgui](https://github.com/ocornut/imgui) library for providing a simple and effective GUI solution
- The [assimp](https://github.com/assimp/assimp) library for model loading capabilities
- The open-source community for inspiration and guidance

---
Made with ❤️ by [smartflame17](https://github.com/smartflame17)
