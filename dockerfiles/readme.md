
## Intro

Docker is not required for using or contributing to this project, but this readme documents our current development setup with docker.

## Images

### clang_dev

Includes latest versions of clang and gcc, and development and build tools like gdb and conan. as well as a default conan setup.

#### Commands for Development:

Install Dependencies:
```
conan install . --profile clang -if build --build missing -s build_type=Debug
```
note: you can omit the profile argument to build with gcc instead

Build:
```
conan build . -bf build
```
note: if you want to switch between building with clang and gcc first delete your build folder and rerun `conan install`

#### Clangd vscode setup

Add the following to your settings.json in vscode (assuming you're using the clang_dev image)

```
"clangd.path":"/llvm-project/build/bin/clangd"
```

### gcc_dev (deprecated)

The same idea as clang_dev but much older compiler versions and also no profile for building for clang. This setup is deprecated and is going to be removed once we're fully comfortable with clang_dev (:

#### Commands:

Install Dependencies:
```
conan install . -if build --build missing -s build_type=Debug
```

Build:
```
conan build . -bf build
```
