# luma_av
still working on initial ideas and conception. not an actual library yet.
## Clangd set up
- The update alternatives command isnt being run in the dockerfile so 
you need to tell the vscode-clangd plugin where the clangd-7 executable is.
The location for the provided docker image is /usr/bin/clangd-7.
-I have put the clangd settings in the .vscode settings folder so they should be
good to go on start up but I would double check just in case.
- Im not sure if clangd is able to work on header only libraries so you may
want to create some cpp files in order to get the compile commands json
database to update and have somthing for clangd to use.
## Building with conan
- ```conan install . -if build --build missing```
- ```conan build . -bf build```
- I updated the tasks.json with these commands.