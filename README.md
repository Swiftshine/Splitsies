# Splitsies

Honestly, I just wanted an easy to use file splitter, so why not make it myself?

## Setup

Ideally you would have access to this application regardless of what your current working directory is.

On Windows, add an environment variable consisting of the application's directory.

## Usage

### Splitting
`splitsies -split -filename="my_file_name" -size=my_target_size`

### Unsplitting
`splitsies -unsplit -foldername="my_folder_name" -filename="optional_specified_name.bin"`

If a folder name is unspecified, the current working directory will be used instead.
## Building
This project is minimalistic. No special process is needed, compile it how you would any other C++ program.

Splitsies uses [argh](https://github.com/adishavit/argh). Be sure to include "arg.h" in your compilation.
