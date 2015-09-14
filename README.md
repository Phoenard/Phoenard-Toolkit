# Phoenard-Toolkit
Access the Phoenard operation using the computer and make use of handy utilities.
Project created and compiled using Qt Creator 5.1 or later. To build the project,
install Qt Creator 5.1 or later.

## Features
- Serial monitor with LCD screen readout (hooks into library)
- Micro-SD listing, reading and writing
- Sketch list listing, reading and writing, including a basic icon editor
- (Chip) Control: Reading and writing registers to remotely control ports/timers/etc.
- Pinmapped control: Setting up digital pins (input/output/low/high) and reading ADC pins
- Image converter: Convert PNG/BMP/JPG into a palette-based 1/2/4/8/16-bit color format
  Reverse conversion is also possible.
- Uploading HEX Files to the device, including firmware updates

## Compiling
To launch the Qt Creator project, follow these steps:

1. Install Qt Creator: http://www.qt.io/download-open-source
2. Download the source code (as zip or using git)
3. Open src/Phoenard_Toolkit.pro using Qt Creator
4. Select the appropriate kit:
  - Windows: Desktop Qt 5.1+ MSVC2012 (or later) OpenGL 32bit
  - Linux: Desktop Qt 5.1+ GCC 32/64bit
  - Mac OS X: [untested]
5. Switch to Release mode
6. Use the Run button to compile and execute the program

## License
The MIT License (MIT)

Copyright (c) 2015 Phoenard

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.