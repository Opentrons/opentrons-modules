# cmake-utils

This is a repository containing cmake scripts useful in embedded c/c++ projects used at opentrons.

If you are at opentrons and you are reading this file somewhere other than the web interface for https://github.com/Opentrons/cmake-utils , it is almost certainly checked into your project with [git subtree](https://www.atlassian.com/git/tutorials/git-subtree), and *you should not check in changes in your own repo's source.* Instead, you should open a pull request on cmake-utils.

## Updating cmake-utils subtree in your project

When you want to get changes from cmake-utils, you can update your local subtree and commit that change into either your branch or your mainline:
`git checkout -b chore_update-cmake-utils`
`git subtree pull --prefix cmake cmake-utils main --squash`
`git push --set-upstream origin chore_update-cmake-utils`
Then open a PR in your local repo.

The subtree checkout should be managed like any other file and changed only in pull requests.

## Testing changes to other projects

Since there's no indication in this repo of which projects use this repo, we can only do a best-effort attempt to test the cmake changes here with other projects. Please make this best-effort attempt! This can be done by either copying the changes to those other projects temporarily, or doing a `git subtree pull` in those other projects to the branch you're testing here.

## How should I use this in my project?

The CMake modules here are designed for reuse, but to reuse cmake modules they have to be somewhere that cmake can find them. You should either submodule or subtree this repo into where you want to use it, probably in a directory called `cmake`. You can then add the directory to cmake's module path in your top-level `CMakeList.txt` (or whichever `CMakeList.txt` you want to use the utils in if it's not the top level):

`set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})`

Adding this project by subtree is a better way to work than adding it as a submodule because it will conform more closely to the way this project is developed. If you add this project by subtree to another (this other project will be called the "host project" after this), then the host project controls which commit is subtreed in - it's whatever commit was specified the last time a `git subtree pull` was done in the host project. If `git subtree pull` specified a branch, then the commit that was the head of that branch at the time of the `git subtree pull` will be checked in and never changed.

However, if the host project uses `git submodule add` to add this repo, then it adds it by symbolic name, which means that subsequent `git submodule update`s may change the commit used in the host project without changing the host project's tracked state. Since this project's `main` branch might be updated anytime, it is generally not a good idea to do this. So use `git subtree`, and anytime you do a `git subtree pull` make sure to test the results.

## What will the modules here do?

The modules in this repo are mostly designed to download and create build systems for various native code dependencies; there are also a couple that download and create synthetic targets for various useful tools. The exact way to use each module depends on what that module is.

Modules that download dependencies or tools will download to a subdirectory called `stm32-tools/${name}/${system}`. For instance, `FindSTM32F303BSP` will download the BSP to `stm32-tools/stm32f303bsp/Linux` on Linux, or `stm32-tools/stm32f303bsp/Darwin` on OSX. Modules whose names start with `Find` generally adhere to the expectations of a [cmake find package module](https://cmake.org/cmake/help/v3.19/manual/cmake-packages.7.html) and set the appropriate cache variables.
