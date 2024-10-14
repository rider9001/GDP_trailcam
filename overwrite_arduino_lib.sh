#!/usr/bin/env bash

# Arduino demands that all of its components be placed in its libraries folder
# This script moves all components into that folder and overwrites any existing folder data
cp -arf src/. $HOME/Arduino/libraries