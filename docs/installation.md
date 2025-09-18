# Installation

MQT QMAP is mainly developed as a C++17 library with Python bindings.
The resulting Python package is available on [PyPI](https://pypi.org/project/mqt.qmap/) and can be installed on all major operating systems using all modern Python versions.

:::::{tip}
We highly recommend using [`uv`](https://docs.astral.sh/uv/) for working with Python projects.
It is an extremely fast Python package and project manager, written in Rust and developed by [Astral](https://astral.sh/) (the same team behind [`ruff`](https://docs.astral.sh/ruff/)).
It can act as a drop-in replacement for `pip` and `virtualenv`, and provides a more modern and faster alternative to the traditional Python package management tools.
It automatically handles the creation of virtual environments and the installation of packages, and is much faster than `pip`.
Additionally, it can even set up Python for you if it is not installed yet.

If you do not have `uv` installed yet, you can install it via:

::::{tab-set}
:::{tab-item} macOS and Linux

```console
$ curl -LsSf https://astral.sh/uv/install.sh | sh
```

:::
:::{tab-item} Windows

```console
$ powershell -ExecutionPolicy ByPass -c "irm https://astral.sh/uv/install.ps1 | iex"
```

::::

Check out their excellent [documentation](https://docs.astral.sh/uv/) for more information.

:::::

::::{tab-set}
:sync-group: installer

:::{tab-item} uv _(recommended)_
:sync: uv

```console
$ uv pip install mqt.qmap
```

:::

:::{tab-item} pip
:sync: pip

```console
(.venv) $ python -m pip install mqt.qmap
```

:::
::::

In most practical cases (under 64-bit Linux, MacOS incl. Apple Silicon, and Windows), this requires no compilation and merely downloads and installs a platform-specific pre-built wheel.

Once installed, you can check if the installation was successful by running:

```console
(.venv) $ python -c "import mqt.qmap; print(mqt.qmap.__version__)"
```

which should print the installed version of the library.

:::{attention}
As of version 2.7.0, support for Python 3.8 has been officially dropped.
We strongly recommend that users upgrade to a more recent version of Python to ensure compatibility and continue receiving updates and support.
Thank you for your understanding.
:::

## Building from source for performance

In order to get the best performance and enable platform-specific optimizations that cannot be enabled on portable wheels, it is recommended to build the library from source via:

::::{tab-set}
:sync-group: installer

:::{tab-item} uv _(recommended)_
:sync: uv

```console
$ uv pip install mqt.qmap --no-binary mqt.qmap --no-binary mqt.core
```

:::

:::{tab-item} pip
:sync: pip

```console
(.venv) $ pip install mqt.qmap --no-binary mqt.qmap --no-binary mqt.core
```

:::
::::

This requires a [C++ compiler supporting C++17](https://en.wikipedia.org/wiki/List_of_compilers#C++_compilers) and a minimum [CMake](https://cmake.org/) version of 3.19.
Additionally, the [SMT solver Z3](https://github.com/Z3Prover/z3) has to be installed and the dynamic linker has to be able to find the library.
This can be accomplished in a multitude of ways:

- Under Ubuntu 20.04 and newer: {code}`sudo apt-get install libz3-dev`
- Under macOS: {code}`brew install z3`
- Alternatively: {code}`pip install z3-solver` in the virtual environment
- Download pre-built binaries from https://github.com/Z3Prover/z3/releases and copy the files to the respective system directories
- Build Z3 from source and install it to the system

The library is continuously tested under Linux, MacOS, and Windows using the [latest available system versions for GitHub Actions](https://github.com/actions/virtual-environments).
In order to access the latest build logs, visit the [GitHub Actions page](https://github.com/cda-tum/mqt-qmap/actions/workflows/ci.yml).

## Integrating MQT QMAP into your project

If you want to use the MQT QMAP Python package in your own project, you can simply add it as a dependency in your `pyproject.toml` or `setup.py` file.
This will automatically install the MQT QMAP package when your project is installed.

::::{tab-set}

:::{tab-item} uv _(recommended)_

```console
$ uv add mqt.qmap
```

:::

:::{tab-item} pyproject.toml

```toml
[project]
# ...
dependencies = ["mqt.qmap>=3.0.0"]
# ...
```

:::

:::{tab-item} setup.py

```python
from setuptools import setup

setup(
    # ...
    install_requires=["mqt.qmap>=3.0.0"],
    # ...
)
```

:::
::::

If you want to integrate the C++ library directly into your project, you can either

- add it as a git submodule and build it as part of your project, or
- use CMake's [FetchContent](https://cmake.org/cmake/help/latest/module/FetchContent.html) module.

::::{tab-set}
:::{tab-item} FetchContent

This is the recommended approach for projects because it allows to detect installed versions of MQT QMAP and only downloads the library if it is not available on the system.
Furthermore, CMake's [FetchContent](https://cmake.org/cmake/help/latest/module/FetchContent.html) module allows for lots of flexibility in how the library is integrated into the project.

```cmake
include(FetchContent)
set(FETCH_PACKAGES "")

# cmake-format: off
set(MQT_QMAP_VERSION 3.0.0
    CACHE STRING "MQT QMAP version")
set(MQT_QMAP_REV "v3.0.0"
    CACHE STRING "MQT QMAP identifier (tag, branch or commit hash)")
set(MQT_QMAP_REPO_OWNER "cda-tum"
    CACHE STRING "MQT QMAP repository owner (change when using a fork)")
# cmake-format: on
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
  FetchContent_Declare(
    mqt-qmap
    GIT_REPOSITORY https://github.com/${MQT_QMAP_REPO_OWNER}/mqt-qmap.git
    GIT_TAG ${MQT_QMAP_REV}
    FIND_PACKAGE_ARGS ${MQT_QMAP_VERSION})
  list(APPEND FETCH_PACKAGES mqt-qmap)
else()
  find_package(mqt-qmap ${MQT_QMAP_VERSION} QUIET)
  if(NOT mqt-qmap_FOUND)
    FetchContent_Declare(
      mqt-qmap
      GIT_REPOSITORY https://github.com/${MQT_QMAP_REPO_OWNER}/mqt-qmap.git
      GIT_TAG ${MQT_QMAP_REV})
    list(APPEND FETCH_PACKAGES mqt-qmap)
  endif()
endif()

# Make all declared dependencies available.
FetchContent_MakeAvailable(${FETCH_PACKAGES})
```

:::

:::{tab-item} git submodule

Integrating the library as a git submodule is the simplest approach.
However, handling git submodules can be cumbersome, especially when working with multiple branches or versions of the library.
First, add the submodule to your project (e.g., in the `external` directory) via:

```console
$ git submodule add https://github.com/cda-tum/mqt-qmap.git external/mqt-qmap
```

Then, add the following lines to your `CMakeLists.txt` to make the library's targets available in your project:

```cmake
add_subdirectory(external/mqt-qmap)
```

:::

::::
