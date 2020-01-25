# luma_av
## Clangd set up
- The update alternatives command isnt being run in the dockerfile so 
you need to tell the vscode-clangd plugin where the clangd-7 executable is.
The location for the provided docker image is /usr/bin/clangd-7.

I have put the clangd settings in the .vscode settings folder so they should be
good to go on start up but I would double check just in case.
## Building with conan
- ```conan install . -if build --build missing```
- ```conan source . -sf build```
- ```conan build . -bf build```
- I updated the tasks.json with these commands. These are conan commands used to 
develop locally in the project directory or wherever else and not in the root 
conan data folder.