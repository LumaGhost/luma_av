
## Intro

Docker is not required for using or contributing to this project, but this readme documents our current development setup with docker.

## Images

### clang_dev

Includes latest versions of clang and gcc, and development and build tools like gdb and conan. as well as a default conan setup.

#### Commands for Development:

if you are familiar with conan feel free to use your own commands (:

Install Dependencies:
```
conan install . --profile clang-libcxx -if build --build missing -s build_type=Debug
```
note: you can omit the profile argument to build with gcc instead

Build:
```
conan build . -bf build
```
note: if you want to switch between building with clang and gcc first delete your build folder and rerun `conan install`

#### Clangd vscode setup

Add the following to you devcontainer.json

```
"extensions": [
		"llvm-vs-code-extensions.vscode-clangd"
]
```

Add the following to your settings.json (assuming you're using the clang_dev image and ./build as your build folder)

```
"clangd.path":"/llvm-install/bin/clangd"
"clangd.arguments": ["-compile-commands-dir=/workspaces/luma_av/build"]
```
Note: clangd will work for some things but probably wont be accurate until we can fully compile the project with clang

Troubleshootng: for the best results, do the following after making any changes to clangd config or the build enviornment: rebuild the contaner and call conan install and conan build before opening your first cpp source/header file. Note: I think closing all open source files will also work to restart/refresh clangd.

#### Formatting with vscode and clang-format

formatting with vscode "just works" with the c++ extension in vscode due to the clang-format file in the project root. the following link shares some more information on how to format with the c++ extension in vscode https://code.visualstudio.com/docs/cpp/cpp-ide#_code-formatting and the following link explains how to customize the keybindings https://code.visualstudio.com/docs/getstarted/keybindings#_keyboard-shortcuts-editor

unfortunately, I think at this time the vscode c++ extension is using its own copy of clang format and not the newer version in the image. this is something we'll ideally address in the future but isnt a priority for now since the formatting still works. 

alternatively, you can use the following command to format your changes (git diff) automatically using the clang format binary in the image.

```
python3 /llvm-install/scripts/clang-format-diff.py -sort-includes -binary /llvm-install/bin/clang-format
```

#### Running clang-tidy

these commands use the install location for the clang_dev image, but they can of course be adapted for other install locations

to run clang-tidy on the whole project run the following from the build folder (or wherever youve placed the compile commands json)

```
python3 /llvm-install/scripts/run-clang-tidy.py -clang-tidy-binary /llvm-install/bin/clang-tidy
```
note: this command will run checks based on the .clang-tidy file in the project root. 

to automatically apply fixes, add the following to your command. this will also apply our formatting to any code thats auto fixed.

```
-fix -clang-apply-replacements-binary /llvm-install/bin/clang-apply-replacements -format
```

see `run-clang-tidy.py --help` and https://clang.llvm.org/extra/clang-tidy/ for more info (: 


you can also add a task to your vscode task.json using the following as an example

```
{
            "label": "clang-tidy",
            "type": "shell",
            "command": "python3 /llvm-install/scripts/run-clang-tidy.py -clang-tidy-binary /llvm-install/bin/clang-tidy",
            "options": {
                "cwd": "${workspaceRoot}/build"
            },
            "problemMatcher": []
}
```

note: there may be a better way, but I ususally have to ask vscode to rebuild the container in order for task.json changes to take affect. 

