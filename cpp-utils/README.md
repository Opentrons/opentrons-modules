# opentrons-cpp-utils

This is a repository containing C++ code that may be useful across various c/c++ projects used at Opentrons.

If you are at opentrons and you are reading this file somewhere other than the web interface for https://github.com/Opentrons/opentrons-cpp-utils , it is almost certainly checked into your project with [git subtree](https://www.atlassian.com/git/tutorials/git-subtree), and *you should not check in changes in your own repo's source.* Instead, you should open a pull request on opentrons-cpp-utils.

## Building and Testing

This repo contains a CMake build system for compiling, testing, formatting, and linting the contained code. This build system is _not_ intended to directly integrate with a host project. See __Including sources & headers in the host project__

## Updating opentrons-cpp-utils subtree in your project

When you want to get changes from opentrons-cpp-utils, you can update your local subtree and commit that change into either your branch or your mainline:
`git checkout -b chore_update-opentrons-cpp-utils`
`git subtree pull --prefix cpp-utils opentrons-cpp-utils main --squash`
`git push --set-upstream origin chore_update-opentrons-cpp-utils`
Then open a PR in your local repo.

The subtree checkout should be managed like any other file and changed only in pull requests.

## Testing changes to other projects

Since there's no indication in this repo of which projects use this repo, we can only do a best-effort attempt to test the cmake changes here with other projects. Please make this best-effort attempt! This can be done by either copying the changes to those other projects temporarily, or doing a `git subtree pull` in those other projects to the branch you're testing here.

## How should I use this in my project?

Adding this project by subtree is a better way to work than adding it as a submodule because it will conform more closely to the way this project is developed. If you add this project by subtree to another (this other project will be called the "__host project__" after this), then the host project controls which commit is subtreed in - it's whatever commit was specified the last time a `git subtree pull` was done in the host project. If `git subtree pull` specified a branch, then the commit that was the head of that branch at the time of the `git subtree pull` will be checked in and never changed.

However, if the host project uses `git submodule add` to add this repo, then it adds it by symbolic name, which means that subsequent `git submodule update`s may change the commit used in the host project without changing the host project's tracked state. Since this project's `main` branch might be updated anytime, it is generally not a good idea to do this. So use `git subtree`, and anytime you do a `git subtree pull` make sure to test the results.

## Including sources & headers in the host project

__Do not__ integrate the subtree using CMake's `add_subdirectory()` function. Instead, directly add the `.cpp` files in your project's build targets, and add the `include` directory to the include path of your proejct's build targets as appropriate.
