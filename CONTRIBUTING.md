# Contributing to vibrant-visuals-patcher

First off, thank you for considering contributing to `vibrant-visuals-patcher`! It's people like you that make open source such a great community.

## Where do I go from here?

If you've noticed a bug or have a feature request, make sure to check the [issue tracker](../../issues) to see if someone else has already created an issue for it. If not, go ahead and [make one](../../issues/new/choose)!

## Fork & create a branch

If this is something you think you can fix, then fork `vibrant-visuals-patcher` and create a branch with a descriptive name.

## Build Requirements

To build the project locally, you will need:
- Windows
- Visual Studio 2022 Build Tools with MSVC
- CMake 3.24 or newer

Follow the build steps in the [README.md](README.md):

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

## Code Style

- Please follow the existing C++ coding style in the repository. If you are using `clang-format`, ensure your changes align with the general project formatting conventions.
- Keep your changes as focused as possible.
- Ensure your code compiles without warnings before submitting a pull request.

## Submitting a Pull Request

1. Push your branch to your fork.
2. Open a Pull Request against the `main` branch.
3. Fill out the Pull Request template completely, providing details about what you changed and why.
4. Ensure any relevant issues are linked in the PR description (e.g., "Fixes #123").

Once submitted, a maintainer will review your code. We may suggest some changes or improvements or alternative approaches.

## Code of Conduct

Please note that this project is released with a [Code of Conduct](CODE_OF_CONDUCT.md). By participating in this project you agree to abide by its terms.
